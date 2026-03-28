// =============================================================================
// SHARED GLOBAL STATE - Definitions
// =============================================================================

#define NOMINMAX
#include <windows.h>

#include "globals.h"

std::chrono::steady_clock::time_point lastDetectionTime = std::chrono::steady_clock::time_point::min();
std::chrono::steady_clock::time_point lastTouchpadActivity = std::chrono::steady_clock::time_point::min();
const int detectionCooldownMillis = 5000; // Cooldown period of 5000 milliseconds (5 seconds)
std::atomic_bool stopDetection{false};

std::unordered_map<HANDLE, MouseDevice> mouseDevices;
RAWINPUTDEVICE Rid[2];
int mouseCount = 0;
bool detectionPaused = false;
bool popupShown = false;
HWND hwndMain, hwndMouseCount, hwndExitButton, hwndStatus, hwndLiveChartButton;
HWND hwndHeader, hwndDevicesGroup, hwndStatusGroup, hwndListView;
HWND hwndFooterText, hwndLink, hwndTagline;
std::vector<HWND> hwndMouseBoxes;
std::mutex mouseDevicesMutex;
HFONT hFontTitle = nullptr, hFontText = nullptr, hFontSubtitle = nullptr;
HBRUSH hBgBrush = nullptr;
COLORREF gBgTopColor = RGB(90, 102, 255);
COLORREF gBgBottomColor = RGB(144, 224, 239);
COLORREF gAccentColor = RGB(255, 148, 112);
COLORREF gPanelColor = RGB(247, 250, 255);
COLORREF gStatusColor = RGB(34, 139, 34); // default green
std::thread detectionThread;
DetectionResult currentDetection; // Track current detection state

// Primary mouse identification via startup calibration
HANDLE primaryMouseHandle = nullptr;
bool primaryMouseIdentified = false;
std::string primaryMouseVid;
std::string primaryMousePid;
bool primaryMouseSelectionDisabled = false;  // User chose "I don't have Physical Mouse"
