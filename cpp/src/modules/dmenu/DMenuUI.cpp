/*****************************************************************************
 * DMenuUI.cpp
 *
 * D-key quick menu implementation
 * Win32 + GDI+ based popup with focus for key interception
 *****************************************************************************/

#include "DMenuUI.h"
#include "GdiPlusIncludes.h"

#ifdef MSWindows
#include <windowsx.h>
#include <cwchar>
#include <chrono>

using namespace Gdiplus;

// External function from SnapPlugin.cpp to get module scale factor
extern float GetModuleScaleFactor(const char* moduleName);

namespace DMenuUI {

// Window dimensions
static const int WINDOW_WIDTH = 140;
static const int WINDOW_HEIGHT = 154;  // 5 items now

// Scale factor for this module (set in ShowMenu)
static float g_scaleFactor = 1.0f;

// Helper functions for scaling
inline int Scaled(int baseValue) {
    return (int)(baseValue * g_scaleFactor);
}

inline int InverseScaled(int screenValue) {
    return (int)(screenValue / g_scaleFactor);
}

static const int ITEM_HEIGHT = 28;
static const int PADDING = 8;

// Grace period to ignore focus loss after showing menu (ms)
// This prevents the menu from closing when D key is released
static const int FOCUS_GRACE_PERIOD_MS = 300;

// Colors
static const Color COLOR_BG(250, 30, 30, 35);
static const Color COLOR_BORDER(255, 60, 60, 70);
static const Color COLOR_ITEM_HOVER(255, 50, 80, 120);
static const Color COLOR_TEXT(255, 220, 220, 220);
static const Color COLOR_KEY(255, 100, 180, 255);
static const Color COLOR_TEXT_DIM(255, 140, 140, 140);

// Menu items
struct MenuItem {
    wchar_t key;
    const wchar_t* label;
    MenuAction action;
};

static const MenuItem MENU_ITEMS[] = {
    { L'A', L"Align", ACTION_ALIGN },
    { L'T', L"Text", ACTION_TEXT },
    { L'K', L"Keyframe", ACTION_KEYFRAME },
    { L'C', L"Layer", ACTION_COMP },  // ACTION_COMP triggers Layer module
    { L'G', L"Settings", ACTION_SETTINGS }
};
static const int MENU_ITEM_COUNT = 5;

// Global state
static HWND g_hwnd = NULL;
static bool g_visible = false;
static ULONG_PTR g_gdiplusToken = 0;
static MenuAction g_action = ACTION_NONE;
static int g_hoverIndex = -1;
static std::chrono::steady_clock::time_point g_showTime;  // When menu was shown

// Forward declarations
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void Draw(HDC hdc);
static int HitTest(int x, int y);

/*****************************************************************************
 * Initialize
 *****************************************************************************/
void Initialize() {
    if (g_hwnd) return;

    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    Status gdipStatus = GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    if (gdipStatus != Ok) {
        return;
    }

    // Register window class
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"DMenuUIWindow";
    ATOM classAtom = RegisterClassExW(&wc);
    if (classAtom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
        return;
    }

    // Create window (initially hidden) - NOTE: NO WS_EX_NOACTIVATE, we WANT focus
    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"DMenuUIWindow",
        L"",
        WS_POPUP,
        0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    if (!g_hwnd) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
        return;
    }
}

/*****************************************************************************
 * Shutdown
 *****************************************************************************/
void Shutdown() {
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }
    if (g_gdiplusToken) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

/*****************************************************************************
 * ShowMenu
 *****************************************************************************/
