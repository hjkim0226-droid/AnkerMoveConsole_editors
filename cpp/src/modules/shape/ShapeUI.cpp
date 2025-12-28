/*****************************************************************************
 * ShapeUI.cpp
 *
 * Native Windows UI implementation for Shape Layer Module
 * Uses Win32 + GDI+ for shape layer property editing
 *****************************************************************************/

#include "ShapeUI.h"
#include "GdiPlusIncludes.h"

#ifdef MSWindows
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <string>
#include <cmath>
#include <vector>
#include <algorithm>

using namespace Gdiplus;

// External functions from SnapPlugin.cpp
extern float GetModuleScaleFactor(const char* moduleName);
extern void ApplyShapePropertyValue(const char* propName, float value);
extern void ApplyShapeColorValue(bool stroke, float r, float g, float b);
extern void ApplyShapeSizeValue(float w, float h);
extern void ApplyShapeAnchorValue(float x, float y);
extern void AddShapePathOperation(int opType);

namespace ShapeUI {

// ============================================================================
// Constants
// ============================================================================

// Window dimensions
static const int WINDOW_WIDTH = 300;
static const int WINDOW_HEIGHT = 320;

// Scale factor for this module
static float g_scaleFactor = 1.0f;

// Helper functions for scaling
inline int Scaled(int baseValue) {
    return (int)(baseValue * g_scaleFactor);
}

inline int InverseScaled(int screenValue) {
    return (int)(screenValue / g_scaleFactor);
}

// Layout constants
static const int HEADER_HEIGHT = 28;
static const int SECTION_HEADER_HEIGHT = 24;
static const int ROW_HEIGHT = 26;
static const int ROW_SPACING = 4;
static const int PADDING = 8;
static const int LABEL_WIDTH = 60;
static const int VALUE_BOX_WIDTH = 70;
static const int VALUE_BOX_HEIGHT = 22;
static const int COLOR_BOX_SIZE = 22;
static const int TOGGLE_SIZE = 18;
static const int PIN_BUTTON_SIZE = 18;

// Anchor grid constants
static const int ANCHOR_GRID_SIZE = 90;
static const int ANCHOR_CELL_SIZE = 26;

// Colors (same as TextUI for consistency)
static const Color COLOR_BG(240, 28, 28, 32);
static const Color COLOR_HEADER_BG(255, 40, 40, 48);
static const Color COLOR_SECTION_BG(255, 35, 35, 42);
static const Color COLOR_SECTION_HEADER(255, 45, 45, 55);
static const Color COLOR_VALUE_BG(255, 45, 45, 55);
static const Color COLOR_VALUE_HOVER(255, 55, 70, 85);
static const Color COLOR_VALUE_DRAG(255, 50, 90, 130);
static const Color COLOR_VALUE_EDIT(255, 35, 55, 80);
static const Color COLOR_VALUE_BORDER(255, 74, 158, 255);
static const Color COLOR_TEXT(255, 220, 220, 220);
static const Color COLOR_TEXT_DIM(255, 140, 140, 140);
static const Color COLOR_PIN_ACTIVE(255, 255, 200, 0);
static const Color COLOR_BORDER(255, 60, 60, 70);
static const Color COLOR_HOVER(255, 60, 80, 100);
static const Color COLOR_ACCENT(255, 74, 158, 255);
static const Color COLOR_TOGGLE_ON(255, 74, 200, 120);
static const Color COLOR_TOGGLE_OFF(255, 80, 80, 90);

// Drag sensitivity configuration
struct DragConfig {
    float sensitivity;
    float minValue;
    float maxValue;
    int decimals;
    const wchar_t* suffix;
};

static const DragConfig DRAG_CONFIGS[] = {
    {0.2f, 0.0f, 100.0f, 1, L"px"},     // STROKE_WIDTH
    {1.0f, 0.0f, 100.0f, 0, L"%"},      // OPACITY
    {1.0f, 1.0f, 9999.0f, 0, L""},      // SIZE_W
    {1.0f, 1.0f, 9999.0f, 0, L""},      // SIZE_H
    {0.5f, 0.0f, 999.0f, 1, L""}        // ROUNDNESS
};

// Path operation names
static const wchar_t* PATH_OP_NAMES[] = {
    L"Trim Paths",
    L"Repeater",
    L"Wiggle Path",
    L"Wiggle Transform",
    L"Offset Paths",
    L"Round Corners",
    L"Zig Zag",
    L"Twist",
    L"Pucker & Bloat"
};

// ============================================================================
// Global State
// ============================================================================

static HWND g_hwnd = NULL;
static HWND g_colorPickerHwnd = NULL;
static HWND g_anchorGridHwnd = NULL;
static bool g_visible = false;
static bool g_needsRefresh = false;
static ULONG_PTR g_gdiplusToken = 0;
static ShapeInfo g_shapeInfo = {};
static ShapeResult g_result;
static bool g_keepPanelOpen = false;

// Section state (accordion)
static bool g_sectionExpanded[SECTION_COUNT] = {true, false, false};

// Hit test rectangles
static RECT g_pinRect;
static RECT g_fillColorRect;
static RECT g_strokeColorRect;
static RECT g_fillToggleRect;
static RECT g_strokeToggleRect;
static RECT g_strokeWidthRect;
static RECT g_opacityRect;
static RECT g_sizeWRect;
static RECT g_sizeHRect;
static RECT g_sizeLinkRect;
static RECT g_roundnessRect;
static RECT g_sectionHeaderRects[SECTION_COUNT];
static RECT g_pathOpRects[PATH_OP_COUNT];

// Hover state
static ValueTarget g_hoverTarget = TARGET_NONE;
static int g_hoverSection = -1;
static int g_hoverPathOp = -1;
static bool g_pinHover = false;
static bool g_fillColorHover = false;
static bool g_strokeColorHover = false;
static bool g_fillToggleHover = false;
static bool g_strokeToggleHover = false;
static bool g_sizeLinkHover = false;

// Drag state
static bool g_dragging = false;
static ValueTarget g_dragTarget = TARGET_NONE;
static int g_dragStartX = 0;
static float g_dragStartValue = 0;

// Window dragging state
static bool g_windowDragging = false;
static POINT g_windowDragOffset = {0, 0};

// Edit state (inline editing)
static bool g_editMode = false;
static ValueTarget g_editTarget = TARGET_NONE;
static std::wstring g_editText;
static int g_editCursorPos = 0;
static DWORD g_lastClickTime = 0;
static ValueTarget g_lastClickTarget = TARGET_NONE;

// Color picker state
static bool g_colorPickerForStroke = false;
static bool g_colorPickerOpen = false;
static float g_pickerColor[3] = {1, 1, 1};

// Anchor grid state
static bool g_anchorGridVisible = false;
static int g_anchorHoverIndex = -1;  // 0-8 for 3x3 grid

// ============================================================================
// Forward Declarations
// ============================================================================

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK ColorPickerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK AnchorGridWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void Draw(HDC hdc);
static void DrawSection(Graphics& g, Section section, int& y);
static void DrawAppearanceSection(Graphics& g, int y);
static void DrawTransformSection(Graphics& g, int y);
static void DrawPathOpsSection(Graphics& g, int y);
static void HandleMouseDown(int x, int y);
static void HandleMouseUp(int x, int y);
static void HandleMouseMove(int x, int y);
static void HandleDoubleClick(int x, int y);
static float GetTargetValue(ValueTarget target);
static void SetTargetValue(ValueTarget target, float value);
static void UpdateLayout();

// ============================================================================
// Public API
// ============================================================================

void Initialize() {
    if (g_gdiplusToken == 0) {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    }

    // Register window class for main panel
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"ShapeUIPanel";
    RegisterClassExW(&wc);

    // Register window class for color picker
    wc.lpfnWndProc = ColorPickerWndProc;
    wc.lpszClassName = L"ShapeUIColorPicker";
    RegisterClassExW(&wc);

    // Register window class for anchor grid
    wc.lpfnWndProc = AnchorGridWndProc;
    wc.lpszClassName = L"ShapeUIAnchorGrid";
    RegisterClassExW(&wc);
}

void Shutdown() {
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }
    if (g_colorPickerHwnd) {
        DestroyWindow(g_colorPickerHwnd);
        g_colorPickerHwnd = NULL;
    }
    if (g_anchorGridHwnd) {
        DestroyWindow(g_anchorGridHwnd);
        g_anchorGridHwnd = NULL;
    }
    if (g_gdiplusToken != 0) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

void ShowPanel(int x, int y) {
    g_scaleFactor = GetModuleScaleFactor("shape");

    int scaledWidth = Scaled(WINDOW_WIDTH);
    int scaledHeight = Scaled(WINDOW_HEIGHT);

    // Adjust position to keep on screen
    HMONITOR hMon = MonitorFromPoint({x, y}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfo(hMon, &mi);

    if (x + scaledWidth > mi.rcWork.right)
        x = mi.rcWork.right - scaledWidth;
    if (y + scaledHeight > mi.rcWork.bottom)
        y = mi.rcWork.bottom - scaledHeight;
    if (x < mi.rcWork.left)
        x = mi.rcWork.left;
    if (y < mi.rcWork.top)
        y = mi.rcWork.top;

    if (!g_hwnd) {
        g_hwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            L"ShapeUIPanel",
            L"Shape",
            WS_POPUP,
            x, y, scaledWidth, scaledHeight,
            NULL, NULL, GetModuleHandle(NULL), NULL);
    } else {
        SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, scaledWidth, scaledHeight, SWP_SHOWWINDOW);
    }

