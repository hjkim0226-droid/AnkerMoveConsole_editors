/*****************************************************************************
 * ControlUI.cpp
 *
 * Native Windows UI for Anchor Snap - Control Module
 * Effect search popup with keyboard input support
 *****************************************************************************/

#include "ControlUI.h"

#ifdef MSWindows

#include "GdiPlusIncludes.h"
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

using namespace Gdiplus;

// External function from SnapPlugin.cpp to get module scale factor
extern float GetModuleScaleFactor(const char* moduleName);

// Current scale factor for this module (1.0 = 100%, 0.8 = 80%, 1.7 = 170%)
static float g_scaleFactor = 1.0f;

// Helper: scale an integer value by the current scale factor
inline int Scaled(int baseValue) {
    return (int)(baseValue * g_scaleFactor);
}

// Helper: inverse scale for mouse coordinates
inline int InverseScaled(int screenValue) {
    return (int)(screenValue / g_scaleFactor);
}

// GDI+ token
static ULONG_PTR g_gdiplusToken = 0;

// Window class name
static const wchar_t* CONTROL_CLASS_NAME = L"AnchorSnapControlClass";

// Window handle
static HWND g_hwnd = NULL;
static bool g_isVisible = false;

// Panel mode
static ControlUI::PanelMode g_panelMode = ControlUI::MODE_SEARCH;

// Search state (Mode 1)
static wchar_t g_searchQuery[256] = {0};
static std::vector<ControlUI::EffectItem> g_searchResults;
static int g_selectedIndex = 0;
static int g_hoverIndex = -1;

// Layer effects state (Mode 2)
static std::vector<ControlUI::EffectItem> g_layerEffects;
static int g_hoveredAction = -1;  // Which action button is hovered (0-3 for each effect row)

// Settings
static ControlUI::ControlSettings g_settings;

// Result
static ControlUI::ControlResult g_result;

// UI Constants
static const int WINDOW_WIDTH = 320;
static const int SEARCH_HEIGHT = 36;
static const int HEADER_HEIGHT = 32;  // Mode 2 header
static const int PRESET_BAR_HEIGHT = 36;  // Mode 2 preset buttons
static const int ITEM_HEIGHT = 32;
static const int PADDING = 8;
static const int MAX_VISIBLE_ITEMS = 8;
static const int CLOSE_BUTTON_SIZE = 20;
static const int ACTION_BUTTON_SIZE = 20;  // Delete, duplicate, move buttons
static const int PRESET_BUTTON_WIDTH = 40;
static const int PRESET_BUTTON_HEIGHT = 28;

// Colors
static const Color COLOR_BG(240, 28, 28, 32);
static const Color COLOR_SEARCH_BG(255, 40, 40, 48);
static const Color COLOR_ITEM_HOVER(255, 60, 80, 100);
static const Color COLOR_ITEM_SELECTED(255, 74, 158, 255);
static const Color COLOR_TEXT(255, 220, 220, 220);
static const Color COLOR_TEXT_DIM(255, 140, 140, 140);
static const Color COLOR_ACCENT(255, 74, 207, 255);
static const Color COLOR_BORDER(255, 60, 60, 70);
static const Color COLOR_CLOSE_HOVER(255, 200, 60, 60);
static const Color COLOR_PRESET_BG(255, 50, 50, 60);
static const Color COLOR_PRESET_HOVER(255, 70, 100, 130);
static const Color COLOR_PRESET_ACTIVE(255, 74, 158, 255);

// New EC window button state (Mode 2)
static bool g_newECButtonHover = false;
static const int NEW_EC_BUTTON_SIZE = 20;

// Preset icons
enum PresetIcon {
    ICON_NONE = 0,      // Empty slot (no preset saved)
    ICON_COLOR,         // Color wheel
    ICON_BLUR,          // Blur circles
    ICON_DISTORT,       // Wave
    ICON_STAR,          // Star
    ICON_LIGHTNING,     // Lightning bolt
    ICON_MAGIC,         // Magic wand/sparkles
    ICON_COUNT          // Number of icons (excluding NONE)
};

// Preset button state (Mode 2)
static int g_hoveredPresetButton = -1;  // -1 = none, 0-5 = slot buttons
static const int PRESET_SLOT_COUNT = 6;
static PresetIcon g_presetIcons[PRESET_SLOT_COUNT] = {ICON_NONE, ICON_NONE, ICON_NONE, ICON_NONE, ICON_NONE, ICON_NONE};

// Icon selection state
static bool g_showingIconSelector = false;
static int g_iconSelectorForSlot = -1;  // Which slot is selecting icon
static int g_hoveredIcon = -1;          // Hovered icon in selector

// Long press state for icon change
static UINT_PTR g_longPressTimerId = 0;
static int g_longPressSlot = -1;
static const UINT LONG_PRESS_MS = 500;  // 0.5 second hold to change icon

// Save mode state (Mode 2)
static bool g_saveMode = false;         // When true, clicking preset button saves
static bool g_saveButtonHover = false;  // Hover state for save button
static const int SAVE_BUTTON_SIZE = 28; // Square button to match preset height

// Delayed close timer (prevent immediate close on focus loss)
static UINT_PTR g_closeTimerId = 0;
static const UINT CLOSE_DELAY_MS = 100;

// Text cursor (caret) state
static int g_cursorPosition = 0;        // Cursor position in search query
static int g_selectionStart = -1;       // Selection start (-1 = no selection)
static int g_selectionEnd = -1;         // Selection end
static bool g_cursorVisible = true;     // Cursor blink state
static UINT_PTR g_cursorTimerId = 0;    // Timer for cursor blinking
static const UINT CURSOR_BLINK_MS = 530; // Cursor blink interval

// Window drag state
static bool g_isDragging = false;       // True while window is being dragged

// Wait for Shift+E release before accepting input
static bool g_waitingForKeyRelease = false;

// Keep panel open option (persisted)
static bool g_keepPanelOpen = false;
static bool g_keepOpenButtonHover = false;

// Current layer info for header display
static wchar_t g_currentLayerName[256] = L"";
static int g_currentLayerLabelColor = 0;  // 0-16 AE label colors

// Label colors (AE default palette)
static const Color LABEL_COLORS[] = {
    Color(255, 128, 128, 128),  // 0: None (gray)
    Color(255, 255, 50, 50),    // 1: Red
    Color(255, 255, 200, 50),   // 2: Yellow
    Color(255, 180, 220, 140),  // 3: Aqua
    Color(255, 255, 180, 200),  // 4: Pink
    Color(255, 200, 180, 255),  // 5: Lavender
    Color(255, 255, 200, 150),  // 6: Peach
    Color(255, 200, 200, 220),  // 7: Sea Foam
    Color(255, 120, 180, 255),  // 8: Blue
    Color(255, 120, 255, 120),  // 9: Green
    Color(255, 200, 120, 255),  // 10: Purple
    Color(255, 255, 160, 80),   // 11: Orange
    Color(255, 165, 120, 80),   // 12: Brown
    Color(255, 255, 120, 200),  // 13: Fuchsia
    Color(255, 80, 200, 180),   // 14: Cyan
    Color(255, 180, 200, 120),  // 15: Sandstone
    Color(255, 100, 160, 100),  // 16: Dark Green
};

// Dynamic effects list (loaded from AE - localized names)
static std::vector<ControlUI::EffectItem> g_availableEffects;

// Built-in effects list (fallback if dynamic list not loaded)
static const wchar_t* BUILTIN_EFFECTS[][3] = {
    // Name, MatchName, Category
    {L"Gaussian Blur", L"ADBE Gaussian Blur 2", L"Blur & Sharpen"},
    {L"Fast Box Blur", L"ADBE Box Blur2", L"Blur & Sharpen"},
    {L"Directional Blur", L"ADBE Motion Blur", L"Blur & Sharpen"},
    {L"Radial Blur", L"ADBE Radial Blur", L"Blur & Sharpen"},
    {L"Sharpen", L"ADBE Sharpen", L"Blur & Sharpen"},
    {L"Unsharp Mask", L"ADBE Unsharp Mask2", L"Blur & Sharpen"},

    {L"Curves", L"ADBE CurvesCustom", L"Color Correction"},
    {L"Levels", L"ADBE Easy Levels2", L"Color Correction"},
    {L"Hue/Saturation", L"ADBE HUE SATURATION", L"Color Correction"},
    {L"Brightness & Contrast", L"ADBE Brightness & Contrast 2", L"Color Correction"},
    {L"Exposure", L"ADBE Exposure2", L"Color Correction"},
    {L"Vibrance", L"ADBE Vibrance", L"Color Correction"},
    {L"Color Balance", L"ADBE Color Balance 2", L"Color Correction"},
    {L"Tint", L"ADBE Tint", L"Color Correction"},
    {L"Tritone", L"ADBE Tritone", L"Color Correction"},
    {L"Lumetri Color", L"ADBE Lumetri", L"Color Correction"},

    {L"Drop Shadow", L"ADBE Drop Shadow", L"Perspective"},
    {L"Bevel Alpha", L"ADBE Bevel Alpha", L"Perspective"},

    {L"Glow", L"ADBE Glo2", L"Stylize"},
    {L"Noise", L"ADBE Noise", L"Noise & Grain"},
    {L"Fractal Noise", L"ADBE Fractal Noise", L"Noise & Grain"},

    {L"Fill", L"ADBE Fill", L"Generate"},
    {L"Gradient Ramp", L"ADBE Ramp", L"Generate"},
    {L"4-Color Gradient", L"ADBE 4ColorGradient", L"Generate"},

    {L"Transform", L"ADBE Geometry2", L"Distort"},
    {L"Corner Pin", L"ADBE Corner Pin", L"Distort"},
    {L"Mesh Warp", L"ADBE MESH WARP", L"Distort"},
    {L"Optics Compensation", L"ADBE Optics Compensation", L"Distort"},
    {L"Turbulent Displace", L"ADBE Turbulent Displace", L"Distort"},

    {L"Set Matte", L"ADBE Set Matte3", L"Channel"},
    {L"Shift Channels", L"ADBE Shift Channels", L"Channel"},

    {L"CC Composite", L"CC Composite", L"Channel"},

    {L"Linear Wipe", L"ADBE Linear Wipe", L"Transition"},
    {L"Radial Wipe", L"ADBE Radial Wipe", L"Transition"},

    {L"Echo", L"ADBE Echo", L"Time"},
    {L"Posterize Time", L"ADBE Posterize Time", L"Time"},

    {NULL, NULL, NULL} // Terminator
};

