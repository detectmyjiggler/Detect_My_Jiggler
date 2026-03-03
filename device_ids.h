#pragma once
// =============================================================================
// COMPREHENSIVE DEVICE VID/PID DATABASE
// =============================================================================
// This file contains known VIDs for:
// 1. Known mouse jiggler devices (BLOCKLIST - high suspicion)
// 2. DIY microcontroller platforms commonly used for jigglers
// 3. Legitimate mouse manufacturers (ALLOWLIST - trusted)
// 4. Touchpad manufacturers (for filtering out touchpads)
// =============================================================================

#include <string>
#include <unordered_set>
#include <algorithm>

// =============================================================================
// KNOWN JIGGLER VIDS - BLOCKLIST (Immediate high suspicion)
// =============================================================================
// These VIDs are associated with known jiggler manufacturers or DIY platforms
// commonly used to build mouse jigglers.

struct JigglerVID {
    const char* vid;
    const char* manufacturer;
    const char* risk_level;  // "CRITICAL", "HIGH", "MEDIUM"
};

const JigglerVID KNOWN_JIGGLER_VIDS[] = {
    // =========================================================================
    // CRITICAL RISK - Known commercial jiggler manufacturers
    // =========================================================================
    {"0E90", "CRU/WiebeTech", "CRITICAL"},  // Primary commercial jiggler manufacturer (Mouse Jiggler MJ-1, MJ-3)

    // =========================================================================
    // HIGH RISK - DIY microcontroller platforms commonly used for jigglers
    // =========================================================================
    {"16C0", "V-USB/Digispark/Teensy", "HIGH"},      // Most common DIY jiggler platform (ATtiny85 Digispark)
    {"2341", "Arduino SA", "HIGH"},                   // Arduino Leonardo, Pro Micro
    {"2A03", "Arduino SA (alt)", "HIGH"},             // Arduino alternate VID
    {"1B4F", "SparkFun", "HIGH"},                     // SparkFun Pro Micro

    // =========================================================================
    // MEDIUM RISK - Other microcontroller platforms that could be jigglers
    // =========================================================================
    {"2E8A", "Raspberry Pi", "MEDIUM"},               // Raspberry Pi Pico
    {"303A", "Espressif", "MEDIUM"},                  // ESP32-S2/S3
    {"239A", "Adafruit", "MEDIUM"},                   // Adafruit CircuitPython boards
    {"0483", "STMicroelectronics", "MEDIUM"},         // STM32 Blue Pill
    {"1A86", "WCH (Jiangsu Qinheng)", "MEDIUM"},      // CH552/CH32V003 chips
    {"2886", "Seeed Studio", "MEDIUM"},               // Seeeduino XIAO
    {"1209", "pid.codes (Open Source)", "MEDIUM"},    // Open source hardware projects
    {"03EB", "Atmel/Microchip (LUFA)", "MEDIUM"},     // LUFA library projects
    {"CAFE", "TinyUSB", "MEDIUM"},                    // TinyUSB development
    {"04D8", "Microchip", "MEDIUM"},                  // SAMD21 HID examples
    {"1FC9", "NXP", "MEDIUM"},                        // LPC microcontrollers

    // =========================================================================
    // SUSPICIOUS - Generic/Unknown VIDs
    // =========================================================================
    {"0000", "Unknown/Failed Enumeration", "HIGH"},   // Failed USB enumeration or counterfeit
    {"FFFF", "Invalid VID", "HIGH"},                  // Invalid VID

    // Null terminator
    {nullptr, nullptr, nullptr}
};

// Known jiggler PIDs for CRU/WiebeTech (VID 0E90)
const char* const KNOWN_JIGGLER_PIDS_0E90[] = {
    "0028",  // Mouse Jiggler MJ-1
    "0045",  // Mouse Jiggler MJ-3
    nullptr
};

// =============================================================================
// LEGITIMATE MOUSE MANUFACTURER VIDS - ALLOWLIST (Trusted)
// =============================================================================
// These VIDs belong to known legitimate mouse/peripheral manufacturers.
// Devices with these VIDs receive lower suspicion scores (but behavior still matters).