void ShowMenu(int x, int y) {
    if (!g_hwnd) Initialize();
    if (!g_hwnd) return;

    g_action = ACTION_NONE;
    g_hoverIndex = -1;

    // Get scale factor from settings
    g_scaleFactor = GetModuleScaleFactor("dmenu");
    int scaledWidth = Scaled(WINDOW_WIDTH);
    int scaledHeight = Scaled(WINDOW_HEIGHT);

    // Position window - use monitor where mouse is located
    int posX = x - scaledWidth / 2;
    int posY = y - 10; // Slightly above cursor

    POINT mousePoint = {x, y};
    HMONITOR hMonitor = MonitorFromPoint(mousePoint, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    if (GetMonitorInfo(hMonitor, &mi)) {
        RECT workArea = mi.rcWork;
        if (posX < workArea.left) posX = workArea.left;
        if (posY < workArea.top) posY = workArea.top;
        if (posX + scaledWidth > workArea.right) posX = workArea.right - scaledWidth;
        if (posY + scaledHeight > workArea.bottom) posY = workArea.bottom - scaledHeight;
    }

    SetWindowPos(g_hwnd, HWND_TOPMOST, posX, posY, scaledWidth, scaledHeight, SWP_SHOWWINDOW);
    ShowWindow(g_hwnd, SW_SHOW);

    // Take focus - this is key to intercepting keyboard input
    SetForegroundWindow(g_hwnd);
    SetFocus(g_hwnd);

    g_visible = true;
    g_showTime = std::chrono::steady_clock::now();  // Record show time for grace period
}

/*****************************************************************************
 * HideMenu
 *****************************************************************************/
void HideMenu() {
    if (g_hwnd) {
        ShowWindow(g_hwnd, SW_HIDE);
    }
    g_visible = false;
}

/*****************************************************************************
 * IsVisible
 *****************************************************************************/
bool IsVisible() {
    return g_visible && g_hwnd && IsWindowVisible(g_hwnd);
}

/*****************************************************************************
 * HasFocus
 * Check if DMenu window has keyboard focus
 *****************************************************************************/
bool HasFocus() {
    if (!g_hwnd || !g_visible) return false;
    HWND fg = GetForegroundWindow();
    return fg == g_hwnd;
}

/*****************************************************************************
 * CheckAndCloseLostFocus
 * Fallback: Close menu if it lost focus after grace period
 * Called from IdleHook to catch cases where WM_ACTIVATE didn't fire
 *****************************************************************************/
void CheckAndCloseLostFocus() {
    if (!g_visible || !g_hwnd) return;

    // Check if grace period has passed
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - g_showTime).count();
    if (elapsed <= FOCUS_GRACE_PERIOD_MS) return;

    // Check if we lost focus
    HWND fg = GetForegroundWindow();
    if (fg != g_hwnd && g_action == ACTION_NONE) {
        // Lost focus - close the menu
        g_action = ACTION_CANCELLED;
        HideMenu();
    }
}

/*****************************************************************************
 * GetAction
 *****************************************************************************/
MenuAction GetAction() {
    return g_action;
}

/*****************************************************************************
 * Update
 *****************************************************************************/
void Update() {
    // Called from idle hook - can be used for animations or timeout
}

/*****************************************************************************
 * HitTest
 *****************************************************************************/
static int HitTest(int x, int y) {
    if (x < PADDING || x > WINDOW_WIDTH - PADDING) return -1;

    int itemY = PADDING;
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        if (y >= itemY && y < itemY + ITEM_HEIGHT) {
            return i;
        }
        itemY += ITEM_HEIGHT;
    }
    return -1;
}

/*****************************************************************************
 * Draw
 *****************************************************************************/
