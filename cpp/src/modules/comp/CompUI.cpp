/*****************************************************************************
 * CompUI.cpp
 *
 * Native Windows UI implementation for Composition Editor Module
 * Uses Win32 + GDI+ with WS_EX_NOACTIVATE
 *****************************************************************************/

#include "CompUI.h"
#include "GdiPlusIncludes.h"

#ifdef MSWindows
#include <windowsx.h>
#include <string>
#include <cmath>

using namespace Gdiplus;

// External function from SnapPlugin.cpp to get module scale factor
extern float GetModuleScaleFactor(const char* moduleName);

namespace CompUI {

// Window dimensions
static const int WINDOW_WIDTH = 280;
static const int WINDOW_HEIGHT = 160;

// Scale factor for this module (set in ShowPanel)
static float g_scaleFactor = 1.0f;

// Helper functions for scaling
inline int Scaled(int baseValue) {
    return (int)(baseValue * g_scaleFactor);
}

inline int InverseScaled(int screenValue) {
    return (int)(screenValue / g_scaleFactor);
}

static const int HEADER_HEIGHT = 32;
static const int BUTTON_HEIGHT = 36;
static const int BUTTON_MARGIN = 8;
static const int PADDING = 8;

// Colors (following UI guidelines)
static const Color COLOR_BG(240, 28, 28, 32);
static const Color COLOR_HEADER_BG(255, 40, 40, 48);
static const Color COLOR_BUTTON_BG(255, 45, 45, 55);
static const Color COLOR_BUTTON_HOVER(255, 60, 80, 100);
static const Color COLOR_BUTTON_ACTIVE(255, 74, 158, 255);
static const Color COLOR_TEXT(255, 220, 220, 220);
static const Color COLOR_TEXT_DIM(255, 140, 140, 140);
static const Color COLOR_BORDER(255, 60, 60, 70);
static const Color COLOR_PIN_ACTIVE(255, 255, 200, 0);

// Button info
struct ButtonInfo {
    const wchar_t* label;
    const wchar_t* shortcut;
    const wchar_t* description;
    CompAction action;
};

static const ButtonInfo BUTTONS[] = {
    {L"Auto Crop", L"1", L"Fit comp to layer bounds", ACTION_AUTO_CROP},
    {L"Duplicate", L"2", L"Full comp duplication", ACTION_DUPLICATE},
    {L"Fit Duration", L"3", L"Fit to layer duration", ACTION_FIT_DURATION},
};
static const int NUM_BUTTONS = sizeof(BUTTONS) / sizeof(BUTTONS[0]);

// Global state
static HWND g_hwnd = NULL;
static bool g_visible = false;
static ULONG_PTR g_gdiplusToken = 0;
static CompInfo g_compInfo = {};
static CompResult g_result;
static bool g_keepPanelOpen = false;

// Button rectangles
static RECT g_buttonRects[5];
static RECT g_pinRect;

// Hover state
static int g_hoverButton = -1;
static bool g_pinHover = false;

// Window drag
static bool g_windowDragging = false;
static POINT g_windowDragOffset = {0, 0};

// Forward declarations
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void Draw(HDC hdc);
static void DrawHeader(Graphics& g);
static void DrawButtons(Graphics& g);
static void DrawButton(Graphics& g, RECT& rect, int index, bool hover);
static int HitTestButton(int x, int y);
static void HandleMouseDown(int x, int y);
static void HandleKeyDown(WPARAM key);
static void ExecuteAction(CompAction action);

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
    wc.lpszClassName = L"CompUIWindow";
    ATOM classAtom = RegisterClassExW(&wc);
    if (classAtom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
        return;
    }

    // Create window (initially hidden)
    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"CompUIWindow",
        L"Comp Editor",
        WS_POPUP,
        0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    if (!g_hwnd) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
        return;
    }

    SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
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
 * ShowPanel
 *****************************************************************************/