struct LegitimateVID {
    const char* vid;
    const char* manufacturer;
};

const LegitimateVID LEGITIMATE_MOUSE_VIDS[] = {
    // ==========================================================================
    // MAJOR GAMING PERIPHERAL MANUFACTURERS
    // ==========================================================================
    {"046D", "Logitech"},
    {"045E", "Microsoft"},
    {"1532", "Razer"},
    {"1689", "Razer"},           // Razer alternate VID
    {"1038", "SteelSeries"},
    {"1B1C", "Corsair"},
    {"0951", "HyperX/Kingston"},
    {"1E7D", "Roccat"},
    {"3057", "Zowie/BenQ"},
    {"0489", "BenQ"},            // BenQ corporate VID
    {"258A", "Glorious/Sinowealth"},  // Sinowealth - used by Glorious, G-Wolves, many OEM mice
    {"361D", "Finalmouse"},
    {"33B6", "Pulsar"},
    {"346D", "Lamzu"},
    {"3367", "Endgame Gear"},
    {"2516", "Cooler Master"},
    {"0B05", "ASUS"},
    {"3511", "Ninjutso"},
    {"3554", "G-Wolves"},
    {"09DA", "A4Tech/Bloody"},
    {"3842", "EVGA"},
    {"05AC", "Apple"},
    {"1E4E", "Cubeternet"},       // Various gaming mice
    {"25A7", "Areson"},
    {"276D", "YSTEK"},
    {"0C45", "Microdia"},         // Various mice sensors
    {"1BCF", "Sunplus"},          // Various mice
    {"1915", "Nordic Semiconductor"},  // Wireless mice chips
    {"1EA7", "SHARKOON"},
    {"3297", "Vaxee"},
    {"34D3", "Xtrfy"},

    // ==========================================================================
    // OEM / ENTERPRISE MANUFACTURERS
    // ==========================================================================
    {"413C", "Dell"},
    {"03F0", "HP"},
    {"17EF", "Lenovo"},
    {"0461", "Primax/HP"},
    {"04CA", "Lite-On"},
    {"0458", "KYE/Genius"},
    {"04B3", "IBM"},
    {"18F8", "Xiaomi"},
    {"248A", "Maxxter"},
    {"28BD", "Holtek/Redragon"},
    {"3938", "MOSART/Redragon"},
    {"04D9", "Holtek"},           // Generic HID
    {"1C4F", "SiGma Micro"},      // Budget mice
    {"10C4", "Silicon Labs"},
    {"1B80", "Afatech"},
    {"062A", "MosArt Semiconductor"},
    {"04FC", "Sunplus"},
    {"1241", "Belkin"},
    {"2188", "No brand"},         // Generic mice
    {"1A2C", "China Resource Semico"},
    {"3151", "YICHIP"},
    {"093A", "Pixart"},           // Mouse sensors

    // ==========================================================================
    // ADDITIONAL PC/LAPTOP MANUFACTURERS
    // ==========================================================================
    {"0502", "Acer"},
    {"1462", "MSI"},
    {"1044", "Gigabyte"},
    {"1D6B", "Linux Foundation"},  // USB gadgets
    {"04E8", "Samsung"},
    {"2717", "Xiaomi"},           // Xiaomi alternate
    {"2A70", "OnePlus"},
    {"22B8", "Motorola"},
    {"0BB4", "HTC"},
    {"18D1", "Google"},
    {"1949", "Lab126/Amazon"},
    {"2833", "Oculus VR"},
    {"054C", "Sony"},
    {"057E", "Nintendo"},
    {"28DE", "Valve"},
    {"0738", "Mad Catz"},
    {"06A3", "Saitek"},
    {"0079", "DragonRise"},       // Generic gamepads
    {"0583", "Padix"},
    {"07B5", "Mega World"},
    {"0810", "Personal Communication Systems"},
    {"20D6", "PowerA"},
    {"24C6", "Thrustmaster"},
    {"2563", "ShenZhen ShanWan"},
    {"2DC8", "8BitDo"},
    {"2F24", "JieLi"},

    // ==========================================================================
    // KEYBOARD MANUFACTURERS (may also make mice)
    // ==========================================================================
    {"05A4", "Ortek"},
    {"05AF", "Jing-Mold Enterprise"},
    {"0603", "Novatek"},
    {"0A5C", "Broadcom"},
    {"0D62", "Darfon Electronics"},
    {"1050", "Yubico"},           // Security keys
    {"1A81", "Holtek"},           // Keyboards
    {"1D57", "Xenta"},
    {"2F68", "Miiiw"},
    {"320F", "Glorious/GMMK"},    // Glorious keyboards
    {"3233", "Keychron"},
    {"3434", "Keychron"},         // Keychron alternate
    {"4B4D", "Keymacs"},
    {"FFC0", "Ducky"},
    {"0416", "Winbond"},
    {"046A", "Cherry"},
    {"04F2", "Chicony"},
    {"0518", "EzKEY"},
    {"0566", "Monterey"},

    // ==========================================================================
    // OTHER INPUT DEVICE MANUFACTURERS
    // ==========================================================================
    {"056A", "Wacom"},            // Drawing tablets
    {"256C", "HUION"},            // Drawing tablets
    {"28BD", "XP-Pen"},           // Drawing tablets
    {"1B96", "N-Trig"},           // Touch/pen digitizers
    {"0457", "Silicon Integrated Systems"},
    {"044F", "ThrustMaster"},     // Joysticks/wheels
    {"046D", "Logitech"},         // Also makes wheels/joysticks
    {"0E8F", "GreenAsia"},        // Generic gamepads
    {"11FF", "Wisetech"},
    {"12BD", "Gembird"},
    {"145F", "Trust"},
    {"1BAD", "Harmonix"},         // Rock Band controllers
    {"1430", "RedOctane"},        // Guitar Hero controllers
    {"0F0D", "Hori"},             // Fighting game controllers
    {"2563", "ShenZhen ShanWan"},
    {"0D8C", "C-Media"},          // Audio (some combo devices)
    {"1235", "Focusrite"},        // Audio interfaces
    {"0582", "Roland"},           // MIDI/Audio
    {"1397", "BEHRINGER"},        // Audio
    {"194F", "PreSonus"},         // Audio
    {"07FD", "Mark of the Unicorn"},
    {"2516", "Cooler Master"},

    // ==========================================================================
    // WIRELESS RECEIVER MANUFACTURERS
    // ==========================================================================
    {"1915", "Nordic Semiconductor"},
    {"8087", "Intel"},            // Bluetooth
    {"0A12", "Cambridge Silicon Radio"}, // Bluetooth
    {"0CF3", "Qualcomm Atheros"}, // Wireless
    {"0BDA", "Realtek"},          // Wireless
    {"148F", "Ralink"},           // Wireless
    {"2357", "TP-Link"},
    {"0B05", "ASUS"},             // Wireless adapters too
    {"7392", "Edimax"},
    {"0846", "NetGear"},
    {"050D", "Belkin"},
    {"2001", "D-Link"},
    {"13B1", "Linksys"},

    // Null terminator
    {nullptr, nullptr}
};