    UpdateLayout();
    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    g_visible = true;
    g_result = {};
}

ShapeResult HidePanel() {
    if (g_hwnd) {
        ShowWindow(g_hwnd, SW_HIDE);
    }
    HideColorPicker();
    HideAnchorGrid();
    g_visible = false;
    return g_result;
}

bool IsVisible() { return g_visible; }

void UpdateHover(int mouseX, int mouseY) {
    if (!g_hwnd || !g_visible) return;

    POINT pt = {mouseX, mouseY};
    ScreenToClient(g_hwnd, &pt);
    HandleMouseMove(InverseScaled(pt.x), InverseScaled(pt.y));
}

ShapeResult GetResult() { return g_result; }

void SetShapeInfo(const wchar_t* jsonInfo) {
    if (!jsonInfo || jsonInfo[0] == L'\0') return;

    std::wstring json(jsonInfo);

    // Simple JSON parsing
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

    auto getColorArray = [&json](const wchar_t* key, float* outRGB) {
        std::wstring search = std::wstring(L"\"") + key + L"\":[";
        size_t pos = json.find(search);
        if (pos == std::wstring::npos) return;
        pos += search.length();
        outRGB[0] = (float)_wtof(json.c_str() + pos);
        size_t comma1 = json.find(L',', pos);
        if (comma1 == std::wstring::npos) return;
        pos = comma1 + 1;
        outRGB[1] = (float)_wtof(json.c_str() + pos);
        size_t comma2 = json.find(L',', pos);
        if (comma2 == std::wstring::npos) return;
        pos = comma2 + 1;
        outRGB[2] = (float)_wtof(json.c_str() + pos);
    };

    getString(L"layerName", g_shapeInfo.layerName, 256);
    getString(L"shapeName", g_shapeInfo.shapeName, 128);
    getString(L"shapeType", g_shapeInfo.shapeType, 64);

    g_shapeInfo.hasFill = getBool(L"hasFill");
    g_shapeInfo.hasStroke = getBool(L"hasStroke");
    g_shapeInfo.strokeWidth = getFloat(L"strokeWidth");
    g_shapeInfo.opacity = getFloat(L"opacity");
    g_shapeInfo.sizeW = getFloat(L"sizeW");
    g_shapeInfo.sizeH = getFloat(L"sizeH");
    g_shapeInfo.roundness = getFloat(L"roundness");
    g_shapeInfo.anchorX = getFloat(L"anchorX");
    g_shapeInfo.anchorY = getFloat(L"anchorY");
    g_shapeInfo.isParametric = getBool(L"isParametric");
    g_shapeInfo.sizeLinkEnabled = getBool(L"sizeLink");

    getColorArray(L"fillColor", g_shapeInfo.fillColor);
    getColorArray(L"strokeColor", g_shapeInfo.strokeColor);

    g_shapeInfo.boundsLeft = getFloat(L"boundsLeft");
    g_shapeInfo.boundsTop = getFloat(L"boundsTop");
    g_shapeInfo.boundsWidth = getFloat(L"boundsWidth");
    g_shapeInfo.boundsHeight = getFloat(L"boundsHeight");

    InvalidateRect(g_hwnd, NULL, FALSE);
}