void ShowPanel(int x, int y) {
    if (!g_hwnd) Initialize();

    // Reset state
    g_result = CompResult();
    g_hoverButton = -1;

    // Get scale factor from settings
    g_scaleFactor = GetModuleScaleFactor("comp");
    int scaledWidth = Scaled(WINDOW_WIDTH);
    int scaledHeight = Scaled(WINDOW_HEIGHT);

    // Position window
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

    int posX = x - scaledWidth / 2;
    int posY = y - scaledHeight / 2;

    if (posX < workArea.left) posX = workArea.left;
    if (posY < workArea.top) posY = workArea.top;
    if (posX + scaledWidth > workArea.right) posX = workArea.right - scaledWidth;
    if (posY + scaledHeight > workArea.bottom) posY = workArea.bottom - scaledHeight;

    SetWindowPos(g_hwnd, HWND_TOPMOST, posX, posY, scaledWidth, scaledHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(g_hwnd, SW_SHOWNA);
    InvalidateRect(g_hwnd, NULL, TRUE);
    g_visible = true;
}

/*****************************************************************************
 * HidePanel
 *****************************************************************************/
CompResult HidePanel() {
    if (g_hwnd && g_visible) {
        ShowWindow(g_hwnd, SW_HIDE);
        g_visible = false;
    }
    return g_result;
}

bool IsVisible() {
    return g_visible && g_hwnd && IsWindowVisible(g_hwnd);
}

/*****************************************************************************
 * UpdateHover
 *****************************************************************************/
void UpdateHover(int mouseX, int mouseY) {
    if (!g_hwnd || !g_visible) return;

    RECT wndRect;
    GetWindowRect(g_hwnd, &wndRect);
    // Convert to local coords and inverse scale for hit testing
    int localX = InverseScaled(mouseX - wndRect.left);
    int localY = InverseScaled(mouseY - wndRect.top);

    bool needsRepaint = false;
    POINT pt = {localX, localY};

    // Check buttons
    int newHover = HitTestButton(localX, localY);
    if (newHover != g_hoverButton) {
        g_hoverButton = newHover;
        needsRepaint = true;
    }

    // Check pin
    bool newPinHover = PtInRect(&g_pinRect, pt);
    if (newPinHover != g_pinHover) {
        g_pinHover = newPinHover;
        needsRepaint = true;
    }

    if (needsRepaint) {
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

CompResult GetResult() { return g_result; }

CompAction GetSelectedAction() { return g_result.action; }

/*****************************************************************************
 * SetCompInfo - Parse JSON from ExtendScript
 *****************************************************************************/
void SetCompInfo(const wchar_t* jsonInfo) {
    if (!jsonInfo || jsonInfo[0] == L'\0') return;

    std::wstring json(jsonInfo);

    auto getFloat = [&json](const wchar_t* key) -> float {
        std::wstring search = std::wstring(L"\"") + key + L"\":";
        size_t pos = json.find(search);
        if (pos == std::wstring::npos) return 0;
        pos += search.length();
        return (float)_wtof(json.c_str() + pos);
    };

    auto getString = [&json](const wchar_t* key, wchar_t* out, int maxLen) {
        std::wstring search = std::wstring(L"\"") + key + L"\":\"";
        size_t pos = json.find(search);
        if (pos == std::wstring::npos) { out[0] = L'\0'; return; }
        pos += search.length();
        size_t end = json.find(L'"', pos);
        if (end == std::wstring::npos) { out[0] = L'\0'; return; }
        int len = (int)(end - pos);
        if (len >= maxLen) len = maxLen - 1;
        wcsncpy(out, json.c_str() + pos, len);
        out[len] = L'\0';
    };

    getString(L"name", g_compInfo.name, 256);
    g_compInfo.width = getFloat(L"width");
    g_compInfo.height = getFloat(L"height");
    g_compInfo.duration = getFloat(L"duration");
    g_compInfo.frameRate = getFloat(L"frameRate");
    g_compInfo.workAreaStart = getFloat(L"workAreaStart");
    g_compInfo.workAreaEnd = getFloat(L"workAreaEnd");

    InvalidateRect(g_hwnd, NULL, FALSE);
}

/*****************************************************************************
 * WndProc
 *****************************************************************************/
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Double buffering - use scaled dimensions
        int scaledWidth = Scaled(WINDOW_WIDTH);
        int scaledHeight = Scaled(WINDOW_HEIGHT);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, scaledWidth, scaledHeight);
        HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

        Draw(memDC);

        BitBlt(hdc, 0, 0, scaledWidth, scaledHeight, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_LBUTTONDOWN: {
        // Inverse scale mouse coordinates for hit testing
        int x = InverseScaled(GET_X_LPARAM(lParam));
        int y = InverseScaled(GET_Y_LPARAM(lParam));

        // Check header drag
        if (y >= PADDING && y < PADDING + HEADER_HEIGHT && x < g_pinRect.left) {
            g_windowDragging = true;
            RECT wndRect;
            GetWindowRect(hwnd, &wndRect);
            POINT cursorPos;
            GetCursorPos(&cursorPos);
            g_windowDragOffset.x = cursorPos.x - wndRect.left;
            g_windowDragOffset.y = cursorPos.y - wndRect.top;
            SetCapture(hwnd);
            return 0;
        }

        HandleMouseDown(x, y);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (g_windowDragging) {
            POINT pt;
            GetCursorPos(&pt);
            SetWindowPos(hwnd, NULL,
                         pt.x - g_windowDragOffset.x,
                         pt.y - g_windowDragOffset.y,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_windowDragging) {
            g_windowDragging = false;
            ReleaseCapture();
        }
        return 0;

    case WM_KEYDOWN:
        HandleKeyDown(wParam);
        return 0;

    case WM_CHAR: {
        wchar_t ch = (wchar_t)wParam;
        if (ch == VK_ESCAPE) {
            g_result.cancelled = true;
            ShowWindow(hwnd, SW_HIDE);
            g_visible = false;
        } else if (ch >= L'1' && ch <= L'0' + NUM_BUTTONS) {
            int index = ch - L'1';
            if (index >= 0 && index < NUM_BUTTONS) {
                ExecuteAction(BUTTONS[index].action);
            }
        }
        return 0;
    }

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && !g_keepPanelOpen && !g_windowDragging) {
            g_result.cancelled = true;
            ShowWindow(hwnd, SW_HIDE);
            g_visible = false;
        }
        return 0;

    case WM_DESTROY:
        g_visible = false;
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/*****************************************************************************
 * Draw - Main rendering
 *****************************************************************************/
static void Draw(HDC hdc) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Apply scale transform - all drawing uses base dimensions
    g.ScaleTransform(g_scaleFactor, g_scaleFactor);

    // Background
    SolidBrush bgBrush(COLOR_BG);
    g.FillRectangle(&bgBrush, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    // Border
    Pen borderPen(COLOR_BORDER, 1);
    g.DrawRectangle(&borderPen, 0, 0, WINDOW_WIDTH - 1, WINDOW_HEIGHT - 1);

    DrawHeader(g);
    DrawButtons(g);
}

/*****************************************************************************
 * DrawHeader
 *****************************************************************************/
static void DrawHeader(Graphics& g) {
    SolidBrush headerBrush(COLOR_HEADER_BG);
    g.FillRectangle(&headerBrush, 0, 0, WINDOW_WIDTH, HEADER_HEIGHT);

    FontFamily fontFamily(L"Segoe UI");
    Font titleFont(&fontFamily, 11, FontStyleBold, UnitPixel);
    Font compFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);

    // Title
    g.DrawString(L"Comp Editor", -1, &titleFont, PointF(10, 8), &textBrush);

    // Comp name (if available)
    if (g_compInfo.name[0] != L'\0') {
        RectF compRect(100, 8, 130, 20);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentFar);
        sf.SetTrimming(StringTrimmingEllipsisCharacter);
        g.DrawString(g_compInfo.name, -1, &compFont, compRect, &sf, &dimBrush);
    }

    // Pin button
    int btnY = (HEADER_HEIGHT - 20) / 2;
    g_pinRect = {WINDOW_WIDTH - 28, btnY, WINDOW_WIDTH - 8, btnY + 20};
    Color pinColor = g_keepPanelOpen ? COLOR_PIN_ACTIVE :
                     (g_pinHover ? COLOR_BUTTON_HOVER : COLOR_BUTTON_BG);
    SolidBrush pinBrush(pinColor);
    g.FillRectangle(&pinBrush, g_pinRect.left, g_pinRect.top, 20, 20);

    // Pin icon
    Pen pinPen(g_keepPanelOpen ? Color(255, 40, 40, 40) : COLOR_TEXT, 1.5f);
    int px = g_pinRect.left + 10, py = g_pinRect.top + 10;
    g.DrawLine(&pinPen, px - 4, py - 4, px + 4, py + 4);
    g.DrawEllipse(&pinPen, (REAL)(px - 3), (REAL)(py - 6), 6.0f, 6.0f);
}

/*****************************************************************************
 * DrawButtons
 *****************************************************************************/
static void DrawButtons(Graphics& g) {
    int y = HEADER_HEIGHT + BUTTON_MARGIN;

    for (int i = 0; i < NUM_BUTTONS; i++) {
        g_buttonRects[i] = {
            BUTTON_MARGIN,
            y,
            WINDOW_WIDTH - BUTTON_MARGIN,
            y + BUTTON_HEIGHT
        };
        DrawButton(g, g_buttonRects[i], i, g_hoverButton == i);
        y += BUTTON_HEIGHT + 4;
    }
}

/*****************************************************************************
 * DrawButton
 *****************************************************************************/
static void DrawButton(Graphics& g, RECT& rect, int index, bool hover) {
    const ButtonInfo& btn = BUTTONS[index];

    // Background
    Color bgColor = hover ? COLOR_BUTTON_HOVER : COLOR_BUTTON_BG;
    SolidBrush bgBrush(bgColor);
    g.FillRectangle(&bgBrush, rect.left, rect.top,
                    rect.right - rect.left, rect.bottom - rect.top);

    // Border on hover
    if (hover) {
        Pen borderPen(COLOR_BUTTON_ACTIVE, 1);
        g.DrawRectangle(&borderPen, rect.left, rect.top,
                        rect.right - rect.left - 1, rect.bottom - rect.top - 1);
    }

    FontFamily fontFamily(L"Segoe UI");
    Font labelFont(&fontFamily, 11, FontStyleBold, UnitPixel);
    Font shortcutFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    Font descFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);
    SolidBrush accentBrush(COLOR_BUTTON_ACTIVE);

    // Shortcut number
    g.DrawString(btn.shortcut, -1, &shortcutFont,
                 PointF((REAL)(rect.left + 10), (REAL)(rect.top + 10)), &accentBrush);

    // Label
    g.DrawString(btn.label, -1, &labelFont,
                 PointF((REAL)(rect.left + 30), (REAL)(rect.top + 6)), &textBrush);

    // Description
    g.DrawString(btn.description, -1, &descFont,
                 PointF((REAL)(rect.left + 30), (REAL)(rect.top + 20)), &dimBrush);
}

/*****************************************************************************
 * HitTestButton
 *****************************************************************************/
static int HitTestButton(int x, int y) {
    POINT pt = {x, y};
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (PtInRect(&g_buttonRects[i], pt)) return i;
    }
    return -1;
}

