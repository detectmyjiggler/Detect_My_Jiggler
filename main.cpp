#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <dbt.h>
#include <sstream>
#include <mutex>
#include <commctrl.h>
#include <algorithm>
#include <wingdi.h>
#include <uxtheme.h>
#include <iomanip>
#include <cmath>
#include <deque>

#include "detection_constants.h"
#include "mouse_device.h"
#include "known_devices.h"
#include "device_utils.h"
#include "detection.h"
#include "globals.h"
#include "ui.h"
#include "live_chart.h"

void RegisterDevices(HWND hwnd) {
    Rid[0].usUsagePage = 0x01;
    Rid[0].usUsage = 0x02;
    Rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    Rid[0].hwndTarget = hwnd;

    Rid[1].usUsagePage = 0x0D;
    Rid[1].usUsage = 0x05;
    Rid[1].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    Rid[1].hwndTarget = hwnd;

    if (RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE) {
        DWORD errorCode = GetLastError();
        std::cerr << "Failed to register raw input device. Error code: " << errorCode << "\n";
    }
}

void UpdateMouseCount(size_t externalCount, size_t ignoredCount) {
    std::ostringstream oss;
    oss << "Number of Mice Connected: " << externalCount;
    if (ignoredCount > 0) {
        oss << "  (ignored touchpads: " << ignoredCount << ")";
    }
    SetWindowText(hwndMouseCount, oss.str().c_str());
}

