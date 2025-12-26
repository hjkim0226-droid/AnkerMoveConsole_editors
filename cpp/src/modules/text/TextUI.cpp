/*****************************************************************************
 * TextUI.cpp
 *
 * Native Windows UI implementation for Text Options Module
 * Uses Win32 + GDI+ for text layer property editing
 *****************************************************************************/

#include "TextUI.h"
#include "GdiPlusIncludes.h"

#ifdef MSWindows
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <string>
#include <cmath>

using namespace Gdiplus;

// External functions from SnapPlugin.cpp
extern float GetModuleScaleFactor(const char* moduleName);
extern void ApplyTextPropertyValue(const char* propName, float value);
extern void ApplyTextColorValue(bool stroke, float r, float g, float b);
extern void ApplyTextJustificationValue(int just);
extern void GetFontsList(wchar_t* outBuffer, size_t bufSize);
extern void ApplyTextFont(const char* postScriptName);

#include <vector>
#include <algorithm>

namespace TextUI {

// Window dimensions - Compact layout
static const int WINDOW_WIDTH = 280;  // Reduced by 10%
static const int WINDOW_HEIGHT = 195;

// Scale factor for this module (set in ShowPanel)
static float g_scaleFactor = 1.0f;

// Helper functions for scaling
inline int Scaled(int baseValue) {
    return (int)(baseValue * g_scaleFactor);
}

inline int InverseScaled(int screenValue) {
    return (int)(screenValue / g_scaleFactor);
}

// Layout constants - Compact design
static const int HEADER_HEIGHT = 28;
static const int ROW_HEIGHT = 24;
static const int ROW_SPACING = 6;
static const int PADDING = 8;
static const int LABEL_WIDTH = 32;

// Value boxes - 3 columns (Size, Tracking, Leading)
static const int VALUE_COL_WIDTH = 90;  // 3 columns fit in ~290px
static const int VALUE_BOX_HEIGHT = 38; // Includes label
static const int VALUE_INNER_HEIGHT = 20;

// Color section
static const int COLOR_BOX_SIZE = 20;

// Align buttons
static const int ALIGN_BTN_SIZE = 26;
static const int ALIGN_GAP = 4;
static const int ALIGN_GROUP_GAP = 12;  // Gap between L/C/R and Justify groups

// Legacy constants for compatibility
static const int SECTION_HEIGHT = 28;
static const int VALUE_BOX_WIDTH = 100;

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
static const Color COLOR_BORDER(255, 60, 60, 70);
static const Color COLOR_HOVER(255, 60, 80, 100);

// Drag sensitivity configuration
struct DragConfig {
    float sensitivity;  // value change per pixel
    float minValue;
    float maxValue;
    int decimals;
    const wchar_t* suffix;
};

static const DragConfig DRAG_CONFIGS[] = {
    {0.5f, 1.0f, 999.0f, 1, L"pt"},       // SIZE
    {2.0f, -10000.0f, 10000.0f, 0, L""}, // TRACKING (±10000)
    {1.0f, 0.0f, 999.0f, 0, L""},         // LEADING (0=Auto)
    {0.1f, 0.0f, 100.0f, 1, L"px"}        // STROKE_WIDTH
};

// Global state
static HWND g_hwnd = NULL;
static HWND g_colorPickerHwnd = NULL;
static bool g_visible = false;
static bool g_needsRefresh = false;  // Request to refresh text info from selection
static ULONG_PTR g_gdiplusToken = 0;
static TextInfo g_textInfo = {};
static TextResult g_result;
static bool g_keepPanelOpen = false;
static bool g_forwardingToAE = false;  // Flag to prevent close during Undo/Redo

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

// Drag state (value drag)
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
static bool g_editSelectAll = false;
static DWORD g_lastClickTime = 0;
static ValueTarget g_lastClickTarget = TARGET_NONE;

// Color picker state
static bool g_colorPickerForStroke = false;
static bool g_colorPickerOpen = false;  // Prevent main window from closing
static float g_pickerColor[3] = {1, 1, 1};
static int g_pickerHue = 0;

// Font dropdown state
struct FontInfo {
    std::wstring familyName;
    std::wstring styleName;
    std::wstring postScriptName;
    std::wstring displayName;  // "familyName styleName"
};
static std::vector<FontInfo> g_allFonts;
static std::vector<FontInfo*> g_filteredFonts;
static bool g_fontsLoaded = false;
static bool g_fontDropdownOpen = false;
static std::wstring g_fontSearchText;
static int g_fontScrollOffset = 0;
static int g_fontHoverIndex = -1;
static RECT g_fontDropdownRect;
static RECT g_fontButtonRect;
static const int FONT_ITEM_HEIGHT = 24;
static const int FONT_DROPDOWN_MAX_ITEMS = 8;

// Text preset state
struct TextPreset {
    std::wstring name;
    std::wstring font;
    float fontSize;
    float tracking;
    float leading;
    float strokeWidth;
    float fillColor[3];
    float strokeColor[3];
    bool applyFill;
    bool applyStroke;
    int justify;
};
static std::vector<TextPreset> g_textPresets;
static bool g_presetDropdownOpen = false;
static int g_presetHoverIndex = -1;
static int g_presetScrollOffset = 0;
static RECT g_presetButtonRect;
static RECT g_presetDropdownRect;
static const int PRESET_ITEM_HEIGHT = 28;
static const int PRESET_DROPDOWN_MAX_ITEMS = 6;
static bool g_presetSaveMode = false;  // True when showing save dialog
static std::wstring g_presetSaveName;

// Forward declarations
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK ColorPickerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
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
static void LoadFonts();
static void FilterFonts(const std::wstring& search);
static void DrawFontDropdown(Graphics& g);
static void DrawPresetDropdown(Graphics& g);
static void DrawPresetSection(Graphics& g, int& y);
static void DeletePreset(int index);
static void SavePreset(const std::wstring& name);
static void ApplyPreset(int index);
static void LoadPresets();
static void SavePresetsToFile();

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

    // Create main window (initially hidden) - NO WS_EX_NOACTIVATE, we need focus
    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
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

    // Register color picker window class (non-critical, ignore failure)
    WNDCLASSEXW wcPicker = {0};
    wcPicker.cbSize = sizeof(WNDCLASSEXW);
    wcPicker.style = CS_HREDRAW | CS_VREDRAW;
    wcPicker.lpfnWndProc = ColorPickerWndProc;
    wcPicker.hInstance = GetModuleHandle(NULL);
    wcPicker.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcPicker.hbrBackground = NULL;
    wcPicker.lpszClassName = L"TextUIColorPicker";
    RegisterClassExW(&wcPicker);
}

/*****************************************************************************
 * Shutdown
 *****************************************************************************/
void Shutdown() {
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
}

/*****************************************************************************
 * ShowPanel
 *****************************************************************************/
void ShowPanel(int x, int y) {
    if (!g_hwnd) Initialize();

    // Reset state
    g_result = TextResult();
    g_dragging = false;
    g_windowDragging = false;
    g_editMode = false;
    g_hoverTarget = TARGET_NONE;

    // Get scale factor from settings
    g_scaleFactor = GetModuleScaleFactor("text");
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
                 SWP_SHOWWINDOW);
    ShowWindow(g_hwnd, SW_SHOW);
    SetForegroundWindow(g_hwnd);
    SetFocus(g_hwnd);
    InvalidateRect(g_hwnd, NULL, TRUE);
    g_visible = true;
}

/*****************************************************************************
 * HidePanel
 *****************************************************************************/