bool NeedsRefresh() {
    if (g_needsRefresh) {
        g_needsRefresh = false;
        return true;
    }
    return false;
}

void ShowColorPicker(bool forStroke, int x, int y) {
    g_colorPickerForStroke = forStroke;
    g_colorPickerOpen = true;

    float* color = forStroke ? g_shapeInfo.strokeColor : g_shapeInfo.fillColor;
    g_pickerColor[0] = color[0];
    g_pickerColor[1] = color[1];
    g_pickerColor[2] = color[2];

    int pickerWidth = Scaled(200);
    int pickerHeight = Scaled(220);

    if (!g_colorPickerHwnd) {
        g_colorPickerHwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            L"ShapeUIColorPicker",
            L"Color",
            WS_POPUP,
            x, y, pickerWidth, pickerHeight,
            NULL, NULL, GetModuleHandle(NULL), NULL);
    } else {
        SetWindowPos(g_colorPickerHwnd, HWND_TOPMOST, x, y, pickerWidth, pickerHeight, SWP_SHOWWINDOW);
    }

    ShowWindow(g_colorPickerHwnd, SW_SHOWNOACTIVATE);
}

void HideColorPicker() {
    if (g_colorPickerHwnd) {
        ShowWindow(g_colorPickerHwnd, SW_HIDE);
    }
    g_colorPickerOpen = false;
}

bool IsColorPickerVisible() { return g_colorPickerOpen; }

void ShowAnchorGrid(int x, int y) {
    int gridSize = Scaled(ANCHOR_GRID_SIZE);

    // Center on cursor
    x -= gridSize / 2;
    y -= gridSize / 2;

    if (!g_anchorGridHwnd) {
        g_anchorGridHwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            L"ShapeUIAnchorGrid",
            L"Anchor",
            WS_POPUP,
            x, y, gridSize, gridSize,
            NULL, NULL, GetModuleHandle(NULL), NULL);
    } else {
        SetWindowPos(g_anchorGridHwnd, HWND_TOPMOST, x, y, gridSize, gridSize, SWP_SHOWWINDOW);
    }

    ShowWindow(g_anchorGridHwnd, SW_SHOWNOACTIVATE);
    g_anchorGridVisible = true;
}

void HideAnchorGrid() {
    if (g_anchorGridHwnd) {
        ShowWindow(g_anchorGridHwnd, SW_HIDE);
    }
    g_anchorGridVisible = false;
}

bool IsAnchorGridVisible() { return g_anchorGridVisible; }