// =============================================================================
// TOUCHPAD MANUFACTURER VIDS - For filtering out touchpads
// =============================================================================
// These VIDs belong to touchpad chip manufacturers.
// Devices with these VIDs should be classified as touchpads, not mice.

struct TouchpadVID {
    const char* vid;
    const char* manufacturer;
};

const TouchpadVID TOUCHPAD_VIDS[] = {
    // Primary touchpad manufacturers
    {"06CB", "Synaptics"},
    {"04F3", "ELAN Microelectronics"},
    {"044E", "Alps Electric"},
    {"27C6", "Goodix"},
    {"2808", "FocalTech"},
    {"0488", "Cirque"},
    {"1EA8", "Sentelic"},
    // NOTE: PixArt (093A) intentionally excluded - they make both touchpad
    // controllers AND standalone mouse sensors (e.g. USB Optical Mouse PID 2510)
    {"04B4", "Cypress"},
    {"03EB", "Atmel (MaxTouch)"},

    // OEM touchpad implementations
    {"045E", "Microsoft (Surface)"},    // Surface touchpads
    {"05AC", "Apple (Trackpad)"},       // MacBook trackpads
    {"1532", "Razer (Blade laptops)"},  // Razer Blade touchpads
    {"0B05", "ASUS (ROG/ZenBook)"},     // ASUS laptop touchpads
    {"17EF", "Lenovo (ThinkPad)"},      // ThinkPad touchpads
    {"413C", "Dell"},                    // Dell laptop touchpads
    {"03F0", "HP"},                      // HP laptop touchpads

    // Null terminator
    {nullptr, nullptr}
};

