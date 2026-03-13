// =============================================================================
// DEVICE UTILITY FUNCTIONS - Implementation
// =============================================================================

#define NOMINMAX
#include <windows.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

extern "C" {
#include <hidsdi.h>
}

#include "device_utils.h"
#include "mouse_device.h"
#include "known_devices.h"
#include "device_ids.h"

// Extract VID/PID from device name
void ExtractVidPid(const std::string& deviceName, std::string& vid, std::string& pid) {
    std::string upper = deviceName;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c){ return (char)std::toupper(c); });
    
    // Try VID_ format first (most common)
    auto vidPos = upper.find("VID_");
    if (vidPos != std::string::npos && vidPos + 8 <= upper.size()) {
        vid = upper.substr(vidPos + 4, 4);
    }
    
    // Try PID_ format first (most common)
    auto pidPos = upper.find("PID_");
    if (pidPos != std::string::npos && pidPos + 8 <= upper.size()) {
        pid = upper.substr(pidPos + 4, 4);
    }
    
    // Alternative formats: VID& or VID# 
    if (vid.empty()) {
        vidPos = upper.find("VID&");
        if (vidPos == std::string::npos) vidPos = upper.find("VID#");
        if (vidPos != std::string::npos && vidPos + 8 <= upper.size()) {
            vid = upper.substr(vidPos + 4, 4);
        }
    }
    
    if (pid.empty()) {
        pidPos = upper.find("PID&");
        if (pidPos == std::string::npos) pidPos = upper.find("PID#");
        if (pidPos != std::string::npos && pidPos + 8 <= upper.size()) {
            pid = upper.substr(pidPos + 4, 4);
        }
    }
}

// Utility: convert wide string to UTF-8
std::string WideToUtf8(const std::wstring &w) {
    if (w.empty()) return std::string();
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), result.data(), sizeNeeded, nullptr, nullptr);
    return result;
}

bool IsLikelyTouchpadName(const std::string &nameUtf8) {
    std::string s = nameUtf8;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });

    // Comprehensive touchpad keyword detection
    const char* hints[] = {
        // Generic touchpad terms
        "touchpad", "trackpad", "precision", "clickpad", "glidepad", "glidepoint",
        // Manufacturer names
        "synaptics", "syna", "elan", "alps", "goodix", "focaltech", "cirque", "sentelic",
        // Device path patterns (using # separator as used in Windows HID paths)
        "etd", "i2c", "hid\\", "acpi\\", "acpi#", "hid_device_system_touchpad",
        // Dell-specific
        "delltp", "dell touchpad",
        // ACPI patterns
        "pnp0c50", "msft0001", "acpi0c50",
        // Additional patterns for I2C devices
        "i2c_hid", "hid-over-i2c",
        // Precision touchpad patterns
        "ptp", "precision touch"
    };
    for (const char* h : hints) {
        if (s.find(h) != std::string::npos) return true;
    }

    // Also check using the ACPI pattern database from device_ids.h
    if (HasTouchpadACPIPattern(nameUtf8)) {
        return true;
    }

    return false;
}



bool DeviceHasTouchpadUsage(HANDLE deviceHandle) {
    RID_DEVICE_INFO info{};
    info.cbSize = sizeof(info);
    UINT size = sizeof(info);
    if (GetRawInputDeviceInfoW(deviceHandle, RIDI_DEVICEINFO, &info, &size) == (UINT)-1) {
        return false;
    }

    if (info.dwType == RIM_TYPEHID) {
        // HID digitizer usages 0x05 (touch pad) and 0x04 (touch screen)
        if (info.hid.usUsagePage == 0x0D && (info.hid.usUsage == 0x05 || info.hid.usUsage == 0x04)) {
            return true;
        }
    }

    return false;
}

// Get VID/PID directly from device handle (works even when device path is unavailable)
bool GetVidPidFromHandle(HANDLE deviceHandle, std::string& vid, std::string& pid) {
    RID_DEVICE_INFO info{};
    info.cbSize = sizeof(info);
    UINT size = sizeof(info);
    if (GetRawInputDeviceInfoW(deviceHandle, RIDI_DEVICEINFO, &info, &size) == (UINT)-1) {
        return false;
    }

    if (info.dwType == RIM_TYPEHID || info.dwType == RIM_TYPEMOUSE) {
        DWORD vendorId = 0, productId = 0;
        if (info.dwType == RIM_TYPEHID) {
            vendorId = info.hid.dwVendorId;
            productId = info.hid.dwProductId;
        } else if (info.dwType == RIM_TYPEMOUSE) {
            // RIM_TYPEMOUSE doesn't have VID/PID in RID_DEVICE_INFO,
            // but we can try to get it from the device name
            return false;
        }

        if (vendorId != 0) {
            std::ostringstream vidss, pidss;
            vidss << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << vendorId;
            pidss << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << productId;
            vid = vidss.str();
            pid = pidss.str();
            return true;
        }
    }

    return false;
}