// Forward declarations
LRESULT CALLBACK ControlWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void DrawControlPanel(HDC hdc, int width, int height);
void DrawEffectsPanel(HDC hdc, int width, int height);
void PerformSearch(const wchar_t* query);
void ParseLayerEffects(const wchar_t* effectList);
void ParseAvailableEffects(const wchar_t* effectList);

namespace ControlUI {

void Initialize() {
    if (g_gdiplusToken == 0) {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    }

    // Register window class
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = ControlWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CONTROL_CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);
}

void Shutdown() {
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }
    UnregisterClassW(CONTROL_CLASS_NAME, GetModuleHandle(NULL));

    if (g_gdiplusToken) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

void SetMode(PanelMode mode) {
    g_panelMode = mode;
}

PanelMode GetMode() {
    return g_panelMode;
}

void ShowPanelAt(int x, int y) {
    if (g_isVisible) return;

    // Get module scale factor from settings
    g_scaleFactor = GetModuleScaleFactor("control");

    // Reset state based on mode
    g_selectedIndex = 0;
    g_hoverIndex = -1;
    g_result = ControlResult();

    // Reset cursor state
    g_cursorPosition = 0;
    g_selectionStart = -1;
    g_selectionEnd = -1;
    g_cursorVisible = true;
    g_isDragging = false;
    g_waitingForKeyRelease = true;  // Don't accept input until Shift+E is released

    int itemCount = 0;
    int headerHeight = 0;

    // Always clear search query when opening panel
    g_searchQuery[0] = L'\0';
    g_searchResults.clear();

    if (g_panelMode == MODE_SEARCH) {
        PerformSearch(L"");
        itemCount = min((int)g_searchResults.size(), MAX_VISIBLE_ITEMS);
        headerHeight = SEARCH_HEIGHT;
    } else {
        // Mode 2: Effects list with preset bar and search
        itemCount = min((int)g_layerEffects.size(), MAX_VISIBLE_ITEMS);
        headerHeight = HEADER_HEIGHT + PRESET_BAR_HEIGHT + SEARCH_HEIGHT;  // Header + presets + search
        if (itemCount == 0) itemCount = 1; // Show "No effects" message
        g_hoveredPresetButton = -1;
    }

    int windowHeight = headerHeight + PADDING * 2 + itemCount * ITEM_HEIGHT + PADDING;

    // Calculate scaled dimensions
    int scaledWidth = Scaled(WINDOW_WIDTH);
    int scaledHeight = Scaled(windowHeight);

    // Calculate Y offset to center the search bar at mouse position
    int searchBarCenterY;
    if (g_panelMode == MODE_SEARCH) {
        // Mode 1: Search bar at top
        searchBarCenterY = Scaled(PADDING + SEARCH_HEIGHT / 2);
    } else {
        // Mode 2: Search bar below header and preset bar
        searchBarCenterY = Scaled(PADDING + HEADER_HEIGHT + 4 + PRESET_BAR_HEIGHT + SEARCH_HEIGHT / 2);
    }

    // Calculate initial window position (centered on mouse)
    int windowX = x - scaledWidth / 2;
    int windowY = y - searchBarCenterY;

    // Clamp window position to stay within monitor bounds
    POINT mousePoint = { x, y };
    HMONITOR hMonitor = MonitorFromPoint(mousePoint, MONITOR_DEFAULTTONEAREST);
    if (hMonitor) {
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (GetMonitorInfo(hMonitor, &mi)) {
            RECT workArea = mi.rcWork;  // Working area excludes taskbar

            // Clamp X: ensure window stays within horizontal bounds
            if (windowX < workArea.left) {
                windowX = workArea.left;
            } else if (windowX + scaledWidth > workArea.right) {
                windowX = workArea.right - scaledWidth;
            }

            // Clamp Y: ensure window stays within vertical bounds
            if (windowY < workArea.top) {
                windowY = workArea.top;
            } else if (windowY + scaledHeight > workArea.bottom) {
                windowY = workArea.bottom - scaledHeight;
            }
        }
    }

    // Create or reposition window with scaled dimensions
    if (!g_hwnd) {
        g_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            CONTROL_CLASS_NAME,
            g_panelMode == MODE_SEARCH ? L"Effect Search" : L"Layer Effects",
            WS_POPUP,
            windowX,
            windowY,
            scaledWidth,
            scaledHeight,
            NULL, NULL,
            GetModuleHandle(NULL),
            NULL
        );
        SetLayeredWindowAttributes(g_hwnd, 0, 245, LWA_ALPHA);
    } else {
        SetWindowPos(g_hwnd, HWND_TOPMOST,
                     windowX,
                     windowY,
                     scaledWidth, scaledHeight,
                     SWP_SHOWWINDOW);
    }

    ShowWindow(g_hwnd, SW_SHOW);
    SetFocus(g_hwnd);
    g_isVisible = true;

    // Start cursor blink timer
    g_cursorTimerId = SetTimer(g_hwnd, 2, CURSOR_BLINK_MS, NULL);

    InvalidateRect(g_hwnd, NULL, TRUE);
}

void ShowPanel() {
    // Get mouse position and delegate to ShowPanelAt
    POINT pt;
    GetCursorPos(&pt);
    ShowPanelAt(pt.x, pt.y);
}

void HidePanel() {
    if (!g_isVisible) return;

    // Stop cursor blink timer
    if (g_cursorTimerId) {
        KillTimer(g_hwnd, g_cursorTimerId);
        g_cursorTimerId = 0;
    }

    ShowWindow(g_hwnd, SW_HIDE);
    g_isVisible = false;
    g_saveMode = false;  // Reset save mode when panel closes
}

bool IsVisible() {
    return g_isVisible;
}

ControlResult GetResult() {
    return g_result;
}

ControlSettings& GetSettings() {
    return g_settings;
}

void UpdateSearch(const wchar_t* query) {
    wcscpy_s(g_searchQuery, query);
    PerformSearch(query);

    // Resize window based on mode
    if (g_hwnd) {
        int itemCount;
        int windowHeight;

        if (g_panelMode == MODE_EFFECTS) {
            // Mode 2: Header + Preset bar + Search + Items
            if (wcslen(g_searchQuery) > 0) {
                itemCount = min((int)g_searchResults.size(), MAX_VISIBLE_ITEMS);
                if (itemCount == 0) itemCount = 1;  // "No matching effects" message
            } else {
                itemCount = min((int)g_layerEffects.size(), MAX_VISIBLE_ITEMS);
                if (itemCount == 0) itemCount = 1;  // "No effects on layer" message
            }
            windowHeight = HEADER_HEIGHT + PRESET_BAR_HEIGHT + SEARCH_HEIGHT +
                           PADDING * 4 + itemCount * ITEM_HEIGHT;
        } else {
            // Mode 1: Search + Items
            itemCount = min((int)g_searchResults.size(), MAX_VISIBLE_ITEMS);
            windowHeight = SEARCH_HEIGHT + PADDING * 2 + itemCount * ITEM_HEIGHT + PADDING;
        }

        SetWindowPos(g_hwnd, NULL, 0, 0, WINDOW_WIDTH, windowHeight,
                     SWP_NOMOVE | SWP_NOZORDER);
    }

    InvalidateRect(g_hwnd, NULL, TRUE);
}

void SetAvailableEffects(const wchar_t* effectList) {
    ParseAvailableEffects(effectList);
}

void SetLayerEffects(const wchar_t* effectList) {
    ParseLayerEffects(effectList);
}

void ClearLayerEffects() {
    g_layerEffects.clear();
}

} // namespace ControlUI

// Search implementation with priority: starts-with > contains
void PerformSearch(const wchar_t* query) {
    g_searchResults.clear();

    std::wstring q(query);
    // Convert to lowercase for case-insensitive search
    std::transform(q.begin(), q.end(), q.begin(), ::towlower);

    // Two lists: starts-with matches first, then contains matches
    std::vector<ControlUI::EffectItem> startsWithMatches;
    std::vector<ControlUI::EffectItem> containsMatches;

    int idx = 0;

    // Helper lambda to check and categorize matches
    auto checkAndAdd = [&](const wchar_t* name, const wchar_t* matchName, const wchar_t* category) {
        std::wstring nameLower(name);
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);

        if (q.empty()) {
            // Empty query: add all
            ControlUI::EffectItem item;
            wcscpy_s(item.name, name);
            wcscpy_s(item.matchName, matchName);
            wcscpy_s(item.category, category);
            item.index = idx++;
            item.isLayerEffect = false;
            startsWithMatches.push_back(item);
        } else {
            size_t pos = nameLower.find(q);
            if (pos == 0) {
                // Starts with query - highest priority
                ControlUI::EffectItem item;
                wcscpy_s(item.name, name);
                wcscpy_s(item.matchName, matchName);
                wcscpy_s(item.category, category);
                item.index = idx++;
                item.isLayerEffect = false;
                startsWithMatches.push_back(item);
            } else if (pos != std::wstring::npos) {
                // Contains query - lower priority
                ControlUI::EffectItem item;
                wcscpy_s(item.name, name);
                wcscpy_s(item.matchName, matchName);
                wcscpy_s(item.category, category);
                item.index = idx++;
                item.isLayerEffect = false;
                containsMatches.push_back(item);
            }
        }
    };

    // Use dynamic effects list if available, otherwise fallback to built-in
    if (!g_availableEffects.empty()) {
        for (const auto& effect : g_availableEffects) {
            checkAndAdd(effect.name, effect.matchName, effect.category);
        }
    } else {
        for (int i = 0; BUILTIN_EFFECTS[i][0] != NULL; i++) {
            checkAndAdd(BUILTIN_EFFECTS[i][0], BUILTIN_EFFECTS[i][1], BUILTIN_EFFECTS[i][2]);
        }
    }

    // Combine: starts-with first, then contains
    for (const auto& item : startsWithMatches) {
        g_searchResults.push_back(item);
        if (g_searchResults.size() >= 20) break;
    }
    if (g_searchResults.size() < 20) {
        for (const auto& item : containsMatches) {
            g_searchResults.push_back(item);
            if (g_searchResults.size() >= 20) break;
        }
    }

    g_selectedIndex = 0;
}

