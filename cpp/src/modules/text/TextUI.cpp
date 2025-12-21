/*****************************************************************************
 * TextUI.cpp
 *
 * Native Windows UI implementation for Text Options Module
 * Uses Win32 + GDI+ with WS_EX_NOACTIVATE for non-focus drag operations
 *****************************************************************************/

#include "TextUI.h"
#include "GdiPlusIncludes.h"

#ifdef MSWindows
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace Gdiplus;

namespace TextUI {

// Window dimensions
static const int WINDOW_WIDTH = 340;
static const int WINDOW_HEIGHT = 260;
static const int HEADER_HEIGHT = 32;
static const int SECTION_HEIGHT = 28;
static const int VALUE_BOX_WIDTH = 100;
static const int VALUE_BOX_HEIGHT = 22;
static const int COLOR_BOX_SIZE = 24;
static const int ALIGN_BTN_SIZE = 28;

// Colors
static const Color COLOR_BG(240, 28, 28, 32);
static const Color COLOR_HEADER_BG(255, 40, 40, 48);
static const Color COLOR_SECTION_LABEL(255, 120, 120, 130);
static const Color COLOR_VALUE_BG(255, 45, 45, 55);
static const Color COLOR_VALUE_HOVER(255, 55, 70, 85);
static const Color COLOR_VALUE_DRAG(255, 50, 90, 130);
static const Color COLOR_VALUE_EDIT(255, 35, 55, 80);
static const Color COLOR_VALUE_BORDER(255, 74, 158, 255);
static const Color COLOR_TEXT(255, 220, 220, 220);
static const Color COLOR_TEXT_DIM(255, 160, 160, 160);
static const Color COLOR_ALIGN_BG(255, 50, 50, 60);
static const Color COLOR_ALIGN_HOVER(255, 70, 100, 130);
static const Color COLOR_ALIGN_ACTIVE(255, 74, 158, 255);
static const Color COLOR_PIN_ACTIVE(255, 255, 200, 0);
static const Color COLOR_CLOSE_HOVER(255, 200, 60, 60);

// Drag sensitivity configuration
struct DragConfig {
    float sensitivity;  // value change per pixel
    float minValue;
    float maxValue;
    int decimals;
    const wchar_t* suffix;
};

static const DragConfig DRAG_CONFIGS[] = {
    {0.5f, 1.0f, 999.0f, 1, L"pt"},      // SIZE
    {2.0f, -1000.0f, 1000.0f, 0, L""},   // TRACKING
    {1.0f, 0.0f, 999.0f, 0, L""},        // LEADING (0=Auto)
    {0.1f, 0.0f, 100.0f, 1, L"px"}       // STROKE_WIDTH
};

// Global state
static HWND g_hwnd = NULL;
static HWND g_colorPickerHwnd = NULL;
static bool g_visible = false;
static ULONG_PTR g_gdiplusToken = 0;
static TextInfo g_textInfo = {};
static TextResult g_result;
static bool g_keepPanelOpen = false;

// Value box rectangles
static RECT g_sizeRect;
static RECT g_trackingRect;
static RECT g_leadingRect;
static RECT g_strokeWidthRect;
static RECT g_fillColorRect;
static RECT g_strokeColorRect;
static RECT g_alignRects[7];
static RECT g_pinRect;
static RECT g_closeRect;

// Hover state
static ValueTarget g_hoverTarget = TARGET_NONE;
static int g_hoverAlign = -1;
static bool g_pinHover = false;
static bool g_closeHover = false;
static bool g_fillColorHover = false;
static bool g_strokeColorHover = false;

// Drag state
static bool g_dragging = false;
static ValueTarget g_dragTarget = TARGET_NONE;
static int g_dragStartX = 0;
static float g_dragStartValue = 0;

// Edit state (inline editing)
static bool g_editMode = false;
static ValueTarget g_editTarget = TARGET_NONE;
static std::wstring g_editText;
static int g_editCursorPos = 0;
static bool g_editSelectAll = false;
static DWORD g_lastClickTime = 0;
static ValueTarget g_lastClickTarget = TARGET_NONE;

// Color picker state
static bool g_colorPickerForStroke = false;
static float g_pickerColor[3] = {1, 1, 1};
static int g_pickerHue = 0;
static bool g_colorPickerVisible = false;
static int g_colorPickerDragMode = 0; // 0=none, 1=hue bar, 2=SV square
static float g_pickerSaturation = 1.0f;
static float g_pickerValue = 1.0f;

// Color picker dimensions
static const int COLOR_PICKER_WIDTH = 220;
static const int COLOR_PICKER_HEIGHT = 200;
static const int HUE_BAR_WIDTH = 20;
static const int SV_SIZE = 180;
static RECT g_svRect;
static RECT g_hueRect;

// Font selector state
static bool g_fontDropdownVisible = false;
static HWND g_fontDropdownHwnd = NULL;
static std::vector<std::wstring> g_fontList;
static int g_fontScrollOffset = 0;
static int g_fontHoverIndex = -1;
static const int FONT_ITEM_HEIGHT = 24;
static const int FONT_DROPDOWN_MAX_ITEMS = 10;
static RECT g_fontBoxRect;

// Forward declarations
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK ColorPickerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK FontDropdownWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void Draw(HDC hdc);
static void DrawHeader(Graphics& g);
static void DrawFontSection(Graphics& g, int& y);
static void DrawColorSection(Graphics& g, int& y);
static void DrawValueSection(Graphics& g, int& y);
static void DrawAlignSection(Graphics& g, int& y);
static void DrawValueBox(Graphics& g, RECT& rect, ValueTarget target, float value, const wchar_t* suffix);
static void DrawColorBox(Graphics& g, RECT& rect, float* color, bool hasColor, bool hover);
static void DrawAlignButton(Graphics& g, RECT& rect, int index, bool active, bool hover);
static ValueTarget HitTestValue(int x, int y);
static int HitTestAlign(int x, int y);
static void HandleMouseDown(int x, int y);
static void HandleMouseUp(int x, int y);
static void HandleMouseMove(int x, int y);
static void HandleDoubleClick(int x, int y);
static void HandleKeyDown(WPARAM key);
static void HandleChar(wchar_t ch);
static void EnterEditMode(ValueTarget target);
static void ExitEditMode(bool apply);
static float GetTargetValue(ValueTarget target);
static void SetTargetValue(ValueTarget target, float value);
static void ApplyTextProperty(const char* propName, float value);
static void ApplyTextColor(bool stroke, float r, float g, float b);
static void ApplyJustification(Justification just);
static std::wstring FormatValue(ValueTarget target, float value);

// Color picker helpers
static void HSVtoRGB(float h, float s, float v, float& r, float& g, float& b);
static void RGBtoHSV(float r, float g, float b, float& h, float& s, float& v);
static void DrawColorPicker(HDC hdc);
static void UpdatePickerFromColor(float* color);

// Font dropdown helpers
static void PopulateFontList();
static void ShowFontDropdown();
static void HideFontDropdown();
static void DrawFontDropdown(HDC hdc);
static void ApplyFont(const wchar_t* fontName);

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

