/*****************************************************************************
 * AlignUI.cpp
 *
 * Native Windows UI implementation for Align/Distribute Module
 * Uses Win32 + GDI+ for custom rendered UI
 *****************************************************************************/

#include "AlignUI.h"

#ifdef MSWindows
#include <windows.h>
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// External function from SnapPlugin.cpp to get module scale factor
extern float GetModuleScaleFactor(const char* moduleName);

namespace AlignUI {

// Window dimensions
static const int WINDOW_WIDTH = 280;
static const int WINDOW_HEIGHT = 180;

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
static const int BUTTON_SIZE = 36;
static const int BUTTON_SPACING = 8;
static const int DIST_BUTTON_WIDTH = 120;
static const int DIST_BUTTON_HEIGHT = 40;

// Colors (ARGB)
static const Color COLOR_BG(240, 28, 28, 32);
static const Color COLOR_HEADER_BG(255, 40, 40, 48);
static const Color COLOR_BUTTON_BG(255, 50, 50, 60);
static const Color COLOR_BUTTON_HOVER(255, 70, 100, 130);
static const Color COLOR_BUTTON_ACTIVE(255, 74, 158, 255);
static const Color COLOR_MODE_ACTIVE_BLUE(255, 74, 158, 255);
static const Color COLOR_MODE_ACTIVE_ORANGE(255, 255, 140, 0);
static const Color COLOR_MODE_INACTIVE(255, 60, 60, 70);
static const Color COLOR_TEXT(255, 220, 220, 220);
static const Color COLOR_TEXT_DIM(255, 140, 140, 140);
static const Color COLOR_ICON(255, 200, 200, 200);
static const Color COLOR_ICON_HOVER(255, 255, 255, 255);
static const Color COLOR_PIN_ACTIVE(255, 255, 200, 0);
static const Color COLOR_CLOSE_HOVER(255, 200, 60, 60);

// Global state
static HWND g_hwnd = NULL;
static bool g_visible = false;
static ULONG_PTR g_gdiplusToken = 0;
static FunctionMode g_funcMode = FUNC_ALIGN;
static ReferenceMode g_refMode = REF_SELECTION;
static AlignResult g_result;
static bool g_keepPanelOpen = false; // Pin state

// Hover state
static bool g_alignModeHover = false;
static bool g_distModeHover = false;
static bool g_selModeHover = false;
static bool g_compModeHover = false;
static int g_hoveredButton = -1; // 0-5 for align, 0-1 for dist
static bool g_pinHover = false;
static bool g_closeHover = false;

// Button rects (calculated in Draw)
static RECT g_alignModeRect;
static RECT g_distModeRect;
static RECT g_selModeRect;
static RECT g_compModeRect;
static RECT g_buttonRects[6]; // Max 6 buttons
static RECT g_pinRect;
static RECT g_closeRect;

// Forward declarations
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void Draw(HDC hdc);
static void DrawHeader(Graphics& g);
static void DrawAlignButtons(Graphics& g);
static void DrawDistributeButtons(Graphics& g);
static void DrawAlignIcon(Graphics& g, int index, RECT& rect, bool hover);
static void DrawDistIcon(Graphics& g, int index, RECT& rect, bool hover);
static void HandleClick(int x, int y);
static void HandleKeyboard(WPARAM key);

/*****************************************************************************
 * Initialize
 *****************************************************************************/
void Initialize() {
    if (g_hwnd) return;

    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    Status gdipStatus = GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    if (gdipStatus != Ok) {
        // GDI+ initialization failed - cannot proceed
        return;
    }

    // Register window class (explicitly use Wide version for Unicode strings)
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"AlignUIWindow";
    ATOM classAtom = RegisterClassExW(&wc);
    if (classAtom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        // Registration failed and class doesn't exist
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
        return;
    }

    // Create window (initially hidden)
    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"AlignUIWindow",
        L"Align",
        WS_POPUP,
        0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    if (!g_hwnd) {
        // Window creation failed
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
        return;
    }

    // Set layered window for transparency
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
    g_result = AlignResult();
    g_hoveredButton = -1;

    // Get scale factor from settings
    g_scaleFactor = GetModuleScaleFactor("align");
    int scaledWidth = Scaled(WINDOW_WIDTH);
    int scaledHeight = Scaled(WINDOW_HEIGHT);

    // Position window - use monitor where mouse is located
    int posX = x - scaledWidth / 2;
    int posY = y - scaledHeight / 2;

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

    SetWindowPos(g_hwnd, HWND_TOPMOST, posX, posY, scaledWidth, scaledHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(g_hwnd, SW_SHOWNA);
    InvalidateRect(g_hwnd, NULL, TRUE);
    g_visible = true;
}

/*****************************************************************************
 * HidePanel
 *****************************************************************************/
AlignResult HidePanel() {
    if (g_hwnd && g_visible) {
        ShowWindow(g_hwnd, SW_HIDE);
        g_visible = false;
    }
    return g_result;
}

/*****************************************************************************
 * IsVisible
 *****************************************************************************/
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

    // Check mode buttons hover
    bool newAlignHover = PtInRect(&g_alignModeRect, {localX, localY});
    bool newDistHover = PtInRect(&g_distModeRect, {localX, localY});
    bool newSelHover = PtInRect(&g_selModeRect, {localX, localY});
    bool newCompHover = PtInRect(&g_compModeRect, {localX, localY});
    bool newPinHover = PtInRect(&g_pinRect, {localX, localY});
    bool newCloseHover = PtInRect(&g_closeRect, {localX, localY});

    if (newAlignHover != g_alignModeHover || newDistHover != g_distModeHover ||
        newSelHover != g_selModeHover || newCompHover != g_compModeHover ||
        newPinHover != g_pinHover || newCloseHover != g_closeHover) {
        g_alignModeHover = newAlignHover;
        g_distModeHover = newDistHover;
        g_selModeHover = newSelHover;
        g_compModeHover = newCompHover;
        g_pinHover = newPinHover;
        g_closeHover = newCloseHover;
        needsRepaint = true;
    }

    // Check function button hover
    int newHovered = -1;
    int buttonCount = (g_funcMode == FUNC_ALIGN) ? 6 : 2;
    for (int i = 0; i < buttonCount; i++) {
        if (PtInRect(&g_buttonRects[i], {localX, localY})) {
            newHovered = i;
            break;
        }
    }

    if (newHovered != g_hoveredButton) {
        g_hoveredButton = newHovered;
        needsRepaint = true;
    }

    if (needsRepaint) {
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

/*****************************************************************************
 * GetResult
 *****************************************************************************/
AlignResult GetResult() {
    return g_result;
}

FunctionMode GetFunctionMode() { return g_funcMode; }
ReferenceMode GetReferenceMode() { return g_refMode; }
void SetFunctionMode(FunctionMode mode) { g_funcMode = mode; }
void SetReferenceMode(ReferenceMode mode) { g_refMode = mode; }

/*****************************************************************************
 * WndProc
 *****************************************************************************/
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Double buffering
        RECT rect;
        GetClientRect(hwnd, &rect);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

        Draw(memDC);

        // Copy to screen
        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

        // Cleanup
        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        // Inverse scale mouse coordinates for hit testing
        int x = InverseScaled(GET_X_LPARAM(lParam));
        int y = InverseScaled(GET_Y_LPARAM(lParam));
        HandleClick(x, y);
        return 0;
    }

    case WM_KEYDOWN:
        HandleKeyboard(wParam);
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && !g_keepPanelOpen) {
            g_result.cancelled = true;
            ShowWindow(hwnd, SW_HIDE);
            g_visible = false;
        }
        return 0;