void DisplayMouseInfo() {
    struct MouseRow {
        std::string name;
        std::string movementCount;
        std::string lastDelta;
        
        MouseRow(const std::string& n, const std::string& mc, const std::string& ld)
            : name(n), movementCount(mc), lastDelta(ld) {}
    };

    size_t externalCount = 0;
    size_t ignoredCount = 0;
    std::vector<MouseRow> rows;

    {
        std::lock_guard<std::mutex> lock(mouseDevicesMutex);
        for (const auto &kv : mouseDevices) {
            HANDLE deviceHandle = kv.first;
            const MouseDevice &device = kv.second;
            if (device.isTouchpad) {
                ignoredCount++;
                continue;
            }

            externalCount++;
            std::string mvCount = std::to_string(device.movements.size());

            std::string lastDelta = "-";
            if (device.movements.size() > 1) {
                auto &mv = device.movements;
                LONG dx = mv[mv.size()-1].x - mv[mv.size()-2].x;
                LONG dy = mv[mv.size()-1].y - mv[mv.size()-2].y;
                std::ostringstream dss; dss << dx << ", " << dy;
                lastDelta = dss.str();
            }

            // Get manufacturer and product for display
            std::string manufacturer = device.manufacturer;
            std::string product = device.product;

            // VID-only fallback: if manufacturer is still empty, try to get VID from device handle
            // and look up manufacturer name (works even if device path was unavailable)
            if (manufacturer.empty()) {
                std::string vid = device.vid;
                std::string pid = device.pid;

                // If device.vid is empty (e.g., device path wasn't available), try getting VID from handle
                if (vid.empty()) {
                    GetVidPidFromHandle(deviceHandle, vid, pid);
                }

                // Look up manufacturer from VID
                if (!vid.empty()) {
                    const char* legitMfr = GetLegitimateManufacturer(vid);
                    if (legitMfr) manufacturer = legitMfr;
                    if (manufacturer.empty()) {
                        const char* jigglerMfr = GetJigglerManufacturer(vid);
                        if (jigglerMfr) manufacturer = jigglerMfr;
                    }
                }
            }

            std::string displayName = BuildDisplayName(device.name, externalCount, manufacturer, product);
            // Mark the primary mouse identified during calibration
            if (primaryMouseIdentified && deviceHandle == primaryMouseHandle) {
                displayName += " [Primary]";
            }
            rows.emplace_back(displayName, mvCount, lastDelta);
        }
    }

    UpdateMouseCount(externalCount, ignoredCount);
    if (hwndListView) {
        // Reduce flicker while rebuilding the list
        SendMessage(hwndListView, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(hwndListView);
        int rowIndex = 0;
        for (const auto &row : rows) {
            LVITEM lvi{};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = rowIndex;
            lvi.iSubItem = 0;
            lvi.pszText = const_cast<char*>(row.name.c_str());
            ListView_InsertItem(hwndListView, &lvi);

            ListView_SetItemText(hwndListView, rowIndex, 1, const_cast<char*>(row.movementCount.c_str()));
            ListView_SetItemText(hwndListView, rowIndex, 2, const_cast<char*>(row.lastDelta.c_str()));
            rowIndex++;
        }
        ListView_SetColumnWidth(hwndListView, 0, LVSCW_AUTOSIZE_USEHEADER);
        ListView_SetColumnWidth(hwndListView, 1, LVSCW_AUTOSIZE_USEHEADER);
        ListView_SetColumnWidth(hwndListView, 2, LVSCW_AUTOSIZE_USEHEADER);
        SendMessage(hwndListView, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(hwndListView, nullptr, TRUE);
    }
}

void DetectMouseJiggler() {
    while (!stopDetection.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        if (stopDetection.load()) break;
        if (detectionPaused || popupShown) continue;

        auto now = std::chrono::steady_clock::now();
        if (lastDetectionTime != std::chrono::steady_clock::time_point::min() &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDetectionTime).count() < detectionCooldownMillis) {
            continue;
        }

        bool jigglerDetected = false;
        bool skipDetection = false;
        DetectionResult detectionResult;
        MouseDevice* detectedDevice = nullptr;

        {
            std::lock_guard<std::mutex> lock(mouseDevicesMutex);

            // Check if touchpad was recently active
            if (lastTouchpadActivity != std::chrono::steady_clock::time_point::min()) {
                auto timeSinceLastTouchpad = std::chrono::duration_cast<std::chrono::seconds>(now - lastTouchpadActivity).count();
                if (timeSinceLastTouchpad < 5) {
                    skipDetection = true;
                }
            }

            if (!skipDetection) {
                // IMPORTANT: Analyze EACH device independently - no comparison needed!
                for (auto& [handle, device] : mouseDevices) {
                    if (device.isTouchpad) continue;

                    // Track whether this is the primary (user's own) mouse.
                    // Primary mouse uses a dedicated wheel-style arc-radius qualifier.
                    bool isPrimaryMouse = (primaryMouseIdentified && handle == primaryMouseHandle);

                    // Skip devices with insufficient movement data
                    // Suspicious VIDs get analyzed faster (fewer samples needed)
                    size_t minMovements = 30;  // Default for verified manufacturers
                    if (device.vidTrustLevel == VIDTrustLevel::BLOCKLIST_CRITICAL ||
                        device.vidTrustLevel == VIDTrustLevel::BLOCKLIST_HIGH) {
                        minMovements = 3;  // Analyze immediately for known suspicious hardware
                    } else if (device.vidTrustLevel == VIDTrustLevel::BLOCKLIST_MEDIUM ||
                               device.vidTrustLevel == VIDTrustLevel::UNKNOWN) {
                        minMovements = 5;  // Quick analysis for unverified devices
                    }
                    // Non-primary devices need fewer samples when primary is known
                    if (primaryMouseIdentified && !isPrimaryMouse && minMovements > 5) {
                        minMovements = 5;
                    }
                    // Primary mouse needs enough data for the per-detector minimums
                    // (rolling windows + confirmation cycles provide false-positive protection).
                    // The gate must stay within the movements-deque cap (200).
                    if (isPrimaryMouse) {
                        minMovements = PRIMARY_MOUSE_MIN_MOVEMENTS;
                    }
                    if (device.movements.size() < minMovements) continue;

                    // Wait minimum movement observation time before analysis.
                    // Primary mouse uses a shorter initial observation window;
                    // per-detector confirmation cycles (15-25 @ 200ms) provide the
                    // main timing protection.  This allows qualification to start
                    // promptly when a jiggler activates at any time.
                    if (device.firstMovementTime == std::chrono::steady_clock::time_point()) continue;
                    double elapsed = std::chrono::duration<double>(now - device.firstMovementTime).count();
                    double minElapsed = isPrimaryMouse ? PRIMARY_MOUSE_MIN_DETECTION_SECONDS : MIN_DETECTION_SECONDS;
                    if (elapsed < minElapsed) continue;

                    // Primary mouse: detect only sustained wheel-style arc radius signatures
                    // or linear zigzag/sawtooth patterns or repetitive micro-delta patterns.
                    // This avoids fragile exceptions and relies on pure movement geometry.
                    if (isPrimaryMouse) {
                        // Staleness gate: only advance detection when the device has
                        // produced new movement data since the last detection cycle.
                        // Without this, a single curved human sweep that then stops
                        // leaves arcHistory full of matching segments, causing
                        // primaryConsecutiveHighScores to climb on frozen data until it
                        // crosses the confirmation threshold.  When no new movement has
                        // arrived, decay the scores and clear arc evidence so the
                        // detector resets naturally — a real jiggler runs continuously
                        // and will never idle long enough to trigger this path.
                        const bool hasNewData =
                            (device.lastMovementTime != device.lastSeenMovementTime);
                        device.lastSeenMovementTime = device.lastMovementTime;

                        if (!hasNewData) {
                            device.arcHistory.clear();
                            device.primaryConsecutiveHighScores =
                                std::max(0, device.primaryConsecutiveHighScores - 1);
                            device.primaryZigzagConsecutiveHighScores =
                                std::max(0, device.primaryZigzagConsecutiveHighScores - 1);
                            device.primaryRepetitiveConsecutiveScores =
                                std::max(0, device.primaryRepetitiveConsecutiveScores - 1);
                            continue;
                        }

                        DetectionResult primaryResult;
                        bool primaryWheelDetected = DetectPrimaryMouseWheelPattern(device, primaryResult);

                        DetectionResult zigzagResult;
                        bool primaryZigzagDetected = DetectPrimaryMouseZigzagPattern(device, zigzagResult);

                        DetectionResult repetitiveResult;
                        bool primaryRepetitiveDetected = DetectPrimaryMouseRepetitiveDelta(device, repetitiveResult);

                        // Reflect qualification progress in behavior score for UI telemetry.
                        int maxProgress = std::max({device.primaryConsecutiveHighScores,
                                                    device.primaryZigzagConsecutiveHighScores,
                                                    device.primaryRepetitiveConsecutiveScores});
                        device.behaviorSuspicionScore = std::min(maxProgress * 4, 100);
                        device.UpdateCombinedScore();

                        if (!primaryWheelDetected && !primaryZigzagDetected && !primaryRepetitiveDetected) {
                            continue;
                        }

                        device.isJiggler = true;
                        DetectionResult& winningResult = primaryWheelDetected ? primaryResult :
                            (primaryZigzagDetected ? zigzagResult : repetitiveResult);
                        device.detectionReason = "Jiggler Detected: " + winningResult.reason;
                        device.detectionType = winningResult.type;
                        detectionResult = winningResult;
                        detectionResult.reason = device.detectionReason;
                        jigglerDetected = true;
                        detectedDevice = &device;
                        break;
                    }

                    // Calculate behavioral suspicion score
                    DetectionResult deviceResult;
                    device.behaviorSuspicionScore = CalculateBehaviorSuspicionScore(device, deviceResult);
                    device.UpdateCombinedScore();

                    // Determine if this device is a jiggler based on combined score
                    // Lower thresholds for more suspicious VIDs:
                    // - BLOCKLIST_CRITICAL/HIGH: 50 (known suspicious hardware)
                    // - BLOCKLIST_MEDIUM: 55 (microcontrollers)
                    // - UNKNOWN: 60 (unrecognized = suspicious!)
                    // - ALLOWLIST: 70 (trusted manufacturers need more evidence)
                    //
                    // When primary mouse is identified, non-primary devices get
                    // lower thresholds since they are inherently more suspicious
                    int threshold = 85;
                    if (device.vidTrustLevel == VIDTrustLevel::BLOCKLIST_CRITICAL ||
                        device.vidTrustLevel == VIDTrustLevel::BLOCKLIST_HIGH) {
                        threshold = primaryMouseIdentified ? 40 : 50;
                    } else if (device.vidTrustLevel == VIDTrustLevel::BLOCKLIST_MEDIUM) {
                        threshold = primaryMouseIdentified ? 45 : 55;
                    } else if (device.vidTrustLevel == VIDTrustLevel::UNKNOWN) {
                        threshold = primaryMouseIdentified ? 50 : 60;
                    } else if (primaryMouseIdentified && !isPrimaryMouse) {
                        threshold = 75; // ALLOWLIST non-primary: still lower than default
                    }

                    if (device.combinedSuspicionScore >= threshold) {
                        device.isJiggler = true;

                        // Build detection reason
                        std::ostringstream reason;
                        reason << "Jiggler Detected: ";

                        // Add VID-based reason if applicable
                        if (device.vidTrustLevel == VIDTrustLevel::BLOCKLIST_CRITICAL) {
                            reason << "Known jiggler hardware (VID " << device.vid << ")";
                        } else if (device.vidTrustLevel == VIDTrustLevel::BLOCKLIST_HIGH) {
                            reason << "DIY/microcontroller hardware (VID " << device.vid << ")";
                        } else if (!deviceResult.reason.empty()) {
                            reason << deviceResult.reason;
                        } else {
                            reason << "Suspicious movement patterns";
                        }

                        device.detectionReason = reason.str();
                        device.detectionType = deviceResult.type;
                        detectionResult = deviceResult;
                        detectionResult.reason = device.detectionReason;
                        jigglerDetected = true;
                        detectedDevice = &device;
                        break;
                    }

                    // Behavior-only detection: suspicious VIDs need LESS behavioral evidence
                    // while verified manufacturers need MORE evidence to flag
                    // Hierarchy: CRITICAL < HIGH < MEDIUM < UNKNOWN < ALLOWLIST

                    // BLOCKLIST_HIGH (DIY boards, VID 0000, etc): very low threshold
                    int highThresh = primaryMouseIdentified ? 15 : 25;
                    if (device.vidTrustLevel == VIDTrustLevel::BLOCKLIST_HIGH && device.behaviorSuspicionScore >= highThresh) {
                        device.isJiggler = true;
                        device.detectionReason = "Jiggler Detected: Suspicious hardware with abnormal patterns (VID " + device.vid + ")";
                        device.detectionType = DetectionType::SMALL_MOVEMENT;
                        detectionResult.type = DetectionType::SMALL_MOVEMENT;
                        detectionResult.reason = device.detectionReason;
                        jigglerDetected = true;
                        detectedDevice = &device;
                        break;
                    }

                    // BLOCKLIST_MEDIUM (microcontrollers): low threshold
                    int medThresh = primaryMouseIdentified ? 25 : 35;
                    if (device.vidTrustLevel == VIDTrustLevel::BLOCKLIST_MEDIUM && device.behaviorSuspicionScore >= medThresh) {
                        device.isJiggler = true;
                        device.detectionReason = "Jiggler Detected: Microcontroller device with suspicious patterns (VID " + device.vid + ")";
                        device.detectionType = DetectionType::SMALL_MOVEMENT;
                        detectionResult.type = DetectionType::SMALL_MOVEMENT;
                        detectionResult.reason = device.detectionReason;
                        jigglerDetected = true;
                        detectedDevice = &device;
                        break;
                    }

                    // UNKNOWN VID: more suspicious than verified - medium threshold
                    int unknownThresh = primaryMouseIdentified ? 40 : 50;
                    if (device.vidTrustLevel == VIDTrustLevel::UNKNOWN && device.behaviorSuspicionScore >= unknownThresh) {
                        device.isJiggler = true;
                        std::string reason = "Jiggler Detected: Unknown device with suspicious patterns";
                        if (!device.vid.empty() && device.vid != "0000") {
                            reason += " (VID " + device.vid + ")";
                        }
                        device.detectionReason = reason;
                        device.detectionType = deviceResult.type;
                        detectionResult.type = deviceResult.type;
                        detectionResult.reason = device.detectionReason;
                        jigglerDetected = true;
                        detectedDevice = &device;
                        break;
                    }

                    // ALLOWLIST (verified manufacturers): requires strongest evidence
                    int allowThresh = primaryMouseIdentified ? 75 : 90;
                    if (device.behaviorSuspicionScore >= allowThresh) {
                        device.isJiggler = true;
                        std::string reason = "Jiggler Detected: ";
                        reason += deviceResult.reason.empty() ? "Highly suspicious movement patterns" : deviceResult.reason;
                        device.detectionReason = reason;
                        device.detectionType = deviceResult.type;
                        detectionResult = deviceResult;
                        detectionResult.reason = device.detectionReason;
                        jigglerDetected = true;
                        detectedDevice = &device;
                        break;
                    }

                    // Also flag if VID is critically suspicious even with lower behavior score
                    int critThresh = primaryMouseIdentified ? 10 : 20;
                    if (device.vidTrustLevel == VIDTrustLevel::BLOCKLIST_CRITICAL && device.behaviorSuspicionScore >= critThresh) {
                        device.isJiggler = true;
                        device.detectionReason = "Jiggler Detected: Known jiggler manufacturer (VID " + device.vid + ")";
                        device.detectionType = DetectionType::SMALL_MOVEMENT;
                        detectionResult.type = DetectionType::SMALL_MOVEMENT;
                        detectionResult.reason = device.detectionReason;
                        jigglerDetected = true;
                        detectedDevice = &device;
                        break;
                    }
                }
            }
        }

        if (skipDetection) continue;

        if (jigglerDetected) {
            currentDetection = detectionResult;
            ShowJigglerDetectedPopup();
            continue;
        }

        // Update status - show analysis is ongoing
        currentDetection.type = DetectionType::NONE;
        currentDetection.reason = "Monitoring... No Jiggler Detected";
        SetWindowText(hwndStatus, currentDetection.reason.c_str());
        gStatusColor = RGB(34, 139, 34);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            // Ensure a minimum window size so the Exit button and footer are visible
            mmi->ptMinTrackSize.x = 700;
            mmi->ptMinTrackSize.y = 520;
            return 0;
        }
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            DrawGradientBackground(hwnd, hdc);
            return 1;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            SetBkMode(hdc, TRANSPARENT);
            if (hCtrl == hwndStatus) {
                SetTextColor(hdc, gStatusColor);
            } else if (hCtrl == hwndTagline) {
                SetTextColor(hdc, gAccentColor);
            } else if (hCtrl == hwndHeader) {
                SetTextColor(hdc, RGB(255, 255, 255));
            } else {
                SetTextColor(hdc, RGB(20, 24, 44));
            }
            if (hCtrl == hwndHeader || hCtrl == hwndTagline) {
                return (LRESULT)GetStockObject(NULL_BRUSH);
            }
            if (hBgBrush) return (LRESULT)hBgBrush;
            break;
        }
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            if ((HWND)lParam == hwndStatusGroup || (HWND)lParam == hwndDevicesGroup) {
                SetTextColor(hdc, gAccentColor);
            } else {
                SetTextColor(hdc, RGB(20, 24, 44));
            }
            if (hBgBrush) return (LRESULT)hBgBrush;
            break;
        }
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            int padding = 16;

            int x = padding;
            int y = 10;
            int headerH = 32;
            if (hwndHeader) MoveWindow(hwndHeader, x, y, std::max(0, width - 2*padding), headerH, TRUE);

            y += headerH + 12;
            int countH = 24;
            if (hwndMouseCount) MoveWindow(hwndMouseCount, x, y, std::max(0, width - 2*padding), countH, TRUE);

            y += countH + 6;
            int statusGroupH = 80;
            if (hwndStatusGroup) MoveWindow(hwndStatusGroup, x, y, std::max(0, width - 2*padding), statusGroupH, TRUE);
            if (hwndStatus) MoveWindow(hwndStatus, x + 16, y + 32, std::max(0, width - 2*padding - 32), 32, TRUE);

            y += statusGroupH + 8;

            int exitBtnW = 80, exitBtnH = 30;
            int liveBtnW = 170, liveBtnH = 30;
            int btnRowH = 30;
            int linkRowH = 24;
            int rowGap = 8;
            int footerTotalH = btnRowH + rowGap + linkRowH + padding; // buttons + gap + link + bottom padding

            int devicesGroupH = std::max(120, height - y - footerTotalH - padding);
            if (hwndDevicesGroup) MoveWindow(hwndDevicesGroup, x, y, std::max(0, width - 2*padding), devicesGroupH, TRUE);
            // ListView inside the group box with padding
            int lvX = x + 16;
            int lvY = y + 26;
            int lvW = std::max(0, width - 2*padding - 32);
            int lvH = std::max(0, devicesGroupH - 42);
            if (hwndListView) MoveWindow(hwndListView, lvX, lvY, lvW, lvH, TRUE);

            // Footer: buttons and branding text on top row, link on its own row below
            int btnAreaW = liveBtnW + 12 + exitBtnW;
            int btnY = height - padding - linkRowH - rowGap - btnRowH;
            int linkY = height - padding - linkRowH;
            int footerTextW = std::max(0, width - 2*padding - btnAreaW - 16);

            if (hwndFooterText) MoveWindow(hwndFooterText, x, btnY + (btnRowH - 24) / 2, footerTextW, 24, TRUE);
            if (hwndLink) MoveWindow(hwndLink, x, linkY, std::max(0, width - 2*padding), linkRowH, TRUE);
            if (hwndLiveChartButton) MoveWindow(hwndLiveChartButton, width - padding - exitBtnW - 12 - liveBtnW, btnY, liveBtnW, liveBtnH, TRUE);
            if (hwndExitButton) MoveWindow(hwndExitButton, width - padding - exitBtnW, btnY, exitBtnW, exitBtnH, TRUE);
            return 0;
        }
        case WM_INPUT: {
            UINT dwSize;
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
            LPBYTE lpb = new BYTE[dwSize];
            if (lpb == NULL) {
                return 0;
            }

            if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
                std::cerr << "GetRawInputData does not return correct size!\n";
                delete[] lpb;
                break;
            }

            RAWINPUT* raw = (RAWINPUT*)lpb;

            // Forward mouse movement to live chart regardless of detection state
            if (raw->header.dwType == RIM_TYPEMOUSE) {
                LONG dx = raw->data.mouse.lLastX;
                LONG dy = raw->data.mouse.lLastY;
                if ((dx != 0 || dy != 0) && IsLiveChartActive()) {
                    LiveChartRecordMovement(raw->header.hDevice, dx, dy);
                }
            }

            if (detectionPaused || stopDetection.load()) {
                delete[] lpb;
                break;
            }

            if (raw->header.dwType == RIM_TYPEMOUSE) {
                HANDLE deviceHandle = raw->header.hDevice;
                std::string deviceName = GetDeviceNameUtf8(deviceHandle);
                if (deviceName.empty()) {
                    deviceName = "Mouse " + std::to_string(mouseCount + 1);
                }

                {
                    std::lock_guard<std::mutex> lock(mouseDevicesMutex);
                    auto it = mouseDevices.find(deviceHandle);
                    if (it == mouseDevices.end()) {
                        bool isTp = IsTouchpadDevice(deviceHandle, raw);
                        if (isTp) {
                            lastTouchpadActivity = std::chrono::steady_clock::now();
                        } else {
                            mouseDevices[deviceHandle] = MouseDevice(deviceName, false);
                            GetHidAttributes(deviceName, mouseDevices[deviceHandle].manufacturer, mouseDevices[deviceHandle].product);
                            mouseCount++;
                            it = mouseDevices.find(deviceHandle);

                            // Primary mouse reconnection recovery: if the primary mouse was
                            // previously calibrated but its kernel handle is no longer in
                            // mouseDevices (the device was physically unplugged/replugged,
                            // or mouseDevices was cleared by a WM_DEVICECHANGE event),
                            // restore primaryMouseHandle automatically via VID/PID matching
                            // so the user does not need to recalibrate.
                            // Guard: only rebind when the OLD handle is truly gone and the
                            // new device's VID/PID matches — avoids mis-binding when two
                            // identical mice are connected simultaneously.
                            if (primaryMouseIdentified
                                && deviceHandle != primaryMouseHandle
                                && mouseDevices.find(primaryMouseHandle) == mouseDevices.end()
                                && !primaryMouseVid.empty()
                                && it != mouseDevices.end()
                                && it->second.vid == primaryMouseVid
                                && it->second.pid == primaryMousePid) {
                                primaryMouseHandle = deviceHandle;
                            }
                        }
                    } else {
                        MouseDevice &device = it->second;
                        if (device.name.empty()) {
                            device.name = deviceName;
                        }
                        if (!device.nameTouchpadHint && IsLikelyTouchpadName(device.name)) {
                            device.nameTouchpadHint = true;
                        }
                        if (DeviceHasTouchpadUsage(deviceHandle)) {
                            device.usageTouchpadHint = true;
                        }
                        if (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
                            device.absoluteMoveCount++;
                        }

                        if (!device.isTouchpad && IsTouchpadDevice(deviceHandle, raw, &device)) {
                        // Protect the explicitly calibrated primary mouse from false
                        // reclassification.  Some legitimate USB mice emit occasional
                        // MOUSE_MOVE_ABSOLUTE events (during DPI changes, firmware init,
                        // etc.) which trigger a false positive through the absoluteHint
                        // path.  Because the user explicitly identified this device during
                        // calibration we trust that decision and do not override it.
                        if (primaryMouseIdentified && primaryMouseHandle == deviceHandle) {
                            // Do not reclassify the calibrated primary mouse as a touchpad.
                        } else {
                            // For all other devices, absolute movement alone is not
                            // sufficient evidence to reclassify an established device —
                            // real USB mice can emit MOUSE_MOVE_ABSOLUTE sporadically.
                            // Require at least one strong structural hint: HID digitizer
                            // usage page, a touchpad keyword in the name, or a known
                            // touchpad-only VID.
                            const bool hasStrongHint = DeviceHasTouchpadUsage(deviceHandle)
                                                     || IsLikelyTouchpadName(device.name)
                                                     || IsTouchpadByVID(device.name);
                            if (hasStrongHint) {
                                // Reclassified as touchpad: drop from tracked mouse devices.
                                mouseDevices.erase(it);
                                if (mouseCount > 0) mouseCount--;
                                it = mouseDevices.end();
                                lastTouchpadActivity = std::chrono::steady_clock::now();
                            }
                            // else: only weak signal (absolute movement) — keep the device.
                        }
                    }
                    }

                    if (it != mouseDevices.end()) {
                        // Use per-device raw deltas to avoid conflating devices
                        LONG dx = raw->data.mouse.lLastX;
                        LONG dy = raw->data.mouse.lLastY;

                        if (dx != 0 || dy != 0) {
                            auto now = std::chrono::steady_clock::now();
                            auto& device = it->second;

                            // Record when this device first started moving
                            if (device.firstMovementTime == std::chrono::steady_clock::time_point()) {
                                device.firstMovementTime = now;
                            }
                            
                            // Update continuous movement tracking
                            double timeSinceLastMove = std::chrono::duration<double>(now - device.lastMovementTime).count();
                            if (timeSinceLastMove < PAUSE_THRESHOLD_SECONDS) {
                                device.continuousDuration += timeSinceLastMove;
                            } else {
                                device.continuousDuration = 0.0;
                            }
                            device.lastMovementTime = now;
                            
                            // Calculate and store speed for constant speed detection
                            double speed = sqrt((double)(dx * dx + dy * dy));
                            device.speedHistory.push_back(speed);
                            if (device.speedHistory.size() > MAX_SPEED_HISTORY_SIZE) {
                                device.speedHistory.pop_front();
                            }
                            
                            // Update position history
                            POINT last = {0, 0};
                            if (!device.movements.empty()) {
                                last = device.movements.back();
                            }
                            POINT next = { last.x + (LONG)dx, last.y + (LONG)dy };
                            device.movements.push_back(next);
                            
                            // Store for pattern detection
                            device.patternHistory.push_back(next);
                            if (device.patternHistory.size() > MAX_PATTERN_HISTORY_SIZE) {
                                device.patternHistory.pop_front();
                            }
                            
                            // Track direction for oscillation detection
                            int dirX = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
                            int dirY = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);
                            device.directionXHistory.push_back(dirX);
                            device.directionYHistory.push_back(dirY);
                            if (device.directionXHistory.size() > MAX_DIRECTION_HISTORY_SIZE) {
                                device.directionXHistory.pop_front();
                                device.directionYHistory.pop_front();
                            }

                            // Bound movement history to keep UI responsive
                            auto &mv = device.movements;
                            const size_t maxHistory = 200;
                            while (mv.size() > maxHistory) {
                                mv.pop_front();
                            }
                        }
                    }
                }
            }
            
            if (raw->header.dwType == RIM_TYPEHID) {
                HANDLE deviceHandle = raw->header.hDevice;
                
                // Get device info to check if it's a touchpad
                UINT infoSize = 0;
                if (GetRawInputDeviceInfo(deviceHandle, RIDI_DEVICEINFO, NULL, &infoSize) == 0) {
                    std::vector<BYTE> infoBuf(infoSize);
                    RID_DEVICE_INFO* deviceInfo = reinterpret_cast<RID_DEVICE_INFO*>(infoBuf.data());
                    deviceInfo->cbSize = sizeof(RID_DEVICE_INFO);
                    if (GetRawInputDeviceInfo(deviceHandle, RIDI_DEVICEINFO, deviceInfo, &infoSize) > 0) {
                        // Check if this is a touchpad (UsagePage 0x0D, Usage 0x05 or 0x04)
                        if (deviceInfo->dwType == RIM_TYPEHID &&
                            deviceInfo->hid.usUsagePage == 0x0D &&
                            (deviceInfo->hid.usUsage == 0x05 || deviceInfo->hid.usUsage == 0x04)) {
                            // This is a touchpad - update last activity time
                            std::lock_guard<std::mutex> lock(mouseDevicesMutex);
                            lastTouchpadActivity = std::chrono::steady_clock::now();
                        }
                    }
                }
            }

            delete[] lpb;  // Memory cleanup
            break;  // Removed DisplayMouseInfo from here
        }
        case WM_NOTIFY: {
            LPNMHDR pnm = (LPNMHDR)lParam;
            if (pnm && pnm->hwndFrom == hwndLink && pnm->code == NM_CLICK) {
                // SysLink clicked; open the GitHub repository URL
                ShellExecute(0, 0, "https://detectmyjiggler.com", 0, 0, SW_SHOW);
                return 0;
            }
            break;
        }
        case WM_DEVICECHANGE: {
            if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
                RegisterDevices(hwnd);
                {
                    std::lock_guard<std::mutex> lock(mouseDevicesMutex);
                    mouseDevices.clear();
                    mouseCount = 0;
                }
                DisplayMouseInfo();
            }
            break;
        }
        case WM_KEYDOWN: {
            // Ctrl+L opens the Live Mouse Movement chart window
            if (wParam == 'L' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                ShowLiveChartWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == 1) {  // Corrected to handle Exit button
                DestroyWindow(hwnd);    // Properly close the window
            } else if (LOWORD(wParam) == 2) {  // Live Mouse Movement button
                ShowLiveChartWindow(hwnd);
            }
            break;
        }
        case WM_TIMER: {  // New case for handling the timer
            if (wParam == 1) {
                DisplayMouseInfo();
            }
            break;
        }
        case WM_DESTROY: {
            stopDetection.store(true);
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Improve clarity on high DPI displays
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    
    // Initialize detection state
    currentDetection.type = DetectionType::NONE;
    currentDetection.reason = "No Jiggler Detected";
    
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RawInputClass";

    RegisterClass(&wc);

    hwndMain = CreateWindowEx(
            0,
            "RawInputClass",
            "Mouse Jiggler Detection",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 720, 560,
            NULL,
            NULL,
            hInstance,
            NULL
    );

    if (hwndMain == NULL) {
        std::cerr << "Failed to create window.\n";
        return -1;
    }

    // Create fonts
    hFontTitle = CreateFont(
            -26, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            VARIABLE_PITCH, "Segoe UI");
    hFontSubtitle = CreateFont(
            -16, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            VARIABLE_PITCH, "Segoe UI");
    hFontText = CreateFont(
            -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            VARIABLE_PITCH, "Segoe UI");

    // Background brush
    hBgBrush = CreateSolidBrush(gPanelColor);

    // Header
    hwndHeader = CreateWindowEx(
            0,
            "STATIC",
            "Mouse Jiggler Detection",
            WS_CHILD | WS_VISIBLE,
            16, 10, 400, 32,
            hwndMain,
            NULL,
            hInstance,
            NULL
    );
    SendMessage(hwndHeader, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

    hwndMouseCount = CreateWindowEx(
            0,
            "STATIC",
            "Number of Mice Connected: 0",
            WS_CHILD | WS_VISIBLE,
            16, 52, 300, 24,
            hwndMain,
            NULL,
            hInstance,
            NULL
    );
    SendMessage(hwndMouseCount, WM_SETFONT, (WPARAM)hFontText, TRUE);

    hwndStatusGroup = CreateWindowEx(
            0,
            "BUTTON",
            "Detection Status",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            16, 78, 560, 80,
            hwndMain,
            NULL,
            hInstance,
            NULL
    );
    SendMessage(hwndStatusGroup, WM_SETFONT, (WPARAM)hFontText, TRUE);

    hwndStatus = CreateWindowEx(
            0,
            "STATIC",
            "No Jiggler Detected",
            WS_CHILD | WS_VISIBLE,
            32, 110, 520, 32,
            hwndMain,
            NULL,
            hInstance,
            NULL
    );
    SendMessage(hwndStatus, WM_SETFONT, (WPARAM)hFontSubtitle, TRUE);

    hwndDevicesGroup = CreateWindowEx(
            0,
            "BUTTON",
            "Connected Devices",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            16, 166, 560, 230,
            hwndMain,
            NULL,
            hInstance,
            NULL
    );
    SendMessage(hwndDevicesGroup, WM_SETFONT, (WPARAM)hFontText, TRUE);

    hwndListView = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            WC_LISTVIEW,
            "",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            32, 192, 528, 190,
            hwndMain,
            NULL,
            hInstance,
            NULL
    );
    SendMessage(hwndListView, WM_SETFONT, (WPARAM)hFontText, TRUE);
    ListView_SetExtendedListViewStyle(hwndListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    // Apply Explorer theme for a modern look
    SetWindowTheme(hwndListView, L"Explorer", nullptr);
    ApplyListViewTheme(hwndListView);

    LVCOLUMN col{}; col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = 200; col.pszText = const_cast<char*>("Device"); ListView_InsertColumn(hwndListView, 0, &col);
    col.cx = 120; col.pszText = const_cast<char*>("Movements"); ListView_InsertColumn(hwndListView, 1, &col);
    col.cx = 120; col.pszText = const_cast<char*>("Last (x,y)"); ListView_InsertColumn(hwndListView, 2, &col);


    // Application information link
    hwndLink = CreateWindowEx(
            0,
            "SysLink",
            "Learn more about mouse jiggler detection algorithms. <a href=\"https://detectmyjiggler.com\">Visit detectmyjiggler.com</a>.",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            16, 416, 560, 24,
            hwndMain,
            NULL,
            hInstance,
            NULL
    );

    SendMessage(hwndLink, WM_SETFONT, (WPARAM)hFontText, TRUE);

        // Footer brand text
    hwndFooterText = CreateWindowEx(
            0,
            "STATIC",
            "Mouse Jiggler Detector - Open Source Project",
            WS_CHILD | WS_VISIBLE,
            16, 392, 360, 24,
            hwndMain,
            NULL,
            hInstance,
            NULL
    );
    SendMessage(hwndFooterText, WM_SETFONT, (WPARAM)hFontText, TRUE);

    hwndExitButton = CreateWindowEx(
            0,
            "BUTTON",
            "Exit",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            496, 392, 80, 30,
            hwndMain,
            (HMENU)1,  // ID for the button
            hInstance,
            NULL
    );
    SendMessage(hwndExitButton, WM_SETFONT, (WPARAM)hFontText, TRUE);
    SetWindowTheme(hwndExitButton, L"Explorer", nullptr);

    hwndLiveChartButton = CreateWindowEx(
            0,
            "BUTTON",
            "Live Mouse Movement",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            304, 392, 170, 30,
            hwndMain,
            (HMENU)2,  // ID for the button
            hInstance,
            NULL
    );
    SendMessage(hwndLiveChartButton, WM_SETFONT, (WPARAM)hFontText, TRUE);
    SetWindowTheme(hwndLiveChartButton, L"Explorer", nullptr);

    RegisterDevices(hwndMain);

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);

    // Show calibration popup to identify the primary mouse before detection starts
    SetWindowText(hwndStatus, "Waiting for mouse calibration...");
    gStatusColor = RGB(90, 102, 255); // Blue during calibration
    InvalidateRect(hwndMain, NULL, TRUE);
    ShowCalibrationPopup(hwndMain);

    // Update status based on calibration result
    if (primaryMouseIdentified) {
        std::string calibMsg = "Primary mouse identified - Monitoring...";
        SetWindowText(hwndStatus, calibMsg.c_str());
        gStatusColor = RGB(34, 139, 34);
    } else {
        if (primaryMouseSelectionDisabled) {
            SetWindowText(hwndStatus, "Monitoring... No primary mouse (touchpad-only mode).");
            gStatusColor = RGB(34, 139, 34);
        } else if (calibrationIgnoredTouchpad) {
            SetWindowText(hwndStatus, "Touchpad ignored. Move/click external mouse to set primary.");
            gStatusColor = RGB(90, 102, 255);
        } else {
            SetWindowText(hwndStatus, "Monitoring... No Jiggler Detected");
            gStatusColor = RGB(34, 139, 34);
        }
    }
    InvalidateRect(hwndMain, NULL, TRUE);

    SetTimer(hwndMain, 1, 200, NULL);  // Update the display every 200ms for smoother UI.

    detectionThread = std::thread(DetectMouseJiggler);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (detectionThread.joinable()) {
        detectionThread.join();
    }

    if (hFontTitle) DeleteObject(hFontTitle);
    if (hFontSubtitle) DeleteObject(hFontSubtitle);
    if (hFontText) DeleteObject(hFontText);
    if (hBgBrush) DeleteObject(hBgBrush);

    return 0;
}