// =============================================================================
// KNOWN TOUCHPAD PIDS - Specific touchpad product IDs
// =============================================================================
// Format: {VID, PID, Description}

struct TouchpadDevice {
    const char* vid;
    const char* pid;
    const char* description;
};

const TouchpadDevice KNOWN_TOUCHPAD_DEVICES[] = {
    // Synaptics (VID 06CB)
    {"06CB", "0001", "Synaptics TouchPad"},
    {"06CB", "0002", "Synaptics Integrated TouchPad"},
    {"06CB", "0009", "Synaptics Composite TouchPad and TrackPoint"},
    {"06CB", "7AF9", "Synaptics HID TouchPad (Dell)"},
    {"06CB", "7D29", "Synaptics HID TouchPad"},
    {"06CB", "CD8F", "Synaptics ClickPad (Dell XPS)"},
    {"06CB", "CE7E", "Synaptics ClickPad (Dell XPS 17)"},
    {"06CB", "CE08", "Synaptics TouchPad"},

    // ELAN (VID 04F3)
    {"04F3", "0001", "ELAN TouchPad"},
    {"04F3", "2A15", "ELAN HID TouchPad (Dell XPS)"},
    {"04F3", "3078", "ELAN I2C TouchPad"},
    {"04F3", "311C", "ELAN I2C TouchPad"},
    {"04F3", "314F", "ELAN I2C TouchPad"},
    {"04F3", "3022", "ELAN I2C TouchPad (ASUS)"},
    {"04F3", "3124", "ELAN I2C TouchPad (Lenovo)"},
    {"04F3", "31E4", "ELAN I2C TouchPad"},
    {"04F3", "3242", "ELAN I2C TouchPad"},

    // Alps (VID 044E)
    {"044E", "120A", "Alps Touchpad"},
    {"044E", "120B", "Alps Touchpad"},
    {"044E", "1216", "Alps Touchpad"},

    // Goodix (VID 27C6)
    {"27C6", "0111", "Goodix Touch Controller"},
    {"27C6", "0113", "Goodix Touch Controller"},
    {"27C6", "0114", "Goodix Touch Controller"},
    {"27C6", "01F0", "Goodix Touch Controller"},
    {"27C6", "5117", "Goodix Touch Controller"},
    {"27C6", "521D", "Goodix Touch Controller"},
    {"27C6", "5335", "Goodix Touch Controller"},
    {"27C6", "533C", "Goodix Touch Controller"},
    {"27C6", "55A4", "Goodix Touch Controller (Surface)"},

    // Microsoft Surface (VID 045E)
    {"045E", "0914", "Surface Pro Type Cover TouchPad"},
    {"045E", "09AF", "Surface Type Cover TouchPad"},
    {"045E", "07E8", "Surface Book TouchPad"},
    {"045E", "0922", "Surface Laptop TouchPad"},
    {"045E", "09B5", "Surface Laptop 4 TouchPad"},
    {"045E", "099A", "Surface Laptop 3 TouchPad"},
    {"045E", "0950", "Surface Pro 7 TouchPad"},
    {"045E", "0953", "Surface Pro 8 TouchPad"},
    {"045E", "0C1A", "Surface Laptop 5 TouchPad"},

    // Apple (VID 05AC)
    {"05AC", "0265", "Apple Magic Trackpad 2"},
    {"05AC", "0273", "Apple Magic Trackpad 2 (USB)"},
    {"05AC", "0259", "Apple Internal Trackpad (MacBook)"},
    {"05AC", "0262", "Apple Internal Trackpad (MacBook Pro)"},
    {"05AC", "0274", "Apple Internal Trackpad (MacBook Air)"},

    // Razer Blade touchpads (VID 1532)
    {"1532", "010D", "Razer Blade TouchPad"},
    {"1532", "010F", "Razer Blade Pro TouchPad"},
    {"1532", "0220", "Razer Blade 14 TouchPad"},
    {"1532", "0240", "Razer Blade 15 TouchPad"},
    {"1532", "0256", "Razer Blade 17 TouchPad"},

    // Dell laptop touchpads (VID 413C)
    {"413C", "8186", "Dell Touchpad"},
    {"413C", "8187", "Dell Touchpad"},

    // PixArt touchpad controllers (VID 093A)
    // NOTE: PixArt also makes standalone mouse sensors (e.g. PID 2510 "USB Optical Mouse"),
    // so only specific touchpad PIDs are listed here rather than treating all 093A as touchpads.
    {"093A", "0255", "PixArt Touchpad Controller"},
    {"093A", "0257", "PixArt Touchpad Controller"},
    {"093A", "2530", "PixArt I2C HID Touchpad"},

    // Null terminator
    {nullptr, nullptr, nullptr}
};