void ToggleSection(Section section) {
    if (section >= 0 && section < SECTION_COUNT) {
        g_sectionExpanded[section] = !g_sectionExpanded[section];
        UpdateLayout();
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

bool IsSectionExpanded(Section section) {
    if (section >= 0 && section < SECTION_COUNT) {
        return g_sectionExpanded[section];
    }
    return false;
}

// ============================================================================
// Layout Calculation
// ============================================================================

static void UpdateLayout() {
    int y = HEADER_HEIGHT + PADDING;
    int contentWidth = WINDOW_WIDTH - PADDING * 2;

    for (int s = 0; s < SECTION_COUNT; s++) {
        // Section header rect
        g_sectionHeaderRects[s] = {
            PADDING, y,
            PADDING + contentWidth, y + SECTION_HEADER_HEIGHT
        };
        y += SECTION_HEADER_HEIGHT + 2;

        if (g_sectionExpanded[s]) {
            // Calculate section content height based on section type
            int contentHeight = 0;
            switch (s) {
                case SECTION_APPEARANCE:
                    contentHeight = (ROW_HEIGHT + ROW_SPACING) * 3;
                    break;
                case SECTION_TRANSFORM:
                    contentHeight = (ROW_HEIGHT + ROW_SPACING) * 3;
                    break;
                case SECTION_PATH_OPS:
                    contentHeight = (ROW_HEIGHT + ROW_SPACING) * 3;
                    break;
            }
            y += contentHeight + PADDING;
        }
    }

    // Update window height
    int newHeight = y + PADDING;
    if (g_hwnd) {
        RECT rc;
        GetWindowRect(g_hwnd, &rc);
        SetWindowPos(g_hwnd, NULL, 0, 0, Scaled(WINDOW_WIDTH), Scaled(newHeight),
                     SWP_NOMOVE | SWP_NOZORDER);
    }
}

// ============================================================================
// Drawing
// ============================================================================

static void Draw(HDC hdc) {
    RECT rect;
    GetClientRect(g_hwnd, &rect);
    int width = rect.right;
    int height = rect.bottom;

    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Scale transform
    graphics.ScaleTransform(g_scaleFactor, g_scaleFactor);
    int logicalWidth = InverseScaled(width);
    int logicalHeight = InverseScaled(height);

    // Background
    SolidBrush bgBrush(COLOR_BG);
    graphics.FillRectangle(&bgBrush, 0, 0, logicalWidth, logicalHeight);

    // Header
    SolidBrush headerBrush(COLOR_HEADER_BG);
    graphics.FillRectangle(&headerBrush, 0, 0, logicalWidth, HEADER_HEIGHT);

    // Title
    FontFamily fontFamily(L"Segoe UI");
    Font titleFont(&fontFamily, 10, FontStyleBold, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);

    // Layer/Shape name
    std::wstring title = L"Shape";
    if (g_shapeInfo.shapeName[0] != L'\0') {
        title = g_shapeInfo.shapeName;
    }
    graphics.DrawString(title.c_str(), -1, &titleFont,
                        PointF((float)PADDING, (float)(HEADER_HEIGHT - 18) / 2 + 2),
                        &textBrush);

    // Pin button
    int pinX = logicalWidth - PADDING - PIN_BUTTON_SIZE;
    int pinY = (HEADER_HEIGHT - PIN_BUTTON_SIZE) / 2;
    g_pinRect = {pinX, pinY, pinX + PIN_BUTTON_SIZE, pinY + PIN_BUTTON_SIZE};

    SolidBrush pinBrush(g_keepPanelOpen ? COLOR_PIN_ACTIVE :
                        (g_pinHover ? COLOR_HOVER : COLOR_SECTION_BG));
    graphics.FillEllipse(&pinBrush, pinX, pinY, PIN_BUTTON_SIZE, PIN_BUTTON_SIZE);

    // Pin icon
    Pen pinPen(g_keepPanelOpen ? Color(255, 40, 40, 40) : COLOR_TEXT_DIM, 1.5f);
    float pcx = pinX + PIN_BUTTON_SIZE / 2.0f;
    float pcy = pinY + PIN_BUTTON_SIZE / 2.0f;
    graphics.DrawEllipse(&pinPen, pcx - 3, pcy - 4, 6, 6);
    graphics.DrawLine(&pinPen, pcx, pcy + 2, pcx, pcy + 6);

    // Draw sections
    int y = HEADER_HEIGHT + PADDING;
    for (int s = 0; s < SECTION_COUNT; s++) {
        DrawSection(graphics, (Section)s, y);
    }

    // Border
    Pen borderPen(COLOR_BORDER, 1);
    graphics.DrawRectangle(&borderPen, 0, 0, logicalWidth - 1, logicalHeight - 1);
}

static void DrawSection(Graphics& g, Section section, int& y) {
    int contentWidth = WINDOW_WIDTH - PADDING * 2;
    bool expanded = g_sectionExpanded[section];
    bool hovered = (g_hoverSection == section);

    // Section header
    const wchar_t* sectionNames[] = {L"Appearance", L"Transform", L"Path Operations"};

    SolidBrush headerBrush(hovered ? COLOR_HOVER : COLOR_SECTION_HEADER);
    g.FillRectangle(&headerBrush, PADDING, y, contentWidth, SECTION_HEADER_HEIGHT);

    // Arrow
    FontFamily fontFamily(L"Segoe UI");
    Font arrowFont(&fontFamily, 10, FontStyleBold, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    g.DrawString(expanded ? L"\x25BC" : L"\x25B6", -1, &arrowFont,
                 PointF((float)PADDING + 6, (float)y + 5), &textBrush);

    // Section name
    Font sectionFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    g.DrawString(sectionNames[section], -1, &sectionFont,
                 PointF((float)PADDING + 22, (float)y + 5), &textBrush);

    g_sectionHeaderRects[section] = {
        PADDING, y, PADDING + contentWidth, y + SECTION_HEADER_HEIGHT
    };
    y += SECTION_HEADER_HEIGHT + 2;

    if (!expanded) return;

    // Section content
    switch (section) {
        case SECTION_APPEARANCE:
            DrawAppearanceSection(g, y);
            y += (ROW_HEIGHT + ROW_SPACING) * 3 + PADDING;
            break;
        case SECTION_TRANSFORM:
            DrawTransformSection(g, y);
            y += (ROW_HEIGHT + ROW_SPACING) * 3 + PADDING;
            break;
        case SECTION_PATH_OPS:
            DrawPathOpsSection(g, y);
            y += (ROW_HEIGHT + ROW_SPACING) * 3 + PADDING;
            break;
    }
}

static void DrawAppearanceSection(Graphics& g, int y) {
    FontFamily fontFamily(L"Segoe UI");
    Font labelFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
    Font valueFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);

    int rowY = y;
    int labelX = PADDING + 4;
    int valueX = PADDING + LABEL_WIDTH;

    // Row 1: Fill
    g.DrawString(L"Fill", -1, &labelFont, PointF((float)labelX, (float)rowY + 4), &dimBrush);

    // Fill color box
    int colorX = valueX;
    SolidBrush fillBrush(Color(255,
        (BYTE)(g_shapeInfo.fillColor[0] * 255),
        (BYTE)(g_shapeInfo.fillColor[1] * 255),
        (BYTE)(g_shapeInfo.fillColor[2] * 255)));
    g.FillRectangle(&fillBrush, colorX, rowY, COLOR_BOX_SIZE, COLOR_BOX_SIZE);
    if (g_fillColorHover) {
        Pen hoverPen(COLOR_ACCENT, 2);
        g.DrawRectangle(&hoverPen, colorX, rowY, COLOR_BOX_SIZE, COLOR_BOX_SIZE);
    }
    g_fillColorRect = {colorX, rowY, colorX + COLOR_BOX_SIZE, rowY + COLOR_BOX_SIZE};

    // Fill toggle
    int toggleX = colorX + COLOR_BOX_SIZE + 8;
    SolidBrush toggleBrush(g_shapeInfo.hasFill ? COLOR_TOGGLE_ON : COLOR_TOGGLE_OFF);
    g.FillEllipse(&toggleBrush, toggleX, rowY + 2, TOGGLE_SIZE, TOGGLE_SIZE);
    g_fillToggleRect = {toggleX, rowY + 2, toggleX + TOGGLE_SIZE, rowY + 2 + TOGGLE_SIZE};

    rowY += ROW_HEIGHT + ROW_SPACING;

    // Row 2: Stroke
    g.DrawString(L"Stroke", -1, &labelFont, PointF((float)labelX, (float)rowY + 4), &dimBrush);

    // Stroke color box
    SolidBrush strokeBrush(Color(255,
        (BYTE)(g_shapeInfo.strokeColor[0] * 255),
        (BYTE)(g_shapeInfo.strokeColor[1] * 255),
        (BYTE)(g_shapeInfo.strokeColor[2] * 255)));
    g.FillRectangle(&strokeBrush, colorX, rowY, COLOR_BOX_SIZE, COLOR_BOX_SIZE);
    if (g_strokeColorHover) {
        Pen hoverPen(COLOR_ACCENT, 2);
        g.DrawRectangle(&hoverPen, colorX, rowY, COLOR_BOX_SIZE, COLOR_BOX_SIZE);
    }
    g_strokeColorRect = {colorX, rowY, colorX + COLOR_BOX_SIZE, rowY + COLOR_BOX_SIZE};

    // Stroke toggle
    SolidBrush strokeToggleBrush(g_shapeInfo.hasStroke ? COLOR_TOGGLE_ON : COLOR_TOGGLE_OFF);
    g.FillEllipse(&strokeToggleBrush, toggleX, rowY + 2, TOGGLE_SIZE, TOGGLE_SIZE);
    g_strokeToggleRect = {toggleX, rowY + 2, toggleX + TOGGLE_SIZE, rowY + 2 + TOGGLE_SIZE};

    // Stroke width
    int widthX = toggleX + TOGGLE_SIZE + 16;
    wchar_t widthStr[32];
    swprintf(widthStr, 32, L"%.1f px", g_shapeInfo.strokeWidth);
    SolidBrush valueBgBrush(g_hoverTarget == TARGET_STROKE_WIDTH ? COLOR_VALUE_HOVER : COLOR_VALUE_BG);
    g.FillRectangle(&valueBgBrush, widthX, rowY, VALUE_BOX_WIDTH, VALUE_BOX_HEIGHT);
    g.DrawString(widthStr, -1, &valueFont, PointF((float)widthX + 6, (float)rowY + 4), &textBrush);
    g_strokeWidthRect = {widthX, rowY, widthX + VALUE_BOX_WIDTH, rowY + VALUE_BOX_HEIGHT};

    rowY += ROW_HEIGHT + ROW_SPACING;

    // Row 3: Opacity
    g.DrawString(L"Opacity", -1, &labelFont, PointF((float)labelX, (float)rowY + 4), &dimBrush);

    wchar_t opacityStr[32];
    swprintf(opacityStr, 32, L"%.0f%%", g_shapeInfo.opacity);
    SolidBrush opacityBgBrush(g_hoverTarget == TARGET_OPACITY ? COLOR_VALUE_HOVER : COLOR_VALUE_BG);
    g.FillRectangle(&opacityBgBrush, valueX, rowY, VALUE_BOX_WIDTH, VALUE_BOX_HEIGHT);
    g.DrawString(opacityStr, -1, &valueFont, PointF((float)valueX + 6, (float)rowY + 4), &textBrush);
    g_opacityRect = {valueX, rowY, valueX + VALUE_BOX_WIDTH, rowY + VALUE_BOX_HEIGHT};
}

static void DrawTransformSection(Graphics& g, int y) {
    if (!g_shapeInfo.isParametric) {
        // Non-parametric shape (Path) - show message
        FontFamily fontFamily(L"Segoe UI");
        Font msgFont(&fontFamily, 9, FontStyleItalic, UnitPixel);
        SolidBrush dimBrush(COLOR_TEXT_DIM);
        g.DrawString(L"Path shape - Size N/A", -1, &msgFont,
                     PointF((float)PADDING + 4, (float)y + 4), &dimBrush);
        return;
    }

    FontFamily fontFamily(L"Segoe UI");
    Font labelFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
    Font valueFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);

    int rowY = y;
    int labelX = PADDING + 4;
    int valueX = PADDING + LABEL_WIDTH;

    // Row 1: Size W / H
    g.DrawString(L"Size", -1, &labelFont, PointF((float)labelX, (float)rowY + 4), &dimBrush);

    // Width
    wchar_t wStr[32];
    swprintf(wStr, 32, L"W: %.0f", g_shapeInfo.sizeW);
    SolidBrush wBgBrush(g_hoverTarget == TARGET_SIZE_W ? COLOR_VALUE_HOVER : COLOR_VALUE_BG);
    g.FillRectangle(&wBgBrush, valueX, rowY, 60, VALUE_BOX_HEIGHT);
    g.DrawString(wStr, -1, &valueFont, PointF((float)valueX + 4, (float)rowY + 4), &textBrush);
    g_sizeWRect = {valueX, rowY, valueX + 60, rowY + VALUE_BOX_HEIGHT};

    // Link button
    int linkX = valueX + 64;
    SolidBrush linkBrush(g_shapeInfo.sizeLinkEnabled ? COLOR_ACCENT : COLOR_VALUE_BG);
    g.FillRectangle(&linkBrush, linkX, rowY + 2, 18, 18);
    g.DrawString(L"\x1F517", -1, &labelFont, PointF((float)linkX + 2, (float)rowY + 4), &textBrush);
    g_sizeLinkRect = {linkX, rowY + 2, linkX + 18, rowY + 20};

    // Height
    int hX = linkX + 24;
    wchar_t hStr[32];
    swprintf(hStr, 32, L"H: %.0f", g_shapeInfo.sizeH);
    SolidBrush hBgBrush(g_hoverTarget == TARGET_SIZE_H ? COLOR_VALUE_HOVER : COLOR_VALUE_BG);
    g.FillRectangle(&hBgBrush, hX, rowY, 60, VALUE_BOX_HEIGHT);
    g.DrawString(hStr, -1, &valueFont, PointF((float)hX + 4, (float)rowY + 4), &textBrush);
    g_sizeHRect = {hX, rowY, hX + 60, rowY + VALUE_BOX_HEIGHT};

    rowY += ROW_HEIGHT + ROW_SPACING;

    // Row 2: Roundness
    g.DrawString(L"Round", -1, &labelFont, PointF((float)labelX, (float)rowY + 4), &dimBrush);

    wchar_t roundStr[32];
    swprintf(roundStr, 32, L"%.1f", g_shapeInfo.roundness);
    SolidBrush roundBgBrush(g_hoverTarget == TARGET_ROUNDNESS ? COLOR_VALUE_HOVER : COLOR_VALUE_BG);
    g.FillRectangle(&roundBgBrush, valueX, rowY, VALUE_BOX_WIDTH, VALUE_BOX_HEIGHT);
    g.DrawString(roundStr, -1, &valueFont, PointF((float)valueX + 6, (float)rowY + 4), &textBrush);
    g_roundnessRect = {valueX, rowY, valueX + VALUE_BOX_WIDTH, rowY + VALUE_BOX_HEIGHT};

    rowY += ROW_HEIGHT + ROW_SPACING;

    // Row 3: Anchor hint
    g.DrawString(L"Anchor", -1, &labelFont, PointF((float)labelX, (float)rowY + 4), &dimBrush);
    g.DrawString(L"Press Y for grid", -1, &labelFont,
                 PointF((float)valueX, (float)rowY + 4), &dimBrush);
}

static void DrawPathOpsSection(Graphics& g, int y) {
    FontFamily fontFamily(L"Segoe UI");
    Font btnFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);

    int btnWidth = 80;
    int btnHeight = 22;
    int btnGap = 4;
    int cols = 3;

    for (int i = 0; i < PATH_OP_COUNT && i < 9; i++) {
        int col = i % cols;
        int row = i / cols;
        int bx = PADDING + 4 + col * (btnWidth + btnGap);
        int by = y + row * (btnHeight + btnGap);

        bool hovered = (g_hoverPathOp == i);
        SolidBrush btnBrush(hovered ? COLOR_HOVER : COLOR_VALUE_BG);
        g.FillRectangle(&btnBrush, bx, by, btnWidth, btnHeight);

        // Truncate long names
        std::wstring name = PATH_OP_NAMES[i];
        if (name.length() > 10) {
            name = name.substr(0, 9) + L"...";
        }
        g.DrawString(name.c_str(), -1, &btnFont,
                     PointF((float)bx + 4, (float)by + 4), &textBrush);

        g_pathOpRects[i] = {bx, by, bx + btnWidth, by + btnHeight};
    }
}