    case WM_KILLFOCUS:
        if (!g_keepPanelOpen) {
            g_result.cancelled = true;
            ShowWindow(hwnd, SW_HIDE);
            g_visible = false;
        }
        return 0;

    case WM_DESTROY:
        g_visible = false;
        return 0;

    case WM_NCHITTEST: {
        // Allow dragging from header area (excluding buttons)
        POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
        ScreenToClient(hwnd, &pt);
        pt.x = InverseScaled(pt.x);
        pt.y = InverseScaled(pt.y);

        // Header area (excluding buttons)
        if (pt.y >= 0 && pt.y < HEADER_HEIGHT) {
            // Pin button (right side)
            int pinBtnX = WINDOW_WIDTH - 8 - 20;
            if (pt.x >= pinBtnX && pt.x <= WINDOW_WIDTH - 8) {
                return HTCLIENT;  // Pin button - clickable
            }
            // Mode buttons (left side)
            if (pt.x <= 120) {
                return HTCLIENT;  // Mode buttons - clickable
            }
            // Rest of header - draggable
            return HTCAPTION;
        }
        return HTCLIENT;
    }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/*****************************************************************************
 * Draw - Main rendering function
 *****************************************************************************/
static void Draw(HDC hdc) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    // Apply scale transform - all drawing uses base dimensions
    g.ScaleTransform(g_scaleFactor, g_scaleFactor);