// =============================================================================
// TOUCHPAD ACPI/I2C DEVICE ID PATTERNS
// =============================================================================
// These patterns are used to identify I2C touchpads that may not have USB VIDs

const char* const TOUCHPAD_ACPI_PATTERNS[] = {
    // ELAN patterns
    "ELAN", "ETD",
    // Synaptics patterns
    "SYNA", "SYN",
    // Alps patterns
    "ALPS", "ALP",
    // FocalTech patterns
    "FTE", "FLT",
    // Goodix patterns
    "GDX", "GDIX",
    // Generic HID-over-I2C
    "PNP0C50", "MSFT0001", "ACPI0C50",
    // Dell patterns
    "DLL",
    // Null terminator
    nullptr
};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

// Check if a VID is a known jiggler VID
inline bool IsKnownJigglerVID(const std::string& vid) {
    std::string upperVid = vid;
    std::transform(upperVid.begin(), upperVid.end(), upperVid.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });

    for (const JigglerVID* j = KNOWN_JIGGLER_VIDS; j->vid != nullptr; ++j) {
        if (upperVid == j->vid) {
            return true;
        }
    }
    return false;
}

// Get jiggler risk level for a VID (returns nullptr if not a jiggler VID)
inline const char* GetJigglerRiskLevel(const std::string& vid) {
    std::string upperVid = vid;
    std::transform(upperVid.begin(), upperVid.end(), upperVid.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });

    for (const JigglerVID* j = KNOWN_JIGGLER_VIDS; j->vid != nullptr; ++j) {
        if (upperVid == j->vid) {
            return j->risk_level;
        }
    }
    return nullptr;
}

// Get jiggler manufacturer name for a VID
inline const char* GetJigglerManufacturer(const std::string& vid) {
    std::string upperVid = vid;
    std::transform(upperVid.begin(), upperVid.end(), upperVid.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });

    for (const JigglerVID* j = KNOWN_JIGGLER_VIDS; j->vid != nullptr; ++j) {
        if (upperVid == j->vid) {
            return j->manufacturer;
        }
    }
    return nullptr;
}

// Check if a VID is a legitimate mouse manufacturer
inline bool IsLegitimateMouseVID(const std::string& vid) {
    std::string upperVid = vid;
    std::transform(upperVid.begin(), upperVid.end(), upperVid.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });

    for (const LegitimateVID* l = LEGITIMATE_MOUSE_VIDS; l->vid != nullptr; ++l) {
        if (upperVid == l->vid) {
            return true;
        }
    }
    return false;
}

// Get legitimate manufacturer name for a VID
inline const char* GetLegitimateManufacturer(const std::string& vid) {
    std::string upperVid = vid;
    std::transform(upperVid.begin(), upperVid.end(), upperVid.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });

    for (const LegitimateVID* l = LEGITIMATE_MOUSE_VIDS; l->vid != nullptr; ++l) {
        if (upperVid == l->vid) {
            return l->manufacturer;
        }
    }
    return nullptr;
}