// ============================================================================
// Window Procedure
// ============================================================================

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
        if (!g_editMode) return MA_NOACTIVATE;
        return MA_ACTIVATE;

    case WM_LBUTTONDOWN:
        HandleMouseDown(InverseScaled(GET_X_LPARAM(lParam)), InverseScaled(GET_Y_LPARAM(lParam)));
        return 0;

    case WM_LBUTTONUP:
        HandleMouseUp(InverseScaled(GET_X_LPARAM(lParam)), InverseScaled(GET_Y_LPARAM(lParam)));
        return 0;

    case WM_MOUSEMOVE:
        HandleMouseMove(InverseScaled(GET_X_LPARAM(lParam)), InverseScaled(GET_Y_LPARAM(lParam)));
        return 0;

    case WM_LBUTTONDBLCLK:
        HandleDoubleClick(InverseScaled(GET_X_LPARAM(lParam)), InverseScaled(GET_Y_LPARAM(lParam)));
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            g_result.cancelled = true;
            HidePanel();
            return 0;
        }
        if (wParam == 'Y') {
            // Show anchor grid at cursor position
            POINT pt;
            GetCursorPos(&pt);
            ShowAnchorGrid(pt.x, pt.y);
            return 0;
        }
        break;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && !g_keepPanelOpen && !g_colorPickerOpen && !g_anchorGridVisible) {
            HidePanel();
        }
        return 0;

    case WM_KILLFOCUS:
        if (!g_keepPanelOpen && !g_colorPickerOpen && !g_anchorGridVisible) {
            HidePanel();
        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Mouse Handling
// ============================================================================

static bool PointInRect(int x, int y, const RECT& r) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

static void HandleMouseDown(int x, int y) {
    // Pin button
    if (PointInRect(x, y, g_pinRect)) {
        g_keepPanelOpen = !g_keepPanelOpen;
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }

    // Section headers (accordion)
    for (int s = 0; s < SECTION_COUNT; s++) {
        if (PointInRect(x, y, g_sectionHeaderRects[s])) {
            ToggleSection((Section)s);
            return;
        }
    }

    // Color boxes
    if (PointInRect(x, y, g_fillColorRect)) {
        POINT pt;
        GetCursorPos(&pt);
        ShowColorPicker(false, pt.x + 10, pt.y);
        return;
    }
    if (PointInRect(x, y, g_strokeColorRect)) {
        POINT pt;
        GetCursorPos(&pt);
        ShowColorPicker(true, pt.x + 10, pt.y);
        return;
    }

    // Toggle buttons
    if (PointInRect(x, y, g_fillToggleRect)) {
        g_shapeInfo.hasFill = !g_shapeInfo.hasFill;
        // TODO: Apply to AE
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    if (PointInRect(x, y, g_strokeToggleRect)) {
        g_shapeInfo.hasStroke = !g_shapeInfo.hasStroke;
        // TODO: Apply to AE
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }

    // Size link
    if (PointInRect(x, y, g_sizeLinkRect)) {
        g_shapeInfo.sizeLinkEnabled = !g_shapeInfo.sizeLinkEnabled;
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }

    // Value drag start
    ValueTarget target = TARGET_NONE;
    if (PointInRect(x, y, g_strokeWidthRect)) target = TARGET_STROKE_WIDTH;
    else if (PointInRect(x, y, g_opacityRect)) target = TARGET_OPACITY;
    else if (PointInRect(x, y, g_sizeWRect)) target = TARGET_SIZE_W;
    else if (PointInRect(x, y, g_sizeHRect)) target = TARGET_SIZE_H;
    else if (PointInRect(x, y, g_roundnessRect)) target = TARGET_ROUNDNESS;

    if (target != TARGET_NONE) {
        g_dragging = true;
        g_dragTarget = target;
        g_dragStartX = x;
        g_dragStartValue = GetTargetValue(target);
        SetCapture(g_hwnd);
        return;
    }

    // Path operations
    for (int i = 0; i < PATH_OP_COUNT && i < 9; i++) {
        if (PointInRect(x, y, g_pathOpRects[i])) {
            AddShapePathOperation(i);
            return;
        }
    }

    // Window drag (header area)
    if (y < HEADER_HEIGHT && x < WINDOW_WIDTH - PIN_BUTTON_SIZE - PADDING * 2) {
        g_windowDragging = true;
        POINT pt;
        GetCursorPos(&pt);
        RECT wr;
        GetWindowRect(g_hwnd, &wr);
        g_windowDragOffset.x = pt.x - wr.left;
        g_windowDragOffset.y = pt.y - wr.top;
        SetCapture(g_hwnd);
    }
}

static void HandleMouseUp(int x, int y) {
    if (g_dragging) {
        g_dragging = false;
        g_dragTarget = TARGET_NONE;
        ReleaseCapture();
    }
    if (g_windowDragging) {
        g_windowDragging = false;
        ReleaseCapture();
    }
}

static void HandleMouseMove(int x, int y) {
    // Window dragging
    if (g_windowDragging) {
        POINT pt;
        GetCursorPos(&pt);
        SetWindowPos(g_hwnd, NULL, pt.x - g_windowDragOffset.x, pt.y - g_windowDragOffset.y,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);
        return;
    }

    // Value dragging
    if (g_dragging && g_dragTarget != TARGET_NONE) {
        int dx = x - g_dragStartX;
        const DragConfig& cfg = DRAG_CONFIGS[g_dragTarget];
        float newValue = g_dragStartValue + dx * cfg.sensitivity;
        newValue = std::max(cfg.minValue, std::min(cfg.maxValue, newValue));

        // Size link
        if (g_shapeInfo.sizeLinkEnabled &&
            (g_dragTarget == TARGET_SIZE_W || g_dragTarget == TARGET_SIZE_H)) {
            float ratio = g_shapeInfo.sizeW / g_shapeInfo.sizeH;
            if (g_dragTarget == TARGET_SIZE_W) {
                g_shapeInfo.sizeW = newValue;
                g_shapeInfo.sizeH = newValue / ratio;
            } else {
                g_shapeInfo.sizeH = newValue;
                g_shapeInfo.sizeW = newValue * ratio;
            }
            ApplyShapeSizeValue(g_shapeInfo.sizeW, g_shapeInfo.sizeH);
        } else {
            SetTargetValue(g_dragTarget, newValue);
        }

        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }

    // Hover detection
    ValueTarget oldHover = g_hoverTarget;
    int oldSection = g_hoverSection;
    int oldPathOp = g_hoverPathOp;
    bool oldPin = g_pinHover;
    bool oldFillColor = g_fillColorHover;
    bool oldStrokeColor = g_strokeColorHover;

    g_hoverTarget = TARGET_NONE;
    g_hoverSection = -1;
    g_hoverPathOp = -1;
    g_pinHover = false;
    g_fillColorHover = false;
    g_strokeColorHover = false;

    if (PointInRect(x, y, g_pinRect)) g_pinHover = true;
    else if (PointInRect(x, y, g_fillColorRect)) g_fillColorHover = true;
    else if (PointInRect(x, y, g_strokeColorRect)) g_strokeColorHover = true;
    else if (PointInRect(x, y, g_strokeWidthRect)) g_hoverTarget = TARGET_STROKE_WIDTH;
    else if (PointInRect(x, y, g_opacityRect)) g_hoverTarget = TARGET_OPACITY;
    else if (PointInRect(x, y, g_sizeWRect)) g_hoverTarget = TARGET_SIZE_W;
    else if (PointInRect(x, y, g_sizeHRect)) g_hoverTarget = TARGET_SIZE_H;
    else if (PointInRect(x, y, g_roundnessRect)) g_hoverTarget = TARGET_ROUNDNESS;

    for (int s = 0; s < SECTION_COUNT; s++) {
        if (PointInRect(x, y, g_sectionHeaderRects[s])) {
            g_hoverSection = s;
            break;
        }
    }

    for (int i = 0; i < PATH_OP_COUNT && i < 9; i++) {
        if (PointInRect(x, y, g_pathOpRects[i])) {
            g_hoverPathOp = i;
            break;
        }
    }

    if (g_hoverTarget != oldHover || g_hoverSection != oldSection ||
        g_hoverPathOp != oldPathOp || g_pinHover != oldPin ||
        g_fillColorHover != oldFillColor || g_strokeColorHover != oldStrokeColor) {
        InvalidateRect(g_hwnd, NULL, FALSE);
    }

    // Cursor
    if (g_hoverTarget != TARGET_NONE) {
        SetCursor(LoadCursor(NULL, IDC_SIZEWE));
    } else {
        SetCursor(LoadCursor(NULL, IDC_ARROW));
    }
}

static void HandleDoubleClick(int x, int y) {
    // TODO: Inline edit for value boxes
}

// ============================================================================
// Value Helpers
// ============================================================================

static float GetTargetValue(ValueTarget target) {
    switch (target) {
        case TARGET_STROKE_WIDTH: return g_shapeInfo.strokeWidth;
        case TARGET_OPACITY: return g_shapeInfo.opacity;
        case TARGET_SIZE_W: return g_shapeInfo.sizeW;
        case TARGET_SIZE_H: return g_shapeInfo.sizeH;
        case TARGET_ROUNDNESS: return g_shapeInfo.roundness;
        default: return 0;
    }
}

static void SetTargetValue(ValueTarget target, float value) {
    const char* propName = nullptr;
    switch (target) {
        case TARGET_STROKE_WIDTH:
            g_shapeInfo.strokeWidth = value;
            propName = "strokeWidth";
            break;
        case TARGET_OPACITY:
            g_shapeInfo.opacity = value;
            propName = "opacity";
            break;
        case TARGET_SIZE_W:
            g_shapeInfo.sizeW = value;
            propName = "sizeW";
            break;
        case TARGET_SIZE_H:
            g_shapeInfo.sizeH = value;
            propName = "sizeH";
            break;
        case TARGET_ROUNDNESS:
            g_shapeInfo.roundness = value;
            propName = "roundness";
            break;
        default:
            return;
    }
    if (propName) {
        ApplyShapePropertyValue(propName, value);
    }
}

// ============================================================================
// Color Picker Window
// ============================================================================

static LRESULT CALLBACK ColorPickerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // TODO: Implement color picker (similar to TextUI)
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        // Placeholder
        RECT r;
        GetClientRect(hwnd, &r);
        HBRUSH brush = CreateSolidBrush(RGB(40, 40, 48));
        FillRect(hdc, &r, brush);
        DeleteObject(brush);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) {
            HideColorPicker();
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Anchor Grid Window
// ============================================================================

static LRESULT CALLBACK AnchorGridWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        // Double buffering
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

        Graphics graphics(memDC);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);

        // Background
        SolidBrush bgBrush(COLOR_BG);
        graphics.FillRectangle(&bgBrush, 0, 0, rect.right, rect.bottom);

        // Draw 3x3 grid
        int cellSize = rect.right / 3;
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                int idx = row * 3 + col;
                int cx = col * cellSize;
                int cy = row * cellSize;

                bool hovered = (g_anchorHoverIndex == idx);
                SolidBrush cellBrush(hovered ? COLOR_HOVER : COLOR_SECTION_BG);
                graphics.FillRectangle(&cellBrush, cx + 2, cy + 2, cellSize - 4, cellSize - 4);

                // Dot
                float dotX = cx + cellSize / 2.0f;
                float dotY = cy + cellSize / 2.0f;
                SolidBrush dotBrush(hovered ? COLOR_ACCENT : COLOR_TEXT_DIM);
                graphics.FillEllipse(&dotBrush, dotX - 4, dotY - 4, 8, 8);
            }
        }

        // Border
        Pen borderPen(COLOR_BORDER, 1);
        graphics.DrawRectangle(&borderPen, 0, 0, rect.right - 1, rect.bottom - 1);

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        RECT r;
        GetClientRect(hwnd, &r);
        int cellSize = r.right / 3;

        int col = x / cellSize;
        int row = y / cellSize;
        if (col >= 0 && col < 3 && row >= 0 && row < 3) {
            int newHover = row * 3 + col;
            if (newHover != g_anchorHoverIndex) {
                g_anchorHoverIndex = newHover;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (g_anchorHoverIndex >= 0 && g_anchorHoverIndex < 9) {
            // Calculate anchor position based on grid index
            // 0=TopLeft, 1=Top, 2=TopRight, 3=Left, 4=Center, 5=Right, 6=BottomLeft, 7=Bottom, 8=BottomRight
            float ratioX = (g_anchorHoverIndex % 3) * 0.5f;
            float ratioY = (g_anchorHoverIndex / 3) * 0.5f;

            // Calculate anchor from bounds
            float newAnchorX = g_shapeInfo.boundsLeft + g_shapeInfo.boundsWidth * ratioX;
            float newAnchorY = g_shapeInfo.boundsTop + g_shapeInfo.boundsHeight * ratioY;

            ApplyShapeAnchorValue(newAnchorX, newAnchorY);
            HideAnchorGrid();
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE || wParam == 'Y') {
            HideAnchorGrid();
            return 0;
        }
        break;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) {
            HideAnchorGrid();
        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace ShapeUI

#else
// Stub implementations for non-Windows platforms
namespace ShapeUI {
void Initialize() {}
void Shutdown() {}
void ShowPanel(int x, int y) { (void)x; (void)y; }
ShapeResult HidePanel() { return {}; }
bool IsVisible() { return false; }
void UpdateHover(int mouseX, int mouseY) { (void)mouseX; (void)mouseY; }
ShapeResult GetResult() { return {}; }
void SetShapeInfo(const wchar_t* jsonInfo) { (void)jsonInfo; }
bool NeedsRefresh() { return false; }
void ShowColorPicker(bool forStroke, int x, int y) { (void)forStroke; (void)x; (void)y; }
void HideColorPicker() {}
bool IsColorPickerVisible() { return false; }
void ShowAnchorGrid(int x, int y) { (void)x; (void)y; }
void HideAnchorGrid() {}
bool IsAnchorGridVisible() { return false; }
void ToggleSection(Section section) { (void)section; }
bool IsSectionExpanded(Section section) { (void)section; return false; }
}
#endif