    // Background
    SolidBrush bgBrush(COLOR_BG);
    g.FillRectangle(&bgBrush, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    // Border
    Pen borderPen(Color(255, 60, 60, 70), 1);
    g.DrawRectangle(&borderPen, 0, 0, WINDOW_WIDTH - 1, WINDOW_HEIGHT - 1);

    // Header
    DrawHeader(g);

    // Content area
    if (g_funcMode == FUNC_ALIGN) {
        DrawAlignButtons(g);
    } else {
        DrawDistributeButtons(g);
    }
}

/*****************************************************************************
 * DrawHeader - Mode buttons and controls
 *****************************************************************************/
static void DrawHeader(Graphics& g) {
    // Header background
    SolidBrush headerBrush(COLOR_HEADER_BG);
    g.FillRectangle(&headerBrush, 0, 0, WINDOW_WIDTH, HEADER_HEIGHT);

    FontFamily fontFamily(L"Segoe UI");
    Font font(&fontFamily, 10, FontStyleRegular, UnitPixel);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);

    // Left side: Align / Dist mode buttons
    int leftX = 8;
    int btnW = 50;
    int btnH = 22;
    int btnY = (HEADER_HEIGHT - btnH) / 2;

    // Align button
    g_alignModeRect = {leftX, btnY, leftX + btnW, btnY + btnH};
    Color alignColor = (g_funcMode == FUNC_ALIGN) ? COLOR_MODE_ACTIVE_BLUE :
                       (g_alignModeHover ? COLOR_BUTTON_HOVER : COLOR_MODE_INACTIVE);
    SolidBrush alignBrush(alignColor);
    g.FillRectangle(&alignBrush, leftX, btnY, btnW, btnH);
    SolidBrush textBrush(COLOR_TEXT);
    RectF alignRect((REAL)leftX, (REAL)btnY, (REAL)btnW, (REAL)btnH);
    g.DrawString(L"Align", -1, &font, alignRect, &sf, &textBrush);

    // Dist button
    leftX += btnW + 4;
    g_distModeRect = {leftX, btnY, leftX + btnW, btnY + btnH};
    Color distColor = (g_funcMode == FUNC_DISTRIBUTE) ? COLOR_MODE_ACTIVE_BLUE :
                      (g_distModeHover ? COLOR_BUTTON_HOVER : COLOR_MODE_INACTIVE);
    SolidBrush distBrush(distColor);
    g.FillRectangle(&distBrush, leftX, btnY, btnW, btnH);
    RectF distRect((REAL)leftX, (REAL)btnY, (REAL)btnW, (REAL)btnH);
    g.DrawString(L"Dist", -1, &font, distRect, &sf, &textBrush);

    // Right side: Sel / Comp mode buttons
    int rightX = WINDOW_WIDTH - 8 - 24 - 4 - 24 - 8 - btnW;

