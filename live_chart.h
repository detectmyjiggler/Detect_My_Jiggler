#pragma once
// =============================================================================
// LIVE MOUSE MOVEMENT CHART
// =============================================================================
//
// Displays real-time mouse movement data from all connected mice in a
// separate window with three chart panels:
//   1. Mouse Path (2D trajectory with per-device colors)
//   2. X Position over Time
//   3. Y Position over Time
//
// Inspired by Live Chart/live_chart.py
// =============================================================================

#define NOMINMAX
#include <windows.h>

// Show the live mouse movement chart window (non-modal, owned by parent).
// If the window is already open it is brought to the foreground.
void ShowLiveChartWindow(HWND parent);

// Record a raw-input movement event.  Called from the main window's
// WM_INPUT handler for every non-touchpad mouse delta.
void LiveChartRecordMovement(HANDLE device, LONG dx, LONG dy);

// Returns true when the live chart window is open and recording.
bool IsLiveChartActive();