    // Register main window class (explicitly use Wide version for Unicode strings)
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"TextUIWindow";
    ATOM classAtom = RegisterClassExW(&wc);
    if (classAtom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        // Registration failed and class doesn't exist
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
        return;
    }

    // Create main window (initially hidden)
    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"TextUIWindow",
        L"Text Options",
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

    SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);

    // Register color picker window class
    WNDCLASSEXW wcPicker = {0};
    wcPicker.cbSize = sizeof(WNDCLASSEXW);
    wcPicker.style = CS_HREDRAW | CS_VREDRAW;
    wcPicker.lpfnWndProc = ColorPickerWndProc;
    wcPicker.hInstance = GetModuleHandle(NULL);
    wcPicker.hCursor = LoadCursor(NULL, IDC_CROSS);
    wcPicker.hbrBackground = NULL;
    wcPicker.lpszClassName = L"TextUIColorPicker";
    RegisterClassExW(&wcPicker);

    // Create color picker window (initially hidden)
    g_colorPickerHwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"TextUIColorPicker",
        L"Color Picker",
        WS_POPUP,
        0, 0, COLOR_PICKER_WIDTH, COLOR_PICKER_HEIGHT,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    // Register font dropdown window class
    WNDCLASSEXW wcFont = {0};
    wcFont.cbSize = sizeof(WNDCLASSEXW);
    wcFont.style = CS_HREDRAW | CS_VREDRAW;
    wcFont.lpfnWndProc = FontDropdownWndProc;
    wcFont.hInstance = GetModuleHandle(NULL);
    wcFont.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcFont.hbrBackground = NULL;
    wcFont.lpszClassName = L"TextUIFontDropdown";
    RegisterClassExW(&wcFont);

    // Populate font list
    PopulateFontList();
}

/*****************************************************************************
 * Shutdown
 *****************************************************************************/
void Shutdown() {
    if (g_fontDropdownHwnd) {
        DestroyWindow(g_fontDropdownHwnd);
        g_fontDropdownHwnd = NULL;
    }
    if (g_colorPickerHwnd) {
        DestroyWindow(g_colorPickerHwnd);
        g_colorPickerHwnd = NULL;
    }
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }
    if (g_gdiplusToken) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
    g_fontList.clear();
}

/*****************************************************************************
 * ShowPanel
 *****************************************************************************/
