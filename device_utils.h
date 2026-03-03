#pragma once
// =============================================================================
// DEVICE UTILITY FUNCTIONS
// =============================================================================

#define NOMINMAX
#include <windows.h>
#include <string>

// Forward declaration of MouseDevice (full definition in mouse_device.h)
class MouseDevice;

// VID/PID extraction
void ExtractVidPid(const std::string& deviceName, std::string& vid, std::string& pid);

// String conversion
std::string WideToUtf8(const std::wstring &w);

// Device name and info
std::string GetDeviceNameUtf8(HANDLE deviceHandle);
void GetHidAttributes(const std::string& devicePath, std::string& manufacturer, std::string& product);
std::string BuildDisplayName(const std::string &rawName, size_t index, const std::string& manufacturer = "", const std::string& product = "");

// Touchpad detection
bool IsLikelyTouchpadName(const std::string &nameUtf8);
bool DeviceHasTouchpadUsage(HANDLE deviceHandle);
bool GetVidPidFromHandle(HANDLE deviceHandle, std::string& vid, std::string& pid);
bool IsTouchpadByVID(const std::string& devicePath);
bool IsTouchpadDevice(HANDLE deviceHandle, const RAWINPUT* rawMouse, MouseDevice* existing = nullptr);

// Movement statistics
double CalculateMovementStdDev(const MouseDevice& device, bool isX);

// URL encoding
std::string UrlEncode(const std::string& value);