std::string GetDeviceNameUtf8(HANDLE deviceHandle) {
    UINT size = 0;
    if (GetRawInputDeviceInfoW(deviceHandle, RIDI_DEVICENAME, nullptr, &size) != 0 || size == 0) {
        return "";
    }
    std::wstring wname(size, L'\0');
    if (GetRawInputDeviceInfoW(deviceHandle, RIDI_DEVICENAME, &wname[0], &size) == (UINT)-1) {
        return "";
    }
    // Trim possible trailing nulls
    if (!wname.empty() && wname.back() == L'\0') {
        while (!wname.empty() && wname.back() == L'\0') wname.pop_back();
    }
    return WideToUtf8(wname);
}

void GetHidAttributes(const std::string& devicePath, std::string& manufacturer, std::string& product) {
    manufacturer.clear();
    product.clear();
    
    if (devicePath.empty()) {
        return;
    }
    
    // Convert device path to wide string for CreateFile
    std::wstring wDevicePath;
    int len = MultiByteToWideChar(CP_UTF8, 0, devicePath.c_str(), -1, nullptr, 0);
    if (len > 0) {
        wDevicePath.resize(len - 1);
        MultiByteToWideChar(CP_UTF8, 0, devicePath.c_str(), -1, &wDevicePath[0], len);
    } else {
        return;
    }
    
    // Open the device handle
    HANDLE hDevice = CreateFileW(
        wDevicePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    
    if (hDevice != INVALID_HANDLE_VALUE) {
        // Get manufacturer string
        WCHAR manufacturerBuffer[256] = {0};
        if (HidD_GetManufacturerString(hDevice, manufacturerBuffer, sizeof(manufacturerBuffer))) {
            manufacturer = WideToUtf8(manufacturerBuffer);
        }
        
        // Get product string
        WCHAR productBuffer[256] = {0};
        if (HidD_GetProductString(hDevice, productBuffer, sizeof(productBuffer))) {
            product = WideToUtf8(productBuffer);
        }
        
        CloseHandle(hDevice);
    }
    
    // Always try VID/PID lookup as fallback if manufacturer or product is still empty
    if (manufacturer.empty() || product.empty()) {
        std::string vid, pid;
        ExtractVidPid(devicePath, vid, pid);
        if (!vid.empty() && !pid.empty()) {
            std::string lookupManufacturer, lookupProduct;
            LookupKnownDevice(vid, pid, lookupManufacturer, lookupProduct);
            if (manufacturer.empty() && !lookupManufacturer.empty()) {
                manufacturer = lookupManufacturer;
            }
            if (product.empty() && !lookupProduct.empty()) {
                product = lookupProduct;
            }
        }

        // VID-only fallback: if exact VID+PID wasn't in the LUT, at least
        // resolve the manufacturer name from the VID databases
        if (manufacturer.empty() && !vid.empty()) {
            const char* jigglerMfr = GetJigglerManufacturer(vid);
            if (jigglerMfr) manufacturer = jigglerMfr;
        }
        if (manufacturer.empty() && !vid.empty()) {
            const char* legitMfr = GetLegitimateManufacturer(vid);
            if (legitMfr) manufacturer = legitMfr;
        }
    }
}

std::string BuildDisplayName(const std::string &rawName, size_t index, const std::string& manufacturer, const std::string& product) {
    std::ostringstream oss;
    oss << "Mouse " << index;

    if (rawName.empty()) return oss.str();

    std::string upper = rawName;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c){ return (char)std::toupper(c); });

    auto vidPos = upper.find("VID_");
    auto pidPos = upper.find("PID_");
    std::string vid, pid;
    bool hasVidPid = false;
    if (vidPos != std::string::npos && pidPos != std::string::npos &&
        vidPos + 8 <= upper.size() && pidPos + 8 <= upper.size()) {
        vid = rawName.substr(vidPos + 4, 4);
        pid = rawName.substr(pidPos + 4, 4);
        hasVidPid = true;
    }

    // Build the information part
    bool hasFriendlyName = !manufacturer.empty() || !product.empty();
    if (hasFriendlyName || hasVidPid) {
        oss << " (";

        if (hasFriendlyName) {
            // Show manufacturer and product - no VID/PID clutter for normal users
            if (!manufacturer.empty()) {
                oss << manufacturer;
            }
            if (!product.empty()) {
                if (!manufacturer.empty()) {
                    oss << " ";
                }
                oss << product;
            }
        } else if (hasVidPid) {
            // Only show VID/PID as fallback when no friendly name is available
            oss << "VID " << vid << " PID " << pid;
        }

        oss << ")";
    }

    return oss.str();
}