// Parse available effects from string format: "displayName|matchName|category;..."
void ParseAvailableEffects(const wchar_t* effectList) {
    g_availableEffects.clear();

    if (!effectList || wcslen(effectList) == 0) return;

    std::wstring list(effectList);
    size_t pos = 0;
    int idx = 0;

    while (pos < list.length()) {
        size_t end = list.find(L';', pos);
        if (end == std::wstring::npos) end = list.length();

        std::wstring item = list.substr(pos, end - pos);
        if (!item.empty()) {
            size_t sep1 = item.find(L'|');
            size_t sep2 = item.find(L'|', sep1 + 1);

            if (sep1 != std::wstring::npos && sep2 != std::wstring::npos) {
                ControlUI::EffectItem effect;
                std::wstring displayName = item.substr(0, sep1);
                std::wstring matchName = item.substr(sep1 + 1, sep2 - sep1 - 1);
                std::wstring category = item.substr(sep2 + 1);

                wcscpy_s(effect.name, displayName.c_str());
                wcscpy_s(effect.matchName, matchName.c_str());
                wcscpy_s(effect.category, category.c_str());
                effect.index = idx;
                effect.isLayerEffect = false;

                g_availableEffects.push_back(effect);
                idx++;
            }
        }
        pos = end + 1;
    }
}

// Parse layer effects from string format: "name1|matchName1|index1;name2|matchName2|index2;..."
void ParseLayerEffects(const wchar_t* effectList) {
    g_layerEffects.clear();

    if (!effectList || wcslen(effectList) == 0) return;

    std::wstring list(effectList);
    size_t pos = 0;
    int idx = 0;

    while (pos < list.length()) {
        // Find next semicolon or end
        size_t end = list.find(L';', pos);
        if (end == std::wstring::npos) end = list.length();

        std::wstring item = list.substr(pos, end - pos);
        if (!item.empty()) {
            // Parse "name|matchName|index"
            size_t sep1 = item.find(L'|');
            size_t sep2 = item.find(L'|', sep1 + 1);

            if (sep1 != std::wstring::npos) {
                ControlUI::EffectItem effect;
                std::wstring name = item.substr(0, sep1);
                wcscpy_s(effect.name, name.c_str());

                if (sep2 != std::wstring::npos) {
                    std::wstring matchName = item.substr(sep1 + 1, sep2 - sep1 - 1);
                    std::wstring idxStr = item.substr(sep2 + 1);
                    wcscpy_s(effect.matchName, matchName.c_str());
                    effect.index = _wtoi(idxStr.c_str());
                } else {
                    std::wstring matchName = item.substr(sep1 + 1);
                    wcscpy_s(effect.matchName, matchName.c_str());
                    effect.index = idx;
                }

                effect.category[0] = L'\0';
                effect.isLayerEffect = true;
                g_layerEffects.push_back(effect);
                idx++;
            }
        }
        pos = end + 1;
    }
}

// Draw the control panel
void DrawControlPanel(HDC hdc, int width, int height) {
    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Apply scale transform - all subsequent drawing will be scaled
    graphics.ScaleTransform(g_scaleFactor, g_scaleFactor);

    // Use base width for layout (transform handles scaling)
    int baseWidth = WINDOW_WIDTH;

    // Calculate base height from actual height
    int baseHeight = (int)(height / g_scaleFactor);

    // Background (fill entire base area)
    SolidBrush bgBrush(COLOR_BG);
    graphics.FillRectangle(&bgBrush, 0, 0, baseWidth, baseHeight);

    // Border
    Pen borderPen(COLOR_BORDER, 1);
    graphics.DrawRectangle(&borderPen, 0, 0, baseWidth - 1, baseHeight - 1);

    // Search box background (full width - only pin button on right)
    SolidBrush searchBgBrush(COLOR_SEARCH_BG);
    RectF searchRect(PADDING, PADDING, baseWidth - PADDING * 2 - NEW_EC_BUTTON_SIZE - 8, SEARCH_HEIGHT);
    graphics.FillRectangle(&searchBgBrush, searchRect);

    // Keep open (pin) button - right side
    int pinBtnX = baseWidth - PADDING - NEW_EC_BUTTON_SIZE;
    int pinBtnY = PADDING + (SEARCH_HEIGHT - NEW_EC_BUTTON_SIZE) / 2;
    RectF pinRect((REAL)pinBtnX, (REAL)pinBtnY, (REAL)NEW_EC_BUTTON_SIZE, (REAL)NEW_EC_BUTTON_SIZE);

    if (g_keepOpenButtonHover || g_keepPanelOpen) {
        SolidBrush pinBgBrush(g_keepPanelOpen ? COLOR_PRESET_ACTIVE : COLOR_PRESET_HOVER);
        graphics.FillRectangle(&pinBgBrush, pinRect);
    }

    // Draw pin icon
    Color pinColor = g_keepPanelOpen ? Color(255, 255, 255, 255) : Color(255, 140, 140, 140);
    Pen pinPen(pinColor, 1.5f);
    float px = (float)pinBtnX, py = (float)pinBtnY;
    float ps = (float)NEW_EC_BUTTON_SIZE;
    graphics.DrawEllipse(&pinPen, px + ps * 0.3f, py + ps * 0.2f, ps * 0.4f, ps * 0.35f);
    graphics.DrawLine(&pinPen, px + ps * 0.5f, py + ps * 0.55f, px + ps * 0.5f, py + ps * 0.8f);

    // Search text
    FontFamily fontFamily(L"Segoe UI");
    Font searchFont(&fontFamily, 14, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);

    RectF textRect(PADDING + 8, PADDING + 8, baseWidth - PADDING * 2 - 16, SEARCH_HEIGHT - 16);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentNear);
    sf.SetLineAlignment(StringAlignmentCenter);

    // Draw selection highlight if any
    bool hasSelection = (g_selectionStart >= 0 && g_selectionEnd >= 0 && g_selectionStart != g_selectionEnd);
    if (hasSelection && wcslen(g_searchQuery) > 0) {
        int selStart = min(g_selectionStart, g_selectionEnd);
        int selEnd = max(g_selectionStart, g_selectionEnd);

        // Measure text up to selection start and end
        RectF startBounds, endBounds;
        if (selStart > 0) {
            wchar_t textBefore[256] = {0};
            wcsncpy(textBefore, g_searchQuery, selStart);
            graphics.MeasureString(textBefore, -1, &searchFont, textRect, &sf, &startBounds);
        } else {
            startBounds.Width = 0;
        }
        wchar_t textToEnd[256] = {0};
        wcsncpy(textToEnd, g_searchQuery, selEnd);
        graphics.MeasureString(textToEnd, -1, &searchFont, textRect, &sf, &endBounds);

        // Draw selection rectangle
        SolidBrush selBrush(Color(128, 74, 158, 255));  // Semi-transparent accent
        RectF selRect(PADDING + 8 + startBounds.Width, PADDING + 8,
                      endBounds.Width - startBounds.Width, SEARCH_HEIGHT - 16);
        graphics.FillRectangle(&selBrush, selRect);
    }

    if (wcslen(g_searchQuery) > 0) {
        graphics.DrawString(g_searchQuery, -1, &searchFont, textRect, &sf, &textBrush);
    } else {
        graphics.DrawString(L"Search effects...", -1, &searchFont, textRect, &sf, &dimBrush);
    }

    // Cursor blink
    if (g_cursorVisible) {
        // Measure text width up to cursor position
        RectF bounds;
        int cursorPos = min(g_cursorPosition, (int)wcslen(g_searchQuery));
        if (cursorPos > 0) {
            wchar_t textBeforeCursor[256] = {0};
            wcsncpy(textBeforeCursor, g_searchQuery, cursorPos);
            graphics.MeasureString(textBeforeCursor, -1, &searchFont, textRect, &sf, &bounds);
        } else {
            bounds.Width = 0;
        }

        Pen cursorPen(COLOR_ACCENT, 2);
        REAL cursorX = (REAL)(PADDING + 8) + bounds.Width + 1.0f;
        REAL cursorY1 = (REAL)(PADDING + 10);
        REAL cursorY2 = (REAL)(PADDING + SEARCH_HEIGHT - 10);
        graphics.DrawLine(&cursorPen, cursorX, cursorY1, cursorX, cursorY2);
    }

    // Results
    Font itemFont(&fontFamily, 12, FontStyleRegular, UnitPixel);
    Font categoryFont(&fontFamily, 10, FontStyleRegular, UnitPixel);

    int y = PADDING + SEARCH_HEIGHT + PADDING;
    int visibleCount = min((int)g_searchResults.size(), MAX_VISIBLE_ITEMS);

    for (int i = 0; i < visibleCount; i++) {
        const auto& item = g_searchResults[i];
        RectF itemRect(PADDING, y, baseWidth - PADDING * 2, ITEM_HEIGHT);

        // Highlight selected/hover
        if (i == g_selectedIndex) {
            SolidBrush selectedBrush(COLOR_ITEM_SELECTED);
            graphics.FillRectangle(&selectedBrush, itemRect);
        } else if (i == g_hoverIndex) {
            SolidBrush hoverBrush(COLOR_ITEM_HOVER);
            graphics.FillRectangle(&hoverBrush, itemRect);
        }

        // Effect name
        RectF nameRect(PADDING + 8, y + 4, baseWidth - PADDING * 2 - 100, ITEM_HEIGHT / 2);
        graphics.DrawString(item.name, -1, &itemFont, nameRect, &sf, &textBrush);

        // Category (right aligned)
        StringFormat sfRight;
        sfRight.SetAlignment(StringAlignmentFar);
        sfRight.SetLineAlignment(StringAlignmentCenter);
        RectF catRect(baseWidth - 110, y, 100, ITEM_HEIGHT);
        graphics.DrawString(item.category, -1, &categoryFont, catRect, &sfRight, &dimBrush);

        y += ITEM_HEIGHT;
    }

    // No results message
    if (g_searchResults.empty() && wcslen(g_searchQuery) > 0) {
        RectF msgRect(PADDING, y, baseWidth - PADDING * 2, ITEM_HEIGHT);
        graphics.DrawString(L"No effects found", -1, &itemFont, msgRect, &sf, &dimBrush);
    }
}

