// =============================================================================
// LIVE MOUSE MOVEMENT CHART - Implementation
// =============================================================================
//
// Opens a resizable window that plots live mouse movement from every
// connected (non-touchpad) mouse.  Three panels are drawn with GDI:
//   - Mouse Path  (2D, one color per device, start/end markers)
//   - X Position over Time
//   - Y Position over Time
// Start / Stop buttons and a status bar round out the UI.
// =============================================================================

#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <iomanip>

#include "live_chart.h"

// ── Colour palette ──────────────────────────────────────────────────────────
static const COLORREF BG_DARK       = RGB(15, 15, 26);
static const COLORREF BG_PANEL      = RGB(26, 26, 46);
static const COLORREF COL_GRID      = RGB(51, 51, 85);
static const COLORREF COL_TICK      = RGB(170, 170, 204);
static const COLORREF COL_TITLE     = RGB(255, 255, 255);
static const COLORREF COL_START_MKR = RGB(0, 255, 136);
static const COLORREF COL_END_MKR   = RGB(255, 68, 102);

static const COLORREF DEVICE_PALETTE[] = {
    RGB(79, 195, 247),   // Light blue
    RGB(244, 143, 177),  // Pink
    RGB(129, 199, 132),  // Green
    RGB(255, 183, 77),   // Orange
    RGB(186, 104, 200),  // Purple
    RGB(255, 241, 118),  // Yellow
    RGB(128, 222, 234),  // Cyan
    RGB(255, 138, 128),  // Coral
};
static const int NUM_PALETTE = sizeof(DEVICE_PALETTE) / sizeof(DEVICE_PALETTE[0]);

// ── Layout constants ────────────────────────────────────────────────────────
static const int MARGIN_LEFT   = 60;
static const int MARGIN_RIGHT  = 20;
static const int MARGIN_TOP    = 28;
static const int MARGIN_BOTTOM = 35;
static const int FOOTER_H      = 70;
static const int GAP            = 10;

// ── Control IDs ─────────────────────────────────────────────────────────────
static const int IDC_LC_START  = 301;
static const int IDC_LC_STOP   = 302;
static const int IDC_LC_STATUS = 303;
static const UINT_PTR TIMER_ID = 100;
static const UINT TIMER_MS     = 500;

// ── Per-point data ──────────────────────────────────────────────────────────
struct ChartPoint {
    double elapsed;   // seconds since recording started
    LONG   x, y;      // cumulative position (sum of deltas)
};

struct DeviceTrack {
    std::deque<ChartPoint> points;
    LONG     cumX  = 0;
    LONG     cumY  = 0;
    LONG     startX = 0;       // Position of the very first recorded point
    LONG     startY = 0;       // (never changes once set)
    bool     hasStart = false;  // true after the first point is recorded
    COLORREF color = RGB(255, 255, 255);
    std::string label;
};

// ── Module-level state ──────────────────────────────────────────────────────
static std::unordered_map<HANDLE, DeviceTrack> s_tracks;
static std::mutex                              s_mutex;
static std::chrono::steady_clock::time_point   s_startTime;
static bool   s_recording     = true;
static HWND   s_chartWnd      = nullptr;
static HWND   s_btnStart      = nullptr;
static HWND   s_btnStop       = nullptr;
static HWND   s_statusLabel   = nullptr;
static HFONT  s_fontTitle     = nullptr;
static HFONT  s_fontLabel     = nullptr;
static HFONT  s_fontSmall     = nullptr;
static HBRUSH s_bgBrush       = nullptr;
static int    s_nextDeviceNum = 1;

static const size_t MAX_PTS_PER_DEVICE = 10000;

// Rolling window for X/Y time-series charts: once data exceeds this duration,
// only the most recent portion is shown.
static const double ROLLING_WINDOW_SECONDS = 15.0 * 60.0;  // 900 seconds

// ── Helpers: coordinate mapping ─────────────────────────────────────────────