void ShowPanel(int x, int y) {
    if (!g_hwnd) Initialize();

    // Reset state
    g_result = TextResult();
    g_dragging = false;
    g_editMode = false;
    g_hoverTarget = TARGET_NONE;

    // Position window
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

    int posX = x - WINDOW_WIDTH / 2;
    int posY = y - WINDOW_HEIGHT / 2;

    if (posX < workArea.left) posX = workArea.left;
    if (posY < workArea.top) posY = workArea.top;
    if (posX + WINDOW_WIDTH > workArea.right) posX = workArea.right - WINDOW_WIDTH;
    if (posY + WINDOW_HEIGHT > workArea.bottom) posY = workArea.bottom - WINDOW_HEIGHT;

    SetWindowPos(g_hwnd, HWND_TOPMOST, posX, posY, WINDOW_WIDTH, WINDOW_HEIGHT,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(g_hwnd, SW_SHOWNA);
    InvalidateRect(g_hwnd, NULL, TRUE);
    g_visible = true;
}

/*****************************************************************************
 * HidePanel
 *****************************************************************************/
TextResult HidePanel() {
    if (g_editMode) ExitEditMode(false);
    HideColorPicker();

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
    if (!g_hwnd || !g_visible || g_dragging) return;

    RECT wndRect;
    GetWindowRect(g_hwnd, &wndRect);
    int localX = mouseX - wndRect.left;
    int localY = mouseY - wndRect.top;

    bool needsRepaint = false;
    POINT pt = {localX, localY};

    // Check value boxes
    ValueTarget newHover = HitTestValue(localX, localY);
    if (newHover != g_hoverTarget) {
        g_hoverTarget = newHover;
        needsRepaint = true;

        // Set cursor
        if (g_hoverTarget != TARGET_NONE && !g_editMode) {
            SetCursor(LoadCursor(NULL, IDC_SIZEWE));
        } else {
            SetCursor(LoadCursor(NULL, IDC_ARROW));
        }
    }

    // Check align buttons
    int newAlign = HitTestAlign(localX, localY);
    if (newAlign != g_hoverAlign) {
        g_hoverAlign = newAlign;
        needsRepaint = true;
    }

    // Check color boxes
    bool newFillHover = PtInRect(&g_fillColorRect, pt);
    bool newStrokeHover = PtInRect(&g_strokeColorRect, pt);
    if (newFillHover != g_fillColorHover || newStrokeHover != g_strokeColorHover) {
        g_fillColorHover = newFillHover;
        g_strokeColorHover = newStrokeHover;
        needsRepaint = true;
    }

    // Check pin/close
    bool newPinHover = PtInRect(&g_pinRect, pt);
    bool newCloseHover = PtInRect(&g_closeRect, pt);
    if (newPinHover != g_pinHover || newCloseHover != g_closeHover) {
        g_pinHover = newPinHover;
        g_closeHover = newCloseHover;
        needsRepaint = true;
    }

    if (needsRepaint) {
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

TextResult GetResult() { return g_result; }

/*****************************************************************************
 * SetTextInfo - Parse JSON from ExtendScript
 *****************************************************************************/
void SetTextInfo(const wchar_t* jsonInfo) {
    if (!jsonInfo || jsonInfo[0] == L'\0') return;

    // Simple JSON parsing (assuming well-formed)
    std::wstring json(jsonInfo);

    auto getFloat = [&json](const wchar_t* key) -> float {
        std::wstring search = std::wstring(L"\"") + key + L"\":";
        size_t pos = json.find(search);
        if (pos == std::wstring::npos) return 0;
        pos += search.length();
        return (float)_wtof(json.c_str() + pos);
    };

    auto getBool = [&json](const wchar_t* key) -> bool {
        std::wstring search = std::wstring(L"\"") + key + L"\":";
        size_t pos = json.find(search);
        if (pos == std::wstring::npos) return false;
        pos += search.length();
        while (json[pos] == L' ') pos++;
        return json[pos] == L't';
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

    getString(L"font", g_textInfo.font, 128);
    getString(L"layerName", g_textInfo.layerName, 256);
    g_textInfo.fontSize = getFloat(L"fontSize");
    g_textInfo.tracking = getFloat(L"tracking");
    g_textInfo.leading = getFloat(L"leading");
    g_textInfo.strokeWidth = getFloat(L"strokeWidth");
    g_textInfo.applyFill = getBool(L"applyFill");
    g_textInfo.applyStroke = getBool(L"applyStroke");
    g_textInfo.justify = (Justification)(int)getFloat(L"justification");

    // Parse colors (simplified - assumes [r,g,b] format)
    // TODO: proper array parsing

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
        Draw(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEACTIVATE:
        // Prevent activation when clicked (unless in edit mode)
        if (!g_editMode) return MA_NOACTIVATE;
        return MA_ACTIVATE;

    case WM_LBUTTONDOWN:
        HandleMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONUP:
        HandleMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSEMOVE:
        HandleMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONDBLCLK:
        HandleDoubleClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_KEYDOWN:
        HandleKeyDown(wParam);
        return 0;

    case WM_CHAR:
        HandleChar((wchar_t)wParam);
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && !g_keepPanelOpen && !g_dragging && !g_editMode) {
            g_result.cancelled = true;
            ShowWindow(hwnd, SW_HIDE);
            g_visible = false;
        }
        return 0;

    case WM_KILLFOCUS:
        if (g_editMode) {
            ExitEditMode(true);
        } else if (!g_keepPanelOpen && !g_dragging) {
            g_result.cancelled = true;
            ShowWindow(hwnd, SW_HIDE);
            g_visible = false;
        }
        return 0;

    case WM_SETCURSOR:
        if (g_hoverTarget != TARGET_NONE && !g_editMode) {
            SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            return TRUE;
        }
        break;

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
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    // Background
    SolidBrush bgBrush(COLOR_BG);
    g.FillRectangle(&bgBrush, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    // Border
    Pen borderPen(Color(255, 60, 60, 70), 1);
    g.DrawRectangle(&borderPen, 0, 0, WINDOW_WIDTH - 1, WINDOW_HEIGHT - 1);

    // Draw sections
    DrawHeader(g);

    int y = HEADER_HEIGHT + 8;
    DrawFontSection(g, y);
    DrawColorSection(g, y);
    DrawValueSection(g, y);
    DrawAlignSection(g, y);
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

    // Title
    g.DrawString(L"Text Options", -1, &titleFont, PointF(10, 8), &textBrush);

    // Layer name
    if (g_textInfo.layerName[0] != L'\0') {
        RectF layerRect(120, 8, 140, 20);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentFar);
        sf.SetTrimming(StringTrimmingEllipsisCharacter);
        g.DrawString(g_textInfo.layerName, -1, &layerFont, layerRect, &sf, &dimBrush);
    }

    // Pin button
    int btnY = (HEADER_HEIGHT - 20) / 2;
    g_pinRect = {WINDOW_WIDTH - 54, btnY, WINDOW_WIDTH - 34, btnY + 20};
    Color pinColor = g_keepPanelOpen ? COLOR_PIN_ACTIVE :
                     (g_pinHover ? COLOR_ALIGN_HOVER : COLOR_ALIGN_BG);
    SolidBrush pinBrush(pinColor);
    g.FillRectangle(&pinBrush, g_pinRect.left, g_pinRect.top, 20, 20);

    Pen pinPen(g_keepPanelOpen ? Color(255, 40, 40, 40) : COLOR_TEXT, 1.5f);
    int px = g_pinRect.left + 10, py = g_pinRect.top + 10;
    g.DrawLine(&pinPen, px - 4, py - 4, px + 4, py + 4);
    g.DrawEllipse(&pinPen, px - 3, py - 6, 6, 6);

    // Close button
    g_closeRect = {WINDOW_WIDTH - 28, btnY, WINDOW_WIDTH - 8, btnY + 20};
    Color closeColor = g_closeHover ? COLOR_CLOSE_HOVER : COLOR_ALIGN_BG;
    SolidBrush closeBrush(closeColor);
    g.FillRectangle(&closeBrush, g_closeRect.left, g_closeRect.top, 20, 20);

    Pen closePen(COLOR_TEXT, 1.5f);
    int cx = g_closeRect.left + 10, cy = g_closeRect.top + 10;
    g.DrawLine(&closePen, cx - 4, cy - 4, cx + 4, cy + 4);
    g.DrawLine(&closePen, cx + 4, cy - 4, cx - 4, cy + 4);
}

/*****************************************************************************
 * DrawFontSection
 *****************************************************************************/
static void DrawFontSection(Graphics& g, int& y) {
    FontFamily fontFamily(L"Segoe UI");
    Font labelFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
    Font valueFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush labelBrush(COLOR_SECTION_LABEL);
    SolidBrush textBrush(COLOR_TEXT);

    g.DrawString(L"Font", -1, &labelFont, PointF(10, (REAL)y), &labelBrush);
    y += 16;

    // Font family display (clickable dropdown)
    g_fontBoxRect = {10, y, 210, y + VALUE_BOX_HEIGHT};
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(g_hwnd, &pt);
    bool fontHover = PtInRect(&g_fontBoxRect, pt);

    Color fontBgColor = fontHover ? COLOR_VALUE_HOVER : COLOR_VALUE_BG;
    SolidBrush valueBgBrush(fontBgColor);
    g.FillRectangle(&valueBgBrush, 10, y, 200, VALUE_BOX_HEIGHT);

    // Font name
    g.DrawString(g_textInfo.font[0] ? g_textInfo.font : L"(Select text layer)",
                 -1, &valueFont, PointF(14, (REAL)(y + 4)), &textBrush);

    // Dropdown arrow
    if (g_textInfo.font[0]) {
        Pen arrowPen(COLOR_TEXT_DIM, 1.5f);
        int ax = 200;
        int ay = y + VALUE_BOX_HEIGHT / 2;
        g.DrawLine(&arrowPen, ax - 4, ay - 2, ax, ay + 2);
        g.DrawLine(&arrowPen, ax, ay + 2, ax + 4, ay - 2);
    }

    y += SECTION_HEIGHT + 4;
}

/*****************************************************************************
 * DrawColorSection
 *****************************************************************************/
static void DrawColorSection(Graphics& g, int& y) {
    FontFamily fontFamily(L"Segoe UI");
    Font labelFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
    Font valueFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush labelBrush(COLOR_SECTION_LABEL);
    SolidBrush textBrush(COLOR_TEXT);

    g.DrawString(L"Color", -1, &labelFont, PointF(10, (REAL)y), &labelBrush);
    y += 16;

    // Fill color
    g.DrawString(L"Fill", -1, &valueFont, PointF(10, (REAL)(y + 4)), &textBrush);
    g_fillColorRect = {50, y, 50 + COLOR_BOX_SIZE, y + COLOR_BOX_SIZE};
    DrawColorBox(g, g_fillColorRect, g_textInfo.fillColor, g_textInfo.applyFill, g_fillColorHover);

    // Stroke color
    g.DrawString(L"Stroke", -1, &valueFont, PointF(90, (REAL)(y + 4)), &textBrush);
    g_strokeColorRect = {140, y, 140 + COLOR_BOX_SIZE, y + COLOR_BOX_SIZE};
    DrawColorBox(g, g_strokeColorRect, g_textInfo.strokeColor, g_textInfo.applyStroke, g_strokeColorHover);

    // Stroke width
    g.DrawString(L"Width", -1, &valueFont, PointF(180, (REAL)(y + 4)), &textBrush);
    g_strokeWidthRect = {220, y, 220 + 70, y + VALUE_BOX_HEIGHT};
    DrawValueBox(g, g_strokeWidthRect, TARGET_STROKE_WIDTH, g_textInfo.strokeWidth, L"px");

    y += SECTION_HEIGHT + 4;
}

/*****************************************************************************
 * DrawValueSection
 *****************************************************************************/
static void DrawValueSection(Graphics& g, int& y) {
    FontFamily fontFamily(L"Segoe UI");
    Font labelFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
    Font valueFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush labelBrush(COLOR_SECTION_LABEL);
    SolidBrush textBrush(COLOR_TEXT);

    // Size
    g.DrawString(L"Size", -1, &valueFont, PointF(10, (REAL)(y + 4)), &textBrush);
    g_sizeRect = {50, y, 50 + VALUE_BOX_WIDTH, y + VALUE_BOX_HEIGHT};
    DrawValueBox(g, g_sizeRect, TARGET_SIZE, g_textInfo.fontSize, L"pt");

    // Tracking
    g.DrawString(L"Tracking", -1, &valueFont, PointF(170, (REAL)(y + 4)), &textBrush);
    g_trackingRect = {230, y, 230 + VALUE_BOX_WIDTH, y + VALUE_BOX_HEIGHT};
    DrawValueBox(g, g_trackingRect, TARGET_TRACKING, g_textInfo.tracking, L"");

    y += SECTION_HEIGHT + 4;

    // Leading
    g.DrawString(L"Leading", -1, &valueFont, PointF(10, (REAL)(y + 4)), &textBrush);
    g_leadingRect = {70, y, 70 + VALUE_BOX_WIDTH, y + VALUE_BOX_HEIGHT};
    DrawValueBox(g, g_leadingRect, TARGET_LEADING, g_textInfo.leading, L"");

    y += SECTION_HEIGHT + 8;
}

/*****************************************************************************
 * DrawAlignSection
 *****************************************************************************/
static void DrawAlignSection(Graphics& g, int& y) {
    FontFamily fontFamily(L"Segoe UI");
    Font labelFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
    SolidBrush labelBrush(COLOR_SECTION_LABEL);

    g.DrawString(L"Align", -1, &labelFont, PointF(10, (REAL)y), &labelBrush);
    y += 16;

    int x = 10;
    for (int i = 0; i < 7; i++) {
        g_alignRects[i] = {x, y, x + ALIGN_BTN_SIZE, y + ALIGN_BTN_SIZE};
        bool active = (g_textInfo.justify == (Justification)i);
        bool hover = (g_hoverAlign == i);
        DrawAlignButton(g, g_alignRects[i], i, active, hover);
        x += ALIGN_BTN_SIZE + 4;
        if (i == 2) x += 8; // Gap between L/C/R and justify buttons
    }

    y += ALIGN_BTN_SIZE + 8;
}

/*****************************************************************************
 * DrawValueBox
 *****************************************************************************/
static void DrawValueBox(Graphics& g, RECT& rect, ValueTarget target, float value, const wchar_t* suffix) {
    bool isHover = (g_hoverTarget == target) && !g_editMode;
    bool isDrag = (g_dragging && g_dragTarget == target);
    bool isEdit = (g_editMode && g_editTarget == target);

    // Background
    Color bgColor = isEdit ? COLOR_VALUE_EDIT :
                    isDrag ? COLOR_VALUE_DRAG :
                    isHover ? COLOR_VALUE_HOVER : COLOR_VALUE_BG;
    SolidBrush bgBrush(bgColor);
    g.FillRectangle(&bgBrush, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);

    // Border for edit mode
    if (isEdit) {
        Pen borderPen(COLOR_VALUE_BORDER, 2);
        g.DrawRectangle(&borderPen, rect.left, rect.top,
                        rect.right - rect.left - 1, rect.bottom - rect.top - 1);
    }

    // Text
    FontFamily fontFamily(L"Segoe UI");
    Font font(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);

    std::wstring displayText;
    if (isEdit) {
        displayText = g_editText;
    } else {
        displayText = FormatValue(target, value);
    }

    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    RectF textRect((REAL)rect.left, (REAL)rect.top,
                   (REAL)(rect.right - rect.left), (REAL)(rect.bottom - rect.top));
    g.DrawString(displayText.c_str(), -1, &font, textRect, &sf, &textBrush);

    // Cursor for edit mode
    if (isEdit) {
        // Simple cursor at end (TODO: proper cursor positioning)
        REAL textWidth = 0;
        RectF bounds;
        g.MeasureString(displayText.c_str(), -1, &font, PointF(0, 0), &bounds);
        textWidth = bounds.Width;

        int cx = (rect.left + rect.right) / 2 + (int)(textWidth / 2) + 2;
        int cy1 = rect.top + 4;
        int cy2 = rect.bottom - 4;
        Pen cursorPen(COLOR_TEXT, 1);
        g.DrawLine(&cursorPen, cx, cy1, cx, cy2);
    }
}

/*****************************************************************************
 * DrawColorBox
 *****************************************************************************/
static void DrawColorBox(Graphics& g, RECT& rect, float* color, bool hasColor, bool hover) {
    // Border
    Pen borderPen(hover ? COLOR_VALUE_BORDER : Color(255, 80, 80, 90), hover ? 2.0f : 1.0f);
    g.DrawRectangle(&borderPen, rect.left, rect.top,
                    rect.right - rect.left - 1, rect.bottom - rect.top - 1);

    // Fill
    if (hasColor) {
        int r = (int)(color[0] * 255);
        int g_val = (int)(color[1] * 255);
        int b = (int)(color[2] * 255);
        SolidBrush colorBrush(Color(255, r, g_val, b));
        g.FillRectangle(&colorBrush, rect.left + 2, rect.top + 2,
                        rect.right - rect.left - 4, rect.bottom - rect.top - 4);
    } else {
        // X pattern for no color
        Pen xPen(Color(255, 100, 100, 100), 1);
        g.DrawLine(&xPen, rect.left + 4, rect.top + 4, rect.right - 4, rect.bottom - 4);
        g.DrawLine(&xPen, rect.right - 4, rect.top + 4, rect.left + 4, rect.bottom - 4);
    }
}

/*****************************************************************************
 * DrawAlignButton
 *****************************************************************************/
static void DrawAlignButton(Graphics& g, RECT& rect, int index, bool active, bool hover) {
    Color bgColor = active ? COLOR_ALIGN_ACTIVE :
                    hover ? COLOR_ALIGN_HOVER : COLOR_ALIGN_BG;
    SolidBrush bgBrush(bgColor);
    g.FillRectangle(&bgBrush, rect.left, rect.top,
                    rect.right - rect.left, rect.bottom - rect.top);

    // Draw alignment icon
    Color iconColor = active ? Color(255, 255, 255, 255) : COLOR_TEXT;
    Pen iconPen(iconColor, 1.5f);
    SolidBrush iconBrush(iconColor);

    int cx = (rect.left + rect.right) / 2;
    int cy = (rect.top + rect.bottom) / 2;

    // Icon patterns based on index
    switch (index) {
    case 0: // Left
        g.DrawLine(&iconPen, cx - 6, cy - 6, cx - 6, cy + 6);
        g.FillRectangle(&iconBrush, cx - 6, cy - 4, 10, 3);
        g.FillRectangle(&iconBrush, cx - 6, cy + 1, 7, 3);
        break;
    case 1: // Center
        g.DrawLine(&iconPen, cx, cy - 7, cx, cy + 7);
        g.FillRectangle(&iconBrush, cx - 5, cy - 4, 10, 3);
        g.FillRectangle(&iconBrush, cx - 3, cy + 1, 7, 3);
        break;
    case 2: // Right
        g.DrawLine(&iconPen, cx + 6, cy - 6, cx + 6, cy + 6);
        g.FillRectangle(&iconBrush, cx - 4, cy - 4, 10, 3);
        g.FillRectangle(&iconBrush, cx - 1, cy + 1, 7, 3);
        break;
    case 3: // Justify Left
    case 4: // Justify Center
    case 5: // Justify Right
    case 6: // Justify Full
        // Full width lines
        g.FillRectangle(&iconBrush, cx - 6, cy - 5, 12, 2);
        g.FillRectangle(&iconBrush, cx - 6, cy - 1, 12, 2);
        // Last line based on justify type
        if (index == 3) g.FillRectangle(&iconBrush, cx - 6, cy + 3, 8, 2);
        else if (index == 4) g.FillRectangle(&iconBrush, cx - 4, cy + 3, 8, 2);
        else if (index == 5) g.FillRectangle(&iconBrush, cx - 2, cy + 3, 8, 2);
        else g.FillRectangle(&iconBrush, cx - 6, cy + 3, 12, 2);
        break;
    }
}

/*****************************************************************************
 * HitTestValue
 *****************************************************************************/
static ValueTarget HitTestValue(int x, int y) {
    POINT pt = {x, y};
    if (PtInRect(&g_sizeRect, pt)) return TARGET_SIZE;
    if (PtInRect(&g_trackingRect, pt)) return TARGET_TRACKING;
    if (PtInRect(&g_leadingRect, pt)) return TARGET_LEADING;
    if (PtInRect(&g_strokeWidthRect, pt)) return TARGET_STROKE_WIDTH;
    return TARGET_NONE;
}

/*****************************************************************************
 * HitTestAlign
 *****************************************************************************/
static int HitTestAlign(int x, int y) {
    POINT pt = {x, y};
    for (int i = 0; i < 7; i++) {
        if (PtInRect(&g_alignRects[i], pt)) return i;
    }
    return -1;
}

/*****************************************************************************
 * Mouse handlers
 *****************************************************************************/
static void HandleMouseDown(int x, int y) {
    POINT pt = {x, y};

    // Close button
    if (PtInRect(&g_closeRect, pt)) {
        g_result.cancelled = true;
        ShowWindow(g_hwnd, SW_HIDE);
        g_visible = false;
        return;
    }

    // Pin button
    if (PtInRect(&g_pinRect, pt)) {
        g_keepPanelOpen = !g_keepPanelOpen;
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }

    // Font box
    if (PtInRect(&g_fontBoxRect, pt) && g_textInfo.font[0]) {
        if (g_fontDropdownVisible) {
            HideFontDropdown();
        } else {
            RECT wndRect;
            GetWindowRect(g_hwnd, &wndRect);
            ShowFontDropdown();
        }
        return;
    }

    // Color boxes
    if (PtInRect(&g_fillColorRect, pt)) {
        HideFontDropdown();  // Close font dropdown if open
        RECT wndRect;
        GetWindowRect(g_hwnd, &wndRect);
        ShowColorPicker(false, wndRect.left + g_fillColorRect.left,
                        wndRect.top + g_fillColorRect.bottom + 5);
        return;
    }
    if (PtInRect(&g_strokeColorRect, pt)) {
        HideFontDropdown();  // Close font dropdown if open
        RECT wndRect;
        GetWindowRect(g_hwnd, &wndRect);
        ShowColorPicker(true, wndRect.left + g_strokeColorRect.left,
                        wndRect.top + g_strokeColorRect.bottom + 5);
        return;
    }

    // Align buttons
    int alignHit = HitTestAlign(x, y);
    if (alignHit >= 0) {
        g_textInfo.justify = (Justification)alignHit;
        ApplyJustification((Justification)alignHit);
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }

    // Value boxes - start drag
    ValueTarget target = HitTestValue(x, y);
    if (target != TARGET_NONE && !g_editMode) {
        g_dragging = true;
        g_dragTarget = target;
        g_dragStartX = x;
        g_dragStartValue = GetTargetValue(target);
        SetCapture(g_hwnd);
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static void HandleMouseUp(int x, int y) {
    if (g_dragging) {
        g_dragging = false;
        g_dragTarget = TARGET_NONE;
        ReleaseCapture();
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static void HandleMouseMove(int x, int y) {
    if (!g_dragging) return;

    int deltaX = x - g_dragStartX;
    float sensitivity = DRAG_CONFIGS[g_dragTarget].sensitivity;

    // Shift for fine adjustment
    if (GetKeyState(VK_SHIFT) & 0x8000) {
        sensitivity *= 0.1f;
    }

    float newValue = g_dragStartValue + deltaX * sensitivity;

    // Clamp
    newValue = max(DRAG_CONFIGS[g_dragTarget].minValue, newValue);
    newValue = min(DRAG_CONFIGS[g_dragTarget].maxValue, newValue);

    SetTargetValue(g_dragTarget, newValue);
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void HandleDoubleClick(int x, int y) {
    ValueTarget target = HitTestValue(x, y);
    if (target != TARGET_NONE) {
        EnterEditMode(target);
    }
}

/*****************************************************************************
 * EnterEditMode - Switch to keyboard input mode
 *****************************************************************************/
static void EnterEditMode(ValueTarget target) {
    g_editMode = true;
    g_editTarget = target;
    g_editText = FormatValue(target, GetTargetValue(target));

    // Remove "Auto" or suffix for editing
    if (target == TARGET_LEADING && GetTargetValue(target) == 0) {
        g_editText = L"0";
    }
    // Remove suffix
    size_t suffixPos = g_editText.find_last_not_of(L"0123456789.-");
    if (suffixPos != std::wstring::npos && suffixPos > 0) {
        g_editText = g_editText.substr(0, suffixPos);
    }

    g_editCursorPos = (int)g_editText.length();
    g_editSelectAll = true;

    // Remove WS_EX_NOACTIVATE to receive keyboard focus
    LONG exStyle = GetWindowLong(g_hwnd, GWL_EXSTYLE);
    SetWindowLong(g_hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_NOACTIVATE);

    SetFocus(g_hwnd);
    InvalidateRect(g_hwnd, NULL, FALSE);
}

/*****************************************************************************
 * ExitEditMode
 *****************************************************************************/
static void ExitEditMode(bool apply) {
    if (!g_editMode) return;

    if (apply && !g_editText.empty()) {
        float value = (float)_wtof(g_editText.c_str());
        value = max(DRAG_CONFIGS[g_editTarget].minValue, value);
        value = min(DRAG_CONFIGS[g_editTarget].maxValue, value);
        SetTargetValue(g_editTarget, value);
    }

    g_editMode = false;
    g_editTarget = TARGET_NONE;
    g_editText.clear();

    // Restore WS_EX_NOACTIVATE
    LONG exStyle = GetWindowLong(g_hwnd, GWL_EXSTYLE);
    SetWindowLong(g_hwnd, GWL_EXSTYLE, exStyle | WS_EX_NOACTIVATE);

    InvalidateRect(g_hwnd, NULL, FALSE);
}

/*****************************************************************************
 * Keyboard handlers
 *****************************************************************************/
static void HandleKeyDown(WPARAM key) {
    if (!g_editMode) {
        if (key == VK_ESCAPE) {
            g_result.cancelled = true;
            ShowWindow(g_hwnd, SW_HIDE);
            g_visible = false;
        }
        return;
    }

    switch (key) {
    case VK_RETURN:
        ExitEditMode(true);
        break;
    case VK_ESCAPE:
        ExitEditMode(false);
        break;
    case VK_BACK:
        if (g_editSelectAll) {
            g_editText.clear();
            g_editSelectAll = false;
        } else if (!g_editText.empty()) {
            g_editText.pop_back();
        }
        InvalidateRect(g_hwnd, NULL, FALSE);
        break;
    }
}

static void HandleChar(wchar_t ch) {
    if (!g_editMode) return;

    // Only allow digits, minus, and decimal point
    if ((ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'.') {
        if (g_editSelectAll) {
            g_editText.clear();
            g_editSelectAll = false;
        }
        g_editText += ch;
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

/*****************************************************************************
 * Value helpers
 *****************************************************************************/
static float GetTargetValue(ValueTarget target) {
    switch (target) {
    case TARGET_SIZE: return g_textInfo.fontSize;
    case TARGET_TRACKING: return g_textInfo.tracking;
    case TARGET_LEADING: return g_textInfo.leading;
    case TARGET_STROKE_WIDTH: return g_textInfo.strokeWidth;
    default: return 0;
    }
}

static void SetTargetValue(ValueTarget target, float value) {
    const char* propName = "";
    switch (target) {
    case TARGET_SIZE:
        g_textInfo.fontSize = value;
        propName = "fontSize";
        break;
    case TARGET_TRACKING:
        g_textInfo.tracking = value;
        propName = "tracking";
        break;
    case TARGET_LEADING:
        g_textInfo.leading = value;
        propName = "leading";
        break;
    case TARGET_STROKE_WIDTH:
        g_textInfo.strokeWidth = value;
        propName = "strokeWidth";
        break;
    default: return;
    }
    ApplyTextProperty(propName, value);
}

static std::wstring FormatValue(ValueTarget target, float value) {
    wchar_t buf[64];
    if (target == TARGET_LEADING && value == 0) {
        return L"Auto";
    }

    int decimals = DRAG_CONFIGS[target].decimals;
    const wchar_t* suffix = DRAG_CONFIGS[target].suffix;

    if (decimals == 0) {
        swprintf(buf, 64, L"%.0f%s", value, suffix);
    } else {
        swprintf(buf, 64, L"%.1f%s", value, suffix);
    }
    return buf;
}

/*****************************************************************************
 * ExtendScript execution (stub - actual implementation in SnapPlugin.cpp)
 *****************************************************************************/
static void ApplyTextProperty(const char* propName, float value) {
    // This will be called from SnapPlugin.cpp via the result
    g_result.applied = true;
}

static void ApplyTextColor(bool stroke, float r, float g, float b) {
    if (stroke) {
        g_textInfo.strokeColor[0] = r;
        g_textInfo.strokeColor[1] = g;
        g_textInfo.strokeColor[2] = b;
        g_textInfo.applyStroke = true;
    } else {
        g_textInfo.fillColor[0] = r;
        g_textInfo.fillColor[1] = g;
        g_textInfo.fillColor[2] = b;
        g_textInfo.applyFill = true;
    }
    g_result.applied = true;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void ApplyJustification(Justification just) {
    g_result.applied = true;
}

/*****************************************************************************
 * HSV <-> RGB Conversion
 *****************************************************************************/
static void HSVtoRGB(float h, float s, float v, float& r, float& g, float& b) {
    if (s == 0) {
        r = g = b = v;
        return;
    }
    h = fmodf(h, 360.0f);
    if (h < 0) h += 360.0f;
    h /= 60.0f;
    int i = (int)h;
    float f = h - i;
    float p = v * (1 - s);
    float q = v * (1 - s * f);
    float t = v * (1 - s * (1 - f));
    switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}

static void RGBtoHSV(float r, float g, float b, float& h, float& s, float& v) {
    float maxC = (std::max)({r, g, b});
    float minC = (std::min)({r, g, b});
    v = maxC;
    float delta = maxC - minC;
    if (maxC > 0) {
        s = delta / maxC;
    } else {
        s = 0;
        h = 0;
        return;
    }
    if (delta < 0.00001f) {
        h = 0;
        return;
    }
    if (r >= maxC) {
        h = (g - b) / delta;
    } else if (g >= maxC) {
        h = 2.0f + (b - r) / delta;
    } else {
        h = 4.0f + (r - g) / delta;
    }
    h *= 60.0f;
    if (h < 0) h += 360.0f;
}

static void UpdatePickerFromColor(float* color) {
    float h, s, v;
    RGBtoHSV(color[0], color[1], color[2], h, s, v);
    g_pickerHue = (int)h;
    g_pickerSaturation = s;
    g_pickerValue = v;
}

/*****************************************************************************
 * Color Picker
 *****************************************************************************/
void ShowColorPicker(bool forStroke, int x, int y) {
    if (!g_colorPickerHwnd) return;

    g_colorPickerForStroke = forStroke;
    g_colorPickerVisible = true;
    g_colorPickerDragMode = 0;

    // Initialize from current color
    float* color = forStroke ? g_textInfo.strokeColor : g_textInfo.fillColor;
    UpdatePickerFromColor(color);

    // Position picker
    SetWindowPos(g_colorPickerHwnd, HWND_TOPMOST, x, y,
                 COLOR_PICKER_WIDTH, COLOR_PICKER_HEIGHT, SWP_SHOWWINDOW);
    InvalidateRect(g_colorPickerHwnd, NULL, TRUE);
}

void HideColorPicker() {
    if (g_colorPickerHwnd) {
        ShowWindow(g_colorPickerHwnd, SW_HIDE);
        g_colorPickerVisible = false;
    }
}

bool IsColorPickerVisible() {
    return g_colorPickerVisible && g_colorPickerHwnd && IsWindowVisible(g_colorPickerHwnd);
}

static void DrawColorPicker(HDC hdc) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    // Background
    SolidBrush bgBrush(COLOR_BG);
    g.FillRectangle(&bgBrush, 0, 0, COLOR_PICKER_WIDTH, COLOR_PICKER_HEIGHT);

    // Border
    Pen borderPen(Color(255, 60, 60, 70), 1);
    g.DrawRectangle(&borderPen, 0, 0, COLOR_PICKER_WIDTH - 1, COLOR_PICKER_HEIGHT - 1);

    int padding = 10;

    // SV Square (Saturation-Value)
    g_svRect = {padding, padding, padding + SV_SIZE, padding + SV_SIZE};

    // Draw SV gradient
    for (int y = 0; y < SV_SIZE; y++) {
        for (int x = 0; x < SV_SIZE; x++) {
            float s = (float)x / SV_SIZE;
            float v = 1.0f - (float)y / SV_SIZE;
            float r, gVal, b;
            HSVtoRGB((float)g_pickerHue, s, v, r, gVal, b);
            SolidBrush pixelBrush(Color(255, (BYTE)(r * 255), (BYTE)(gVal * 255), (BYTE)(b * 255)));
            g.FillRectangle(&pixelBrush, padding + x, padding + y, 1, 1);
        }
    }

    // SV cursor
    int svCursorX = padding + (int)(g_pickerSaturation * SV_SIZE);
    int svCursorY = padding + (int)((1.0f - g_pickerValue) * SV_SIZE);
    Pen cursorPenW(Color(255, 255, 255, 255), 2);
    Pen cursorPenB(Color(255, 0, 0, 0), 1);
    g.DrawEllipse(&cursorPenW, svCursorX - 5, svCursorY - 5, 10, 10);
    g.DrawEllipse(&cursorPenB, svCursorX - 6, svCursorY - 6, 12, 12);

    // Hue Bar
    int hueX = padding + SV_SIZE + 10;
    g_hueRect = {hueX, padding, hueX + HUE_BAR_WIDTH, padding + SV_SIZE};

    for (int y = 0; y < SV_SIZE; y++) {
        float hue = (float)y / SV_SIZE * 360.0f;
        float r, gVal, b;
        HSVtoRGB(hue, 1.0f, 1.0f, r, gVal, b);
        SolidBrush hueBrush(Color(255, (BYTE)(r * 255), (BYTE)(gVal * 255), (BYTE)(b * 255)));
        g.FillRectangle(&hueBrush, hueX, padding + y, HUE_BAR_WIDTH, 1);
    }

    // Hue cursor
    int hueCursorY = padding + (int)((float)g_pickerHue / 360.0f * SV_SIZE);
    g.DrawRectangle(&cursorPenW, hueX - 2, hueCursorY - 2, HUE_BAR_WIDTH + 3, 4);
}

static LRESULT CALLBACK ColorPickerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawColorPicker(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        POINT pt = {x, y};

        if (PtInRect(&g_svRect, pt)) {
            g_colorPickerDragMode = 2;
            SetCapture(hwnd);
        } else if (PtInRect(&g_hueRect, pt)) {
            g_colorPickerDragMode = 1;
            SetCapture(hwnd);
        }
        // Fall through to update
    }

    case WM_MOUSEMOVE: {
        if (g_colorPickerDragMode == 0) return 0;

        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        if (g_colorPickerDragMode == 1) {
            // Hue bar
            int relY = y - g_hueRect.top;
            relY = (std::max)(0, (std::min)(relY, SV_SIZE - 1));
            g_pickerHue = (int)((float)relY / SV_SIZE * 360.0f);
        } else if (g_colorPickerDragMode == 2) {
            // SV square
            int relX = x - g_svRect.left;
            int relY = y - g_svRect.top;
            relX = (std::max)(0, (std::min)(relX, SV_SIZE - 1));
            relY = (std::max)(0, (std::min)(relY, SV_SIZE - 1));
            g_pickerSaturation = (float)relX / SV_SIZE;
            g_pickerValue = 1.0f - (float)relY / SV_SIZE;
        }

        // Update color
        float r, gVal, b;
        HSVtoRGB((float)g_pickerHue, g_pickerSaturation, g_pickerValue, r, gVal, b);
        ApplyTextColor(g_colorPickerForStroke, r, gVal, b);

        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_LBUTTONUP:
        if (g_colorPickerDragMode != 0) {
            g_colorPickerDragMode = 0;
            ReleaseCapture();
        }
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) {
            HideColorPicker();
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            HideColorPicker();
        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/*****************************************************************************
 * Font Dropdown Helpers
 *****************************************************************************/
static int CALLBACK EnumFontFamProc(const LOGFONTW* lpelf, const TEXTMETRICW* lpntm,
                                     DWORD FontType, LPARAM lParam) {
    (void)lpntm;
    (void)FontType;
    (void)lParam;
    // Add font if not already in list
    std::wstring fontName(lpelf->lfFaceName);
    if (fontName[0] != L'@') {  // Skip vertical fonts
        auto it = std::find(g_fontList.begin(), g_fontList.end(), fontName);
        if (it == g_fontList.end()) {
            g_fontList.push_back(fontName);
        }
    }
    return 1;
}

static void PopulateFontList() {
    g_fontList.clear();
    HDC hdc = GetDC(NULL);
    LOGFONTW lf = {0};
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfFaceName[0] = L'\0';
    EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontFamProc, 0, 0);
    ReleaseDC(NULL, hdc);

    // Sort alphabetically
    std::sort(g_fontList.begin(), g_fontList.end());
}

static void ShowFontDropdown() {
    if (!g_hwnd) return;

    // Create dropdown window if needed
    if (!g_fontDropdownHwnd) {
        g_fontDropdownHwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"TextUIFontDropdown",
            L"Font Selector",
            WS_POPUP | WS_VSCROLL,
            0, 0, 200, FONT_ITEM_HEIGHT * FONT_DROPDOWN_MAX_ITEMS,
            NULL, NULL, GetModuleHandle(NULL), NULL
        );
    }

    // Position below font box
    RECT wndRect;
    GetWindowRect(g_hwnd, &wndRect);
    int dropX = wndRect.left + g_fontBoxRect.left;
    int dropY = wndRect.top + g_fontBoxRect.bottom;
    int dropH = FONT_ITEM_HEIGHT * (std::min)((int)g_fontList.size(), FONT_DROPDOWN_MAX_ITEMS);

    SetWindowPos(g_fontDropdownHwnd, HWND_TOPMOST, dropX, dropY, 200, dropH, SWP_SHOWWINDOW);

    // Find current font in list for scroll position
    g_fontScrollOffset = 0;
    for (size_t i = 0; i < g_fontList.size(); i++) {
        if (g_fontList[i] == g_textInfo.font) {
            g_fontScrollOffset = (std::max)(0, (int)i - FONT_DROPDOWN_MAX_ITEMS / 2);
            break;
        }
    }

    // Update scrollbar
    SCROLLINFO si = {0};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = (int)g_fontList.size() - 1;
    si.nPage = FONT_DROPDOWN_MAX_ITEMS;
    si.nPos = g_fontScrollOffset;
    SetScrollInfo(g_fontDropdownHwnd, SB_VERT, &si, TRUE);

    g_fontDropdownVisible = true;
    g_fontHoverIndex = -1;
    InvalidateRect(g_fontDropdownHwnd, NULL, TRUE);
}

static void HideFontDropdown() {
    if (g_fontDropdownHwnd) {
        ShowWindow(g_fontDropdownHwnd, SW_HIDE);
    }
    g_fontDropdownVisible = false;
}

static void DrawFontDropdown(HDC hdc) {
    RECT clientRect;
    GetClientRect(g_fontDropdownHwnd, &clientRect);

    Graphics g(hdc);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Background
    SolidBrush bgBrush(COLOR_BG);
    g.FillRectangle(&bgBrush, 0, 0, clientRect.right, clientRect.bottom);

    // Border
    Pen borderPen(Color(255, 60, 60, 70), 1);
    g.DrawRectangle(&borderPen, 0, 0, clientRect.right - 1, clientRect.bottom - 1);

    FontFamily fontFamily(L"Segoe UI");
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush hoverBrush(COLOR_VALUE_HOVER);
    SolidBrush selectedBrush(COLOR_ALIGN_ACTIVE);

    int visibleItems = (std::min)((int)g_fontList.size() - g_fontScrollOffset, FONT_DROPDOWN_MAX_ITEMS);

    for (int i = 0; i < visibleItems; i++) {
        int fontIndex = g_fontScrollOffset + i;
        if (fontIndex >= (int)g_fontList.size()) break;

        const std::wstring& fontName = g_fontList[fontIndex];
        int itemY = i * FONT_ITEM_HEIGHT;

        // Highlight current font or hover
        bool isCurrent = (fontName == g_textInfo.font);
        bool isHover = (fontIndex == g_fontHoverIndex);

        if (isCurrent) {
            g.FillRectangle(&selectedBrush, 1, itemY, clientRect.right - 2, FONT_ITEM_HEIGHT);
        } else if (isHover) {
            g.FillRectangle(&hoverBrush, 1, itemY, clientRect.right - 2, FONT_ITEM_HEIGHT);
        }

        // Draw font name in its own font
        Font itemFont(&fontFamily, 11, FontStyleRegular, UnitPixel);
        g.DrawString(fontName.c_str(), -1, &itemFont,
                     PointF(8, (REAL)(itemY + 4)), &textBrush);
    }
}

static void ApplyFont(const wchar_t* fontName) {
    wcscpy_s(g_textInfo.font, 128, fontName);
    g_result.applied = true;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static LRESULT CALLBACK FontDropdownWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawFontDropdown(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int y = GET_Y_LPARAM(lParam);
        int newHover = g_fontScrollOffset + y / FONT_ITEM_HEIGHT;
        if (newHover != g_fontHoverIndex && newHover < (int)g_fontList.size()) {
            g_fontHoverIndex = newHover;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int y = GET_Y_LPARAM(lParam);
        int clickIndex = g_fontScrollOffset + y / FONT_ITEM_HEIGHT;
        if (clickIndex >= 0 && clickIndex < (int)g_fontList.size()) {
            ApplyFont(g_fontList[clickIndex].c_str());
            HideFontDropdown();
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int scroll = (delta > 0) ? -3 : 3;
        g_fontScrollOffset = (std::max)(0, (std::min)(g_fontScrollOffset + scroll,
                                         (int)g_fontList.size() - FONT_DROPDOWN_MAX_ITEMS));
        SetScrollPos(hwnd, SB_VERT, g_fontScrollOffset, TRUE);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_VSCROLL: {
        int action = LOWORD(wParam);
        int newPos = g_fontScrollOffset;

        switch (action) {
        case SB_LINEUP: newPos--; break;
        case SB_LINEDOWN: newPos++; break;
        case SB_PAGEUP: newPos -= FONT_DROPDOWN_MAX_ITEMS; break;
        case SB_PAGEDOWN: newPos += FONT_DROPDOWN_MAX_ITEMS; break;
        case SB_THUMBTRACK: newPos = HIWORD(wParam); break;
        }

        newPos = (std::max)(0, (std::min)(newPos, (int)g_fontList.size() - FONT_DROPDOWN_MAX_ITEMS));
        if (newPos != g_fontScrollOffset) {
            g_fontScrollOffset = newPos;
            SetScrollPos(hwnd, SB_VERT, g_fontScrollOffset, TRUE);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) {
            HideFontDropdown();
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            HideFontDropdown();
        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace TextUI

#else // macOS stubs

namespace TextUI {

void Initialize() {}
void Shutdown() {}
void ShowPanel(int x, int y) { (void)x; (void)y; }
TextResult HidePanel() { return TextResult(); }
bool IsVisible() { return false; }
void UpdateHover(int mouseX, int mouseY) { (void)mouseX; (void)mouseY; }
TextResult GetResult() { return TextResult(); }
void SetTextInfo(const wchar_t* jsonInfo) { (void)jsonInfo; }
void ShowColorPicker(bool forStroke, int x, int y) { (void)forStroke; (void)x; (void)y; }
void HideColorPicker() {}
bool IsColorPickerVisible() { return false; }

} // namespace TextUI

#endif // MSWindows
