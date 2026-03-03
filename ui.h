#pragma once
// =============================================================================
// UI HELPER FUNCTIONS AND POPUP DIALOGS
// =============================================================================

#define NOMINMAX
#include <windows.h>
#include <string>

// UI Constants
extern const int CUSTOM_POPUP_ICON_ID;
extern const int CUSTOM_POPUP_TITLE_ID;
extern const int CUSTOM_POPUP_MSG_ID;
extern const int CUSTOM_POPUP_REASON_ID;
extern const int TOUCHPAD_HELP_MSG_ID;
extern const int TOUCHPAD_BTN_USE_MOUSE_ID;
extern const int TOUCHPAD_BTN_NO_MOUSE_ID;
extern const char* DETECTION_INFO_MESSAGE;
extern const char* CALIBRATION_NEUTRAL_MESSAGE;

// Calibration state (accessible from main.cpp)
extern bool calibrationIgnoredTouchpad;

// UI helper functions
void DrawGradientBackground(HWND hwnd, HDC hdc);
void ApplyListViewTheme(HWND listView);

// Popup dialog functions
LRESULT CALLBACK CustomPopupProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int ShowCustomJigglerPopup(HWND parent);

LRESULT CALLBACK TouchpadRequirementPopupProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int ShowTouchpadRequirementPopup(HWND parent);

LRESULT CALLBACK CalibrationPopupProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ShowCalibrationPopup(HWND parent);

void ShowJigglerDetectedPopup();

// Forward declaration for RegisterDevices (defined in main.cpp, used by ShowCalibrationPopup)
void RegisterDevices(HWND hwnd);