TextResult HidePanel() {
    if (g_editMode) ExitEditMode(false);
    HideColorPicker();

    // Reset dropdown states
    g_fontDropdownOpen = false;
    g_fontSearchText.clear();
    g_presetDropdownOpen = false;
    g_presetScrollOffset = 0;

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
    // Convert to local coords and inverse scale for hit testing
    int localX = InverseScaled(mouseX - wndRect.left);
    int localY = InverseScaled(mouseY - wndRect.top);

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

    // Check font button hover
    int newFontHover = -1;
    if (PtInRect(&g_fontButtonRect, pt)) {
        newFontHover = -2;  // -2 means button hover
    } else if (g_fontDropdownOpen && PtInRect(&g_fontDropdownRect, pt)) {
        // Calculate which font is being hovered
        int searchH = 28;
        int listY = g_fontDropdownRect.top + searchH;
        if (localY >= listY) {
            newFontHover = g_fontScrollOffset + (localY - listY) / FONT_ITEM_HEIGHT;
            if (newFontHover >= (int)g_filteredFonts.size()) newFontHover = -1;
        }
    }
    if (newFontHover != g_fontHoverIndex) {
        g_fontHoverIndex = newFontHover;
        needsRepaint = true;
    }

    // Check preset button and dropdown hover
    int newPresetHover = -10;  // -10 = nothing
    if (PtInRect(&g_presetButtonRect, pt)) {
        newPresetHover = -2;  // -2 = button hover
    } else if (g_presetDropdownOpen && PtInRect(&g_presetDropdownRect, pt)) {
        // Calculate which item is being hovered
        int listY = g_presetDropdownRect.top + 4;

        // "Save Current" item
        if (g_presetScrollOffset == 0 && localY >= listY && localY < listY + PRESET_ITEM_HEIGHT) {
            newPresetHover = -1;  // -1 = "Save Current"
        } else {
            // Preset items
            int itemY = listY + (g_presetScrollOffset == 0 ? PRESET_ITEM_HEIGHT : 0);
            int startIdx = max(0, g_presetScrollOffset - 1);

            for (int i = startIdx; i < (int)g_textPresets.size(); i++) {
                if (localY >= itemY && localY < itemY + PRESET_ITEM_HEIGHT) {
                    newPresetHover = i;
                    break;
                }
                itemY += PRESET_ITEM_HEIGHT;
                if (itemY > g_presetDropdownRect.bottom) break;
            }
        }
    }
    if (newPresetHover != g_presetHoverIndex) {
        g_presetHoverIndex = newPresetHover;
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
    g_textInfo.justify = (Justification)(int)getFloat(L"justify");

    // Parse color arrays [r,g,b]
    auto getColorArray = [&json](const wchar_t* key, float* outRGB) {
        std::wstring search = std::wstring(L"\"") + key + L"\":[";
        size_t pos = json.find(search);
        if (pos == std::wstring::npos) return;
        pos += search.length();

        // Parse r
        outRGB[0] = (float)_wtof(json.c_str() + pos);

        // Skip to next comma
        size_t comma1 = json.find(L',', pos);
        if (comma1 == std::wstring::npos) return;
        pos = comma1 + 1;

        // Parse g
        outRGB[1] = (float)_wtof(json.c_str() + pos);

        // Skip to next comma
        size_t comma2 = json.find(L',', pos);
        if (comma2 == std::wstring::npos) return;
        pos = comma2 + 1;

        // Parse b
        outRGB[2] = (float)_wtof(json.c_str() + pos);
    };

    getColorArray(L"fillColor", g_textInfo.fillColor);
    getColorArray(L"strokeColor", g_textInfo.strokeColor);

    InvalidateRect(g_hwnd, NULL, FALSE);
}

/*****************************************************************************
 * NeedsRefresh - Check if refresh is requested, and clear the flag
 *****************************************************************************/
bool NeedsRefresh() {
    if (g_needsRefresh) {
        g_needsRefresh = false;
        return true;
    }
    return false;
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

        // Copy to screen
        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

        // Cleanup
        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEACTIVATE:
        // Prevent activation when clicked (unless in edit mode)
        if (!g_editMode) return MA_NOACTIVATE;
        return MA_ACTIVATE;

    case WM_LBUTTONDOWN:
        // Inverse scale mouse coordinates for hit testing
        HandleMouseDown(InverseScaled(GET_X_LPARAM(lParam)), InverseScaled(GET_Y_LPARAM(lParam)));
        return 0;

    case WM_LBUTTONUP:
        // Inverse scale mouse coordinates for hit testing
        HandleMouseUp(InverseScaled(GET_X_LPARAM(lParam)), InverseScaled(GET_Y_LPARAM(lParam)));
        return 0;

    case WM_MOUSEMOVE:
        // Inverse scale mouse coordinates for hit testing
        HandleMouseMove(InverseScaled(GET_X_LPARAM(lParam)), InverseScaled(GET_Y_LPARAM(lParam)));
        return 0;

    case WM_LBUTTONDBLCLK:
        // Inverse scale mouse coordinates for hit testing
        HandleDoubleClick(InverseScaled(GET_X_LPARAM(lParam)), InverseScaled(GET_Y_LPARAM(lParam)));
        return 0;

    case WM_KEYDOWN:
        HandleKeyDown(wParam);
        return 0;

    case WM_CHAR:
        HandleChar((wchar_t)wParam);
        return 0;

    case WM_MOUSEWHEEL:
        if (g_fontDropdownOpen) {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int scrollAmount = (delta > 0) ? -3 : 3;
            int maxScroll = max(0, (int)g_filteredFonts.size() - FONT_DROPDOWN_MAX_ITEMS);
            g_fontScrollOffset = max(0, min(maxScroll, g_fontScrollOffset + scrollAmount));
            InvalidateRect(g_hwnd, NULL, FALSE);
            return 0;
        }
        if (g_presetDropdownOpen) {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int scrollAmount = (delta > 0) ? -1 : 1;
            int totalItems = 1 + (int)g_textPresets.size();
            int maxScroll = max(0, totalItems - PRESET_DROPDOWN_MAX_ITEMS);
            g_presetScrollOffset = max(0, min(maxScroll, g_presetScrollOffset + scrollAmount));
            InvalidateRect(g_hwnd, NULL, FALSE);
            return 0;
        }
        break;

    case WM_ACTIVATE:
        // Auto-refresh text info when panel is activated (e.g., clicked)
        if (LOWORD(wParam) == WA_ACTIVE || LOWORD(wParam) == WA_CLICKACTIVE) {
            g_needsRefresh = true;  // Request refresh from SnapPlugin
        }
        if (LOWORD(wParam) == WA_INACTIVE && !g_keepPanelOpen && !g_dragging && !g_editMode && !g_windowDragging && !g_fontDropdownOpen && !g_presetDropdownOpen && !g_forwardingToAE && !g_colorPickerOpen) {
            g_result.cancelled = true;
            ShowWindow(hwnd, SW_HIDE);
            g_visible = false;
        }
        return 0;

    case WM_KILLFOCUS:
        if (g_editMode) {
            ExitEditMode(true);
        } else if (!g_keepPanelOpen && !g_dragging && !g_colorPickerOpen &&
                   !g_fontDropdownOpen && !g_presetDropdownOpen && !g_forwardingToAE && !g_windowDragging) {
            // Only close if not in any special mode (same conditions as WM_ACTIVATE)
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

    // Apply scale transform - all drawing uses base dimensions
    g.ScaleTransform(g_scaleFactor, g_scaleFactor);

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

    // Draw dropdowns if open (on top of everything)
    DrawFontDropdown(g);
    DrawPresetDropdown(g);
}

/*****************************************************************************
 * DrawHeader - Compact header with title + layer name + pin + preset
 *****************************************************************************/
static void DrawHeader(Graphics& g) {
    SolidBrush headerBrush(COLOR_HEADER_BG);
    g.FillRectangle(&headerBrush, 0, 0, WINDOW_WIDTH, HEADER_HEIGHT);

    FontFamily fontFamily(L"Segoe UI");
    Font titleFont(&fontFamily, 10, FontStyleBold, UnitPixel);
    Font layerFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);

    // Title "TEXT"
    g.DrawString(L"TEXT", -1, &titleFont, PointF(PADDING, 7), &textBrush);

    // Separator dash
    g.DrawString(L"\u2014", -1, &layerFont, PointF(40, 8), &dimBrush);

    // Layer name (truncated)
    if (g_textInfo.layerName[0] != L'\0') {
        RectF layerRect(52, 8, 130, 16);
        StringFormat sf;
        sf.SetTrimming(StringTrimmingEllipsisCharacter);
        g.DrawString(g_textInfo.layerName, -1, &layerFont, layerRect, &sf, &dimBrush);
    }

    int btnSize = 18;
    int btnY = (HEADER_HEIGHT - btnSize) / 2;

    // Preset button (star) - moved to header
    g_presetButtonRect = {WINDOW_WIDTH - 72, btnY, WINDOW_WIDTH - 72 + btnSize, btnY + btnSize};
    bool presetHover = (g_presetHoverIndex == -2);
    SolidBrush presetBrush(presetHover ? COLOR_ALIGN_HOVER : Color(0, 0, 0, 0));  // Transparent unless hover
    g.FillRectangle(&presetBrush, g_presetButtonRect.left, g_presetButtonRect.top, btnSize, btnSize);

    // Star icon
    Pen starPen(presetHover ? Color(255, 255, 200, 0) : COLOR_TEXT_DIM, 1.0f);
    int starX = g_presetButtonRect.left + btnSize / 2;
    int starY = g_presetButtonRect.top + btnSize / 2;
    g.DrawLine(&starPen, starX, starY - 5, starX, starY + 5);
    g.DrawLine(&starPen, starX - 5, starY, starX + 5, starY);
    g.DrawLine(&starPen, starX - 3, starY - 3, starX + 3, starY + 3);
    g.DrawLine(&starPen, starX + 3, starY - 3, starX - 3, starY + 3);

    // Pin button
    g_pinRect = {WINDOW_WIDTH - 48, btnY, WINDOW_WIDTH - 48 + btnSize, btnY + btnSize};
    Color pinColor = g_keepPanelOpen ? COLOR_PIN_ACTIVE :
                     (g_pinHover ? COLOR_ALIGN_HOVER : Color(0, 0, 0, 0));
    SolidBrush pinBrush(pinColor);
    g.FillRectangle(&pinBrush, g_pinRect.left, g_pinRect.top, btnSize, btnSize);

    Pen pinPen(g_keepPanelOpen ? Color(255, 40, 40, 40) : (g_pinHover ? COLOR_TEXT : COLOR_TEXT_DIM), 1.2f);
    int px = g_pinRect.left + btnSize / 2, py = g_pinRect.top + btnSize / 2;
    g.DrawLine(&pinPen, px - 3, py - 3, px + 3, py + 3);
    g.DrawEllipse(&pinPen, (REAL)(px - 2), (REAL)(py - 5), 5.0f, 5.0f);

    // Close button (X)
    g_closeRect = {WINDOW_WIDTH - 24, btnY, WINDOW_WIDTH - 24 + btnSize, btnY + btnSize};
    Color closeColor = g_closeHover ? COLOR_CLOSE_HOVER : Color(0, 0, 0, 0);
    SolidBrush closeBrush(closeColor);
    g.FillRectangle(&closeBrush, g_closeRect.left, g_closeRect.top, btnSize, btnSize);

    Pen closePen(g_closeHover ? COLOR_TEXT : COLOR_TEXT_DIM, 1.2f);
    int cx = g_closeRect.left + btnSize / 2, cy = g_closeRect.top + btnSize / 2;
    g.DrawLine(&closePen, cx - 4, cy - 4, cx + 4, cy + 4);
    g.DrawLine(&closePen, cx + 4, cy - 4, cx - 4, cy + 4);
}

/*****************************************************************************
 * DrawFontSection - Compact: "Font" label inline with dropdown
 *****************************************************************************/
static void DrawFontSection(Graphics& g, int& y) {
    FontFamily fontFamily(L"Segoe UI");
    Font labelFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
    Font valueFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush labelBrush(COLOR_SECTION_LABEL);
    SolidBrush textBrush(COLOR_TEXT);

    // "Font" label inline
    g.DrawString(L"Font", -1, &labelFont, PointF((REAL)PADDING, (REAL)(y + 4)), &labelBrush);

    // Font dropdown button (after label)
    int dropdownX = PADDING + 32;
    bool fontHover = (g_fontHoverIndex == -2);
    SolidBrush valueBgBrush(fontHover ? COLOR_VALUE_HOVER : COLOR_VALUE_BG);
    g_fontButtonRect = {dropdownX, y, WINDOW_WIDTH - PADDING, y + ROW_HEIGHT};
    g.FillRectangle(&valueBgBrush, g_fontButtonRect.left, g_fontButtonRect.top,
                    g_fontButtonRect.right - g_fontButtonRect.left, ROW_HEIGHT);

    // Display current font
    std::wstring displayFont = g_textInfo.font[0] ? g_textInfo.font : L"(Select text layer)";
    RectF fontTextRect((REAL)g_fontButtonRect.left + 6, (REAL)y + 4,
                       (REAL)(g_fontButtonRect.right - g_fontButtonRect.left - 24), (REAL)ROW_HEIGHT - 8);
    StringFormat sf;
    sf.SetTrimming(StringTrimmingEllipsisCharacter);
    g.DrawString(displayFont.c_str(), -1, &valueFont, fontTextRect, &sf, &textBrush);

    // Dropdown arrow
    Pen arrowPen(COLOR_TEXT, 1.5f);
    int arrowX = g_fontButtonRect.right - 12;
    int arrowY = y + ROW_HEIGHT / 2;
    g.DrawLine(&arrowPen, arrowX - 3, arrowY - 2, arrowX, arrowY + 1);
    g.DrawLine(&arrowPen, arrowX, arrowY + 1, arrowX + 3, arrowY - 2);

    y += ROW_HEIGHT + ROW_SPACING;
}

/*****************************************************************************
 * DrawColorSection - Compact: Fill [■] Stroke [■] [Width]
 *****************************************************************************/
static void DrawColorSection(Graphics& g, int& y) {
    FontFamily fontFamily(L"Segoe UI");
    Font labelFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
    SolidBrush labelBrush(COLOR_SECTION_LABEL);
    SolidBrush textBrush(COLOR_TEXT);

    int boxY = y;
    int x = PADDING;

    // Fill label + color box
    g.DrawString(L"Fill", -1, &labelFont, PointF((REAL)x, (REAL)(boxY + 3)), &labelBrush);
    x += 22;
    g_fillColorRect = {x, boxY, x + COLOR_BOX_SIZE, boxY + COLOR_BOX_SIZE};
    DrawColorBox(g, g_fillColorRect, g_textInfo.fillColor, g_textInfo.applyFill, g_fillColorHover);
    x += COLOR_BOX_SIZE + 12;

    // Stroke label + color box
    g.DrawString(L"Stroke", -1, &labelFont, PointF((REAL)x, (REAL)(boxY + 3)), &labelBrush);
    x += 38;
    g_strokeColorRect = {x, boxY, x + COLOR_BOX_SIZE, boxY + COLOR_BOX_SIZE};
    DrawColorBox(g, g_strokeColorRect, g_textInfo.strokeColor, g_textInfo.applyStroke, g_strokeColorHover);
    x += COLOR_BOX_SIZE + 12;

    // Stroke width value box
    int widthBoxWidth = WINDOW_WIDTH - PADDING - x;
    g_strokeWidthRect = {x, boxY, x + widthBoxWidth, boxY + COLOR_BOX_SIZE};
    DrawValueBox(g, g_strokeWidthRect, TARGET_STROKE_WIDTH, g_textInfo.strokeWidth, L"px");

    y += COLOR_BOX_SIZE + ROW_SPACING;
}

/*****************************************************************************
 * DrawValueSection - Compact: 3 columns (Size, Tracking, Leading)
 *****************************************************************************/
static void DrawValueSection(Graphics& g, int& y) {
    FontFamily fontFamily(L"Segoe UI");
    Font labelFont(&fontFamily, 8, FontStyleRegular, UnitPixel);
    Font valueFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush labelBrush(COLOR_SECTION_LABEL);
    SolidBrush textBrush(COLOR_TEXT);

    // 3 columns layout
    int colWidth = (WINDOW_WIDTH - PADDING * 2 - 8) / 3;  // 8px gap total
    int boxHeight = VALUE_INNER_HEIGHT;

    // Column 1: Size
    int col1X = PADDING;
    g.DrawString(L"Size", -1, &labelFont, PointF((REAL)col1X, (REAL)y), &labelBrush);
    g_sizeRect = {col1X, y + 12, col1X + colWidth, y + 12 + boxHeight};
    DrawValueBox(g, g_sizeRect, TARGET_SIZE, g_textInfo.fontSize, L"pt");

    // Column 2: Tracking
    int col2X = col1X + colWidth + 4;
    g.DrawString(L"Tracking", -1, &labelFont, PointF((REAL)col2X, (REAL)y), &labelBrush);
    g_trackingRect = {col2X, y + 12, col2X + colWidth, y + 12 + boxHeight};
    DrawValueBox(g, g_trackingRect, TARGET_TRACKING, g_textInfo.tracking, L"");

    // Column 3: Leading
    int col3X = col2X + colWidth + 4;
    g.DrawString(L"Leading", -1, &labelFont, PointF((REAL)col3X, (REAL)y), &labelBrush);
    g_leadingRect = {col3X, y + 12, col3X + colWidth, y + 12 + boxHeight};
    DrawValueBox(g, g_leadingRect, TARGET_LEADING, g_textInfo.leading, L"");

    y += 12 + boxHeight + ROW_SPACING;
}

/*****************************************************************************
 * DrawAlignSection - Compact: Two groups [L|C|R] and [JL|JC|JR|JF]
 *****************************************************************************/
static void DrawAlignSection(Graphics& g, int& y) {
    // Two button groups: alignment (L/C/R) and justify (JL/JC/JR/JF)
    // No label - icons are self-explanatory

    // Calculate total width for center alignment
    int group1Width = 3 * ALIGN_BTN_SIZE + 2 * ALIGN_GAP;  // L, C, R
    int group2Width = 4 * ALIGN_BTN_SIZE + 3 * ALIGN_GAP;  // JL, JC, JR, JF
    int totalWidth = group1Width + ALIGN_GROUP_GAP + group2Width;
    int x = (WINDOW_WIDTH - totalWidth) / 2;  // Center aligned

    // Group 1: Left, Center, Right (3 buttons)
    for (int i = 0; i < 3; i++) {
        g_alignRects[i] = {x, y, x + ALIGN_BTN_SIZE, y + ALIGN_BTN_SIZE};
        bool active = (g_textInfo.justify == (Justification)i);
        bool hover = (g_hoverAlign == i);
        DrawAlignButton(g, g_alignRects[i], i, active, hover);
        x += ALIGN_BTN_SIZE + ALIGN_GAP;
    }

    // Gap between groups
    x += ALIGN_GROUP_GAP - ALIGN_GAP;  // Subtract one ALIGN_GAP since loop added it

    // Group 2: Justify Left, Center, Right, Full (4 buttons)
    for (int i = 3; i < 7; i++) {
        g_alignRects[i] = {x, y, x + ALIGN_BTN_SIZE, y + ALIGN_BTN_SIZE};
        bool active = (g_textInfo.justify == (Justification)i);
        bool hover = (g_hoverAlign == i);
        DrawAlignButton(g, g_alignRects[i], i, active, hover);
        x += ALIGN_BTN_SIZE + ALIGN_GAP;
    }

    y += ALIGN_BTN_SIZE + PADDING;
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

    // Header area (excluding buttons) - start window dragging
    if (y < HEADER_HEIGHT && x < g_pinRect.left) {
        g_windowDragging = true;
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        RECT wndRect;
        GetWindowRect(g_hwnd, &wndRect);
        g_windowDragOffset.x = cursorPos.x - wndRect.left;
        g_windowDragOffset.y = cursorPos.y - wndRect.top;
        SetCapture(g_hwnd);
        return;
    }

    // Preset dropdown handling
    if (g_presetDropdownOpen) {
        if (PtInRect(&g_presetDropdownRect, pt)) {
            // Calculate which item was clicked
            int dropX = g_presetDropdownRect.left;
            int dropW = g_presetDropdownRect.right - g_presetDropdownRect.left;
            int listY = g_presetDropdownRect.top + 4;

            // "Save Current" item (when scroll is at top)
            if (g_presetScrollOffset == 0 && y >= listY && y < listY + PRESET_ITEM_HEIGHT) {
                // Save current style
                SavePreset(L"Preset " + std::to_wstring(g_textPresets.size() + 1));
                g_presetDropdownOpen = false;
                InvalidateRect(g_hwnd, NULL, FALSE);
                return;
            }

            // Preset items
            int itemY = listY + (g_presetScrollOffset == 0 ? PRESET_ITEM_HEIGHT : 0);
            int startIdx = max(0, g_presetScrollOffset - 1);

            for (int i = startIdx; i < (int)g_textPresets.size(); i++) {
                if (y >= itemY && y < itemY + PRESET_ITEM_HEIGHT) {
                    // Check if delete button was clicked (right side)
                    int delX = dropX + dropW - 28;
                    if (x >= delX) {
                        // Delete preset
                        DeletePreset(i);
                    } else {
                        // Apply preset
                        ApplyPreset(i);
                        g_presetDropdownOpen = false;
                    }
                    InvalidateRect(g_hwnd, NULL, FALSE);
                    return;
                }
                itemY += PRESET_ITEM_HEIGHT;
                if (itemY > g_presetDropdownRect.bottom) break;
            }
            return;
        } else {
            // Click outside - close dropdown
            g_presetDropdownOpen = false;
            InvalidateRect(g_hwnd, NULL, FALSE);
        }
    }

    // Font dropdown handling
    if (g_fontDropdownOpen) {
        // Check if clicking inside dropdown
        if (PtInRect(&g_fontDropdownRect, pt)) {
            // Calculate which font item was clicked
            int searchH = 28;
            int listY = g_fontDropdownRect.top + searchH;
            if (y >= listY) {
                int clickedIdx = g_fontScrollOffset + (y - listY) / FONT_ITEM_HEIGHT;
                if (clickedIdx >= 0 && clickedIdx < (int)g_filteredFonts.size()) {
                    // Apply font
                    FontInfo* fi = g_filteredFonts[clickedIdx];
                    char psName[256];
                    WideCharToMultiByte(CP_UTF8, 0, fi->postScriptName.c_str(), -1, psName, sizeof(psName), NULL, NULL);
                    ApplyTextFont(psName);
                    wcscpy_s(g_textInfo.font, 128, fi->familyName.c_str());
                    g_fontDropdownOpen = false;
                    g_fontSearchText.clear();
                    g_result.applied = true;
                    InvalidateRect(g_hwnd, NULL, FALSE);
                }
            }
            return;
        } else {
            // Click outside dropdown - close it
            g_fontDropdownOpen = false;
            g_fontSearchText.clear();
            InvalidateRect(g_hwnd, NULL, FALSE);
            // Don't return - continue processing other clicks
        }
    }

    // Font dropdown button
    if (PtInRect(&g_fontButtonRect, pt)) {
        if (!g_fontsLoaded) LoadFonts();
        g_fontDropdownOpen = !g_fontDropdownOpen;
        g_fontSearchText.clear();
        FilterFonts(L"");
        // Close preset dropdown if open
        if (g_fontDropdownOpen && g_presetDropdownOpen) {
            g_presetDropdownOpen = false;
        }
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }

    // Preset button - toggle dropdown
    if (PtInRect(&g_presetButtonRect, pt)) {
        g_presetDropdownOpen = !g_presetDropdownOpen;
        g_presetScrollOffset = 0;
        g_presetHoverIndex = -1;
        // Close font dropdown if open
        if (g_presetDropdownOpen && g_fontDropdownOpen) {
            g_fontDropdownOpen = false;
            g_fontSearchText.clear();
        }
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }

    // Color boxes
    if (PtInRect(&g_fillColorRect, pt)) {
        RECT wndRect;
        GetWindowRect(g_hwnd, &wndRect);
        ShowColorPicker(false, wndRect.left + g_fillColorRect.left,
                        wndRect.top + g_fillColorRect.bottom + 5);
        return;
    }
    if (PtInRect(&g_strokeColorRect, pt)) {
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
    // Window dragging
    if (g_windowDragging) {
        g_windowDragging = false;
        ReleaseCapture();
        return;
    }

    // Value dragging
    if (g_dragging) {
        g_dragging = false;
        g_dragTarget = TARGET_NONE;
        ReleaseCapture();
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static void HandleMouseMove(int x, int y) {
    // Window dragging (use raw screen coordinates)
    if (g_windowDragging) {
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        SetWindowPos(g_hwnd, NULL,
                     cursorPos.x - g_windowDragOffset.x,
                     cursorPos.y - g_windowDragOffset.y,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);
        return;
    }

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

    InvalidateRect(g_hwnd, NULL, FALSE);
}

/*****************************************************************************
 * ForwardUndoRedoToAE - Send Undo/Redo to After Effects
 *****************************************************************************/
static void ForwardUndoRedoToAE(bool isRedo) {
    HWND aeWnd = NULL;
    HWND hw = GetTopWindow(NULL);
    while (hw) {
        wchar_t title[256] = {0};
        GetWindowTextW(hw, title, 256);
        if (wcsstr(title, L"After Effects") != NULL) { aeWnd = hw; break; }
        hw = GetNextWindow(hw, GW_HWNDNEXT);
    }
    if (aeWnd) {
        g_forwardingToAE = true;  // Prevent WM_ACTIVATE from closing panel
        SetForegroundWindow(aeWnd);
        keybd_event(VK_CONTROL, 0, 0, 0);
        if (isRedo) keybd_event(VK_SHIFT, 0, 0, 0);
        keybd_event('Z', 0, 0, 0);
        keybd_event('Z', 0, KEYEVENTF_KEYUP, 0);
        if (isRedo) keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        Sleep(30);
        SetForegroundWindow(g_hwnd);
        SetFocus(g_hwnd);
        g_forwardingToAE = false;
    }
}

/*****************************************************************************
 * Keyboard handlers
 *****************************************************************************/
static void HandleKeyDown(WPARAM key) {
    // Forward Ctrl+Z (Undo) or Ctrl+Shift+Z (Redo) to AE
    if ((GetKeyState(VK_CONTROL) & 0x8000) && key == 'Z') {
        bool isRedo = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        ForwardUndoRedoToAE(isRedo);
        return;
    }

    // Font dropdown navigation
    if (g_fontDropdownOpen) {
        switch (key) {
        case VK_UP:
            if (g_fontHoverIndex > 0) {
                g_fontHoverIndex--;
                if (g_fontHoverIndex < g_fontScrollOffset) {
                    g_fontScrollOffset = g_fontHoverIndex;
                }
                InvalidateRect(g_hwnd, NULL, FALSE);
            }
            return;
        case VK_DOWN:
            if (g_fontHoverIndex < (int)g_filteredFonts.size() - 1) {
                g_fontHoverIndex++;
                if (g_fontHoverIndex >= g_fontScrollOffset + FONT_DROPDOWN_MAX_ITEMS) {
                    g_fontScrollOffset = g_fontHoverIndex - FONT_DROPDOWN_MAX_ITEMS + 1;
                }
                InvalidateRect(g_hwnd, NULL, FALSE);
            } else if (g_fontHoverIndex == -1 && !g_filteredFonts.empty()) {
                g_fontHoverIndex = 0;
                InvalidateRect(g_hwnd, NULL, FALSE);
            }
            return;
        case VK_RETURN:
            if (g_fontHoverIndex >= 0 && g_fontHoverIndex < (int)g_filteredFonts.size()) {
                FontInfo* fi = g_filteredFonts[g_fontHoverIndex];
                char psName[256];
                WideCharToMultiByte(CP_UTF8, 0, fi->postScriptName.c_str(), -1, psName, sizeof(psName), NULL, NULL);
                ApplyTextFont(psName);
                wcscpy_s(g_textInfo.font, 128, fi->familyName.c_str());
                g_fontDropdownOpen = false;
                g_fontSearchText.clear();
                g_result.applied = true;
                InvalidateRect(g_hwnd, NULL, FALSE);
            }
            return;
        case VK_ESCAPE:
            g_fontDropdownOpen = false;
            g_fontSearchText.clear();
            InvalidateRect(g_hwnd, NULL, FALSE);
            return;
        }
    }

    // Preset dropdown navigation
    if (g_presetDropdownOpen) {
        switch (key) {
        case VK_UP:
            if (g_presetHoverIndex > -1) {
                g_presetHoverIndex--;
                InvalidateRect(g_hwnd, NULL, FALSE);
            }
            return;
        case VK_DOWN:
            if (g_presetHoverIndex < (int)g_textPresets.size() - 1) {
                g_presetHoverIndex++;
                InvalidateRect(g_hwnd, NULL, FALSE);
            }
            return;
        case VK_RETURN:
            if (g_presetHoverIndex == -1) {
                // "Save Current"
                SavePreset(L"Preset " + std::to_wstring(g_textPresets.size() + 1));
                g_presetDropdownOpen = false;
                InvalidateRect(g_hwnd, NULL, FALSE);
            } else if (g_presetHoverIndex >= 0 && g_presetHoverIndex < (int)g_textPresets.size()) {
                ApplyPreset(g_presetHoverIndex);
                g_presetDropdownOpen = false;
                InvalidateRect(g_hwnd, NULL, FALSE);
            }
            return;
        case VK_DELETE:
            if (g_presetHoverIndex >= 0 && g_presetHoverIndex < (int)g_textPresets.size()) {
                DeletePreset(g_presetHoverIndex);
                if (g_presetHoverIndex >= (int)g_textPresets.size()) {
                    g_presetHoverIndex = (int)g_textPresets.size() - 1;
                }
            }
            return;
        case VK_ESCAPE:
            g_presetDropdownOpen = false;
            InvalidateRect(g_hwnd, NULL, FALSE);
            return;
        }
    }

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
    // Font search input
    if (g_fontDropdownOpen) {
        if (ch == 8) {  // Backspace
            if (!g_fontSearchText.empty()) {
                g_fontSearchText.pop_back();
                FilterFonts(g_fontSearchText);
                InvalidateRect(g_hwnd, NULL, FALSE);
            }
        } else if (ch == 27) {  // Escape
            g_fontDropdownOpen = false;
            g_fontSearchText.clear();
            InvalidateRect(g_hwnd, NULL, FALSE);
        } else if (ch >= 32) {  // Printable character
            g_fontSearchText += ch;
            FilterFonts(g_fontSearchText);
            InvalidateRect(g_hwnd, NULL, FALSE);
        }
        return;
    }

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
 * ExtendScript execution - calls SnapPlugin.cpp functions
 *****************************************************************************/
static void ApplyTextProperty(const char* propName, float value) {
    // Call the external function to apply via ExtendScript
    ApplyTextPropertyValue(propName, value);
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
    // Call the external function to apply via ExtendScript
    ApplyTextColorValue(stroke, r, g, b);
    g_result.applied = true;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void ApplyJustification(Justification just) {
    // Call the external function to apply via ExtendScript
    ApplyTextJustificationValue((int)just);
    g_result.applied = true;
}

/*****************************************************************************
 * Font Loading and Filtering
 *****************************************************************************/
static void LoadFonts() {
    if (g_fontsLoaded) return;

    wchar_t* buffer = new wchar_t[65536];
    if (!buffer) return;
    memset(buffer, 0, 65536 * sizeof(wchar_t));

    GetFontsList(buffer, 65536);

    if (buffer[0] == L'\0') {
        delete[] buffer;
        return;
    }

    // Parse "familyName|styleName|postScriptName;..."
    g_allFonts.clear();
    std::wstring data(buffer);
    delete[] buffer;

    size_t pos = 0;
    while (pos < data.length()) {
        size_t end = data.find(L';', pos);
        if (end == std::wstring::npos) end = data.length();

        std::wstring entry = data.substr(pos, end - pos);
        if (!entry.empty()) {
            size_t p1 = entry.find(L'|');
            size_t p2 = entry.find(L'|', p1 + 1);
            if (p1 != std::wstring::npos && p2 != std::wstring::npos) {
                FontInfo fi;
                fi.familyName = entry.substr(0, p1);
                fi.styleName = entry.substr(p1 + 1, p2 - p1 - 1);
                fi.postScriptName = entry.substr(p2 + 1);
                fi.displayName = fi.familyName + L" " + fi.styleName;
                g_allFonts.push_back(fi);
            }
        }
        pos = end + 1;
    }

    g_fontsLoaded = true;
    FilterFonts(L"");
}

static void FilterFonts(const std::wstring& search) {
    g_filteredFonts.clear();
    g_fontScrollOffset = 0;

    std::wstring searchLower = search;
    for (auto& c : searchLower) c = towlower(c);

    for (auto& font : g_allFonts) {
        if (search.empty()) {
            g_filteredFonts.push_back(&font);
        } else {
            std::wstring displayLower = font.displayName;
            for (auto& c : displayLower) c = towlower(c);
            if (displayLower.find(searchLower) != std::wstring::npos) {
                g_filteredFonts.push_back(&font);
            }
        }
    }
}

static void DrawFontDropdown(Graphics& g) {
    if (!g_fontDropdownOpen) return;

    FontFamily fontFamily(L"Segoe UI");
    Font searchFont(&fontFamily, 11, FontStyleRegular, UnitPixel);
    Font itemFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush bgBrush(Color(255, 35, 35, 42));
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);
    Pen borderPen(COLOR_BORDER, 1.0f);

    // Dropdown position (below font button)
    int dropX = g_fontButtonRect.left;
    int dropY = g_fontButtonRect.bottom + 2;
    int dropW = g_fontButtonRect.right - g_fontButtonRect.left;
    int searchH = 28;
    int listH = min((int)g_filteredFonts.size(), FONT_DROPDOWN_MAX_ITEMS) * FONT_ITEM_HEIGHT;
    if (listH < FONT_ITEM_HEIGHT) listH = FONT_ITEM_HEIGHT;
    int dropH = searchH + listH;

    g_fontDropdownRect = {dropX, dropY, dropX + dropW, dropY + dropH};

    // Background
    g.FillRectangle(&bgBrush, dropX, dropY, dropW, dropH);
    g.DrawRectangle(&borderPen, dropX, dropY, dropW - 1, dropH - 1);

    // Search box
    SolidBrush searchBgBrush(COLOR_VALUE_BG);
    g.FillRectangle(&searchBgBrush, dropX + 4, dropY + 4, dropW - 8, searchH - 8);

    std::wstring searchDisplay = g_fontSearchText.empty() ? L"Search fonts..." : g_fontSearchText;
    SolidBrush& searchTextBrush = g_fontSearchText.empty() ? dimBrush : textBrush;
    g.DrawString(searchDisplay.c_str(), -1, &searchFont, PointF((REAL)(dropX + 8), (REAL)(dropY + 6)), &searchTextBrush);

    // Font list
    int listY = dropY + searchH;
    int visibleCount = min((int)g_filteredFonts.size() - g_fontScrollOffset, FONT_DROPDOWN_MAX_ITEMS);

    for (int i = 0; i < visibleCount; i++) {
        int idx = g_fontScrollOffset + i;
        if (idx >= (int)g_filteredFonts.size()) break;

        FontInfo* fi = g_filteredFonts[idx];
        int itemY = listY + i * FONT_ITEM_HEIGHT;

        // Hover highlight
        if (idx == g_fontHoverIndex) {
            SolidBrush hoverBrush(COLOR_HOVER);
            g.FillRectangle(&hoverBrush, dropX + 2, itemY, dropW - 4, FONT_ITEM_HEIGHT);
        }

        // Font name (use the font itself for preview if possible)
        g.DrawString(fi->displayName.c_str(), -1, &itemFont,
                     PointF((REAL)(dropX + 8), (REAL)(itemY + 4)), &textBrush);
    }

    // Scroll indicator if needed
    if (g_filteredFonts.size() > FONT_DROPDOWN_MAX_ITEMS) {
        int scrollMax = (int)g_filteredFonts.size() - FONT_DROPDOWN_MAX_ITEMS;
        float scrollRatio = (float)g_fontScrollOffset / scrollMax;
        int scrollBarH = listH * FONT_DROPDOWN_MAX_ITEMS / (int)g_filteredFonts.size();
        int scrollBarY = listY + (int)((listH - scrollBarH) * scrollRatio);

        SolidBrush scrollBrush(Color(128, 100, 100, 120));
        g.FillRectangle(&scrollBrush, dropX + dropW - 6, scrollBarY, 4, scrollBarH);
    }
}

/*****************************************************************************
 * DrawPresetDropdown
 *****************************************************************************/
static void DrawPresetDropdown(Graphics& g) {
    if (!g_presetDropdownOpen) return;

    FontFamily fontFamily(L"Segoe UI");
    Font itemFont(&fontFamily, 11, FontStyleRegular, UnitPixel);
    Font labelFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(Color(255, 140, 140, 140));

    // Dropdown position (below preset button)
    int dropX = g_presetButtonRect.left - 150;  // Wider dropdown, positioned left
    int dropY = g_presetButtonRect.bottom + 2;
    int dropW = 180;

    // Calculate total items: "Save Current" + presets
    int totalItems = 1 + (int)g_textPresets.size();
    int visibleItems = min(totalItems, PRESET_DROPDOWN_MAX_ITEMS);
    int dropH = visibleItems * PRESET_ITEM_HEIGHT + 8;

    // Store dropdown rect for hit testing
    g_presetDropdownRect = {dropX, dropY, dropX + dropW, dropY + dropH};

    // Background with shadow
    SolidBrush shadowBrush(Color(80, 0, 0, 0));
    g.FillRectangle(&shadowBrush, dropX + 3, dropY + 3, dropW, dropH);

    SolidBrush bgBrush(Color(255, 40, 42, 48));
    g.FillRectangle(&bgBrush, dropX, dropY, dropW, dropH);

    // Border
    Pen borderPen(Color(255, 60, 60, 70), 1.0f);
    g.DrawRectangle(&borderPen, dropX, dropY, dropW - 1, dropH - 1);

    // Draw items
    int listY = dropY + 4;
    int itemIndex = -g_presetScrollOffset;  // -1 for "Save Current"

    // "Save Current" item (always at top when scrollOffset is 0)
    if (g_presetScrollOffset == 0) {
        RECT itemRect = {dropX + 4, listY, dropX + dropW - 4, listY + PRESET_ITEM_HEIGHT};

        // Hover highlight
        if (g_presetHoverIndex == -1) {
            SolidBrush hoverBrush(COLOR_HOVER);
            g.FillRectangle(&hoverBrush, itemRect.left, itemRect.top,
                           itemRect.right - itemRect.left, PRESET_ITEM_HEIGHT);
        }

        // Star icon (small)
        Pen starPen(Color(255, 255, 200, 0), 1.0f);
        int starCx = dropX + 18;
        int starCy = listY + PRESET_ITEM_HEIGHT / 2;
        g.DrawLine(&starPen, starCx, starCy - 4, starCx, starCy + 4);
        g.DrawLine(&starPen, starCx - 4, starCy, starCx + 4, starCy);
        g.DrawLine(&starPen, starCx - 3, starCy - 3, starCx + 3, starCy + 3);
        g.DrawLine(&starPen, starCx + 3, starCy - 3, starCx - 3, starCy + 3);

        // Text
        SolidBrush saveBrush(Color(255, 255, 200, 0));
        g.DrawString(L"Save Current Style", -1, &itemFont,
                    PointF((REAL)dropX + 32, (REAL)listY + 5), &saveBrush);

        listY += PRESET_ITEM_HEIGHT;
        itemIndex++;
    }

    // Preset items
    for (int i = max(0, g_presetScrollOffset - 1);
         i < (int)g_textPresets.size() && itemIndex < PRESET_DROPDOWN_MAX_ITEMS;
         i++, itemIndex++) {

        if (itemIndex < 0) continue;

        TextPreset& p = g_textPresets[i];
        RECT itemRect = {dropX + 4, listY, dropX + dropW - 4, listY + PRESET_ITEM_HEIGHT};

        // Hover highlight
        if (g_presetHoverIndex == i) {
            SolidBrush hoverBrush(COLOR_HOVER);
            g.FillRectangle(&hoverBrush, itemRect.left, itemRect.top,
                           itemRect.right - itemRect.left, PRESET_ITEM_HEIGHT);
        }

        // Preset name
        StringFormat sf;
        sf.SetTrimming(StringTrimmingEllipsisCharacter);
        RectF textRect((REAL)dropX + 12, (REAL)listY + 2, (REAL)dropW - 50, (REAL)PRESET_ITEM_HEIGHT - 4);
        g.DrawString(p.name.c_str(), -1, &itemFont, textRect, &sf, &textBrush);

        // Font info (smaller, dim)
        std::wstring fontInfo = p.font.substr(0, 15);
        if (p.font.length() > 15) fontInfo += L"...";
        g.DrawString(fontInfo.c_str(), -1, &labelFont,
                    PointF((REAL)dropX + 12, (REAL)listY + 14), &dimBrush);

        // Delete button (X) on hover
        if (g_presetHoverIndex == i) {
            int delX = dropX + dropW - 24;
            int delY = listY + PRESET_ITEM_HEIGHT / 2;
            Pen delPen(Color(255, 200, 80, 80), 1.5f);
            g.DrawLine(&delPen, delX - 4, delY - 4, delX + 4, delY + 4);
            g.DrawLine(&delPen, delX + 4, delY - 4, delX - 4, delY + 4);
        }

        listY += PRESET_ITEM_HEIGHT;
    }

    // Scrollbar if needed
    if (totalItems > PRESET_DROPDOWN_MAX_ITEMS) {
        int scrollMax = totalItems - PRESET_DROPDOWN_MAX_ITEMS;
        int listH = PRESET_DROPDOWN_MAX_ITEMS * PRESET_ITEM_HEIGHT;
        float scrollRatio = (float)g_presetScrollOffset / scrollMax;
        int scrollBarH = listH * PRESET_DROPDOWN_MAX_ITEMS / totalItems;
        int scrollBarY = dropY + 4 + (int)((listH - scrollBarH) * scrollRatio);

        SolidBrush scrollBrush(Color(128, 100, 100, 120));
        g.FillRectangle(&scrollBrush, dropX + dropW - 6, scrollBarY, 4, scrollBarH);
    }
}

/*****************************************************************************
 * Text Preset Functions
 *****************************************************************************/
static void DeletePreset(int index) {
    if (index < 0 || index >= (int)g_textPresets.size()) return;
    g_textPresets.erase(g_textPresets.begin() + index);
    SavePresetsToFile();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void LoadPresets() {
    // TODO: Load from file ~/.ae-anchor/text-presets.json
    g_textPresets.clear();
}

static void SavePresetsToFile() {
    // TODO: Save to file ~/.ae-anchor/text-presets.json
}

static void SavePreset(const std::wstring& name) {
    TextPreset preset;
    preset.name = name;
    wcscpy_s((wchar_t*)preset.font.c_str(), 128, g_textInfo.font);
    preset.font = g_textInfo.font;
    preset.fontSize = g_textInfo.fontSize;
    preset.tracking = g_textInfo.tracking;
    preset.leading = g_textInfo.leading;
    preset.strokeWidth = g_textInfo.strokeWidth;
    preset.fillColor[0] = g_textInfo.fillColor[0];
    preset.fillColor[1] = g_textInfo.fillColor[1];
    preset.fillColor[2] = g_textInfo.fillColor[2];
    preset.strokeColor[0] = g_textInfo.strokeColor[0];
    preset.strokeColor[1] = g_textInfo.strokeColor[1];
    preset.strokeColor[2] = g_textInfo.strokeColor[2];
    preset.applyFill = g_textInfo.applyFill;
    preset.applyStroke = g_textInfo.applyStroke;
    preset.justify = (int)g_textInfo.justify;

    g_textPresets.push_back(preset);
    SavePresetsToFile();
}

static void ApplyPreset(int index) {
    if (index < 0 || index >= (int)g_textPresets.size()) return;

    TextPreset& p = g_textPresets[index];

    // Apply font
    char psName[256];
    WideCharToMultiByte(CP_UTF8, 0, p.font.c_str(), -1, psName, sizeof(psName), NULL, NULL);
    ApplyTextFont(psName);
    wcscpy_s(g_textInfo.font, 128, p.font.c_str());

    // Apply other properties
    ApplyTextPropertyValue("fontSize", p.fontSize);
    g_textInfo.fontSize = p.fontSize;

    ApplyTextPropertyValue("tracking", p.tracking);
    g_textInfo.tracking = p.tracking;

    ApplyTextPropertyValue("leading", p.leading);
    g_textInfo.leading = p.leading;

    ApplyTextPropertyValue("strokeWidth", p.strokeWidth);
    g_textInfo.strokeWidth = p.strokeWidth;

    if (p.applyFill) {
        ApplyTextColorValue(false, p.fillColor[0], p.fillColor[1], p.fillColor[2]);
        g_textInfo.fillColor[0] = p.fillColor[0];
        g_textInfo.fillColor[1] = p.fillColor[1];
        g_textInfo.fillColor[2] = p.fillColor[2];
    }

    if (p.applyStroke) {
        ApplyTextColorValue(true, p.strokeColor[0], p.strokeColor[1], p.strokeColor[2]);
        g_textInfo.strokeColor[0] = p.strokeColor[0];
        g_textInfo.strokeColor[1] = p.strokeColor[1];
        g_textInfo.strokeColor[2] = p.strokeColor[2];
    }

    ApplyTextJustificationValue(p.justify);
    g_textInfo.justify = (Justification)p.justify;

    g_result.applied = true;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void DrawPresetSection(Graphics& g, int& y) {
    // Presets are shown via the star button in font section
    // This function is for future expansion
}

/*****************************************************************************
 * Color Picker - Simple HSV picker
 *****************************************************************************/
static const int PICKER_WIDTH = 200;
static const int PICKER_HEIGHT = 180;
static const int HUE_BAR_HEIGHT = 20;
static const int SV_SIZE = 150;

// HSV to RGB conversion
static void HSVtoRGB(float h, float s, float v, float* r, float* g, float* b) {
    int i = (int)(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);
    switch (i % 6) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        case 5: *r = v; *g = p; *b = q; break;
    }
}

// RGB to HSV conversion
static void RGBtoHSV(float r, float g, float b, float* h, float* s, float* v) {
    float maxC = max(r, max(g, b));
    float minC = min(r, min(g, b));
    *v = maxC;
    float delta = maxC - minC;
    if (delta < 0.00001f) {
        *s = 0;
        *h = 0;
        return;
    }
    *s = delta / maxC;
    if (r >= maxC) *h = (g - b) / delta;
    else if (g >= maxC) *h = 2.0f + (b - r) / delta;
    else *h = 4.0f + (r - g) / delta;
    *h /= 6.0f;
    if (*h < 0) *h += 1.0f;
}

static float g_pickerH = 0, g_pickerS = 1, g_pickerV = 1;
static bool g_pickerDraggingSV = false;
static bool g_pickerDraggingHue = false;

// Bitmap caching for performance (using DIB for fast pixel access)
static HBITMAP g_svBitmap = NULL;
static HBITMAP g_hueBitmap = NULL;
static float g_cachedHue = -1.0f;  // Track when SV needs redraw
static BYTE* g_svPixels = NULL;    // Direct pixel access for SV
static BYTE* g_huePixels = NULL;   // Direct pixel access for Hue

static void CreateSVBitmap(float hue) {
    if (g_svBitmap) {
        DeleteObject(g_svBitmap);
        g_svBitmap = NULL;
        g_svPixels = NULL;
    }

    // Create DIB for direct pixel access
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = SV_SIZE;
    bmi.bmiHeader.biHeight = -SV_SIZE;  // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDC = GetDC(NULL);
    g_svBitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, (void**)&g_svPixels, NULL, 0);
    ReleaseDC(NULL, screenDC);

    if (g_svPixels) {
        // Direct pixel write - much faster than SetPixel
        for (int py = 0; py < SV_SIZE; py++) {
            BYTE* row = g_svPixels + py * SV_SIZE * 4;
            for (int px = 0; px < SV_SIZE; px++) {
                float s = (float)px / SV_SIZE;
                float v = 1.0f - (float)py / SV_SIZE;
                float r, g, b;
                HSVtoRGB(hue, s, v, &r, &g, &b);
                row[px * 4 + 0] = (BYTE)(b * 255);  // Blue
                row[px * 4 + 1] = (BYTE)(g * 255);  // Green
                row[px * 4 + 2] = (BYTE)(r * 255);  // Red
                row[px * 4 + 3] = 255;              // Alpha
            }
        }
    }
    g_cachedHue = hue;
}

static void CreateHueBitmap() {
    if (g_hueBitmap) return;  // Only create once

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = SV_SIZE;
    bmi.bmiHeader.biHeight = -HUE_BAR_HEIGHT;  // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDC = GetDC(NULL);
    g_hueBitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, (void**)&g_huePixels, NULL, 0);
    ReleaseDC(NULL, screenDC);

    if (g_huePixels) {
        for (int py = 0; py < HUE_BAR_HEIGHT; py++) {
            BYTE* row = g_huePixels + py * SV_SIZE * 4;
            for (int px = 0; px < SV_SIZE; px++) {
                float h = (float)px / SV_SIZE;
                float r, g, b;
                HSVtoRGB(h, 1, 1, &r, &g, &b);
                row[px * 4 + 0] = (BYTE)(b * 255);
                row[px * 4 + 1] = (BYTE)(g * 255);
                row[px * 4 + 2] = (BYTE)(r * 255);
                row[px * 4 + 3] = 255;
            }
        }
    }
}

void ShowColorPicker(bool forStroke, int x, int y) {
    g_colorPickerForStroke = forStroke;
    g_colorPickerOpen = true;  // Prevent main window from closing

    // Get current color and convert to HSV
    float* currentColor = forStroke ? g_textInfo.strokeColor : g_textInfo.fillColor;
    RGBtoHSV(currentColor[0], currentColor[1], currentColor[2], &g_pickerH, &g_pickerS, &g_pickerV);

    // Create picker window if needed
    if (!g_colorPickerHwnd) {
        g_colorPickerHwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"TextUIColorPicker",
            L"Color",
            WS_POPUP,
            x, y, Scaled(PICKER_WIDTH), Scaled(PICKER_HEIGHT),
            g_hwnd, NULL, GetModuleHandle(NULL), NULL
        );
    }

    if (g_colorPickerHwnd) {
        SetWindowPos(g_colorPickerHwnd, HWND_TOPMOST, x, y,
                     Scaled(PICKER_WIDTH), Scaled(PICKER_HEIGHT), SWP_SHOWWINDOW);
        ShowWindow(g_colorPickerHwnd, SW_SHOW);
        InvalidateRect(g_colorPickerHwnd, NULL, TRUE);
    }
}

void HideColorPicker() {
    g_colorPickerOpen = false;  // Allow main window to close again
    if (g_colorPickerHwnd) {
        ShowWindow(g_colorPickerHwnd, SW_HIDE);
    }
    g_pickerDraggingSV = false;
    g_pickerDraggingHue = false;
}

bool IsColorPickerVisible() {
    return g_colorPickerHwnd && IsWindowVisible(g_colorPickerHwnd);
}

static void DrawColorPicker(HDC hdc) {
    // Ensure bitmaps are created/updated
    CreateHueBitmap();
    if (g_cachedHue != g_pickerH) {
        CreateSVBitmap(g_pickerH);
    }

    int padding = 8;
    int svX = padding, svY = padding;
    int hueY = svY + SV_SIZE + 6;

    // Create scaled dimensions
    int scaledSvX = (int)(svX * g_scaleFactor);
    int scaledSvY = (int)(svY * g_scaleFactor);
    int scaledSvSize = (int)(SV_SIZE * g_scaleFactor);
    int scaledHueY = (int)(hueY * g_scaleFactor);
    int scaledHueH = (int)(HUE_BAR_HEIGHT * g_scaleFactor);

    // Draw background
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.ScaleTransform(g_scaleFactor, g_scaleFactor);

    SolidBrush bgBrush(Color(255, 35, 35, 42));
    g.FillRectangle(&bgBrush, 0, 0, PICKER_WIDTH, PICKER_HEIGHT);

    Pen borderPen(Color(255, 80, 80, 90), 1);
    g.DrawRectangle(&borderPen, 0, 0, PICKER_WIDTH - 1, PICKER_HEIGHT - 1);

    // Reset transform to draw bitmaps at actual pixel positions
    g.ResetTransform();

    // Draw cached SV bitmap (stretched to scaled size)
    if (g_svBitmap) {
        HDC memDC = CreateCompatibleDC(hdc);
        SelectObject(memDC, g_svBitmap);
        StretchBlt(hdc, scaledSvX, scaledSvY, scaledSvSize, scaledSvSize,
                   memDC, 0, 0, SV_SIZE, SV_SIZE, SRCCOPY);
        DeleteDC(memDC);
    }

    // Draw cached Hue bitmap
    if (g_hueBitmap) {
        HDC memDC = CreateCompatibleDC(hdc);
        SelectObject(memDC, g_hueBitmap);
        StretchBlt(hdc, scaledSvX, scaledHueY, scaledSvSize, scaledHueH,
                   memDC, 0, 0, SV_SIZE, HUE_BAR_HEIGHT, SRCCOPY);
        DeleteDC(memDC);
    }

    // Restore scale for cursors and preview
    g.ScaleTransform(g_scaleFactor, g_scaleFactor);

    // SV cursor
    int svCursorX = svX + (int)(g_pickerS * SV_SIZE);
    int svCursorY = svY + (int)((1.0f - g_pickerV) * SV_SIZE);
    Pen cursorPen(Color(255, 255, 255, 255), 2);
    g.DrawEllipse(&cursorPen, svCursorX - 5, svCursorY - 5, 10, 10);
    Pen cursorPenInner(Color(255, 0, 0, 0), 1);
    g.DrawEllipse(&cursorPenInner, svCursorX - 4, svCursorY - 4, 8, 8);

    // Hue cursor
    int hueBarWidth = SV_SIZE;
    int hueCursorX = svX + (int)(g_pickerH * hueBarWidth);
    Pen hueCursorPen(Color(255, 255, 255, 255), 2);
    g.DrawRectangle(&hueCursorPen, hueCursorX - 2, hueY - 1, 4, HUE_BAR_HEIGHT + 2);

    // Current color preview
    int previewX = svX + SV_SIZE + 8;
    int previewSize = 30;
    float r, gVal, b;
    HSVtoRGB(g_pickerH, g_pickerS, g_pickerV, &r, &gVal, &b);
    SolidBrush previewBrush(Color(255, (BYTE)(r * 255), (BYTE)(gVal * 255), (BYTE)(b * 255)));
    g.FillRectangle(&previewBrush, previewX, svY, previewSize, previewSize);
    g.DrawRectangle(&borderPen, previewX, svY, previewSize - 1, previewSize - 1);
}

static void ApplyPickerColor() {
    float r, g, b;
    HSVtoRGB(g_pickerH, g_pickerS, g_pickerV, &r, &g, &b);
    ApplyTextColor(g_colorPickerForStroke, r, g, b);
}

static LRESULT CALLBACK ColorPickerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    int padding = 8;
    int svX = padding, svY = padding;
    int hueY = svY + SV_SIZE + 6;

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);
        DrawColorPicker(memDC);
        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = InverseScaled(GET_X_LPARAM(lParam));
        int y = InverseScaled(GET_Y_LPARAM(lParam));

        // Check SV area
        if (x >= svX && x < svX + SV_SIZE && y >= svY && y < svY + SV_SIZE) {
            g_pickerDraggingSV = true;
            g_pickerS = (float)(x - svX) / SV_SIZE;
            g_pickerV = 1.0f - (float)(y - svY) / SV_SIZE;
            g_pickerS = max(0.0f, min(1.0f, g_pickerS));
            g_pickerV = max(0.0f, min(1.0f, g_pickerV));
            SetCapture(hwnd);
            ApplyPickerColor();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        // Check Hue bar
        else if (x >= svX && x < svX + SV_SIZE && y >= hueY && y < hueY + HUE_BAR_HEIGHT) {
            g_pickerDraggingHue = true;
            g_pickerH = (float)(x - svX) / SV_SIZE;
            g_pickerH = max(0.0f, min(1.0f, g_pickerH));
            SetCapture(hwnd);
            ApplyPickerColor();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!g_pickerDraggingSV && !g_pickerDraggingHue) return 0;
        int x = InverseScaled(GET_X_LPARAM(lParam));
        int y = InverseScaled(GET_Y_LPARAM(lParam));

        if (g_pickerDraggingSV) {
            g_pickerS = (float)(x - svX) / SV_SIZE;
            g_pickerV = 1.0f - (float)(y - svY) / SV_SIZE;
            g_pickerS = max(0.0f, min(1.0f, g_pickerS));
            g_pickerV = max(0.0f, min(1.0f, g_pickerV));
            ApplyPickerColor();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (g_pickerDraggingHue) {
            g_pickerH = (float)(x - svX) / SV_SIZE;
            g_pickerH = max(0.0f, min(1.0f, g_pickerH));
            ApplyPickerColor();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP:
        g_pickerDraggingSV = false;
        g_pickerDraggingHue = false;
        ReleaseCapture();
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
bool NeedsRefresh() { return false; }
void ShowColorPicker(bool forStroke, int x, int y) { (void)forStroke; (void)x; (void)y; }
void HideColorPicker() {}
bool IsColorPickerVisible() { return false; }

} // namespace TextUI

#endif // MSWindows
