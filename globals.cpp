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
HFONT hFontTitle = nullptr, hFontText = nullptr, hFontSubtitle = nullptr, hFontMono = nullptr;
HBRUSH hBgBrush = nullptr;
// Background paints as flat warm paper (top == bottom) with a graph-paper grid
// overlaid in DrawGradientBackground — matches the site's instrument surface.
COLORREF gBgTopColor = RGB(239, 237, 228);    // paper   #EFEDE4
COLORREF gBgBottomColor = RGB(239, 237, 228); // paper   #EFEDE4 (flat)
COLORREF gAccentColor = RGB(47, 106, 76);     // signal green #2F6A4C (kicker/labels)
COLORREF gPanelColor = RGB(239, 237, 228);    // paper — static control backgrounds
COLORREF gStatusColor = RGB(47, 106, 76);     // default green (clean)

// Shared "Motion Forensics" palette (see globals.h).
const COLORREF gPaperColor   = RGB(239, 237, 228); // #EFEDE4
const COLORREF gSurfaceColor = RGB(251, 250, 245); // #FBFAF5
const COLORREF gInkColor     = RGB( 27,  29,  22); // #1B1D16
const COLORREF gInk2Color    = RGB( 84,  87,  74); // #54574A
const COLORREF gInk3Color    = RGB(139, 141, 126); // #8B8D7E
const COLORREF gSignalColor  = RGB( 47, 106,  76); // #2F6A4C
const COLORREF gFlagColor    = RGB(177,  77,  44); // #B14D2C
const COLORREF gWarnColor    = RGB(138, 106,  34); // #8A6A22
const COLORREF gLineColor    = RGB(201, 200, 190); // #C9C8BE
std::thread detectionThread;
DetectionResult currentDetection; // Track current detection state

// Primary mouse identification via startup calibration
HANDLE primaryMouseHandle = nullptr;
bool primaryMouseIdentified = false;
std::string primaryMouseVid;
std::string primaryMousePid;
bool primaryMouseSelectionDisabled = false;  // User chose "I don't have Physical Mouse"
