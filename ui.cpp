// =============================================================================
// UI HELPER FUNCTIONS AND POPUP DIALOGS - Implementation
// =============================================================================

#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <wingdi.h>
#include <shellapi.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cmath>

#ifdef _MSC_VER
#pragma comment(lib, "Msimg32.lib")
#endif

#include "ui.h"
#include "globals.h"
#include "device_utils.h"
#include "known_devices.h"
#include "detection.h"

#ifndef PT_TOUCHPAD
#define PT_TOUCHPAD 0x00000005
#endif

// UI Constants
const int CUSTOM_POPUP_ICON_ID = 1001;
const int CUSTOM_POPUP_TITLE_ID = 1002;
const int CUSTOM_POPUP_MSG_ID = 1003;
const int CUSTOM_POPUP_REASON_ID = 1004;
const int TOUCHPAD_HELP_MSG_ID = 1101;
const int TOUCHPAD_BTN_USE_MOUSE_ID = 1102;
const int TOUCHPAD_BTN_NO_MOUSE_ID = 1103;
const char* DETECTION_INFO_MESSAGE = 
    "A mouse jiggler has been detected on your system.\n\n"
    "We use advanced algorithms to detect automated mouse movements including repetitive patterns, constant speeds, and circular motions.\n\n"
    "This detection helps ensure genuine user activity on your system.";
const char* CALIBRATION_NEUTRAL_MESSAGE =
    "Click OK to start jiggler detection.";

// Calibration state
static HANDLE calibrationLastDevice = nullptr;
bool calibrationIgnoredTouchpad = false;