static void Draw(HDC hdc) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Apply scale transform - all drawing uses base dimensions
    g.ScaleTransform(g_scaleFactor, g_scaleFactor);

    // Background with rounded corners
    SolidBrush bgBrush(COLOR_BG);
    Pen borderPen(COLOR_BORDER, 1);

    // Simple rectangle for now
    g.FillRectangle(&bgBrush, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    g.DrawRectangle(&borderPen, 0, 0, WINDOW_WIDTH - 1, WINDOW_HEIGHT - 1);

    // Menu items
    FontFamily fontFamily(L"Segoe UI");
    Font keyFont(&fontFamily, 11, FontStyleBold, UnitPixel);
    Font labelFont(&fontFamily, 11, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush keyBrush(COLOR_KEY);
    SolidBrush hoverBrush(COLOR_ITEM_HOVER);

    int itemY = PADDING;
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        RECT itemRect = { PADDING, itemY, WINDOW_WIDTH - PADDING, itemY + ITEM_HEIGHT };

        // Hover background
        if (i == g_hoverIndex) {
            g.FillRectangle(&hoverBrush, itemRect.left, itemRect.top,
                           itemRect.right - itemRect.left, itemRect.bottom - itemRect.top);
        }

        // Key indicator [A]
        wchar_t keyText[4];
        swprintf(keyText, 4, L"[%c]", MENU_ITEMS[i].key);
        g.DrawString(keyText, -1, &keyFont, PointF((REAL)(PADDING + 4), (REAL)(itemY + 5)), &keyBrush);

        // Label
        g.DrawString(MENU_ITEMS[i].label, -1, &labelFont,
                    PointF((REAL)(PADDING + 40), (REAL)(itemY + 6)), &textBrush);

        itemY += ITEM_HEIGHT;
    }
}

/*****************************************************************************
 * WndProc
 *****************************************************************************/
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Draw(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        // Inverse scale mouse coordinates for hit testing
        int x = InverseScaled(GET_X_LPARAM(lParam));
        int y = InverseScaled(GET_Y_LPARAM(lParam));
        int newHover = HitTest(x, y);
        if (newHover != g_hoverIndex) {
            g_hoverIndex = newHover;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        // Inverse scale mouse coordinates for hit testing
        int x = InverseScaled(GET_X_LPARAM(lParam));
        int y = InverseScaled(GET_Y_LPARAM(lParam));
        int index = HitTest(x, y);
        if (index >= 0 && index < MENU_ITEM_COUNT) {
            g_action = MENU_ITEMS[index].action;
            HideMenu();
        }
        return 0;
    }

    case WM_KEYDOWN: {
        // Handle key shortcuts
        switch (wParam) {
        case 'A':
        case 'a':
            g_action = ACTION_ALIGN;
            HideMenu();
            break;
        case 'T':
        case 't':
            g_action = ACTION_TEXT;
            HideMenu();
            break;
        case 'K':
        case 'k':
            g_action = ACTION_KEYFRAME;
            HideMenu();
            break;
        case 'C':
        case 'c':
            g_action = ACTION_COMP;
            HideMenu();
            break;
        case 'G':
        case 'g':
            g_action = ACTION_SETTINGS;
            HideMenu();
            break;
        case VK_ESCAPE:
            g_action = ACTION_CANCELLED;
            HideMenu();
            break;
        }
        return 0;
    }

    case WM_CHAR:
        // Consume char events to prevent beeps
        return 0;

    case WM_ACTIVATE:
        // WM_ACTIVATE is more reliable than WM_KILLFOCUS for detecting outside clicks
        if (LOWORD(wParam) == WA_INACTIVE) {
            // Check if within grace period (ignore focus loss right after showing)
            // Only cancel if no action was already set (prevents overwriting click selection)
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - g_showTime).count();
            if (elapsed > FOCUS_GRACE_PERIOD_MS && g_action == ACTION_NONE) {
                // Window is being deactivated (clicked outside)
                g_action = ACTION_CANCELLED;
                HideMenu();
            }
        }
        return 0;

    case WM_KILLFOCUS:
        // Backup: also handle kill focus (with grace period check)
        // Only cancel if no action was already set (prevents overwriting click selection)
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - g_showTime).count();
            if (elapsed > FOCUS_GRACE_PERIOD_MS && g_action == ACTION_NONE) {
                g_action = ACTION_CANCELLED;
                HideMenu();
            }
        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace DMenuUI

#else // macOS stubs

namespace DMenuUI {

void Initialize() {}
void Shutdown() {}
void ShowMenu(int x, int y) { (void)x; (void)y; }
void HideMenu() {}
bool IsVisible() { return false; }
bool HasFocus() { return false; }
void CheckAndCloseLostFocus() {}
MenuAction GetAction() { return ACTION_NONE; }
void Update() {}

} // namespace DMenuUI

#endif
