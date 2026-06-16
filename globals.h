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
extern HWND hwndMain, hwndMouseCount, hwndExitButton, hwndStatus, hwndLiveChartButton;
extern HWND hwndHeader, hwndDevicesGroup, hwndStatusGroup, hwndListView;
extern HWND hwndFooterText, hwndLink, hwndTagline;
extern std::vector<HWND> hwndMouseBoxes;
extern std::mutex mouseDevicesMutex;
extern HFONT hFontTitle, hFontText, hFontSubtitle, hFontMono;
extern HBRUSH hBgBrush;
extern COLORREF gBgTopColor;
extern COLORREF gBgBottomColor;
extern COLORREF gAccentColor;
extern COLORREF gPanelColor;
extern COLORREF gStatusColor;

// -----------------------------------------------------------------------------
// "Motion Forensics" design tokens — mirrors detectmyjiggler.com (site.css).
// Warm instrument-paper light theme: cream surfaces, ink text, green = human/
// clean, rust = jiggler/flagged, amber = waiting/info.
// -----------------------------------------------------------------------------
extern const COLORREF gPaperColor;    // #EFEDE4  window background (paper)
extern const COLORREF gSurfaceColor;  // #FBFAF5  panels / data table surface
extern const COLORREF gInkColor;      // #1B1D16  primary text
extern const COLORREF gInk2Color;     // #54574A  secondary text
extern const COLORREF gInk3Color;     // #8B8D7E  muted / mono annotations
extern const COLORREF gSignalColor;   // #2F6A4C  green — human / clean / trusted
extern const COLORREF gFlagColor;     // #B14D2C  rust  — robotic / flagged
extern const COLORREF gWarnColor;     // #8A6A22  amber — waiting / calibration
extern const COLORREF gLineColor;     // #C9C8BE  hairline borders / grid
extern std::thread detectionThread;
extern DetectionResult currentDetection;

// Primary mouse identification via startup calibration
extern HANDLE primaryMouseHandle;
extern bool primaryMouseIdentified;
extern std::string primaryMouseVid;
extern std::string primaryMousePid;
extern bool primaryMouseSelectionDisabled;