void DrawGradientBackground(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    TRIVERTEX vertices[2] = {};
    vertices[0].x = rc.left;
    vertices[0].y = rc.top;
    vertices[0].Red   = GetRValue(gBgTopColor) * 256;
    vertices[0].Green = GetGValue(gBgTopColor) * 256;
    vertices[0].Blue  = GetBValue(gBgTopColor) * 256;
    vertices[0].Alpha = 0x0000;

    vertices[1].x = rc.right;
    vertices[1].y = rc.bottom;
    vertices[1].Red   = GetRValue(gBgBottomColor) * 256;
    vertices[1].Green = GetGValue(gBgBottomColor) * 256;
    vertices[1].Blue  = GetBValue(gBgBottomColor) * 256;
    vertices[1].Alpha = 0x0000;

    GRADIENT_RECT gRect{0, 1};
    GradientFill(hdc, vertices, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
}

void ApplyListViewTheme(HWND listView) {
    if (!listView) return;
    ListView_SetBkColor(listView, gPanelColor);
    ListView_SetTextBkColor(listView, gPanelColor);
    ListView_SetTextColor(listView, RGB(35, 35, 48));

    HWND header = (HWND)SendMessage(listView, LVM_GETHEADER, 0, 0);
    if (header) {
        SetWindowTheme(header, L"Explorer", nullptr);
        SendMessage(header, WM_SETFONT, (WPARAM)hFontText, TRUE);
    }
}

// Custom Modern Popup Dialog
LRESULT CALLBACK CustomPopupProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HFONT hTitleFont = nullptr;
    static HFONT hTextFont = nullptr;
    static HFONT hReasonFont = nullptr;
    static HBRUSH hPopupBgBrush = nullptr;
    
    switch (uMsg) {
        case WM_CREATE: {
            hTitleFont = CreateFont(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH, "Segoe UI");
            hTextFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH, "Segoe UI");
            hReasonFont = CreateFont(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH, "Segoe UI");
            hPopupBgBrush = CreateSolidBrush(RGB(255, 255, 255));

            // Warning icon using system icon
            HWND hwndIcon = CreateWindowEx(0, "STATIC", NULL,
                WS_CHILD | WS_VISIBLE | SS_ICON | SS_CENTERIMAGE,
                20, 30, 64, 64, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(CUSTOM_POPUP_ICON_ID)), NULL, NULL);
            // Load the warning icon (yellow triangle with !)
            HICON hWarningIcon = LoadIcon(NULL, IDI_WARNING);
            SendMessage(hwndIcon, STM_SETICON, (WPARAM)hWarningIcon, 0);
            
            // Title
            HWND hwndTitle = CreateWindowEx(0, "STATIC", "Mouse Jiggler Detected",
                WS_CHILD | WS_VISIBLE,
                100, 30, 360, 30, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(CUSTOM_POPUP_TITLE_ID)), NULL, NULL);
            SendMessage(hwndTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);
            
            // Detection Reason - display WHY the jiggler was detected
            // Extract just the reason part, removing "Jiggler Detected: " prefix if present
            std::string reasonText = currentDetection.reason;
            const std::string prefix = "Jiggler Detected: ";
            if (reasonText.length() >= prefix.length() && 
                reasonText.compare(0, prefix.length(), prefix) == 0) {
                reasonText = reasonText.substr(prefix.length());
            }
            reasonText = "Reason: " + reasonText;
            HWND hwndReason = CreateWindowEx(0, "STATIC", reasonText.c_str(),
                WS_CHILD | WS_VISIBLE,
                100, 65, 360, 40, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(CUSTOM_POPUP_REASON_ID)), NULL, NULL);
            SendMessage(hwndReason, WM_SETFONT, (WPARAM)hReasonFont, TRUE);
            
            // Message
            HWND hwndMsg = CreateWindowEx(0, "STATIC", DETECTION_INFO_MESSAGE,
                WS_CHILD | WS_VISIBLE,
                100, 110, 360, 155, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(CUSTOM_POPUP_MSG_ID)), NULL, NULL);
            SendMessage(hwndMsg, WM_SETFONT, (WPARAM)hTextFont, TRUE);

            // View Report Button - opens detection report on detectmyjiggler.com
            HWND hwndOK = CreateWindowEx(0, "BUTTON", "View Report",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
                100, 275, 180, 35, hwnd, (HMENU)IDOK, NULL, NULL);
            SendMessage(hwndOK, WM_SETFONT, (WPARAM)hTextFont, TRUE);
            SetWindowTheme(hwndOK, L"Explorer", nullptr);

            // Dismiss Button
            HWND hwndCancel = CreateWindowEx(0, "BUTTON", "Dismiss",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                290, 275, 120, 35, hwnd, (HMENU)IDCANCEL, NULL, NULL);
            SendMessage(hwndCancel, WM_SETFONT, (WPARAM)hTextFont, TRUE);
            SetWindowTheme(hwndCancel, L"Explorer", nullptr);
            
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            int ctrlId = GetDlgCtrlID(hCtrl);
            
            SetBkMode(hdc, TRANSPARENT);
            if (ctrlId == CUSTOM_POPUP_TITLE_ID) { // Title
                SetTextColor(hdc, RGB(227, 52, 90)); // Red for warning
            } else if (ctrlId == CUSTOM_POPUP_ICON_ID) { // Icon
                SetTextColor(hdc, RGB(255, 165, 0)); // Orange warning color
            } else if (ctrlId == CUSTOM_POPUP_REASON_ID) { // Detection reason
                SetTextColor(hdc, RGB(90, 102, 255)); // Blue to highlight the reason
            } else {
                SetTextColor(hdc, RGB(50, 50, 50));
            }
            return (LRESULT)hPopupBgBrush;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDOK) {
                SetWindowLongPtr(hwnd, GWLP_USERDATA, IDOK);
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            } else if (LOWORD(wParam) == IDCANCEL) {
                SetWindowLongPtr(hwnd, GWLP_USERDATA, IDCANCEL);
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            }
            return 0;
        }
        case WM_CLOSE:
            // Only set GWLP_USERDATA if not already set by a button handler
            if (GetWindowLongPtr(hwnd, GWLP_USERDATA) == 0) {
                SetWindowLongPtr(hwnd, GWLP_USERDATA, IDCANCEL);
            }
            PostMessage(hwnd, WM_NULL, 0, 0); // Wake up message loop
            return 0;
        case WM_DESTROY:
            if (hTitleFont) DeleteObject(hTitleFont);
            if (hTextFont) DeleteObject(hTextFont);
            if (hReasonFont) DeleteObject(hReasonFont);
            if (hPopupBgBrush) DeleteObject(hPopupBgBrush);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int ShowCustomJigglerPopup(HWND parent) {
    // Register custom dialog class
    static bool registered = false;
    if (!registered) {
        WNDCLASS wc = {0};
        wc.lpfnWndProc = CustomPopupProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "CustomJigglerPopup";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClass(&wc);
        registered = true;
    }
    
    // Create and show the dialog
    HWND hwndPopup = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "CustomJigglerPopup",
        "Mouse Jiggler Detection",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 370,
        parent, NULL, GetModuleHandle(NULL), NULL);
    
    if (!hwndPopup) return IDCANCEL;
    
    // Center on parent
    RECT rcParent, rcPopup;
    GetWindowRect(parent, &rcParent);
    GetWindowRect(hwndPopup, &rcPopup);
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcPopup.right - rcPopup.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcPopup.bottom - rcPopup.top)) / 2;
    SetWindowPos(hwndPopup, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    ShowWindow(hwndPopup, SW_SHOW);
    UpdateWindow(hwndPopup);
    
    // Modal message loop
    EnableWindow(parent, FALSE);
    MSG msg;
    int result = IDCANCEL;
    bool dialogActive = true;
    
    while (dialogActive && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_QUIT) {
            PostQuitMessage((int)msg.wParam);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        // Check if dialog was closed (GWLP_USERDATA is set by button handlers or WM_CLOSE)
        LONG_PTR userData = GetWindowLongPtr(hwndPopup, GWLP_USERDATA);
        if (userData != 0) {
            result = (int)userData;
            dialogActive = false;
        }
    }
    
    EnableWindow(parent, TRUE);
    DestroyWindow(hwndPopup);
    SetForegroundWindow(parent);
    
    return result;
}