    // Sel button
    g_selModeRect = {rightX, btnY, rightX + btnW/2 + 4, btnY + btnH};
    Color selColor = (g_refMode == REF_SELECTION) ? COLOR_MODE_ACTIVE_BLUE :
                     (g_selModeHover ? COLOR_BUTTON_HOVER : COLOR_MODE_INACTIVE);
    SolidBrush selBrush(selColor);
    g.FillRectangle(&selBrush, rightX, btnY, btnW/2 + 4, btnH);
    RectF selRect((REAL)rightX, (REAL)btnY, (REAL)(btnW/2 + 4), (REAL)btnH);
    g.DrawString(L"Sel", -1, &font, selRect, &sf, &textBrush);

    // Comp button
    rightX += btnW/2 + 4 + 4;
    g_compModeRect = {rightX, btnY, rightX + btnW/2 + 8, btnY + btnH};
    Color compColor = (g_refMode == REF_COMPOSITION) ? COLOR_MODE_ACTIVE_ORANGE :
                      (g_compModeHover ? COLOR_BUTTON_HOVER : COLOR_MODE_INACTIVE);
    SolidBrush compBrush(compColor);
    g.FillRectangle(&compBrush, rightX, btnY, btnW/2 + 8, btnH);
    RectF compRect((REAL)rightX, (REAL)btnY, (REAL)(btnW/2 + 8), (REAL)btnH);
    g.DrawString(L"Comp", -1, &font, compRect, &sf, &textBrush);

    // Pin button
    rightX = WINDOW_WIDTH - 8 - 24 - 4 - 24;
    g_pinRect = {rightX, btnY, rightX + 20, btnY + btnH};
    Color pinColor = g_keepPanelOpen ? COLOR_PIN_ACTIVE :
                     (g_pinHover ? COLOR_BUTTON_HOVER : COLOR_MODE_INACTIVE);
    SolidBrush pinBrush(pinColor);
    g.FillRectangle(&pinBrush, rightX, btnY, 20, btnH);

    // Pin icon (simple pushpin shape)
    Pen pinPen(g_keepPanelOpen ? Color(255, 40, 40, 40) : COLOR_TEXT, 1.5f);
    int px = rightX + 10, py = btnY + 11;
    g.DrawLine(&pinPen, px - 4, py - 4, px + 4, py + 4);
    g.DrawLine(&pinPen, px, py - 6, px, py + 6);
    g.DrawEllipse(&pinPen, px - 3, py - 8, 6, 6);

    // Close button
    rightX = WINDOW_WIDTH - 8 - 24;
    g_closeRect = {rightX, btnY, rightX + 20, btnY + btnH};
    Color closeColor = g_closeHover ? COLOR_CLOSE_HOVER : COLOR_MODE_INACTIVE;
    SolidBrush closeBrush(closeColor);
    g.FillRectangle(&closeBrush, rightX, btnY, 20, btnH);

    // X icon
    Pen closePen(COLOR_TEXT, 1.5f);
    int cx = rightX + 10, cy = btnY + btnH/2;
    g.DrawLine(&closePen, cx - 4, cy - 4, cx + 4, cy + 4);
    g.DrawLine(&closePen, cx + 4, cy - 4, cx - 4, cy + 4);
}

/*****************************************************************************
 * DrawAlignButtons - 6 alignment buttons (3x2 grid)
 *****************************************************************************/
static void DrawAlignButtons(Graphics& g) {
    int startX = (WINDOW_WIDTH - (3 * BUTTON_SIZE + 2 * BUTTON_SPACING)) / 2;
    int startY = HEADER_HEIGHT + 20;

    // Row 1: Left, Center H, Right
    for (int i = 0; i < 3; i++) {
        int x = startX + i * (BUTTON_SIZE + BUTTON_SPACING);
        int y = startY;
        g_buttonRects[i] = {x, y, x + BUTTON_SIZE, y + BUTTON_SIZE};
        DrawAlignIcon(g, i, g_buttonRects[i], g_hoveredButton == i);
    }

    // Row 2: Top, Middle V, Bottom
    for (int i = 0; i < 3; i++) {
        int x = startX + i * (BUTTON_SIZE + BUTTON_SPACING);
        int y = startY + BUTTON_SIZE + BUTTON_SPACING;
        g_buttonRects[i + 3] = {x, y, x + BUTTON_SIZE, y + BUTTON_SIZE};
        DrawAlignIcon(g, i + 3, g_buttonRects[i + 3], g_hoveredButton == (i + 3));
    }
}