// Check if device VID/PID matches known touchpad devices
bool IsTouchpadByVID(const std::string& devicePath) {
    std::string vid, pid;
    ExtractVidPid(devicePath, vid, pid);

    if (vid.empty()) return false;

    // Check if it's a known touchpad device by VID/PID
    if (!pid.empty() && IsKnownTouchpadDevice(vid, pid)) {
        return true;
    }

    // Check if the VID belongs to a touchpad-only manufacturer
    // (Synaptics 06CB, ELAN 04F3, Alps 044E, Goodix 27C6, FocalTech 2808, Cirque 0488)
    std::string upperVid = vid;
    std::transform(upperVid.begin(), upperVid.end(), upperVid.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });

    // These VIDs are touchpad-only manufacturers (never make standalone mice)
    // NOTE: PixArt (093A) is intentionally excluded - they make both touchpad
    // controllers AND mouse sensors (e.g. USB Optical Mouse PID 2510).
    if (upperVid == "06CB" || upperVid == "04F3" || upperVid == "044E" ||
        upperVid == "27C6" || upperVid == "2808" || upperVid == "0488" ||
        upperVid == "1EA8" || upperVid == "04B4") {
        return true;
    }

    return false;
}

bool IsTouchpadDevice(HANDLE deviceHandle, const RAWINPUT* rawMouse, MouseDevice* existing) {
    // Some touchpad/legacy pointer paths report with null device handle.
    if (deviceHandle == nullptr) {
        return true;
    }

    const bool usageHint = DeviceHasTouchpadUsage(deviceHandle);

    std::string nameUtf8;
    if (existing && !existing->name.empty()) {
        nameUtf8 = existing->name;
    } else {
        nameUtf8 = GetDeviceNameUtf8(deviceHandle);
    }
    const bool nameHint = IsLikelyTouchpadName(nameUtf8);
    bool absoluteHint = rawMouse && (rawMouse->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE);

    if (existing) {
        absoluteHint = absoluteHint || (existing->absoluteMoveCount > 0);
    }

    // NEW: Check VID-based touchpad detection
    const bool vidHint = IsTouchpadByVID(nameUtf8);

    // Check if this is an internal (non-USB) HID device by looking for
    // absence of VID_/PID_ in the path. Internal I2C/ACPI devices that
    // register as mice are almost always touchpads.
    // IMPORTANT: Bluetooth mice must NOT be caught by this check.
    //   - Bluetooth Classic HID uses "VID&" format (e.g. VID&02046D), not "VID_"
    //   - Bluetooth LE HID uses "BTHLE" paths with no VID_ at all
    // Both cases would be incorrectly flagged as internal touchpads without
    // the explicit Bluetooth path exclusion and the VID& check below.
    bool internalDeviceHint = false;
    if (!nameUtf8.empty()) {
        std::string upperPath = nameUtf8;
        std::transform(upperPath.begin(), upperPath.end(), upperPath.begin(),
                       [](unsigned char c) { return (char)std::toupper(c); });
        // Accept VID in all formats: VID_ (USB), VID& (Bluetooth Classic), VID# (other)
        bool hasVidPid = upperPath.find("VID_") != std::string::npos ||
                         upperPath.find("VID&") != std::string::npos ||
                         upperPath.find("VID#") != std::string::npos;
        bool isHidDevice = upperPath.find("HID") != std::string::npos;
        // Bluetooth device paths contain BTH or BTHLE — never treat them as
        // internal touchpads regardless of whether VID is present in the path.
        bool isBluetooth = upperPath.find("BTHLE") != std::string::npos ||
                           upperPath.find("BTH\\") != std::string::npos ||
                           upperPath.find("BLUETOOTH") != std::string::npos;
        if (isHidDevice && !hasVidPid && !isBluetooth) {
            // HID device without any VID/PID and not Bluetooth = internal I2C/ACPI touchpad
            internalDeviceHint = true;
        }
    }

    return usageHint || nameHint || absoluteHint || vidHint || internalDeviceHint;
}

// Calculate standard deviation for a device's movements
double CalculateMovementStdDev(const MouseDevice& device, bool isX) {
    if (device.movements.size() < 2) return 0.0;
    
    // Calculate deltas
    std::vector<double> deltas;
    for (size_t i = 1; i < device.movements.size(); ++i) {
        double delta = isX ? 
            std::abs(device.movements[i].x - device.movements[i-1].x) :
            std::abs(device.movements[i].y - device.movements[i-1].y);
        deltas.push_back(delta);
    }
    
    if (deltas.empty()) return 0.0;
    
    // Calculate mean
    double sum = 0.0;
    for (double d : deltas) {
        sum += d;
    }
    double mean = sum / deltas.size();
    
    // Calculate variance
    double variance = 0.0;
    for (double d : deltas) {
        variance += (d - mean) * (d - mean);
    }
    variance /= deltas.size();
    
    // Return standard deviation
    return sqrt(variance);
}

// URL Encoding Helper Function
std::string UrlEncode(const std::string& value) {
    std::ostringstream encoded;
    for (unsigned char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else if (c == ' ') {
            encoded << "%20";
        } else {
            encoded << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(c);
        }
    }
    return encoded.str();
}
