/*****************************************************************************
 * CompUI.cpp (Layer Module)
 *
 * Native Windows UI implementation for Layer Editor Module
 * Displays layer-type specific actions
 * Uses Win32 + GDI+
 *****************************************************************************/

#include "CompUI.h"
#include "GdiPlusIncludes.h"

#ifdef MSWindows
#include <windowsx.h>
#include <string>
#include <cmath>
#include <vector>

using namespace Gdiplus;

// External function from SnapPlugin.cpp to get module scale factor
extern float GetModuleScaleFactor(const char* moduleName);

namespace CompUI {

// Window dimensions
static const int WINDOW_WIDTH = 280;
static const int HEADER_HEIGHT = 32;
static const int BUTTON_HEIGHT = 36;
static const int BUTTON_MARGIN = 8;
static const int PADDING = 8;

// Scale factor for this module (set in ShowPanel)
static float g_scaleFactor = 1.0f;

// Helper functions for scaling
inline int Scaled(int baseValue) {
    return (int)(baseValue * g_scaleFactor);
}

inline int InverseScaled(int screenValue) {
    return (int)(screenValue / g_scaleFactor);
}

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

// Layer type colors
static const Color COLOR_TEXT_LAYER(255, 255, 180, 80);    // Orange-yellow
static const Color COLOR_SHAPE_LAYER(255, 80, 180, 255);   // Blue
static const Color COLOR_SOLID_LAYER(255, 180, 180, 180);  // Gray
static const Color COLOR_NULL_LAYER(255, 255, 80, 80);     // Red
static const Color COLOR_FOOTAGE_LAYER(255, 80, 255, 180); // Green
static const Color COLOR_CAMERA_LAYER(255, 200, 150, 255); // Purple
static const Color COLOR_LIGHT_LAYER(255, 255, 255, 150);  // Yellow-white

// Button info
struct ButtonInfo {
    const wchar_t* label;
    const wchar_t* shortcut;
    const wchar_t* description;
    LayerAction action;
};

// Buttons for each layer type
static const ButtonInfo TEXT_BUTTONS[] = {
    {L"Typewriter", L"1", L"Animate text typing", ACTION_TEXT_ANIMATOR_TYPEWRITER},
    {L"Fade In", L"2", L"Fade in characters", ACTION_TEXT_ANIMATOR_FADE},
    {L"Scale", L"3", L"Scale characters", ACTION_TEXT_ANIMATOR_SCALE},
    {L"Blur", L"4", L"Blur characters", ACTION_TEXT_ANIMATOR_BLUR},
    {L"Tracking", L"5", L"Animate tracking", ACTION_TEXT_ANIMATOR_TRACKING},
};
static const int NUM_TEXT_BUTTONS = 5;

static const ButtonInfo SHAPE_BUTTONS[] = {
    {L"Trim Path", L"1", L"Add trim paths", ACTION_SHAPE_TRIM_PATH},
    {L"Repeater", L"2", L"Add repeater", ACTION_SHAPE_REPEATER},
    {L"Wiggle Path", L"3", L"Add wiggle paths", ACTION_SHAPE_WIGGLE_PATH},
    {L"Wiggle Transform", L"4", L"Add wiggle transform", ACTION_SHAPE_WIGGLE_TRANSFORM},
};
static const int NUM_SHAPE_BUTTONS = 4;

static const ButtonInfo SOLID_BUTTONS[] = {
    {L"Change Color", L"1", L"Change solid color", ACTION_SOLID_CHANGE_COLOR},
    {L"Fit to Comp", L"2", L"Match comp dimensions", ACTION_SOLID_FIT_TO_COMP},
    {L"Reset Transform", L"3", L"Reset pos/scale/rot", ACTION_RESET_TRANSFORM},
};
static const int NUM_SOLID_BUTTONS = 3;

static const ButtonInfo NULL_BUTTONS[] = {
    {L"Reset Transform", L"1", L"Reset pos/scale/rot (parent-aware)", ACTION_RESET_TRANSFORM},
};
static const int NUM_NULL_BUTTONS = 1;

static const ButtonInfo FOOTAGE_BUTTONS[] = {
    {L"Loop (Cycle)", L"1", L"Loop with cycle", ACTION_FOOTAGE_LOOP_CYCLE},
    {L"Loop (Ping Pong)", L"2", L"Loop back and forth", ACTION_FOOTAGE_LOOP_PINGPONG},
    {L"Last Frame Hold", L"3", L"Freeze last frame", ACTION_FOOTAGE_LAST_FRAME_HOLD},
    {L"Reset Transform", L"4", L"Reset pos/scale/rot", ACTION_RESET_TRANSFORM},
};
static const int NUM_FOOTAGE_BUTTONS = 4;

static const ButtonInfo CAMERA_BUTTONS[] = {
    {L"Reset Position", L"1", L"Reset camera position", ACTION_RESET_POSITION},
};
static const int NUM_CAMERA_BUTTONS = 1;

static const ButtonInfo LIGHT_BUTTONS[] = {
    {L"Reset Position", L"1", L"Reset light position", ACTION_RESET_POSITION},
};
static const int NUM_LIGHT_BUTTONS = 1;

static const ButtonInfo NONE_BUTTONS[] = {
    // Empty - no layer selected
};
static const int NUM_NONE_BUTTONS = 0;

// Global state
static HWND g_hwnd = NULL;
static bool g_visible = false;
static ULONG_PTR g_gdiplusToken = 0;
static LayerInfo g_layerInfo = {};
static CompResult g_result;
static bool g_keepPanelOpen = false;

// Button rectangles (dynamic based on layer type)
static RECT g_buttonRects[10];
static RECT g_pinRect;
static int g_numButtons = 0;
static const ButtonInfo* g_currentButtons = nullptr;

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
static void ExecuteAction(LayerAction action);
static void UpdateButtonsForLayerType();
static int CalculateWindowHeight();
static Color GetLayerTypeColor();
static const wchar_t* GetLayerTypeName();

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
        L"Layer Editor",
        WS_POPUP,
        0, 0, WINDOW_WIDTH, 200,  // Height will be adjusted
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
 * UpdateButtonsForLayerType
 *****************************************************************************/
static void UpdateButtonsForLayerType() {
    switch (g_layerInfo.type) {
    case LAYER_TEXT:
        g_currentButtons = TEXT_BUTTONS;
        g_numButtons = NUM_TEXT_BUTTONS;
        break;
    case LAYER_SHAPE:
        g_currentButtons = SHAPE_BUTTONS;
        g_numButtons = NUM_SHAPE_BUTTONS;
        break;
    case LAYER_SOLID:
    case LAYER_ADJUSTMENT:
        g_currentButtons = SOLID_BUTTONS;
        g_numButtons = NUM_SOLID_BUTTONS;
        break;
    case LAYER_NULL:
        g_currentButtons = NULL_BUTTONS;
        g_numButtons = NUM_NULL_BUTTONS;
        break;
    case LAYER_FOOTAGE:
    case LAYER_PRECOMP:
        g_currentButtons = FOOTAGE_BUTTONS;
        g_numButtons = NUM_FOOTAGE_BUTTONS;
        break;
    case LAYER_CAMERA:
        g_currentButtons = CAMERA_BUTTONS;
        g_numButtons = NUM_CAMERA_BUTTONS;
        break;
    case LAYER_LIGHT:
        g_currentButtons = LIGHT_BUTTONS;
        g_numButtons = NUM_LIGHT_BUTTONS;
        break;
    default:
        g_currentButtons = NONE_BUTTONS;
        g_numButtons = NUM_NONE_BUTTONS;
        break;
    }
}

/*****************************************************************************
 * CalculateWindowHeight
 *****************************************************************************/
static int CalculateWindowHeight() {
    if (g_numButtons == 0) {
        return HEADER_HEIGHT + BUTTON_MARGIN + 40;  // "No layer selected" message
    }
    return HEADER_HEIGHT + BUTTON_MARGIN + g_numButtons * (BUTTON_HEIGHT + 4) + BUTTON_MARGIN;
}

/*****************************************************************************
 * GetLayerTypeColor
 *****************************************************************************/
static Color GetLayerTypeColor() {
    switch (g_layerInfo.type) {
    case LAYER_TEXT: return COLOR_TEXT_LAYER;
    case LAYER_SHAPE: return COLOR_SHAPE_LAYER;
    case LAYER_SOLID:
    case LAYER_ADJUSTMENT: return COLOR_SOLID_LAYER;
    case LAYER_NULL: return COLOR_NULL_LAYER;
    case LAYER_FOOTAGE:
    case LAYER_PRECOMP: return COLOR_FOOTAGE_LAYER;
    case LAYER_CAMERA: return COLOR_CAMERA_LAYER;
    case LAYER_LIGHT: return COLOR_LIGHT_LAYER;
    default: return COLOR_TEXT_DIM;
    }
}

/*****************************************************************************
 * GetLayerTypeName
 *****************************************************************************/
static const wchar_t* GetLayerTypeName() {
    switch (g_layerInfo.type) {
    case LAYER_TEXT: return L"Text";
    case LAYER_SHAPE: return L"Shape";
    case LAYER_SOLID: return L"Solid";
    case LAYER_ADJUSTMENT: return L"Adjustment";
    case LAYER_NULL: return L"Null";
    case LAYER_FOOTAGE: return L"Footage";
    case LAYER_PRECOMP: return L"Pre-comp";
    case LAYER_CAMERA: return L"Camera";
    case LAYER_LIGHT: return L"Light";
    default: return L"No Layer";
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

    // Update buttons based on layer type
    UpdateButtonsForLayerType();

    // Get scale factor from settings
    g_scaleFactor = GetModuleScaleFactor("comp");
    int scaledWidth = Scaled(WINDOW_WIDTH);
    int windowHeight = CalculateWindowHeight();
    int scaledHeight = Scaled(windowHeight);

    // Store layer type in result
    g_result.layerType = g_layerInfo.type;

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

LayerAction GetSelectedAction() { return g_result.action; }

LayerType GetCurrentLayerType() { return g_layerInfo.type; }

/*****************************************************************************
 * SetLayerInfo - Parse JSON from ExtendScript
 *****************************************************************************/
void SetLayerInfo(const wchar_t* jsonInfo) {
    if (!jsonInfo || jsonInfo[0] == L'\0') {
        // No layer selected
        g_layerInfo = LayerInfo();
        g_layerInfo.type = LAYER_NONE;
        return;
    }

    std::wstring json(jsonInfo);

    auto getInt = [&json](const wchar_t* key) -> int {
        std::wstring search = std::wstring(L"\"") + key + L"\":";
        size_t pos = json.find(search);
        if (pos == std::wstring::npos) return 0;
        pos += search.length();
        return _wtoi(json.c_str() + pos);
    };

    auto getBool = [&json](const wchar_t* key) -> bool {
        std::wstring search = std::wstring(L"\"") + key + L"\":";
        size_t pos = json.find(search);
        if (pos == std::wstring::npos) return false;
        pos += search.length();
        // Skip whitespace
        while (pos < json.length() && (json[pos] == L' ' || json[pos] == L'\t')) pos++;
        return json.substr(pos, 4) == L"true";
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

    getString(L"name", g_layerInfo.name, 256);
    g_layerInfo.type = (LayerType)getInt(L"type");
    g_layerInfo.index = getInt(L"index");
    g_layerInfo.hasParent = getBool(L"hasParent");
    g_layerInfo.parentIndex = getInt(L"parentIndex");
    g_layerInfo.isSelected = getBool(L"isSelected");
    g_layerInfo.solidColor = (unsigned int)getInt(L"solidColor");
    g_layerInfo.isSequence = getBool(L"isSequence");
    g_layerInfo.hasTimeRemap = getBool(L"hasTimeRemap");

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

        // Double buffering
        RECT rect;
        GetClientRect(hwnd, &rect);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

        Draw(memDC);

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_LBUTTONDOWN: {
        int x = InverseScaled(GET_X_LPARAM(lParam));
        int y = InverseScaled(GET_Y_LPARAM(lParam));

        // Check header drag
        if (y >= 0 && y < HEADER_HEIGHT && x < g_pinRect.left) {
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
        } else if (ch >= L'1' && ch <= L'9' && g_currentButtons) {
            int index = ch - L'1';
            if (index >= 0 && index < g_numButtons) {
                ExecuteAction(g_currentButtons[index].action);
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

    // Apply scale transform
    g.ScaleTransform(g_scaleFactor, g_scaleFactor);

    int windowHeight = CalculateWindowHeight();

    // Background
    SolidBrush bgBrush(COLOR_BG);
    g.FillRectangle(&bgBrush, 0, 0, WINDOW_WIDTH, windowHeight);

    // Border
    Pen borderPen(COLOR_BORDER, 1);
    g.DrawRectangle(&borderPen, 0, 0, WINDOW_WIDTH - 1, windowHeight - 1);

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
    Font layerFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);

    // Layer type with color indicator
    Color typeColor = GetLayerTypeColor();
    SolidBrush typeBrush(typeColor);

    // Type indicator circle
    g.FillEllipse(&typeBrush, 10.0f, 10.0f, 12.0f, 12.0f);

    // Layer type name
    g.DrawString(GetLayerTypeName(), -1, &titleFont, PointF(28, 8), &textBrush);

    // Layer name (if available)
    if (g_layerInfo.name[0] != L'\0') {
        RectF nameRect(100, 8, 130, 20);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentFar);
        sf.SetTrimming(StringTrimmingEllipsisCharacter);
        g.DrawString(g_layerInfo.name, -1, &layerFont, nameRect, &sf, &dimBrush);
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

    if (g_numButtons == 0) {
        // No layer selected message
        FontFamily fontFamily(L"Segoe UI");
        Font msgFont(&fontFamily, 11, FontStyleRegular, UnitPixel);
        SolidBrush dimBrush(COLOR_TEXT_DIM);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        RectF msgRect(0, (REAL)y, (REAL)WINDOW_WIDTH, 30);
        g.DrawString(L"No layer selected", -1, &msgFont, msgRect, &sf, &dimBrush);
        return;
    }

    for (int i = 0; i < g_numButtons; i++) {
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
    if (!g_currentButtons || index >= g_numButtons) return;
    const ButtonInfo& btn = g_currentButtons[index];

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
    for (int i = 0; i < g_numButtons; i++) {
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
    if (hit >= 0 && hit < g_numButtons && g_currentButtons) {
        ExecuteAction(g_currentButtons[hit].action);
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
static void ExecuteAction(LayerAction action) {
    g_result.action = action;
    g_result.applied = true;
    g_result.layerType = g_layerInfo.type;

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
void SetLayerInfo(const wchar_t* jsonInfo) { (void)jsonInfo; }
LayerAction GetSelectedAction() { return ACTION_NONE; }
LayerType GetCurrentLayerType() { return LAYER_UNKNOWN; }

} // namespace CompUI

#endif // MSWindows