/*****************************************************************************
 * DrawDistributeButtons - 2 distribution buttons (stacked)
 *****************************************************************************/
static void DrawDistributeButtons(Graphics& g) {
    int startX = (WINDOW_WIDTH - DIST_BUTTON_WIDTH) / 2;
    int startY = HEADER_HEIGHT + 25;

    // Horizontal distribute
    g_buttonRects[0] = {startX, startY, startX + DIST_BUTTON_WIDTH, startY + DIST_BUTTON_HEIGHT};
    DrawDistIcon(g, 0, g_buttonRects[0], g_hoveredButton == 0);

    // Vertical distribute
    startY += DIST_BUTTON_HEIGHT + BUTTON_SPACING;
    g_buttonRects[1] = {startX, startY, startX + DIST_BUTTON_WIDTH, startY + DIST_BUTTON_HEIGHT};
    DrawDistIcon(g, 1, g_buttonRects[1], g_hoveredButton == 1);
}

/*****************************************************************************
 * DrawAlignIcon - Draw individual align button with icon
 *****************************************************************************/
static void DrawAlignIcon(Graphics& g, int index, RECT& rect, bool hover) {
    // Button background
    Color bgColor = hover ? COLOR_BUTTON_HOVER : COLOR_BUTTON_BG;
    SolidBrush brush(bgColor);
    g.FillRectangle(&brush, rect.left, rect.top,
                    rect.right - rect.left, rect.bottom - rect.top);

    // Icon
    Color iconColor = hover ? COLOR_ICON_HOVER : COLOR_ICON;
    Pen pen(iconColor, 2.0f);
    SolidBrush iconBrush(iconColor);

    int cx = (rect.left + rect.right) / 2;
    int cy = (rect.top + rect.bottom) / 2;
    int w = 20, h = 20;

    switch (index) {
    case 0: // Left align - vertical line on left with horizontal lines
        g.DrawLine(&pen, cx - 8, cy - 8, cx - 8, cy + 8);
        g.FillRectangle(&iconBrush, cx - 8, cy - 6, 14, 4);
        g.FillRectangle(&iconBrush, cx - 8, cy + 2, 10, 4);
        break;

    case 1: // Center H - vertical center line with centered rectangles
        g.DrawLine(&pen, cx, cy - 10, cx, cy + 10);
        g.FillRectangle(&iconBrush, cx - 7, cy - 6, 14, 4);
        g.FillRectangle(&iconBrush, cx - 5, cy + 2, 10, 4);
        break;

    case 2: // Right align - vertical line on right with right-aligned lines
        g.DrawLine(&pen, cx + 8, cy - 8, cx + 8, cy + 8);
        g.FillRectangle(&iconBrush, cx - 6, cy - 6, 14, 4);
        g.FillRectangle(&iconBrush, cx - 2, cy + 2, 10, 4);
        break;

    case 3: // Top align - horizontal line on top with top-aligned rectangles
        g.DrawLine(&pen, cx - 8, cy - 8, cx + 8, cy - 8);
        g.FillRectangle(&iconBrush, cx - 6, cy - 8, 4, 14);
        g.FillRectangle(&iconBrush, cx + 2, cy - 8, 4, 10);
        break;

    case 4: // Middle V - horizontal center line with centered rectangles
        g.DrawLine(&pen, cx - 10, cy, cx + 10, cy);
        g.FillRectangle(&iconBrush, cx - 6, cy - 7, 4, 14);
        g.FillRectangle(&iconBrush, cx + 2, cy - 5, 4, 10);
        break;

    case 5: // Bottom align - horizontal line on bottom
        g.DrawLine(&pen, cx - 8, cy + 8, cx + 8, cy + 8);
        g.FillRectangle(&iconBrush, cx - 6, cy - 6, 4, 14);
        g.FillRectangle(&iconBrush, cx + 2, cy - 2, 4, 10);
        break;
    }
}