LRESULT CALLBACK TouchpadRequirementPopupProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HFONT hTitleFont = nullptr;
    static HFONT hTextFont = nullptr;
    static HBRUSH hTpBgBrush = nullptr;

    switch (uMsg) {
        case WM_CREATE: {
            hTitleFont = CreateFont(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH, "Segoe UI");
            hTextFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH, "Segoe UI");
            hTpBgBrush = CreateSolidBrush(RGB(255, 255, 255));

            HWND hwndTitle = CreateWindowEx(0, "STATIC", "Touch Pad Detected",
                WS_CHILD | WS_VISIBLE, 24, 20, 452, 28, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(CUSTOM_POPUP_TITLE_ID)), NULL, NULL);
            SendMessage(hwndTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);

            HWND hwndMsg = CreateWindowEx(0, "STATIC",
                "Use a physical mouse to click OK.\n\n"
                "If you don't have a physical mouse, continue without primary mouse detection.",
                WS_CHILD | WS_VISIBLE,
                24, 60, 452, 70, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(TOUCHPAD_HELP_MSG_ID)), NULL, NULL);
            SendMessage(hwndMsg, WM_SETFONT, (WPARAM)hTextFont, TRUE);

            HWND hwndUseMouse = CreateWindowEx(0, "BUTTON", "Use Physical Mouse",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                24, 150, 220, 36, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(TOUCHPAD_BTN_USE_MOUSE_ID)), NULL, NULL);
            SendMessage(hwndUseMouse, WM_SETFONT, (WPARAM)hTextFont, TRUE);
            SetWindowTheme(hwndUseMouse, L"Explorer", nullptr);

            HWND hwndNoMouse = CreateWindowEx(0, "BUTTON", "I don't have Physical Mouse",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                256, 150, 220, 36, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(TOUCHPAD_BTN_NO_MOUSE_ID)), NULL, NULL);
            SendMessage(hwndNoMouse, WM_SETFONT, (WPARAM)hTextFont, TRUE);
            SetWindowTheme(hwndNoMouse, L"Explorer", nullptr);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            int ctrlId = GetDlgCtrlID(hCtrl);
            SetBkMode(hdc, TRANSPARENT);
            if (ctrlId == CUSTOM_POPUP_TITLE_ID) {
                SetTextColor(hdc, RGB(90, 102, 255));
            } else {
                SetTextColor(hdc, RGB(50, 50, 50));
            }
            return (LRESULT)hTpBgBrush;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == TOUCHPAD_BTN_USE_MOUSE_ID) {
                SetWindowLongPtr(hwnd, GWLP_USERDATA, IDYES);
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            } else if (LOWORD(wParam) == TOUCHPAD_BTN_NO_MOUSE_ID) {
                SetWindowLongPtr(hwnd, GWLP_USERDATA, IDNO);
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            }
            return 0;
        }
        case WM_CLOSE:
            if (GetWindowLongPtr(hwnd, GWLP_USERDATA) == 0) {
                SetWindowLongPtr(hwnd, GWLP_USERDATA, IDYES);
            }
            PostMessage(hwnd, WM_NULL, 0, 0);
            return 0;
        case WM_DESTROY:
            if (hTitleFont) DeleteObject(hTitleFont);
            if (hTextFont) DeleteObject(hTextFont);
            if (hTpBgBrush) DeleteObject(hTpBgBrush);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int ShowTouchpadRequirementPopup(HWND parent) {
    static bool registered = false;
    if (!registered) {
        WNDCLASS wc = {0};
        wc.lpfnWndProc = TouchpadRequirementPopupProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "TouchpadRequirementPopup";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClass(&wc);
        registered = true;
    }

    HWND hwndPopup = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "TouchpadRequirementPopup",
        "Primary Mouse Setup",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 510, 240,
        parent, NULL, GetModuleHandle(NULL), NULL);

    if (!hwndPopup) return IDYES;

    RECT rcParent, rcPopup;
    GetWindowRect(parent, &rcParent);
    GetWindowRect(hwndPopup, &rcPopup);
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcPopup.right - rcPopup.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcPopup.bottom - rcPopup.top)) / 2;
    SetWindowPos(hwndPopup, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hwndPopup, SW_SHOW);
    UpdateWindow(hwndPopup);

    EnableWindow(parent, FALSE);
    MSG msg;
    int result = IDYES;
    bool dialogActive = true;

    while (dialogActive && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_QUIT) {
            PostQuitMessage((int)msg.wParam);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        LONG_PTR userData = GetWindowLongPtr(hwndPopup, GWLP_USERDATA);
        if (userData != 0) {
            result = (int)userData;
            dialogActive = false;
        }
    }

    EnableWindow(parent, TRUE);
    DestroyWindow(hwndPopup);
    SetForegroundWindow(parent);
    return result;
}