// Draw preset icon inside a rectangle
void DrawPresetIcon(Graphics& graphics, PresetIcon icon, RectF rect, bool filled) {
    float cx = rect.X + rect.Width / 2;
    float cy = rect.Y + rect.Height / 2;
    float r = min(rect.Width, rect.Height) / 2 - 4;

    Color iconColor = filled ? Color(255, 255, 255, 255) : Color(255, 100, 100, 110);
    Pen iconPen(iconColor, 2);
    SolidBrush iconBrush(iconColor);

    switch (icon) {
        case ICON_COLOR: {
            // Color wheel - three overlapping circles (RGB)
            float sr = r * 0.5f;
            graphics.DrawEllipse(&iconPen, cx - sr, cy - r * 0.4f, sr * 1.4f, sr * 1.4f);
            graphics.DrawEllipse(&iconPen, cx - sr * 0.7f - sr * 0.4f, cy + r * 0.1f, sr * 1.4f, sr * 1.4f);
            graphics.DrawEllipse(&iconPen, cx + sr * 0.7f - sr * 0.7f, cy + r * 0.1f, sr * 1.4f, sr * 1.4f);
            break;
        }
        case ICON_BLUR: {
            // Concentric circles (blur effect)
            graphics.DrawEllipse(&iconPen, cx - r, cy - r, r * 2, r * 2);
            graphics.DrawEllipse(&iconPen, cx - r * 0.6f, cy - r * 0.6f, r * 1.2f, r * 1.2f);
            graphics.DrawEllipse(&iconPen, cx - r * 0.25f, cy - r * 0.25f, r * 0.5f, r * 0.5f);
            break;
        }
        case ICON_DISTORT: {
            // Wave/distort
            GraphicsPath path;
            float waveY = cy;
            PointF points[7];
            for (int i = 0; i < 7; i++) {
                float wx = cx - r + (r * 2 * i / 6.0f);
                float wy = cy + sinf(i * 3.14159f / 2) * r * 0.5f;
                points[i] = PointF(wx, wy);
            }
            graphics.DrawCurve(&iconPen, points, 7, 0.5f);
            break;
        }
        case ICON_STAR: {
            // 5-pointed star
            GraphicsPath path;
            PointF starPoints[10];
            for (int i = 0; i < 10; i++) {
                float angle = (i * 36.0f - 90.0f) * 3.14159f / 180.0f;
                float sr = (i % 2 == 0) ? r : r * 0.45f;
                starPoints[i] = PointF(cx + cosf(angle) * sr, cy + sinf(angle) * sr);
            }
            path.AddPolygon(starPoints, 10);
            graphics.FillPath(&iconBrush, &path);
            break;
        }
        case ICON_LIGHTNING: {
            // Lightning bolt
            PointF bolt[] = {
                PointF(cx + r * 0.2f, cy - r),
                PointF(cx - r * 0.3f, cy - r * 0.1f),
                PointF(cx + r * 0.1f, cy - r * 0.1f),
                PointF(cx - r * 0.2f, cy + r),
                PointF(cx + r * 0.3f, cy + r * 0.1f),
                PointF(cx - r * 0.1f, cy + r * 0.1f),
                PointF(cx + r * 0.2f, cy - r)
            };
            GraphicsPath path;
            path.AddPolygon(bolt, 7);
            graphics.FillPath(&iconBrush, &path);
            break;
        }
        case ICON_MAGIC: {
            // Magic wand with sparkle
            // Wand diagonal line
            Pen wandPen(iconColor, 2.5f);
            graphics.DrawLine(&wandPen, cx - r * 0.7f, cy + r * 0.7f, cx + r * 0.5f, cy - r * 0.5f);
            // Sparkle at tip
            float sx = cx + r * 0.5f, sy = cy - r * 0.5f;
            Pen sparklePen(iconColor, 1.5f);
            graphics.DrawLine(&sparklePen, sx - r * 0.3f, sy, sx + r * 0.3f, sy);
            graphics.DrawLine(&sparklePen, sx, sy - r * 0.3f, sx, sy + r * 0.3f);
            graphics.DrawLine(&sparklePen, sx - r * 0.2f, sy - r * 0.2f, sx + r * 0.2f, sy + r * 0.2f);
            graphics.DrawLine(&sparklePen, sx + r * 0.2f, sy - r * 0.2f, sx - r * 0.2f, sy + r * 0.2f);
            break;
        }
        default:
            break;
    }
}

// Draw save icon (floppy disk)
void DrawSaveIcon(Graphics& graphics, RectF rect, bool hover) {
    float cx = rect.X + rect.Width / 2;
    float cy = rect.Y + rect.Height / 2;
    float s = min(rect.Width, rect.Height) / 2 - 3;

    Color iconColor = hover ? Color(255, 74, 207, 255) : Color(255, 200, 200, 200);
    Pen iconPen(iconColor, 1.5f);
    SolidBrush iconBrush(iconColor);

    // Floppy disk outline
    RectF diskRect(cx - s, cy - s, s * 2, s * 2);
    graphics.DrawRectangle(&iconPen, diskRect);

    // Metal slider area at top
    RectF sliderRect(cx - s * 0.6f, cy - s, s * 1.2f, s * 0.5f);
    graphics.FillRectangle(&iconBrush, sliderRect);

    // Label area at bottom
    RectF labelRect(cx - s * 0.7f, cy + s * 0.1f, s * 1.4f, s * 0.7f);
    graphics.DrawRectangle(&iconPen, labelRect);
}