// Map a data value to a horizontal pixel position (left → right).
static int MapX(double val, double lo, double hi, int pxL, int pxR) {
    if (hi <= lo) return (pxL + pxR) / 2;
    return pxL + (int)((val - lo) / (hi - lo) * (pxR - pxL));
}

// Map a data value to a vertical pixel position (value increases upward).
static int MapY(double val, double lo, double hi, int pxT, int pxB) {
    if (hi <= lo) return (pxT + pxB) / 2;
    return pxB - (int)((val - lo) / (hi - lo) * (pxB - pxT));
}

// ── Helpers: GDI primitives ─────────────────────────────────────────────────

static void FillRectSolid(HDC hdc, const RECT& rc, COLORREF c) {
    HBRUSH br = CreateSolidBrush(c);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
}

static void DrawFrame(HDC hdc, const RECT& rc) {
    HPEN pen = CreatePen(PS_SOLID, 1, COL_GRID);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void GridLineH(HDC hdc, int x1, int x2, int y) {
    HPEN pen = CreatePen(PS_DOT, 1, COL_GRID);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x1, y, NULL);
    LineTo(hdc, x2, y);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void GridLineV(HDC hdc, int x, int y1, int y2) {
    HPEN pen = CreatePen(PS_DOT, 1, COL_GRID);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x, y1, NULL);
    LineTo(hdc, x, y2);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

// Axis-range helper
struct Range {
    double lo =  1e18;
    double hi = -1e18;
    void update(double v) { if (v < lo) lo = v; if (v > hi) hi = v; }
    void pad(double frac = 0.05) {
        double span = hi - lo;
        if (span < 1.0) span = 1.0;
        lo -= span * frac;
        hi += span * frac;
    }
};

// ── Chart drawing ───────────────────────────────────────────────────────────

using Snapshot = std::vector<std::pair<HANDLE, DeviceTrack>>;

static void DrawPathChart(HDC hdc, RECT area, const Snapshot& snap) {
    FillRectSolid(hdc, area, BG_PANEL);
    DrawFrame(hdc, area);

    // Title
    SetTextColor(hdc, COL_TITLE);
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, s_fontTitle);
    RECT titleRc = { area.left + 10, area.top + 4, area.right - 10, area.top + 24 };
    DrawTextA(hdc, "Mouse Path (color = device)", -1, &titleRc, DT_LEFT | DT_SINGLELINE);

    // Plot region inside margins
    int pL = area.left   + MARGIN_LEFT;
    int pR = area.right  - MARGIN_RIGHT;
    int pT = area.top    + MARGIN_TOP;
    int pB = area.bottom - MARGIN_BOTTOM;
    if (pR <= pL || pB <= pT) return;

    // Compute data range (negate Y so "up on screen" → "up on chart")
    Range xR, yR;
    bool hasData = false;
    for (auto& [h, t] : snap) {
        // Include the fixed start position so the start marker is always visible
        if (t.hasStart) {
            xR.update((double)t.startX);
            yR.update((double)(-t.startY));
            hasData = true;
        }
        for (auto& pt : t.points) {
            xR.update((double)pt.x);
            yR.update((double)(-pt.y));
            hasData = true;
        }
    }

    if (!hasData) {
        SetTextColor(hdc, COL_TICK);
        SelectObject(hdc, s_fontLabel);
        RECT msgRc = { pL, pT, pR, pB };
        DrawTextA(hdc, "Move your mouse to start...", -1, &msgRc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    xR.pad();
    yR.pad();

    // Grid + tick labels  (5 ticks per axis)
    SetTextColor(hdc, COL_TICK);
    SelectObject(hdc, s_fontSmall);
    for (int i = 0; i <= 4; ++i) {
        double xVal = xR.lo + (xR.hi - xR.lo) * i / 4.0;
        int px = MapX(xVal, xR.lo, xR.hi, pL, pR);
        GridLineV(hdc, px, pT, pB);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f", xVal);
        TextOutA(hdc, px - 15, pB + 3, buf, (int)strlen(buf));

        double yVal = yR.lo + (yR.hi - yR.lo) * i / 4.0;
        int py = MapY(yVal, yR.lo, yR.hi, pT, pB);
        GridLineH(hdc, pL, pR, py);
        snprintf(buf, sizeof(buf), "%.0f", -yVal);   // show original Y
        TextOutA(hdc, pL - 55, py - 7, buf, (int)strlen(buf));
    }

    // Axis labels
    TextOutA(hdc, (pL + pR) / 2 - 12, pB + 18, "X (px)", 6);

    // Draw each device's path
    for (auto& [h, t] : snap) {
        if (t.points.size() < 2) continue;

        HPEN pen = CreatePen(PS_SOLID, 2, t.color);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);

        auto& pts = t.points;
        int prevPx = MapX((double)pts[0].x,  xR.lo, xR.hi, pL, pR);
        int prevPy = MapY((double)(-pts[0].y), yR.lo, yR.hi, pT, pB);

        for (size_t i = 1; i < pts.size(); ++i) {
            int cx = MapX((double)pts[i].x,  xR.lo, xR.hi, pL, pR);
            int cy = MapY((double)(-pts[i].y), yR.lo, yR.hi, pT, pB);
            MoveToEx(hdc, prevPx, prevPy, NULL);
            LineTo(hdc, cx, cy);
            prevPx = cx;
            prevPy = cy;
        }
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        // Start marker (green dot) — pinned to original start position
        {
            int sx = MapX((double)t.startX,  xR.lo, xR.hi, pL, pR);
            int sy = MapY((double)(-t.startY), yR.lo, yR.hi, pT, pB);
            HBRUSH br = CreateSolidBrush(COL_START_MKR);
            HPEN mp   = CreatePen(PS_SOLID, 1, COL_START_MKR);
            HBRUSH ob = (HBRUSH)SelectObject(hdc, br);
            HPEN   op = (HPEN)SelectObject(hdc, mp);
            Ellipse(hdc, sx - 5, sy - 5, sx + 6, sy + 6);
            SelectObject(hdc, ob);
            SelectObject(hdc, op);
            DeleteObject(br);
            DeleteObject(mp);
        }

        // End marker (red X)
        {
            int ex = MapX((double)pts.back().x,  xR.lo, xR.hi, pL, pR);
            int ey = MapY((double)(-pts.back().y), yR.lo, yR.hi, pT, pB);
            HPEN ep = CreatePen(PS_SOLID, 2, COL_END_MKR);
            HPEN op = (HPEN)SelectObject(hdc, ep);
            MoveToEx(hdc, ex - 5, ey - 5, NULL); LineTo(hdc, ex + 6, ey + 6);
            MoveToEx(hdc, ex + 5, ey - 5, NULL); LineTo(hdc, ex - 6, ey + 6);
            SelectObject(hdc, op);
            DeleteObject(ep);
        }
    }

    // Legend (top-right)
    int legX = pR - 130;
    int legY = pT + 4;
    SelectObject(hdc, s_fontSmall);
    for (auto& [h, t] : snap) {
        RECT cb = { legX, legY + 2, legX + 12, legY + 12 };
        FillRectSolid(hdc, cb, t.color);
        SetTextColor(hdc, COL_TICK);
        TextOutA(hdc, legX + 16, legY, t.label.c_str(), (int)t.label.size());
        legY += 16;
        if (legY > pB - 10) break;   // don't overflow
    }
}

static void DrawTimeSeries(HDC hdc, RECT area, const Snapshot& snap,
                           bool isX, const char* title, COLORREF accentCol) {
    FillRectSolid(hdc, area, BG_PANEL);
    DrawFrame(hdc, area);

    // Title
    SetTextColor(hdc, COL_TITLE);
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, s_fontTitle);
    RECT titleRc = { area.left + 10, area.top + 4, area.right - 10, area.top + 24 };
    DrawTextA(hdc, title, -1, &titleRc, DT_LEFT | DT_SINGLELINE);

    int pL = area.left   + MARGIN_LEFT;
    int pR = area.right  - MARGIN_RIGHT;
    int pT = area.top    + MARGIN_TOP;
    int pB = area.bottom - MARGIN_BOTTOM;
    if (pR <= pL || pB <= pT) return;

    Range tR, vR;
    bool hasData = false;
    for (auto& [h, t] : snap) {
        for (auto& pt : t.points) {
            tR.update(pt.elapsed);
            vR.update((double)(isX ? pt.x : pt.y));
            hasData = true;
        }
    }

    if (!hasData) {
        SetTextColor(hdc, COL_TICK);
        SelectObject(hdc, s_fontLabel);
        RECT msgRc = { pL, pT, pR, pB };
        DrawTextA(hdc, "No data", -1, &msgRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    vR.pad(0.10);
    if (tR.hi <= tR.lo) tR.hi = tR.lo + 1.0;

    // Rolling window: once data exceeds 15 minutes, only show the last 15 min.
    if (tR.hi - tR.lo > ROLLING_WINDOW_SECONDS) {
        tR.lo = tR.hi - ROLLING_WINDOW_SECONDS;

        // Recompute value range for only the visible portion of data
        vR = Range{};
        for (auto& [h, t] : snap) {
            for (auto& pt : t.points) {
                if (pt.elapsed >= tR.lo) {
                    vR.update((double)(isX ? pt.x : pt.y));
                }
            }
        }
        vR.pad(0.10);
    }

    // Grid + tick labels
    SetTextColor(hdc, COL_TICK);
    SelectObject(hdc, s_fontSmall);
    for (int i = 0; i <= 4; ++i) {
        double tVal = tR.lo + (tR.hi - tR.lo) * i / 4.0;
        int px = MapX(tVal, tR.lo, tR.hi, pL, pR);
        GridLineV(hdc, px, pT, pB);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fs", tVal);
        TextOutA(hdc, px - 15, pB + 3, buf, (int)strlen(buf));

        double vVal = vR.lo + (vR.hi - vR.lo) * i / 4.0;
        int py = MapY(vVal, vR.lo, vR.hi, pT, pB);
        GridLineH(hdc, pL, pR, py);
        snprintf(buf, sizeof(buf), "%.0f", vVal);
        TextOutA(hdc, pL - 50, py - 7, buf, (int)strlen(buf));
    }

    // Axis labels
    TextOutA(hdc, (pL + pR) / 2 - 30, pB + 18, "Elapsed (s)", 11);
    const char* yLbl = isX ? "X (px)" : "Y (px)";
    TextOutA(hdc, area.left + 5, (pT + pB) / 2 - 7, yLbl, (int)strlen(yLbl));

    // Lines per device
    for (auto& [h, t] : snap) {
        if (t.points.size() < 2) continue;

        HPEN pen = CreatePen(PS_SOLID, 1, t.color);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);

        auto& pts = t.points;
        double v0 = (double)(isX ? pts[0].x : pts[0].y);
        int prevPx = MapX(pts[0].elapsed, tR.lo, tR.hi, pL, pR);
        int prevPy = MapY(v0, vR.lo, vR.hi, pT, pB);

        for (size_t i = 1; i < pts.size(); ++i) {
            double v = (double)(isX ? pts[i].x : pts[i].y);
            int cx = MapX(pts[i].elapsed, tR.lo, tR.hi, pL, pR);
            int cy = MapY(v, vR.lo, vR.hi, pT, pB);
            MoveToEx(hdc, prevPx, prevPy, NULL);
            LineTo(hdc, cx, cy);
            prevPx = cx;
            prevPy = cy;
        }

        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }
}

// ── Window procedure ────────────────────────────────────────────────────────

static LRESULT CALLBACK LiveChartProc(HWND hwnd, UINT uMsg,
                                      WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        s_fontTitle = CreateFont(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, VARIABLE_PITCH, "Segoe UI");
        s_fontLabel = CreateFont(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, VARIABLE_PITCH, "Segoe UI");
        s_fontSmall = CreateFont(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, VARIABLE_PITCH, "Segoe UI");
        s_bgBrush = CreateSolidBrush(BG_DARK);

        s_btnStart = CreateWindowEx(0, "BUTTON", "Start",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 0, 90, 28, hwnd, (HMENU)(INT_PTR)IDC_LC_START, NULL, NULL);
        SendMessage(s_btnStart, WM_SETFONT, (WPARAM)s_fontLabel, TRUE);

        s_btnStop = CreateWindowEx(0, "BUTTON", "Stop",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            110, 0, 90, 28, hwnd, (HMENU)(INT_PTR)IDC_LC_STOP, NULL, NULL);
        SendMessage(s_btnStop, WM_SETFONT, (WPARAM)s_fontLabel, TRUE);

        s_statusLabel = CreateWindowEx(0, "STATIC",
            "Move your mouse to start...",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 0, 600, 20, hwnd, (HMENU)(INT_PTR)IDC_LC_STATUS, NULL, NULL);
        SendMessage(s_statusLabel, WM_SETFONT, (WPARAM)s_fontSmall, TRUE);

        // Initialise recording state
        s_recording = true;
        s_startTime = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lk(s_mutex);
            s_tracks.clear();
            s_nextDeviceNum = 1;
        }

        SetTimer(hwnd, TIMER_ID, TIMER_MS, NULL);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 640;
        mmi->ptMinTrackSize.y = 480;
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, s_bgBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, COL_TICK);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)s_bgBrush;
    }
    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        int btnY = h - FOOTER_H + 8;
        if (s_btnStart)   MoveWindow(s_btnStart,   10,  btnY, 90,    28, TRUE);
        if (s_btnStop)    MoveWindow(s_btnStop,    110,  btnY, 90,    28, TRUE);
        if (s_statusLabel) MoveWindow(s_statusLabel, 10, btnY + 34, w - 20, 20, TRUE);
        return 0;
    }
    case WM_TIMER: {
        if (wParam == TIMER_ID) {
            // Update the status bar text
            size_t totalPts   = 0;
            size_t devCount   = 0;
            double maxElapsed = 0;
            LONG   lastX = 0, lastY = 0;
            {
                std::lock_guard<std::mutex> lk(s_mutex);
                devCount = s_tracks.size();
                for (auto& [h, t] : s_tracks) {
                    totalPts += t.points.size();
                    if (!t.points.empty()) {
                        if (t.points.back().elapsed > maxElapsed) {
                            maxElapsed = t.points.back().elapsed;
                            lastX = t.points.back().x;
                            lastY = t.points.back().y;
                        }
                    }
                }
            }
            std::ostringstream oss;
            oss << (s_recording ? "REC" : "PAUSED");
            oss << "   |   Devices: " << devCount;
            oss << "   |   Points: " << totalPts;
            oss << "   |   Duration: " << std::fixed << std::setprecision(1)
                << maxElapsed << "s";
            if (totalPts > 0) {
                oss << "   |   Last: (" << lastX << ", " << lastY << ")";
            }
            SetWindowTextA(s_statusLabel, oss.str().c_str());
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT cli;
        GetClientRect(hwnd, &cli);
        int w = cli.right;
        int h = cli.bottom;

        // Double-buffer
        HDC memDC  = CreateCompatibleDC(hdc);
        HBITMAP bm = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP ob = (HBITMAP)SelectObject(memDC, bm);

        // Background
        RECT bgRc = { 0, 0, w, h };
        FillRectSolid(memDC, bgRc, BG_DARK);

        // Take a snapshot of the data
        Snapshot snap;
        {
            std::lock_guard<std::mutex> lk(s_mutex);
            for (auto& [handle, track] : s_tracks)
                snap.push_back({ handle, track });
        }

        // Compute chart regions
        int chartH  = h - FOOTER_H;
        int pathH   = (int)(chartH * 0.55);
        int seriesH = chartH - pathH - GAP;
        int halfW   = (w - 3 * GAP) / 2;

        RECT pathArea = { GAP, GAP, w - GAP, pathH };
        RECT xArea    = { GAP, pathH + GAP,
                          GAP + halfW, pathH + GAP + seriesH };
        RECT yArea    = { GAP + halfW + GAP, pathH + GAP,
                          w - GAP, pathH + GAP + seriesH };

        DrawPathChart(memDC, pathArea, snap);
        DrawTimeSeries(memDC, xArea, snap, true,
                       "X Position Over Time", RGB(79, 195, 247));
        DrawTimeSeries(memDC, yArea, snap, false,
                       "Y Position Over Time", RGB(244, 143, 177));

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, ob);
        DeleteObject(bm);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDC_LC_START) {
            s_recording = true;
            SetTimer(hwnd, TIMER_ID, TIMER_MS, NULL);
        } else if (LOWORD(wParam) == IDC_LC_STOP) {
            s_recording = false;
            KillTimer(hwnd, TIMER_ID);
            // One final status update
            SetWindowTextA(s_statusLabel, "PAUSED");
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN: {
        // Ctrl+R starts recording, Ctrl+P pauses/stops recording
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            if (wParam == 'R') {
                SendMessage(hwnd, WM_COMMAND, IDC_LC_START, 0);
                return 0;
            } else if (wParam == 'P') {
                SendMessage(hwnd, WM_COMMAND, IDC_LC_STOP, 0);
                return 0;
            }
        }
        break;
    }
    case WM_DESTROY: {
        KillTimer(hwnd, TIMER_ID);
        s_chartWnd  = nullptr;
        s_recording = false;

        if (s_fontTitle) { DeleteObject(s_fontTitle); s_fontTitle = nullptr; }
        if (s_fontLabel) { DeleteObject(s_fontLabel); s_fontLabel = nullptr; }
        if (s_fontSmall) { DeleteObject(s_fontSmall); s_fontSmall = nullptr; }
        if (s_bgBrush)  { DeleteObject(s_bgBrush);  s_bgBrush  = nullptr; }

        {
            std::lock_guard<std::mutex> lk(s_mutex);
            s_tracks.clear();
        }
        return 0;
    }
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// ── Public API ──────────────────────────────────────────────────────────────