// Startup Calibration Popup - identifies the primary mouse
static void CalibrationSetTouchpadHint(HWND hwnd) {
    (void)hwnd;
    calibrationIgnoredTouchpad = true;
}

LRESULT CALLBACK CalibrationPopupProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HFONT hTitleFont = nullptr;
    static HFONT hTextFont = nullptr;
    static HBRUSH hCalBgBrush = nullptr;

    switch (uMsg) {
        case WM_CREATE: {
            hTitleFont = CreateFont(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH, "Segoe UI");
            hTextFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH, "Segoe UI");
            hCalBgBrush = CreateSolidBrush(RGB(255, 255, 255));

            // Info icon
            HWND hwndIcon = CreateWindowEx(0, "STATIC", NULL,
                WS_CHILD | WS_VISIBLE | SS_ICON | SS_CENTERIMAGE,
                20, 20, 64, 64, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(CUSTOM_POPUP_ICON_ID)), NULL, NULL);
            HICON hInfoIcon = LoadIcon(NULL, IDI_INFORMATION);
            SendMessage(hwndIcon, STM_SETICON, (WPARAM)hInfoIcon, 0);

            // Title
            HWND hwndTitle = CreateWindowEx(0, "STATIC", "Start the Detection Process",
                WS_CHILD | WS_VISIBLE,
                100, 22, 360, 30, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(CUSTOM_POPUP_TITLE_ID)), NULL, NULL);
            SendMessage(hwndTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);

            // Message
            HWND hwndMsg = CreateWindowEx(0, "STATIC",
                CALIBRATION_NEUTRAL_MESSAGE,
                WS_CHILD | WS_VISIBLE,
                100, 62, 360, 48, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(CUSTOM_POPUP_MSG_ID)), NULL, NULL);
            SendMessage(hwndMsg, WM_SETFONT, (WPARAM)hTextFont, TRUE);

            // OK Button
            HWND hwndOK = CreateWindowEx(0, "BUTTON", "OK",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
                202, 140, 90, 28, hwnd, (HMENU)IDOK, NULL, NULL);
            SendMessage(hwndOK, WM_SETFONT, (WPARAM)hTextFont, TRUE);
            SetWindowTheme(hwndOK, L"Explorer", nullptr);

            // Register for raw input on this popup window
            // 1) Generic mouse, 2) Touchpad usage page for precision touchpads.
            RAWINPUTDEVICE rid[2];
            rid[0].usUsagePage = 0x01;
            rid[0].usUsage = 0x02;
            rid[0].dwFlags = RIDEV_INPUTSINK;
            rid[0].hwndTarget = hwnd;

            rid[1].usUsagePage = 0x0D;
            rid[1].usUsage = 0x05;
            rid[1].dwFlags = RIDEV_INPUTSINK;
            rid[1].hwndTarget = hwnd;

            RegisterRawInputDevices(rid, 2, sizeof(rid[0]));

            return 0;
        }
        case WM_INPUT: {
            UINT dwSize;
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
            LPBYTE lpb = new BYTE[dwSize];
            if (lpb == NULL) return 0;

            if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize) {
                RAWINPUT* raw = (RAWINPUT*)lpb;
                if (raw->header.dwType == RIM_TYPEMOUSE && raw->header.hDevice != nullptr) {
                    // Track any mouse movement or button click as the active device
                    LONG dx = raw->data.mouse.lLastX;
                    LONG dy = raw->data.mouse.lLastY;
                    USHORT buttonFlags = raw->data.mouse.usButtonFlags;
                    if (dx != 0 || dy != 0 || buttonFlags != 0) {
                        const bool isTouchpad = IsTouchpadDevice(raw->header.hDevice, raw);
                        if (!isTouchpad) {
                            // Only a real button click should qualify the primary device.
                            if (buttonFlags != 0) {
                                calibrationLastDevice = raw->header.hDevice;
                            }
                        } else {
                            CalibrationSetTouchpadHint(hwnd);
                        }
                    }
                }

                // Precision touchpads may surface as HID digitizer input; treat as touchpad intent.
                if (raw->header.dwType == RIM_TYPEHID && raw->header.hDevice != nullptr) {
                    RID_DEVICE_INFO info{};
                    info.cbSize = sizeof(info);
                    UINT infoSize = sizeof(info);
                    if (GetRawInputDeviceInfoW(raw->header.hDevice, RIDI_DEVICEINFO, &info, &infoSize) != (UINT)-1) {
                        if (info.dwType == RIM_TYPEHID &&
                            info.hid.usUsagePage == 0x0D &&
                            (info.hid.usUsage == 0x05 || info.hid.usUsage == 0x04)) {
                            CalibrationSetTouchpadHint(hwnd);
                        }
                    }
                }
            }
            delete[] lpb;
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            int ctrlId = GetDlgCtrlID(hCtrl);

            SetBkMode(hdc, TRANSPARENT);
            if (ctrlId == CUSTOM_POPUP_TITLE_ID) {
                SetTextColor(hdc, RGB(90, 102, 255)); // Blue to match app theme
            } else if (ctrlId == CUSTOM_POPUP_ICON_ID) {
                SetTextColor(hdc, RGB(90, 102, 255));
            } else {
                SetTextColor(hdc, RGB(50, 50, 50));
            }
            return (LRESULT)hCalBgBrush;
        }
        case WM_POINTERDOWN:
        case WM_POINTERUP: {
            UINT32 pointerId = GET_POINTERID_WPARAM(wParam);
            POINTER_INPUT_TYPE pointerType = PT_POINTER;
            if (GetPointerType(pointerId, &pointerType) && pointerType == PT_TOUCHPAD) {
                CalibrationSetTouchpadHint(hwnd);
            }
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDOK) {
                // If user clicked OK using touchpad, keep calibration open and prompt.
                if (calibrationLastDevice == nullptr) {
                    int choice = ShowTouchpadRequirementPopup(hwnd);
                    if (choice == IDNO) {
                        primaryMouseSelectionDisabled = true;
                        primaryMouseHandle = nullptr;
                        primaryMouseIdentified = false;
                        primaryMouseVid.clear();
                        primaryMousePid.clear();
                        SetWindowLongPtr(hwnd, GWLP_USERDATA, IDOK);
                        SendMessage(hwnd, WM_CLOSE, 0, 0);
                        return 0;
                    }
                    return 0;
                }
                SetWindowLongPtr(hwnd, GWLP_USERDATA, IDOK);
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            }
            return 0;
        }
        case WM_CLOSE:
            if (GetWindowLongPtr(hwnd, GWLP_USERDATA) == 0) {
                SetWindowLongPtr(hwnd, GWLP_USERDATA, IDOK); // Default to OK on close
            }
            PostMessage(hwnd, WM_NULL, 0, 0);
            return 0;
        case WM_DESTROY:
            if (hTitleFont) DeleteObject(hTitleFont);
            if (hTextFont) DeleteObject(hTextFont);
            if (hCalBgBrush) DeleteObject(hCalBgBrush);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ShowCalibrationPopup(HWND parent) {
    // Register custom calibration dialog class
    static bool registered = false;
    if (!registered) {
        WNDCLASS wc = {0};
        wc.lpfnWndProc = CalibrationPopupProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "CalibrationPopup";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClass(&wc);
        registered = true;
    }

    calibrationLastDevice = nullptr;
    calibrationIgnoredTouchpad = false;
    primaryMouseSelectionDisabled = false;

    HWND hwndPopup = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "CalibrationPopup",
        "Mouse Jiggler Detection - Calibration",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 220,
        parent, NULL, GetModuleHandle(NULL), NULL);

    if (!hwndPopup) return;

    // Center on parent
    RECT rcParent, rcPopup;
    GetWindowRect(parent, &rcParent);
    GetWindowRect(hwndPopup, &rcPopup);
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcPopup.right - rcPopup.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcPopup.bottom - rcPopup.top)) / 2;
    SetWindowPos(hwndPopup, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hwndPopup, SW_SHOW);
    UpdateWindow(hwndPopup);

    // Modal message loop
    EnableWindow(parent, FALSE);
    MSG msg;
    bool dialogActive = true;

    while (dialogActive && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_QUIT) {
            PostQuitMessage((int)msg.wParam);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        LONG_PTR userData = GetWindowLongPtr(hwndPopup, GWLP_USERDATA);
        if (userData != 0) {
            dialogActive = false;
        }
    }

    EnableWindow(parent, TRUE);
    DestroyWindow(hwndPopup);
    SetForegroundWindow(parent);

    // Re-register raw input devices for the main window.
    // The popup's registration overrode the main window's, and destroying
    // the popup unregisters it - leaving no active raw input listener.
    RegisterDevices(parent);

    // Capture the primary mouse from whatever non-touchpad device was used.
    if (!primaryMouseSelectionDisabled && calibrationLastDevice != nullptr) {
        if (!IsTouchpadDevice(calibrationLastDevice, nullptr)) {
            primaryMouseHandle = calibrationLastDevice;
            primaryMouseIdentified = true;

            // Extract VID/PID from the primary mouse
            std::string deviceName = GetDeviceNameUtf8(primaryMouseHandle);
            ExtractVidPid(deviceName, primaryMouseVid, primaryMousePid);

            // Pre-populate mouseDevices with the primary mouse so it appears
            // in the device list immediately, even if the user doesn't move
            // the mouse after clicking OK
            {
                std::lock_guard<std::mutex> lock(mouseDevicesMutex);
                if (mouseDevices.find(primaryMouseHandle) == mouseDevices.end()) {
                    bool isTp = IsTouchpadDevice(primaryMouseHandle, nullptr);
                    mouseDevices[primaryMouseHandle] = MouseDevice(deviceName, isTp);
                    GetHidAttributes(deviceName, mouseDevices[primaryMouseHandle].manufacturer, mouseDevices[primaryMouseHandle].product);
                    mouseCount++;
                }
            }
        } else {
            primaryMouseHandle = nullptr;
            primaryMouseIdentified = false;
            primaryMouseVid.clear();
            primaryMousePid.clear();
        }
    } else if (primaryMouseSelectionDisabled) {
        primaryMouseHandle = nullptr;
        primaryMouseIdentified = false;
        primaryMouseVid.clear();
        primaryMousePid.clear();
    }
}