// Draw the effects panel (Mode 2)
void DrawEffectsPanel(HDC hdc, int width, int height) {
    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Apply scale transform
    graphics.ScaleTransform(g_scaleFactor, g_scaleFactor);

    // Use base dimensions for layout
    int baseWidth = WINDOW_WIDTH;
    int baseHeight = (int)(height / g_scaleFactor);

    // Background
    SolidBrush bgBrush(COLOR_BG);
    graphics.FillRectangle(&bgBrush, 0, 0, baseWidth, baseHeight);

    // Border
    Pen borderPen(COLOR_BORDER, 1);
    graphics.DrawRectangle(&borderPen, 0, 0, baseWidth - 1, baseHeight - 1);

    // Fonts
    FontFamily fontFamily(L"Segoe UI");
    Font headerFont(&fontFamily, 12, FontStyleBold, UnitPixel);
    Font itemFont(&fontFamily, 12, FontStyleRegular, UnitPixel);
    Font indexFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);
    SolidBrush accentBrush(COLOR_ACCENT);

    StringFormat sf;
    sf.SetAlignment(StringAlignmentNear);
    sf.SetLineAlignment(StringAlignmentCenter);

    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);

    int currentY = PADDING;

    // ===== Header bar with layer name and label color =====
    // Label color indicator (small square)
    int labelSize = 8;
    int labelX = PADDING + 4;
    int labelY = currentY + (HEADER_HEIGHT - labelSize) / 2;
    int labelIndex = (g_currentLayerLabelColor >= 0 && g_currentLayerLabelColor <= 16) ? g_currentLayerLabelColor : 0;
    SolidBrush labelBrush(LABEL_COLORS[labelIndex]);
    graphics.FillRectangle(&labelBrush, labelX, labelY, labelSize, labelSize);

    // Layer name
    RectF titleRect(PADDING + labelSize + 10, currentY, baseWidth - PADDING * 2 - NEW_EC_BUTTON_SIZE - 10, HEADER_HEIGHT);
    if (wcslen(g_currentLayerName) > 0) {
        graphics.DrawString(g_currentLayerName, -1, &headerFont, titleRect, &sf, &textBrush);
    } else {
        graphics.DrawString(L"Selected Layer", -1, &headerFont, titleRect, &sf, &dimBrush);
    }

    // Keep open (pin) button - right side
    int pinBtnX = baseWidth - PADDING - NEW_EC_BUTTON_SIZE;
    int pinBtnY = currentY + (HEADER_HEIGHT - NEW_EC_BUTTON_SIZE) / 2;
    RectF pinRect((REAL)pinBtnX, (REAL)pinBtnY, (REAL)NEW_EC_BUTTON_SIZE, (REAL)NEW_EC_BUTTON_SIZE);

    if (g_keepOpenButtonHover || g_keepPanelOpen) {
        SolidBrush pinBgBrush(g_keepPanelOpen ? COLOR_PRESET_ACTIVE : COLOR_PRESET_HOVER);
        graphics.FillRectangle(&pinBgBrush, pinRect);
    }

    // Draw pin icon (pushpin)
    Color pinColor = g_keepPanelOpen ? Color(255, 255, 255, 255) : Color(255, 140, 140, 140);
    Pen pinPen(pinColor, 1.5f);
    float px = (float)pinBtnX, py = (float)pinBtnY;
    float ps = (float)NEW_EC_BUTTON_SIZE;
    // Pin head (circle)
    graphics.DrawEllipse(&pinPen, px + ps * 0.3f, py + ps * 0.2f, ps * 0.4f, ps * 0.35f);
    // Pin needle (line down)
    graphics.DrawLine(&pinPen, px + ps * 0.5f, py + ps * 0.55f, px + ps * 0.5f, py + ps * 0.8f);

    // New EC window button [+] - left of pin button
    int newECBtnX = pinBtnX - NEW_EC_BUTTON_SIZE - 4;
    int newECBtnY = currentY + (HEADER_HEIGHT - NEW_EC_BUTTON_SIZE) / 2;
    RectF newECRect((REAL)newECBtnX, (REAL)newECBtnY, (REAL)NEW_EC_BUTTON_SIZE, (REAL)NEW_EC_BUTTON_SIZE);

    if (g_newECButtonHover) {
        SolidBrush newECBgBrush(COLOR_PRESET_HOVER);
        graphics.FillRectangle(&newECBgBrush, newECRect);
    }

    // Draw + sign
    Pen plusPen(COLOR_ACCENT, 2);
    float plusMargin = 5.0f;
    graphics.DrawLine(&plusPen,
        (REAL)(newECBtnX + plusMargin), (REAL)(newECBtnY + NEW_EC_BUTTON_SIZE / 2),
        (REAL)(newECBtnX + NEW_EC_BUTTON_SIZE - plusMargin), (REAL)(newECBtnY + NEW_EC_BUTTON_SIZE / 2));
    graphics.DrawLine(&plusPen,
        (REAL)(newECBtnX + NEW_EC_BUTTON_SIZE / 2), (REAL)(newECBtnY + plusMargin),
        (REAL)(newECBtnX + NEW_EC_BUTTON_SIZE / 2), (REAL)(newECBtnY + NEW_EC_BUTTON_SIZE - plusMargin));

    currentY += HEADER_HEIGHT + 4;

    // ===== Preset buttons bar with icons =====
    int presetBarY = currentY;
    int presetBtnSpacing = 4;
    // 6 preset buttons + save button
    int totalPresetWidth = PRESET_SLOT_COUNT * (PRESET_BUTTON_HEIGHT + presetBtnSpacing) + SAVE_BUTTON_SIZE + presetBtnSpacing;
    int presetStartX = (baseWidth - totalPresetWidth) / 2;

    for (int i = 0; i < PRESET_SLOT_COUNT; i++) {
        int btnX = presetStartX + i * (PRESET_BUTTON_HEIGHT + presetBtnSpacing);
        int btnY = presetBarY + (PRESET_BAR_HEIGHT - PRESET_BUTTON_HEIGHT) / 2;

        RectF btnRect((REAL)btnX, (REAL)btnY, (REAL)PRESET_BUTTON_HEIGHT, (REAL)PRESET_BUTTON_HEIGHT);
        bool hasFx = (g_presetIcons[i] != ICON_NONE);

        // Button background
        Color btnColor;
        if (g_saveMode) {
            // Save mode: orange glow
            btnColor = (i == g_hoveredPresetButton) ? Color(255, 255, 180, 0) : Color(255, 200, 120, 0);
        } else {
            btnColor = (i == g_hoveredPresetButton) ? COLOR_PRESET_HOVER :
                       hasFx ? COLOR_PRESET_ACTIVE : COLOR_PRESET_BG;
        }
        SolidBrush btnBrush(btnColor);
        graphics.FillRectangle(&btnBrush, btnRect);

        // Button border
        if (hasFx) {
            Pen btnBorder(COLOR_BORDER, 1);
            graphics.DrawRectangle(&btnBorder, btnRect);
        } else {
            // Empty slot: dashed border
            Pen dashPen(Color(255, 80, 80, 90), 1);
            float dashes[] = {3.0f, 3.0f};
            dashPen.SetDashPattern(dashes, 2);
            graphics.DrawRectangle(&dashPen, btnRect);
        }

        // Draw icon or empty state
        if (hasFx) {
            DrawPresetIcon(graphics, g_presetIcons[i], btnRect, true);
        }
    }

    // Save button (floppy disk icon) - right of preset buttons
    int saveBtnX = presetStartX + PRESET_SLOT_COUNT * (PRESET_BUTTON_HEIGHT + presetBtnSpacing);
    int saveBtnY = presetBarY + (PRESET_BAR_HEIGHT - SAVE_BUTTON_SIZE) / 2;
    RectF saveRect((REAL)saveBtnX, (REAL)saveBtnY, (REAL)SAVE_BUTTON_SIZE, (REAL)SAVE_BUTTON_SIZE);

    Color saveBtnColor = g_saveMode ? COLOR_PRESET_ACTIVE :
                         g_saveButtonHover ? COLOR_PRESET_HOVER : COLOR_PRESET_BG;
    SolidBrush saveBtnBrush(saveBtnColor);
    graphics.FillRectangle(&saveBtnBrush, saveRect);

    Pen saveBorder(COLOR_BORDER, 1);
    graphics.DrawRectangle(&saveBorder, saveRect);

    // Draw save icon
    DrawSaveIcon(graphics, saveRect, g_saveButtonHover || g_saveMode);

    currentY += PRESET_BAR_HEIGHT;

    // ===== Search bar =====
    SolidBrush searchBgBrush(COLOR_SEARCH_BG);
    RectF searchRect(PADDING, currentY, baseWidth - PADDING * 2, SEARCH_HEIGHT);
    graphics.FillRectangle(&searchBgBrush, searchRect);

    // Search text or placeholder
    RectF searchTextRect(PADDING + 8, currentY, baseWidth - PADDING * 2 - 16, SEARCH_HEIGHT);
    if (wcslen(g_searchQuery) > 0) {
        graphics.DrawString(g_searchQuery, -1, &itemFont, searchTextRect, &sf, &textBrush);
    } else {
        graphics.DrawString(L"Search effects...", -1, &itemFont, searchTextRect, &sf, &dimBrush);
    }

    currentY += SEARCH_HEIGHT + PADDING;

    // ===== Show search results or layer effects =====
    bool isSearching = wcslen(g_searchQuery) > 0;

    if (isSearching) {
        // Show search results
        int visibleCount = min((int)g_searchResults.size(), MAX_VISIBLE_ITEMS);

        if (visibleCount == 0) {
            RectF msgRect(PADDING, currentY, baseWidth - PADDING * 2, ITEM_HEIGHT);
            graphics.DrawString(L"No matching effects", -1, &itemFont, msgRect, &sf, &dimBrush);
            return;
        }

        for (int i = 0; i < visibleCount; i++) {
            const auto& item = g_searchResults[i];
            RectF itemRect(PADDING, currentY, baseWidth - PADDING * 2, ITEM_HEIGHT);

            // Highlight selected/hover
            if (i == g_selectedIndex) {
                SolidBrush selectedBrush(COLOR_ITEM_SELECTED);
                graphics.FillRectangle(&selectedBrush, itemRect);
            } else if (i == g_hoverIndex) {
                SolidBrush hoverBrush(COLOR_ITEM_HOVER);
                graphics.FillRectangle(&hoverBrush, itemRect);
            }

            // Effect name
            RectF nameRect(PADDING + 8, currentY, baseWidth - PADDING * 2 - 100, ITEM_HEIGHT);
            graphics.DrawString(item.name, -1, &itemFont, nameRect, &sf, &textBrush);

            // Category
            RectF catRect(baseWidth - PADDING - 90, currentY, 80, ITEM_HEIGHT);
            StringFormat sfRight;
            sfRight.SetAlignment(StringAlignmentFar);
            sfRight.SetLineAlignment(StringAlignmentCenter);
            graphics.DrawString(item.category, -1, &indexFont, catRect, &sfRight, &dimBrush);

            currentY += ITEM_HEIGHT;
        }
    } else {
        // Show layer effects list
        int visibleCount = min((int)g_layerEffects.size(), MAX_VISIBLE_ITEMS);

        if (visibleCount == 0) {
            RectF msgRect(PADDING, currentY, baseWidth - PADDING * 2, ITEM_HEIGHT);
            graphics.DrawString(L"No effects on layer", -1, &itemFont, msgRect, &sf, &dimBrush);
            return;
        }

        for (int i = 0; i < visibleCount; i++) {
            const auto& item = g_layerEffects[i];
            RectF itemRect(PADDING, currentY, baseWidth - PADDING * 2, ITEM_HEIGHT);

            // Highlight selected/hover
            if (i == g_selectedIndex) {
                SolidBrush selectedBrush(COLOR_ITEM_SELECTED);
                graphics.FillRectangle(&selectedBrush, itemRect);
            } else if (i == g_hoverIndex) {
                SolidBrush hoverBrush(COLOR_ITEM_HOVER);
                graphics.FillRectangle(&hoverBrush, itemRect);
            }

            // Expand icon (▶)
            RectF expandRect(PADDING + 4, currentY, 16, ITEM_HEIGHT);
            graphics.DrawString(L"\u25B6", -1, &indexFont, expandRect, &sf, &dimBrush);

            // Effect index number
            wchar_t indexStr[8];
            swprintf_s(indexStr, L"%d.", item.index + 1);
            RectF indexRect(PADDING + 20, currentY, 24, ITEM_HEIGHT);
            graphics.DrawString(indexStr, -1, &indexFont, indexRect, &sf, &dimBrush);

            // Effect name
            RectF nameRect(PADDING + 44, currentY, baseWidth - PADDING * 2 - 80, ITEM_HEIGHT);
            graphics.DrawString(item.name, -1, &itemFont, nameRect, &sf, &textBrush);

            // Delete button [x]
            int btnX = baseWidth - PADDING - ACTION_BUTTON_SIZE - 4;
            int btnY = currentY + (ITEM_HEIGHT - ACTION_BUTTON_SIZE) / 2;

            Pen deletePen(Color(255, 200, 80, 80), 1.5f);
            float btnMargin = 5.0f;
            graphics.DrawLine(&deletePen,
                (REAL)(btnX + btnMargin), (REAL)(btnY + btnMargin),
                (REAL)(btnX + ACTION_BUTTON_SIZE - btnMargin), (REAL)(btnY + ACTION_BUTTON_SIZE - btnMargin));
            graphics.DrawLine(&deletePen,
                (REAL)(btnX + ACTION_BUTTON_SIZE - btnMargin), (REAL)(btnY + btnMargin),
                (REAL)(btnX + btnMargin), (REAL)(btnY + ACTION_BUTTON_SIZE - btnMargin));

            currentY += ITEM_HEIGHT;
        }
    }
}