/*****************************************************************************
 * HandleMouseDown
 *****************************************************************************/
static void HandleMouseDown(int x, int y) {
    POINT pt = {x, y};

    // Pin button
    if (PtInRect(&g_pinRect, pt)) {
        g_keepPanelOpen = !g_keepPanelOpen;
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }

    // Buttons
    int hit = HitTestButton(x, y);
    if (hit >= 0 && hit < NUM_BUTTONS) {
        ExecuteAction(BUTTONS[hit].action);
    }
}

/*****************************************************************************
 * HandleKeyDown
 *****************************************************************************/
static void HandleKeyDown(WPARAM key) {
    if (key == VK_ESCAPE) {
        g_result.cancelled = true;
        ShowWindow(g_hwnd, SW_HIDE);
        g_visible = false;
    }
}

/*****************************************************************************
 * ExecuteAction
 *****************************************************************************/
static void ExecuteAction(CompAction action) {
    g_result.action = action;
    g_result.applied = true;

    if (!g_keepPanelOpen) {
        ShowWindow(g_hwnd, SW_HIDE);
        g_visible = false;
    }
}

} // namespace CompUI

#else // macOS stubs

namespace CompUI {

void Initialize() {}
void Shutdown() {}
void ShowPanel(int x, int y) { (void)x; (void)y; }
CompResult HidePanel() { return CompResult(); }
bool IsVisible() { return false; }
void UpdateHover(int mouseX, int mouseY) { (void)mouseX; (void)mouseY; }
CompResult GetResult() { return CompResult(); }
void SetCompInfo(const wchar_t* jsonInfo) { (void)jsonInfo; }
CompAction GetSelectedAction() { return ACTION_NONE; }

} // namespace CompUI

#endif // MSWindows