// Check if a VID is a touchpad manufacturer
inline bool IsTouchpadVID(const std::string& vid) {
    std::string upperVid = vid;
    std::transform(upperVid.begin(), upperVid.end(), upperVid.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });

    for (const TouchpadVID* t = TOUCHPAD_VIDS; t->vid != nullptr; ++t) {
        if (upperVid == t->vid) {
            return true;
        }
    }
    return false;
}

// Check if a VID/PID combination is a known touchpad device
inline bool IsKnownTouchpadDevice(const std::string& vid, const std::string& pid) {
    std::string upperVid = vid;
    std::string upperPid = pid;
    std::transform(upperVid.begin(), upperVid.end(), upperVid.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });
    std::transform(upperPid.begin(), upperPid.end(), upperPid.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });

    for (const TouchpadDevice* t = KNOWN_TOUCHPAD_DEVICES; t->vid != nullptr; ++t) {
        if (upperVid == t->vid && upperPid == t->pid) {
            return true;
        }
    }
    return false;
}

// Check if device path contains touchpad ACPI patterns
inline bool HasTouchpadACPIPattern(const std::string& devicePath) {
    std::string upperPath = devicePath;
    std::transform(upperPath.begin(), upperPath.end(), upperPath.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });

    for (const char* const* p = TOUCHPAD_ACPI_PATTERNS; *p != nullptr; ++p) {
        if (upperPath.find(*p) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// =============================================================================
// DEVICE TRUST SCORING
// =============================================================================
// Returns a suspicion score from 0-100
// 0 = Fully trusted (legitimate manufacturer with normal behavior)
// 100 = Highly suspicious (known jiggler VID)

inline int CalculateVIDSuspicionScore(const std::string& vid) {
    // Check for known jiggler VIDs first
    const char* riskLevel = GetJigglerRiskLevel(vid);
    if (riskLevel != nullptr) {
        if (strcmp(riskLevel, "CRITICAL") == 0) return 100;
        if (strcmp(riskLevel, "HIGH") == 0) return 80;
        if (strcmp(riskLevel, "MEDIUM") == 0) return 50;
    }

    // Check if it's a legitimate mouse manufacturer
    if (IsLegitimateMouseVID(vid)) {
        return 0;  // Fully trusted
    }

    // Unknown VID - moderate suspicion
    return 30;
}

// =============================================================================
// VID TRUST LEVEL ENUM
// =============================================================================

enum class VIDTrustLevel {
    BLOCKLIST_CRITICAL,  // Known jiggler manufacturer (VID 0E90)
    BLOCKLIST_HIGH,      // DIY platforms commonly used for jigglers
    BLOCKLIST_MEDIUM,    // Microcontroller platforms that could be jigglers
    UNKNOWN,             // VID not in any list
    ALLOWLIST            // Known legitimate mouse manufacturer
};

inline VIDTrustLevel GetVIDTrustLevel(const std::string& vid) {
    const char* riskLevel = GetJigglerRiskLevel(vid);
    if (riskLevel != nullptr) {
        if (strcmp(riskLevel, "CRITICAL") == 0) return VIDTrustLevel::BLOCKLIST_CRITICAL;
        if (strcmp(riskLevel, "HIGH") == 0) return VIDTrustLevel::BLOCKLIST_HIGH;
        if (strcmp(riskLevel, "MEDIUM") == 0) return VIDTrustLevel::BLOCKLIST_MEDIUM;
    }

    if (IsLegitimateMouseVID(vid)) {
        return VIDTrustLevel::ALLOWLIST;
    }

    return VIDTrustLevel::UNKNOWN;
}

inline const char* GetVIDTrustLevelString(VIDTrustLevel level) {
    switch (level) {
        case VIDTrustLevel::BLOCKLIST_CRITICAL: return "Known Jiggler";
        case VIDTrustLevel::BLOCKLIST_HIGH: return "DIY Hardware";
        case VIDTrustLevel::BLOCKLIST_MEDIUM: return "Suspicious Hardware";
        case VIDTrustLevel::UNKNOWN: return "Unknown Manufacturer";
        case VIDTrustLevel::ALLOWLIST: return "Verified Manufacturer";
        default: return "Unknown";
    }
}