void ShowLiveChartWindow(HWND parent) {
    // If the window is already open, just bring it to the foreground.
    if (s_chartWnd && IsWindow(s_chartWnd)) {
        SetForegroundWindow(s_chartWnd);
        return;
    }

    static bool registered = false;
    if (!registered) {
        WNDCLASS wc     = {0};
        wc.lpfnWndProc  = LiveChartProc;
        wc.hInstance     = GetModuleHandle(NULL);
        wc.lpszClassName = "LiveMouseChartWindow";
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        RegisterClass(&wc);
        registered = true;
    }

    s_chartWnd = CreateWindowEx(
        0,
        "LiveMouseChartWindow",
        "Live Mouse Movement - All Devices",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
        parent, NULL, GetModuleHandle(NULL), NULL);

    if (!s_chartWnd) return;

    ShowWindow(s_chartWnd, SW_SHOW);
    UpdateWindow(s_chartWnd);
}

void LiveChartRecordMovement(HANDLE device, LONG dx, LONG dy) {
    if (!s_recording || !s_chartWnd) return;

    std::lock_guard<std::mutex> lk(s_mutex);

    auto it = s_tracks.find(device);
    if (it == s_tracks.end()) {
        DeviceTrack t;
        t.color = DEVICE_PALETTE[(s_nextDeviceNum - 1) % NUM_PALETTE];
        t.label = "Mouse " + std::to_string(s_nextDeviceNum);
        ++s_nextDeviceNum;
        s_tracks[device] = std::move(t);
        it = s_tracks.find(device);
    }

    auto& track = it->second;
    track.cumX += dx;
    track.cumY += dy;

    // Remember the very first position so the start marker stays fixed.
    if (!track.hasStart) {
        track.startX   = track.cumX;
        track.startY   = track.cumY;
        track.hasStart = true;
    }

    double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - s_startTime).count();
    track.points.push_back({ elapsed, track.cumX, track.cumY });

    while (track.points.size() > MAX_PTS_PER_DEVICE)
        track.points.pop_front();
}

bool IsLiveChartActive() {
    return s_chartWnd != nullptr && IsWindow(s_chartWnd) && s_recording;
}