/*****************************************************************************
 * DrawDistIcon - Draw distribute button with icon and label
 *****************************************************************************/
static void DrawDistIcon(Graphics& g, int index, RECT& rect, bool hover) {
    // Button background
    Color bgColor = hover ? COLOR_BUTTON_HOVER : COLOR_BUTTON_BG;
    SolidBrush brush(bgColor);
    g.FillRectangle(&brush, rect.left, rect.top,
                    rect.right - rect.left, rect.bottom - rect.top);

    // Icon and text
    Color iconColor = hover ? COLOR_ICON_HOVER : COLOR_ICON;
    Pen pen(iconColor, 2.0f);
    SolidBrush iconBrush(iconColor);

    int iconX = rect.left + 15;
    int cy = (rect.top + rect.bottom) / 2;

    FontFamily fontFamily(L"Segoe UI");
    Font font(&fontFamily, 11, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(hover ? COLOR_TEXT : COLOR_TEXT_DIM);

    if (index == 0) { // Horizontal distribute
        // Three vertical bars with equal spacing
        g.FillRectangle(&iconBrush, iconX, cy - 10, 3, 20);
        g.FillRectangle(&iconBrush, iconX + 12, cy - 10, 3, 20);
        g.FillRectangle(&iconBrush, iconX + 24, cy - 10, 3, 20);

        // Text
        RectF textRect((REAL)(iconX + 40), (REAL)(rect.top), 70.0f, (REAL)(rect.bottom - rect.top));
        StringFormat sf;
        sf.SetAlignment(StringAlignmentNear);
        sf.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(L"Horizontal", -1, &font, textRect, &sf, &textBrush);
    } else { // Vertical distribute
        // Three horizontal bars with equal spacing
        g.FillRectangle(&iconBrush, iconX, cy - 10, 20, 3);
        g.FillRectangle(&iconBrush, iconX, cy - 2, 20, 3);
        g.FillRectangle(&iconBrush, iconX, cy + 6, 20, 3);

        // Text
        RectF textRect((REAL)(iconX + 40), (REAL)(rect.top), 70.0f, (REAL)(rect.bottom - rect.top));
        StringFormat sf;
        sf.SetAlignment(StringAlignmentNear);
        sf.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(L"Vertical", -1, &font, textRect, &sf, &textBrush);
    }
}

/*****************************************************************************
 * HandleClick - Process mouse click
 *****************************************************************************/
static void HandleClick(int x, int y) {
    POINT pt = {x, y};

    // Check close button
    if (PtInRect(&g_closeRect, pt)) {
        g_result.cancelled = true;
        ShowWindow(g_hwnd, SW_HIDE);
        g_visible = false;
        return;
    }

    // Check pin button
    if (PtInRect(&g_pinRect, pt)) {
        g_keepPanelOpen = !g_keepPanelOpen;
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }

    // Check mode buttons
    if (PtInRect(&g_alignModeRect, pt)) {
        g_funcMode = FUNC_ALIGN;
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    if (PtInRect(&g_distModeRect, pt)) {
        g_funcMode = FUNC_DISTRIBUTE;
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    if (PtInRect(&g_selModeRect, pt)) {
        g_refMode = REF_SELECTION;
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    if (PtInRect(&g_compModeRect, pt)) {
        g_refMode = REF_COMPOSITION;
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }

    // Check function buttons
    int buttonCount = (g_funcMode == FUNC_ALIGN) ? 6 : 2;
    for (int i = 0; i < buttonCount; i++) {
        if (PtInRect(&g_buttonRects[i], pt)) {
            g_result.cancelled = false;
            g_result.applied = true;
            g_result.funcMode = g_funcMode;
            g_result.refMode = g_refMode;

            if (g_funcMode == FUNC_ALIGN) {
                g_result.alignDir = static_cast<AlignDirection>(i);
            } else {
                g_result.distDir = static_cast<DistributeDirection>(i);
            }

            // Close panel unless pinned
            if (!g_keepPanelOpen) {
                ShowWindow(g_hwnd, SW_HIDE);
                g_visible = false;
            }
            return;
        }
    }
}

/*****************************************************************************
 * HandleKeyboard - Process keyboard input
 *****************************************************************************/
static void HandleKeyboard(WPARAM key) {
    switch (key) {
    case VK_ESCAPE:
        g_result.cancelled = true;
        ShowWindow(g_hwnd, SW_HIDE);
        g_visible = false;
        break;

    case VK_TAB: // Toggle function mode (Align <-> Distribute)
        g_funcMode = (g_funcMode == FUNC_ALIGN) ? FUNC_DISTRIBUTE : FUNC_ALIGN;
        InvalidateRect(g_hwnd, NULL, FALSE);
        break;

    case VK_SPACE: // Toggle reference mode (Selection <-> Composition)
        g_refMode = (g_refMode == REF_SELECTION) ? REF_COMPOSITION : REF_SELECTION;
        InvalidateRect(g_hwnd, NULL, FALSE);
        break;

    case 'A': // Align mode
        g_funcMode = FUNC_ALIGN;
        InvalidateRect(g_hwnd, NULL, FALSE);
        break;

    case 'D': // Distribute mode
        g_funcMode = FUNC_DISTRIBUTE;
        InvalidateRect(g_hwnd, NULL, FALSE);
        break;

    case 'S': // Selection mode
        g_refMode = REF_SELECTION;
        InvalidateRect(g_hwnd, NULL, FALSE);
        break;

    case 'C': // Composition mode
        g_refMode = REF_COMPOSITION;
        InvalidateRect(g_hwnd, NULL, FALSE);
        break;

    // Number keys for quick action
    case '1':
        if (g_funcMode == FUNC_ALIGN) {
            g_result.alignDir = ALIGN_LEFT;
        } else {
            g_result.distDir = DIST_HORIZONTAL;
        }
        goto apply;
    case '2':
        if (g_funcMode == FUNC_ALIGN) g_result.alignDir = ALIGN_CENTER_H;
        goto apply;
    case '3':
        if (g_funcMode == FUNC_ALIGN) g_result.alignDir = ALIGN_RIGHT;
        goto apply;
    case '4':
        if (g_funcMode == FUNC_ALIGN) {
            g_result.alignDir = ALIGN_TOP;
        } else {
            g_result.distDir = DIST_VERTICAL;
        }
        goto apply;
    case '5':
        if (g_funcMode == FUNC_ALIGN) g_result.alignDir = ALIGN_MIDDLE_V;
        goto apply;
    case '6':
        if (g_funcMode == FUNC_ALIGN) g_result.alignDir = ALIGN_BOTTOM;
        goto apply;

    apply:
        g_result.cancelled = false;
        g_result.applied = true;
        g_result.funcMode = g_funcMode;
        g_result.refMode = g_refMode;
        if (!g_keepPanelOpen) {
            ShowWindow(g_hwnd, SW_HIDE);
            g_visible = false;
        }
        break;
    }
}

} // namespace AlignUI

#else // macOS stubs

namespace AlignUI {

void Initialize() {}
void Shutdown() {}
void ShowPanel(int x, int y) { (void)x; (void)y; }
AlignResult HidePanel() { return AlignResult(); }
bool IsVisible() { return false; }
void UpdateHover(int mouseX, int mouseY) { (void)mouseX; (void)mouseY; }
AlignResult GetResult() { return AlignResult(); }
FunctionMode GetFunctionMode() { return FUNC_ALIGN; }
ReferenceMode GetReferenceMode() { return REF_SELECTION; }
void SetFunctionMode(FunctionMode mode) { (void)mode; }
void SetReferenceMode(ReferenceMode mode) { (void)mode; }

} // namespace AlignUI

#endif // MSWindows