// Window procedure
LRESULT CALLBACK ControlWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);

            // Double buffer
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

            // Draw based on mode
            if (g_panelMode == ControlUI::MODE_SEARCH) {
                DrawControlPanel(memDC, rc.right, rc.bottom);
            } else {
                DrawEffectsPanel(memDC, rc.right, rc.bottom);
            }

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CHAR: {
            wchar_t ch = (wchar_t)wParam;

            // Don't accept input until Shift+E is released
            if (g_waitingForKeyRelease) {
                // Only allow ESC while waiting
                if (ch == VK_ESCAPE) {
                    g_result.cancelled = true;
                    ControlUI::HidePanel();
                }
                return 0;
            }

            // Reset cursor blink on any input
            g_cursorVisible = true;

            if (ch == VK_ESCAPE) {
                // Escape - cancel
                g_result.cancelled = true;
                ControlUI::HidePanel();
                return 0;
            }

            // Handle text input for search (both modes)
            size_t len = wcslen(g_searchQuery);

            // Handle selection delete first
            bool hasSelection = (g_selectionStart >= 0 && g_selectionEnd >= 0 && g_selectionStart != g_selectionEnd);
            if (hasSelection && (ch == VK_BACK || (ch >= 32))) {
                // Delete selection
                int selStart = min(g_selectionStart, g_selectionEnd);
                int selEnd = max(g_selectionStart, g_selectionEnd);
                memmove(&g_searchQuery[selStart], &g_searchQuery[selEnd],
                        (len - selEnd + 1) * sizeof(wchar_t));
                g_cursorPosition = selStart;
                g_selectionStart = g_selectionEnd = -1;
                len = wcslen(g_searchQuery);
                if (ch == VK_BACK) {
                    ControlUI::UpdateSearch(g_searchQuery);
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }
            }

            if (ch == VK_BACK) {
                // Backspace - delete char before cursor
                if (g_cursorPosition > 0 && len > 0) {
                    memmove(&g_searchQuery[g_cursorPosition - 1],
                            &g_searchQuery[g_cursorPosition],
                            (len - g_cursorPosition + 1) * sizeof(wchar_t));
                    g_cursorPosition--;
                    ControlUI::UpdateSearch(g_searchQuery);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (ch == VK_RETURN) {
                // Enter - apply selected effect or action
                if (wcslen(g_searchQuery) > 0 && !g_searchResults.empty() && g_selectedIndex < (int)g_searchResults.size()) {
                    // Search result selected - add effect to layer
                    g_result.effectSelected = true;
                    g_result.selectedEffect = g_searchResults[g_selectedIndex];
                    wcscpy_s(g_result.searchQuery, g_searchQuery);
                    ControlUI::HidePanel();
                } else if (wcslen(g_searchQuery) == 0 && !g_layerEffects.empty() && g_selectedIndex < (int)g_layerEffects.size()) {
                    // No search query - expand selected layer effect
                    g_result.effectSelected = true;
                    g_result.selectedEffect = g_layerEffects[g_selectedIndex];
                    g_result.action = ControlUI::ACTION_EXPAND;
                    g_result.effectIndex = g_layerEffects[g_selectedIndex].index;
                    ControlUI::HidePanel();
                }
            } else if (ch >= 32) {
                // Regular character input - insert at cursor position
                if (len < 254) {
                    memmove(&g_searchQuery[g_cursorPosition + 1],
                            &g_searchQuery[g_cursorPosition],
                            (len - g_cursorPosition + 1) * sizeof(wchar_t));
                    g_searchQuery[g_cursorPosition] = ch;
                    g_cursorPosition++;
                    ControlUI::UpdateSearch(g_searchQuery);
                    g_selectedIndex = 0;  // Reset selection on new search
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            return 0;
        }

        case WM_KEYUP: {
            // Check if Shift+E has been released - then allow input
            if (g_waitingForKeyRelease) {
                bool shiftUp = !(GetKeyState(VK_SHIFT) & 0x8000);
                bool eKeyUp = !(GetKeyState(0x45) & 0x8000);  // 'E' key
                if (shiftUp && eKeyUp) {
                    g_waitingForKeyRelease = false;
                }
            }
            return 0;
        }

        case WM_KEYDOWN: {
            // Forward Ctrl+Z (Undo) or Ctrl+Shift+Z (Redo) to AE
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'Z') {
                bool isRedo = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                // Find AE window and send keystroke
                HWND aeWnd = NULL;
                HWND hw = GetTopWindow(NULL);
                while (hw) {
                    wchar_t title[256] = {0};
                    GetWindowTextW(hw, title, 256);
                    if (wcsstr(title, L"After Effects") != NULL) { aeWnd = hw; break; }
                    hw = GetNextWindow(hw, GW_HWNDNEXT);
                }
                if (aeWnd) {
                    SetForegroundWindow(aeWnd);
                    keybd_event(VK_CONTROL, 0, 0, 0);
                    if (isRedo) keybd_event(VK_SHIFT, 0, 0, 0);
                    keybd_event('Z', 0, 0, 0);
                    keybd_event('Z', 0, KEYEVENTF_KEYUP, 0);
                    if (isRedo) keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
                    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
                    Sleep(30);
                    SetForegroundWindow(hwnd);
                    SetFocus(hwnd);
                }
                return 0;
            }

            // Reset cursor blink on any key
            g_cursorVisible = true;

            // Get max index based on whether we're searching or showing layer effects
            int maxIndex = (wcslen(g_searchQuery) > 0)
                ? (int)g_searchResults.size() - 1
                : (int)g_layerEffects.size() - 1;

            bool ctrlHeld = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            size_t len = wcslen(g_searchQuery);

            if (wParam == VK_UP) {
                if (g_selectedIndex > 0) {
                    g_selectedIndex--;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (wParam == VK_DOWN) {
                if (g_selectedIndex < maxIndex) {
                    g_selectedIndex++;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (wParam == VK_LEFT) {
                // Move cursor left
                if (g_cursorPosition > 0) {
                    g_cursorPosition--;
                    g_selectionStart = g_selectionEnd = -1;  // Clear selection
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (wParam == VK_RIGHT) {
                // Move cursor right
                if (g_cursorPosition < (int)len) {
                    g_cursorPosition++;
                    g_selectionStart = g_selectionEnd = -1;  // Clear selection
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (wParam == VK_HOME) {
                // Move cursor to start
                g_cursorPosition = 0;
                g_selectionStart = g_selectionEnd = -1;
                InvalidateRect(hwnd, NULL, TRUE);
            } else if (wParam == VK_END) {
                // Move cursor to end
                g_cursorPosition = (int)len;
                g_selectionStart = g_selectionEnd = -1;
                InvalidateRect(hwnd, NULL, TRUE);
            } else if (wParam == VK_DELETE) {
                // Delete char after cursor
                if (g_cursorPosition < (int)len) {
                    memmove(&g_searchQuery[g_cursorPosition],
                            &g_searchQuery[g_cursorPosition + 1],
                            (len - g_cursorPosition) * sizeof(wchar_t));
                    ControlUI::UpdateSearch(g_searchQuery);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (ctrlHeld && wParam == 'A') {
                // Ctrl+A - Select all
                if (len > 0) {
                    g_selectionStart = 0;
                    g_selectionEnd = (int)len;
                    g_cursorPosition = (int)len;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            // Inverse scale mouse coordinates for hit testing
            int x = InverseScaled(LOWORD(lParam));
            int y = InverseScaled(HIWORD(lParam));
            bool needRedraw = false;

            if (g_panelMode == ControlUI::MODE_SEARCH) {
                // Mode 1: Search mode
                int headerHeight = SEARCH_HEIGHT;
                int startY = PADDING + headerHeight + PADDING;

                // Check pin button hover
                int pinBtnX = WINDOW_WIDTH - PADDING - NEW_EC_BUTTON_SIZE;
                int pinBtnY = PADDING + (headerHeight - NEW_EC_BUTTON_SIZE) / 2;
                bool wasPinHover = g_keepOpenButtonHover;
                g_keepOpenButtonHover = (x >= pinBtnX && x < pinBtnX + NEW_EC_BUTTON_SIZE &&
                                         y >= pinBtnY && y < pinBtnY + NEW_EC_BUTTON_SIZE);
                if (wasPinHover != g_keepOpenButtonHover) needRedraw = true;

                // Check item hover
                int itemCount = (int)g_searchResults.size();
                if (y >= startY) {
                    int idx = (y - startY) / ITEM_HEIGHT;
                    if (idx >= 0 && idx < itemCount) {
                        if (g_hoverIndex != idx) {
                            g_hoverIndex = idx;
                            needRedraw = true;
                        }
                    }
                } else if (g_hoverIndex != -1) {
                    g_hoverIndex = -1;
                    needRedraw = true;
                }
            } else {
                // Mode 2: Effects panel with preset buttons
                // Layout: Header -> Presets -> Search -> Items
                int headerY = PADDING;
                int presetBarY = headerY + HEADER_HEIGHT + 4;
                int searchBarY = presetBarY + PRESET_BAR_HEIGHT;
                int itemsStartY = searchBarY + SEARCH_HEIGHT + PADDING;

                // Check pin button hover (in header - right side)
                int pinBtnX = WINDOW_WIDTH - PADDING - NEW_EC_BUTTON_SIZE;
                int pinBtnY = headerY + (HEADER_HEIGHT - NEW_EC_BUTTON_SIZE) / 2;
                bool wasPinHover = g_keepOpenButtonHover;
                g_keepOpenButtonHover = (x >= pinBtnX && x < pinBtnX + NEW_EC_BUTTON_SIZE &&
                                         y >= pinBtnY && y < pinBtnY + NEW_EC_BUTTON_SIZE);
                if (wasPinHover != g_keepOpenButtonHover) needRedraw = true;

                // Check new EC button hover [+] - left of pin
                int newECBtnX = pinBtnX - NEW_EC_BUTTON_SIZE - 4;
                int newECBtnY = headerY + (HEADER_HEIGHT - NEW_EC_BUTTON_SIZE) / 2;
                bool wasNewECHover = g_newECButtonHover;
                g_newECButtonHover = (x >= newECBtnX && x < newECBtnX + NEW_EC_BUTTON_SIZE &&
                                      y >= newECBtnY && y < newECBtnY + NEW_EC_BUTTON_SIZE);
                if (wasNewECHover != g_newECButtonHover) needRedraw = true;

                // Check preset buttons and save button hover (in preset bar)
                int presetBtnSpacing = 4;
                int totalPresetWidth = PRESET_SLOT_COUNT * (PRESET_BUTTON_HEIGHT + presetBtnSpacing) + SAVE_BUTTON_SIZE + presetBtnSpacing;
                int presetStartX = (WINDOW_WIDTH - totalPresetWidth) / 2;
                int oldHoveredPreset = g_hoveredPresetButton;
                g_hoveredPresetButton = -1;

                for (int i = 0; i < PRESET_SLOT_COUNT; i++) {
                    int btnX = presetStartX + i * (PRESET_BUTTON_HEIGHT + presetBtnSpacing);
                    int btnY = presetBarY + (PRESET_BAR_HEIGHT - PRESET_BUTTON_HEIGHT) / 2;
                    if (x >= btnX && x < btnX + PRESET_BUTTON_HEIGHT &&
                        y >= btnY && y < btnY + PRESET_BUTTON_HEIGHT) {
                        g_hoveredPresetButton = i;
                        break;
                    }
                }
                if (oldHoveredPreset != g_hoveredPresetButton) needRedraw = true;

                // Check save button hover (right of preset buttons)
                int saveBtnX = presetStartX + PRESET_SLOT_COUNT * (PRESET_BUTTON_HEIGHT + presetBtnSpacing);
                int saveBtnY = presetBarY + (PRESET_BAR_HEIGHT - SAVE_BUTTON_SIZE) / 2;
                bool wasSaveHover = g_saveButtonHover;
                g_saveButtonHover = (x >= saveBtnX && x < saveBtnX + SAVE_BUTTON_SIZE &&
                                     y >= saveBtnY && y < saveBtnY + SAVE_BUTTON_SIZE);
                if (wasSaveHover != g_saveButtonHover) needRedraw = true;

                // Check item hover
                int itemCount = (int)g_layerEffects.size();
                if (y >= itemsStartY) {
                    int idx = (y - itemsStartY) / ITEM_HEIGHT;
                    if (idx >= 0 && idx < itemCount) {
                        if (g_hoverIndex != idx) {
                            g_hoverIndex = idx;
                            needRedraw = true;
                        }
                    }
                } else if (g_hoverIndex != -1) {
                    g_hoverIndex = -1;
                    needRedraw = true;
                }
            }

            if (needRedraw) InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            // Inverse scale mouse coordinates for hit testing
            int x = InverseScaled(LOWORD(lParam));
            int y = InverseScaled(HIWORD(lParam));

            if (g_panelMode == ControlUI::MODE_SEARCH) {
                // Mode 1: Search mode
                int headerHeight = SEARCH_HEIGHT;
                int startY = PADDING + headerHeight + PADDING;

                // Check pin button click (toggle keep-open)
                int pinBtnX = WINDOW_WIDTH - PADDING - NEW_EC_BUTTON_SIZE;
                int pinBtnY = PADDING + (headerHeight - NEW_EC_BUTTON_SIZE) / 2;
                if (x >= pinBtnX && x < pinBtnX + NEW_EC_BUTTON_SIZE &&
                    y >= pinBtnY && y < pinBtnY + NEW_EC_BUTTON_SIZE) {
                    g_keepPanelOpen = !g_keepPanelOpen;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }

                // Check item click
                if (y >= startY) {
                    int idx = (y - startY) / ITEM_HEIGHT;
                    if (idx >= 0 && idx < (int)g_searchResults.size()) {
                        g_result.effectSelected = true;
                        g_result.selectedEffect = g_searchResults[idx];
                        wcscpy_s(g_result.searchQuery, g_searchQuery);
                        // Auto-close unless pinned
                        if (!g_keepPanelOpen) {
                            ControlUI::HidePanel();
                        }
                    }
                }
            } else {
                // Mode 2: Effects panel with preset buttons
                // Layout: Header -> Presets -> Search -> Items
                int headerY = PADDING;
                int presetBarY = headerY + HEADER_HEIGHT + 4;
                int searchBarY = presetBarY + PRESET_BAR_HEIGHT;
                int itemsStartY = searchBarY + SEARCH_HEIGHT + PADDING;

                // Check pin button click (toggle keep-open) - in header, right side
                int pinBtnX = WINDOW_WIDTH - PADDING - NEW_EC_BUTTON_SIZE;
                int pinBtnY = headerY + (HEADER_HEIGHT - NEW_EC_BUTTON_SIZE) / 2;
                if (x >= pinBtnX && x < pinBtnX + NEW_EC_BUTTON_SIZE &&
                    y >= pinBtnY && y < pinBtnY + NEW_EC_BUTTON_SIZE) {
                    g_keepPanelOpen = !g_keepPanelOpen;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }

                // Check new EC button click [+] - left of pin button
                int newECBtnX = pinBtnX - NEW_EC_BUTTON_SIZE - 4;
                int newECBtnY = headerY + (HEADER_HEIGHT - NEW_EC_BUTTON_SIZE) / 2;
                if (x >= newECBtnX && x < newECBtnX + NEW_EC_BUTTON_SIZE &&
                    y >= newECBtnY && y < newECBtnY + NEW_EC_BUTTON_SIZE) {
                    g_result.action = ControlUI::ACTION_NEW_EC_WINDOW;
                    if (!g_keepPanelOpen) {
                        ControlUI::HidePanel();
                    }
                    return 0;
                }

                // Check preset buttons and save button in preset bar
                int presetBtnSpacing = 4;
                int totalPresetWidth = PRESET_SLOT_COUNT * (PRESET_BUTTON_HEIGHT + presetBtnSpacing) + SAVE_BUTTON_SIZE + presetBtnSpacing;
                int presetStartX = (WINDOW_WIDTH - totalPresetWidth) / 2;

                // Check 6 preset buttons (square, PRESET_BUTTON_HEIGHT x PRESET_BUTTON_HEIGHT)
                for (int i = 0; i < PRESET_SLOT_COUNT; i++) {
                    int btnX = presetStartX + i * (PRESET_BUTTON_HEIGHT + presetBtnSpacing);
                    int btnY = presetBarY + (PRESET_BAR_HEIGHT - PRESET_BUTTON_HEIGHT) / 2;
                    if (x >= btnX && x < btnX + PRESET_BUTTON_HEIGHT &&
                        y >= btnY && y < btnY + PRESET_BUTTON_HEIGHT) {
                        // Preset button clicked
                        if (g_saveMode) {
                            // Save mode: save current effects to this slot
                            g_result.action = ControlUI::ACTION_SAVE_PRESET;
                            g_result.presetSlotIndex = i;
                            g_result.effectSelected = true;
                            g_saveMode = false;
                            if (!g_keepPanelOpen) {
                                ControlUI::HidePanel();
                            } else {
                                InvalidateRect(hwnd, NULL, FALSE);
                            }
                        } else {
                            // Normal mode: apply preset (if slot has icon, meaning preset exists)
                            if (g_presetIcons[i] != ICON_NONE) {
                                g_result.action = ControlUI::ACTION_APPLY_PRESET;
                                g_result.presetSlotIndex = i;
                                g_result.effectSelected = true;
                                if (!g_keepPanelOpen) {
                                    ControlUI::HidePanel();
                                }
                            }
                            // Empty slot - do nothing
                        }
                        return 0;
                    }
                }

                // Check save button click (floppy icon) - right of preset buttons in preset bar
                int saveBtnX = presetStartX + PRESET_SLOT_COUNT * (PRESET_BUTTON_HEIGHT + presetBtnSpacing);
                int saveBtnY = presetBarY + (PRESET_BAR_HEIGHT - SAVE_BUTTON_SIZE) / 2;
                if (x >= saveBtnX && x < saveBtnX + SAVE_BUTTON_SIZE &&
                    y >= saveBtnY && y < saveBtnY + SAVE_BUTTON_SIZE) {
                    // Toggle save mode
                    g_saveMode = !g_saveMode;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }

                // Check effect item click (search results or layer effects)
                bool isSearching = wcslen(g_searchQuery) > 0;
                if (y >= itemsStartY) {
                    int idx = (y - itemsStartY) / ITEM_HEIGHT;
                    if (isSearching) {
                        // Search results mode - add effect to layer
                        if (idx >= 0 && idx < (int)g_searchResults.size()) {
                            g_result.effectSelected = true;
                            g_result.selectedEffect = g_searchResults[idx];
                            wcscpy_s(g_result.searchQuery, g_searchQuery);
                            if (!g_keepPanelOpen) {
                                ControlUI::HidePanel();
                            }
                        }
                    } else {
                        // Layer effects mode
                        if (idx >= 0 && idx < (int)g_layerEffects.size()) {
                            int itemY = itemsStartY + idx * ITEM_HEIGHT;
                            int btnX = WINDOW_WIDTH - PADDING - ACTION_BUTTON_SIZE - 4;
                            int btnY = itemY + (ITEM_HEIGHT - ACTION_BUTTON_SIZE) / 2;

                            // Check if clicked on delete button
                            if (x >= btnX && x < btnX + ACTION_BUTTON_SIZE &&
                                y >= btnY && y < btnY + ACTION_BUTTON_SIZE) {
                                g_result.effectSelected = true;
                                g_result.selectedEffect = g_layerEffects[idx];
                                g_result.action = ControlUI::ACTION_DELETE;
                                g_result.effectIndex = g_layerEffects[idx].index;
                                if (!g_keepPanelOpen) {
                                    ControlUI::HidePanel();
                                }
                            } else {
                                // Clicked on effect name -> expand this effect
                                g_result.effectSelected = true;
                                g_result.selectedEffect = g_layerEffects[idx];
                                g_result.action = ControlUI::ACTION_EXPAND;
                                g_result.effectIndex = g_layerEffects[idx].index;
                                if (!g_keepPanelOpen) {
                                    ControlUI::HidePanel();
                                }
                            }
                        }
                    }
                }
            }
            return 0;
        }

        case WM_ACTIVATE: {
            // Close when window is deactivated (clicked outside)
            if (LOWORD(wParam) == WA_INACTIVE && !g_isDragging) {
                g_result.cancelled = true;
                ControlUI::HidePanel();
            }
            return 0;
        }

        case WM_KILLFOCUS: {
            // Don't close if we're being dragged
            if (g_isDragging) {
                return 0;
            }
            // Close panel when focus is lost
            g_result.cancelled = true;
            ControlUI::HidePanel();
            return 0;
        }

        case WM_NCLBUTTONDOWN: {
            // Starting a drag operation
            g_isDragging = true;
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        case WM_EXITSIZEMOVE:
        case WM_CAPTURECHANGED: {
            // Drag operation ended
            g_isDragging = false;
            return 0;
        }

        case WM_NCHITTEST: {
            // Allow dragging from non-interactive areas
            // Use (short) cast to handle negative screen coordinates properly
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
            ScreenToClient(hwnd, &pt);
            // Inverse scale for hit testing (coordinates are in scaled window space)
            pt.x = InverseScaled(pt.x);
            pt.y = InverseScaled(pt.y);

            if (g_panelMode == ControlUI::MODE_SEARCH) {
                // Mode 1: Search mode
                int headerHeight = SEARCH_HEIGHT;
                int searchBottom = PADDING + headerHeight;
                int itemsStartY = searchBottom + PADDING;

                // Pin button (right side of search bar)
                int pinBtnX = WINDOW_WIDTH - PADDING - NEW_EC_BUTTON_SIZE;
                int pinBtnY = PADDING + (headerHeight - NEW_EC_BUTTON_SIZE) / 2;
                if (pt.x >= pinBtnX && pt.x <= pinBtnX + NEW_EC_BUTTON_SIZE &&
                    pt.y >= pinBtnY && pt.y <= pinBtnY + NEW_EC_BUTTON_SIZE) {
                    return HTCLIENT;  // Pin button - clickable
                }

                // Search input area (excluding pin button area)
                if (pt.y >= PADDING && pt.y < searchBottom && pt.x < pinBtnX) {
                    return HTCLIENT;  // Text input area - no drag
                }

                // Result items area
                int visibleCount = min((int)g_searchResults.size(), MAX_VISIBLE_ITEMS);
                int itemsEndY = itemsStartY + visibleCount * ITEM_HEIGHT;
                if (pt.y >= itemsStartY && pt.y < itemsEndY &&
                    pt.x >= PADDING && pt.x <= WINDOW_WIDTH - PADDING) {
                    return HTCLIENT;  // Item area - clickable
                }
            } else {
                // Mode 2: Effects list mode
                int headerY = PADDING;
                int headerBottom = headerY + HEADER_HEIGHT;
                int presetBarY = headerBottom + 4;
                int presetBottom = presetBarY + PRESET_BAR_HEIGHT;
                int searchBarY = presetBottom;
                int searchBottom = searchBarY + SEARCH_HEIGHT;
                int itemsStartY = searchBottom + PADDING;

                // Pin button (header, right side)
                int pinBtnX = WINDOW_WIDTH - PADDING - NEW_EC_BUTTON_SIZE;
                int pinBtnY = headerY + (HEADER_HEIGHT - NEW_EC_BUTTON_SIZE) / 2;
                if (pt.x >= pinBtnX && pt.x <= pinBtnX + NEW_EC_BUTTON_SIZE &&
                    pt.y >= pinBtnY && pt.y <= pinBtnY + NEW_EC_BUTTON_SIZE) {
                    return HTCLIENT;  // Pin button - clickable
                }

                // New EC button [+] (header, left of pin)
                int newECBtnX = pinBtnX - NEW_EC_BUTTON_SIZE - 4;
                int newECBtnY = headerY + (HEADER_HEIGHT - NEW_EC_BUTTON_SIZE) / 2;
                if (pt.x >= newECBtnX && pt.x <= newECBtnX + NEW_EC_BUTTON_SIZE &&
                    pt.y >= newECBtnY && pt.y <= newECBtnY + NEW_EC_BUTTON_SIZE) {
                    return HTCLIENT;  // New EC button - clickable
                }

                // Preset buttons area (6 preset buttons + save button)
                if (pt.y >= presetBarY && pt.y < presetBottom) {
                    int presetBtnSpacing = 4;
                    int totalPresetWidth = PRESET_SLOT_COUNT * (PRESET_BUTTON_HEIGHT + presetBtnSpacing) + SAVE_BUTTON_SIZE + presetBtnSpacing;
                    int presetStartX = (WINDOW_WIDTH - totalPresetWidth) / 2;
                    int presetEndX = presetStartX + totalPresetWidth;
                    if (pt.x >= presetStartX && pt.x < presetEndX) {
                        return HTCLIENT;  // Preset buttons - clickable
                    }
                }

                // Search input area
                if (pt.y >= searchBarY && pt.y < searchBottom &&
                    pt.x >= PADDING && pt.x <= WINDOW_WIDTH - PADDING) {
                    return HTCLIENT;  // Text input area - no drag
                }

                // Effect items area
                int visibleCount = min((int)(wcslen(g_searchQuery) > 0 ? g_searchResults.size() : g_layerEffects.size()), MAX_VISIBLE_ITEMS);
                int itemsEndY = itemsStartY + visibleCount * ITEM_HEIGHT;
                if (pt.y >= itemsStartY && pt.y < itemsEndY &&
                    pt.x >= PADDING && pt.x <= WINDOW_WIDTH - PADDING) {
                    return HTCLIENT;  // Item area - clickable
                }
            }

            // Everything else is draggable
            return HTCAPTION;
        }

        case WM_TIMER: {
            if (wParam == 2) {  // Cursor blink timer
                g_cursorVisible = !g_cursorVisible;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_DESTROY:
            g_hwnd = NULL;
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ControlUI::SetPresetSlotFilled(int slotIndex, bool filled) {
    if (slotIndex >= 0 && slotIndex < PRESET_SLOT_COUNT) {
        // If filled but icon is NONE, set a default icon
        if (filled && g_presetIcons[slotIndex] == ICON_NONE) {
            g_presetIcons[slotIndex] = ICON_MAGIC;  // Default icon
        } else if (!filled) {
            g_presetIcons[slotIndex] = ICON_NONE;
        }
    }
}

bool ControlUI::IsPresetSlotFilled(int slotIndex) {
    if (slotIndex >= 0 && slotIndex < PRESET_SLOT_COUNT) {
        return g_presetIcons[slotIndex] != ICON_NONE;
    }
    return false;
}

// Set icon for a preset slot
void ControlUI::SetPresetSlotIcon(int slotIndex, int iconType) {
    if (slotIndex >= 0 && slotIndex < PRESET_SLOT_COUNT) {
        if (iconType >= ICON_NONE && iconType < ICON_COUNT) {
            g_presetIcons[slotIndex] = static_cast<PresetIcon>(iconType);
        }
    }
}

// Get icon type for a preset slot
int ControlUI::GetPresetSlotIcon(int slotIndex) {
    if (slotIndex >= 0 && slotIndex < PRESET_SLOT_COUNT) {
        return static_cast<int>(g_presetIcons[slotIndex]);
    }
    return ICON_NONE;
}

// Set layer info for header display
void ControlUI::SetLayerInfo(const wchar_t* layerName, int labelColor) {
    if (layerName) {
        wcscpy_s(g_currentLayerName, layerName);
    } else {
        g_currentLayerName[0] = L'\0';
    }
    g_currentLayerLabelColor = labelColor;
}

#else // macOS stub

namespace ControlUI {

void Initialize() {}
void Shutdown() {}
void SetMode(PanelMode) {}
PanelMode GetMode() { return MODE_SEARCH; }
void ShowPanel() {}
void ShowPanelAt(int, int) {}
void HidePanel() {}
bool IsVisible() { return false; }
ControlResult GetResult() { return ControlResult(); }
ControlSettings& GetSettings() { static ControlSettings s; return s; }
void UpdateSearch(const wchar_t*) {}
void SetAvailableEffects(const wchar_t*) {}
void SetLayerEffects(const wchar_t*) {}
void ClearLayerEffects() {}
void SetPresetSlotFilled(int, bool) {}
bool IsPresetSlotFilled(int) { return false; }
void SetPresetSlotIcon(int, int) {}
int GetPresetSlotIcon(int) { return 0; }
void SetLayerInfo(const wchar_t*, int) {}

} // namespace ControlUI

#endif // MSWindows