void ShowJigglerDetectedPopup() {
    detectionPaused = true;
    popupShown = true;

    // Update status with detection reason
    SetWindowText(hwndStatus, currentDetection.reason.c_str());
    gStatusColor = RGB(227, 52, 90);
    InvalidateRect(hwndMain, NULL, TRUE);

    int msgboxID = ShowCustomJigglerPopup(hwndMain);

    // If OK was clicked, redirect to detection URL with device information
    if (msgboxID == IDOK) {
        std::lock_guard<std::mutex> lock(mouseDevicesMutex);

        // NEW APPROACH: Collect all devices with their independent analysis
        struct DeviceInfo {
            std::string manufacturer;
            std::string product;
            std::string vid;
            std::string pid;
            double varianceX;
            double varianceY;
            int suspicionScore;
            std::string trustLevel;
            std::string verdict;
            std::string pattern;
            bool isJiggler;
        };

        // Map DetectionType enum to a human-readable pattern name
        auto detectionTypeToPattern = [](DetectionType type) -> std::string {
            switch (type) {
                case DetectionType::CIRCULAR_ARC_PATTERN:       return "Circular Arc";
                case DetectionType::CIRCULAR_PATTERN:           return "Circular";
                case DetectionType::OSCILLATION_PATTERN:        return "Oscillation";
                case DetectionType::ZIGZAG_PATTERN:             return "Zigzag";
                case DetectionType::GEOMETRIC_PATTERN:          return "Geometric";
                case DetectionType::ALTERNATING_PATTERN:        return "Alternating";
                case DetectionType::CONSTANT_DELTA:             return "Constant Delta";
                case DetectionType::CONSTANT_SPEED:             return "Constant Speed";
                case DetectionType::LARGE_REPETITIVE_MOVEMENT:  return "Repetitive Movement";
                case DetectionType::CONTINUOUS_MOVEMENT:        return "Continuous Movement";
                case DetectionType::REPETITIVE_DELTA:           return "Repetitive Micro-Delta";
                case DetectionType::SMALL_MOVEMENT:             return "Small Repetitive Movement";
                default:                                        return "";
            }
        };

        std::vector<DeviceInfo> devices;

        for (const auto& [handle, device] : mouseDevices) {
            if (device.isTouchpad) continue;

            DeviceInfo info;
            info.manufacturer = device.manufacturer;
            info.product = device.product;
            info.vid = device.vid;
            info.pid = device.pid;
            info.suspicionScore = device.combinedSuspicionScore;
            info.trustLevel = GetVIDTrustLevelString(device.vidTrustLevel);
            info.isJiggler = device.isJiggler;
            info.verdict = device.isJiggler ? "jiggler" : (device.combinedSuspicionScore >= 50 ? "suspicious" : "trusted");

            // Use LUT lookup if manufacturer or product is empty
            if (info.manufacturer.empty() || info.product.empty()) {
                std::string lutManufacturer, lutProduct;
                LookupKnownDevice(info.vid, info.pid, lutManufacturer, lutProduct);
                if (info.manufacturer.empty() && !lutManufacturer.empty()) {
                    info.manufacturer = lutManufacturer;
                }
                if (info.product.empty() && !lutProduct.empty()) {
                    info.product = lutProduct;
                }
            }

            // Also check jiggler manufacturer database
            if (info.manufacturer.empty()) {
                const char* jigglerMfr = GetJigglerManufacturer(info.vid);
                if (jigglerMfr) info.manufacturer = jigglerMfr;
            }
            if (info.manufacturer.empty()) {
                const char* legitMfr = GetLegitimateManufacturer(info.vid);
                if (legitMfr) info.manufacturer = legitMfr;
            }

            info.varianceX = CalculateMovementStdDev(device, true);
            info.varianceY = CalculateMovementStdDev(device, false);
            info.pattern = detectionTypeToPattern(device.detectionType);

            devices.push_back(info);
        }

        // Sort: jigglers first, then by suspicion score (highest first)
        std::sort(devices.begin(), devices.end(), [](const DeviceInfo& a, const DeviceInfo& b) {
            if (a.isJiggler != b.isJiggler) return a.isJiggler;
            return a.suspicionScore > b.suspicionScore;
        });

        // Build detection URL with the new format
        if (!devices.empty()) {
            std::ostringstream url;
            url << "https://detectmyjiggler.com/detected?";

            // Include all devices (up to 10 max to avoid URL length issues)
            size_t maxDevices = std::min(devices.size(), (size_t)10);
            for (size_t i = 0; i < maxDevices; ++i) {
                if (i > 0) url << "&";  // Separator between devices

                std::string prefix = "m" + std::to_string(i + 1) + "_";

                url << prefix << "name=" << UrlEncode(devices[i].manufacturer.empty() ? "Unknown" : devices[i].manufacturer);
                url << "&" << prefix << "model=" << UrlEncode(devices[i].product.empty() ? "Unknown" : devices[i].product);
                url << "&" << prefix << "vid=" << (devices[i].vid.empty() ? "0000" : devices[i].vid);
                url << "&" << prefix << "pid=" << (devices[i].pid.empty() ? "0000" : devices[i].pid);
                url << "&" << prefix << "x=" << std::fixed << std::setprecision(2) << devices[i].varianceX;
                url << "&" << prefix << "y=" << std::fixed << std::setprecision(2) << devices[i].varianceY;
                url << "&" << prefix << "score=" << devices[i].suspicionScore;
                url << "&" << prefix << "trust=" << UrlEncode(devices[i].trustLevel);
                url << "&" << prefix << "verdict=" << devices[i].verdict;
                if (!devices[i].pattern.empty()) {
                    url << "&" << prefix << "pattern=" << UrlEncode(devices[i].pattern);
                }
            }

            // Add device count to URL
            url << "&count=" << maxDevices;

            // Include primary mouse info if identified during calibration
            if (primaryMouseIdentified) {
                url << "&primary_vid=" << (primaryMouseVid.empty() ? "0000" : primaryMouseVid);
                url << "&primary_pid=" << (primaryMousePid.empty() ? "0000" : primaryMousePid);
            }

            // Open URL in default browser
            HINSTANCE result = ShellExecute(NULL, "open", url.str().c_str(), NULL, NULL, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(result) <= 32) {
                // ShellExecute failed - silently fail
            }
        }
    }

    popupShown = false;

    // Stop detection permanently after popup is shown
    stopDetection.store(true);
    detectionPaused = false;

    // Update status to show detection was stopped
    SetWindowText(hwndStatus, "Detection Stopped - Jiggler Was Detected");
    gStatusColor = RGB(128, 128, 128);
    InvalidateRect(hwndMain, NULL, TRUE);
}
