#pragma once
// =============================================================================
// SHARED GLOBAL STATE
// =============================================================================

#define NOMINMAX
#include <windows.h>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <string>

#include "mouse_device.h"

extern std::chrono::steady_clock::time_point lastDetectionTime;
extern std::chrono::steady_clock::time_point lastTouchpadActivity;
extern const int detectionCooldownMillis;
extern std::atomic_bool stopDetection;

extern std::unordered_map<HANDLE, MouseDevice> mouseDevices;
extern RAWINPUTDEVICE Rid[2];
extern int mouseCount;
extern bool detectionPaused;
extern bool popupShown;
extern HWND hwndMain, hwndMouseCount, hwndExitButton, hwndStatus;
extern HWND hwndHeader, hwndDevicesGroup, hwndStatusGroup, hwndListView;
extern HWND hwndFooterText, hwndLink, hwndTagline;
extern std::vector<HWND> hwndMouseBoxes;
extern std::mutex mouseDevicesMutex;
extern HFONT hFontTitle, hFontText, hFontSubtitle;
extern HBRUSH hBgBrush;
extern COLORREF gBgTopColor;
extern COLORREF gBgBottomColor;
extern COLORREF gAccentColor;
extern COLORREF gPanelColor;
extern COLORREF gStatusColor;
extern std::thread detectionThread;
extern DetectionResult currentDetection;

// Primary mouse identification via startup calibration
extern HANDLE primaryMouseHandle;
extern bool primaryMouseIdentified;
extern std::string primaryMouseVid;
extern std::string primaryMousePid;
extern bool primaryMouseSelectionDisabled;
