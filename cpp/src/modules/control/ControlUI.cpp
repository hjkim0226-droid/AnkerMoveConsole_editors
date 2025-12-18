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

// Close button state
static bool g_closeButtonHover = false;

// New EC window button state (Mode 2)
static bool g_newECButtonHover = false;
static const int NEW_EC_BUTTON_SIZE = 20;

// Preset button state (Mode 2)
static int g_hoveredPresetButton = -1;  // -1 = none, 0-2 = slot buttons
static int g_presetSlots[3] = {-1, -1, -1};  // Assigned preset indices

// Save mode state (Mode 2)
static bool g_saveMode = false;         // When true, clicking preset button saves
static bool g_saveButtonHover = false;  // Hover state for save button
static const int SAVE_BUTTON_WIDTH = 36;
static const int SAVE_BUTTON_HEIGHT = 20;

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

    // Calculate Y offset to center the search bar at mouse position
    int searchBarCenterY;
    if (g_panelMode == MODE_SEARCH) {
        // Mode 1: Search bar at top
        searchBarCenterY = PADDING + SEARCH_HEIGHT / 2;
    } else {
        // Mode 2: Search bar below header and preset bar
        searchBarCenterY = PADDING + HEADER_HEIGHT + 4 + PRESET_BAR_HEIGHT + SEARCH_HEIGHT / 2;
    }

    // Create or reposition window
    if (!g_hwnd) {
        g_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            CONTROL_CLASS_NAME,
            g_panelMode == MODE_SEARCH ? L"Effect Search" : L"Layer Effects",
            WS_POPUP,
            x - WINDOW_WIDTH / 2,
            y - searchBarCenterY,
            WINDOW_WIDTH,
            windowHeight,
            NULL, NULL,
            GetModuleHandle(NULL),
            NULL
        );
        SetLayeredWindowAttributes(g_hwnd, 0, 245, LWA_ALPHA);
    } else {
        SetWindowPos(g_hwnd, HWND_TOPMOST,
                     x - WINDOW_WIDTH / 2,
                     y - searchBarCenterY,
                     WINDOW_WIDTH, windowHeight,
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

// Search implementation
void PerformSearch(const wchar_t* query) {
    g_searchResults.clear();

    std::wstring q(query);
    // Convert to lowercase for case-insensitive search
    std::transform(q.begin(), q.end(), q.begin(), ::towlower);

    int idx = 0;

    // Use dynamic effects list if available, otherwise fallback to built-in
    if (!g_availableEffects.empty()) {
        // Search in dynamic list (localized names from AE)
        for (const auto& effect : g_availableEffects) {
            std::wstring name(effect.name);
            std::wstring nameLower = name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);

            if (q.empty() || nameLower.find(q) != std::wstring::npos) {
                ControlUI::EffectItem item;
                wcscpy_s(item.name, effect.name);
                wcscpy_s(item.matchName, effect.matchName);
                wcscpy_s(item.category, effect.category);
                item.index = idx++;
                item.isLayerEffect = false;
                g_searchResults.push_back(item);

                if (g_searchResults.size() >= 20) break;
            }
        }
    } else {
        // Fallback to built-in effects list
        for (int i = 0; BUILTIN_EFFECTS[i][0] != NULL; i++) {
            std::wstring name(BUILTIN_EFFECTS[i][0]);
            std::wstring nameLower = name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);

            if (q.empty() || nameLower.find(q) != std::wstring::npos) {
                ControlUI::EffectItem item;
                wcscpy_s(item.name, BUILTIN_EFFECTS[i][0]);
                wcscpy_s(item.matchName, BUILTIN_EFFECTS[i][1]);
                wcscpy_s(item.category, BUILTIN_EFFECTS[i][2]);
                item.index = idx++;
                item.isLayerEffect = false;
                g_searchResults.push_back(item);

                if (g_searchResults.size() >= 20) break;
            }
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

    // Background
    SolidBrush bgBrush(COLOR_BG);
    graphics.FillRectangle(&bgBrush, 0, 0, width, height);

    // Border
    Pen borderPen(COLOR_BORDER, 1);
    graphics.DrawRectangle(&borderPen, 0, 0, width - 1, height - 1);

    // Search box background
    SolidBrush searchBgBrush(COLOR_SEARCH_BG);
    RectF searchRect(PADDING, PADDING, width - PADDING * 2 - CLOSE_BUTTON_SIZE - 4, SEARCH_HEIGHT);
    graphics.FillRectangle(&searchBgBrush, searchRect);

    // Close button [x]
    int closeBtnX = width - PADDING - CLOSE_BUTTON_SIZE;
    int closeBtnY = PADDING + (SEARCH_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
    RectF closeRect((REAL)closeBtnX, (REAL)closeBtnY, (REAL)CLOSE_BUTTON_SIZE, (REAL)CLOSE_BUTTON_SIZE);

    if (g_closeButtonHover) {
        SolidBrush closeBgBrush(COLOR_CLOSE_HOVER);
        graphics.FillRectangle(&closeBgBrush, closeRect);
    }

    // Draw X
    Pen xPen(COLOR_TEXT, 2);
    float margin = 5.0f;
    graphics.DrawLine(&xPen,
        closeBtnX + margin, closeBtnY + margin,
        closeBtnX + CLOSE_BUTTON_SIZE - margin, closeBtnY + CLOSE_BUTTON_SIZE - margin);
    graphics.DrawLine(&xPen,
        closeBtnX + CLOSE_BUTTON_SIZE - margin, closeBtnY + margin,
        closeBtnX + margin, closeBtnY + CLOSE_BUTTON_SIZE - margin);

    // Search text
    FontFamily fontFamily(L"Segoe UI");
    Font searchFont(&fontFamily, 14, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);

    RectF textRect(PADDING + 8, PADDING + 8, width - PADDING * 2 - 16, SEARCH_HEIGHT - 16);
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
        RectF itemRect(PADDING, y, width - PADDING * 2, ITEM_HEIGHT);

        // Highlight selected/hover
        if (i == g_selectedIndex) {
            SolidBrush selectedBrush(COLOR_ITEM_SELECTED);
            graphics.FillRectangle(&selectedBrush, itemRect);
        } else if (i == g_hoverIndex) {
            SolidBrush hoverBrush(COLOR_ITEM_HOVER);
            graphics.FillRectangle(&hoverBrush, itemRect);
        }

        // Effect name
        RectF nameRect(PADDING + 8, y + 4, width - PADDING * 2 - 100, ITEM_HEIGHT / 2);
        graphics.DrawString(item.name, -1, &itemFont, nameRect, &sf, &textBrush);

        // Category (right aligned)
        StringFormat sfRight;
        sfRight.SetAlignment(StringAlignmentFar);
        sfRight.SetLineAlignment(StringAlignmentCenter);
        RectF catRect(width - 110, y, 100, ITEM_HEIGHT);
        graphics.DrawString(item.category, -1, &categoryFont, catRect, &sfRight, &dimBrush);

        y += ITEM_HEIGHT;
    }

    // No results message
    if (g_searchResults.empty() && wcslen(g_searchQuery) > 0) {
        RectF msgRect(PADDING, y, width - PADDING * 2, ITEM_HEIGHT);
        graphics.DrawString(L"No effects found", -1, &itemFont, msgRect, &sf, &dimBrush);
    }
}

// Draw the effects panel (Mode 2)
void DrawEffectsPanel(HDC hdc, int width, int height) {
    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Background
    SolidBrush bgBrush(COLOR_BG);
    graphics.FillRectangle(&bgBrush, 0, 0, width, height);

    // Border
    Pen borderPen(COLOR_BORDER, 1);
    graphics.DrawRectangle(&borderPen, 0, 0, width - 1, height - 1);

    // Fonts
    FontFamily fontFamily(L"Segoe UI");
    Font headerFont(&fontFamily, 12, FontStyleBold, UnitPixel);
    Font itemFont(&fontFamily, 12, FontStyleRegular, UnitPixel);
    Font indexFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    Font presetFont(&fontFamily, 14, FontStyleBold, UnitPixel);
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

    // ===== Header bar =====
    SolidBrush headerBgBrush(COLOR_SEARCH_BG);
    RectF headerRect(PADDING, currentY, width - PADDING * 2 - CLOSE_BUTTON_SIZE - NEW_EC_BUTTON_SIZE - 8, HEADER_HEIGHT);
    graphics.FillRectangle(&headerBgBrush, headerRect);

    // Close button [x]
    int closeBtnX = width - PADDING - CLOSE_BUTTON_SIZE;
    int closeBtnY = currentY + (HEADER_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
    RectF closeRect((REAL)closeBtnX, (REAL)closeBtnY, (REAL)CLOSE_BUTTON_SIZE, (REAL)CLOSE_BUTTON_SIZE);

    if (g_closeButtonHover) {
        SolidBrush closeBgBrush(COLOR_CLOSE_HOVER);
        graphics.FillRectangle(&closeBgBrush, closeRect);
    }

    Pen xPen(COLOR_TEXT, 2);
    float margin = 5.0f;
    graphics.DrawLine(&xPen,
        closeBtnX + margin, closeBtnY + margin,
        closeBtnX + CLOSE_BUTTON_SIZE - margin, closeBtnY + CLOSE_BUTTON_SIZE - margin);
    graphics.DrawLine(&xPen,
        closeBtnX + CLOSE_BUTTON_SIZE - margin, closeBtnY + margin,
        closeBtnX + margin, closeBtnY + CLOSE_BUTTON_SIZE - margin);

    // New EC window button [+] - left of close button
    int newECBtnX = closeBtnX - NEW_EC_BUTTON_SIZE - 4;
    int newECBtnY = currentY + (HEADER_HEIGHT - NEW_EC_BUTTON_SIZE) / 2;
    RectF newECRect((REAL)newECBtnX, (REAL)newECBtnY, (REAL)NEW_EC_BUTTON_SIZE, (REAL)NEW_EC_BUTTON_SIZE);

    if (g_newECButtonHover) {
        SolidBrush newECBgBrush(COLOR_PRESET_HOVER);
        graphics.FillRectangle(&newECBgBrush, newECRect);
    }

    // Draw + sign
    Pen plusPen(COLOR_ACCENT, 2);
    float plusMargin = 5.0f;
    // Horizontal line
    graphics.DrawLine(&plusPen,
        (REAL)(newECBtnX + plusMargin), (REAL)(newECBtnY + NEW_EC_BUTTON_SIZE / 2),
        (REAL)(newECBtnX + NEW_EC_BUTTON_SIZE - plusMargin), (REAL)(newECBtnY + NEW_EC_BUTTON_SIZE / 2));
    // Vertical line
    graphics.DrawLine(&plusPen,
        (REAL)(newECBtnX + NEW_EC_BUTTON_SIZE / 2), (REAL)(newECBtnY + plusMargin),
        (REAL)(newECBtnX + NEW_EC_BUTTON_SIZE / 2), (REAL)(newECBtnY + NEW_EC_BUTTON_SIZE - plusMargin));

    // Save button [S] - left of [+] button
    int saveBtnX = newECBtnX - SAVE_BUTTON_WIDTH - 4;
    int saveBtnY = currentY + (HEADER_HEIGHT - SAVE_BUTTON_HEIGHT) / 2;
    RectF saveRect((REAL)saveBtnX, (REAL)saveBtnY, (REAL)SAVE_BUTTON_WIDTH, (REAL)SAVE_BUTTON_HEIGHT);

    // Save button background - highlight when in save mode
    Color saveBtnColor = g_saveMode ? COLOR_PRESET_ACTIVE :
                         g_saveButtonHover ? COLOR_PRESET_HOVER : COLOR_PRESET_BG;
    SolidBrush saveBtnBrush(saveBtnColor);
    graphics.FillRectangle(&saveBtnBrush, saveRect);

    // Save button border
    Pen saveBorder(COLOR_BORDER, 1);
    graphics.DrawRectangle(&saveBorder, saveRect);

    // Save button text "S"
    Font saveFont(L"Segoe UI", 10, FontStyleBold, UnitPixel);
    graphics.DrawString(L"S", -1, &saveFont, saveRect, &sfCenter, &textBrush);

    // Header text
    RectF titleRect(PADDING + 8, currentY, width - PADDING * 2 - CLOSE_BUTTON_SIZE - NEW_EC_BUTTON_SIZE - 20, HEADER_HEIGHT);
    graphics.DrawString(L"Layer Effects", -1, &headerFont, titleRect, &sf, &textBrush);

    currentY += HEADER_HEIGHT + 4;

    // ===== Preset buttons bar =====
    int presetBarY = currentY;
    int presetBtnSpacing = 4;
    int totalPresetWidth = 3 * PRESET_BUTTON_WIDTH + 2 * presetBtnSpacing;
    int presetStartX = (width - totalPresetWidth) / 2;

    for (int i = 0; i < 3; i++) {
        int btnX = presetStartX + i * (PRESET_BUTTON_WIDTH + presetBtnSpacing);
        int btnY = presetBarY + (PRESET_BAR_HEIGHT - PRESET_BUTTON_HEIGHT) / 2;

        RectF btnRect((REAL)btnX, (REAL)btnY, (REAL)PRESET_BUTTON_WIDTH, (REAL)PRESET_BUTTON_HEIGHT);

        // Button background - different color in save mode
        Color btnColor;
        if (g_saveMode) {
            // In save mode: all buttons glow orange to indicate ready for save
            btnColor = (i == g_hoveredPresetButton) ? Color(255, 255, 180, 0) :  // Bright orange on hover
                       Color(255, 200, 120, 0);  // Orange glow
        } else {
            // Normal mode
            btnColor = (i == g_hoveredPresetButton) ? COLOR_PRESET_HOVER :
                       (g_presetSlots[i] >= 0) ? COLOR_PRESET_ACTIVE : COLOR_PRESET_BG;
        }
        SolidBrush btnBrush(btnColor);
        graphics.FillRectangle(&btnBrush, btnRect);

        // Button border - thicker in save mode
        if (g_saveMode) {
            Pen btnBorder(Color(255, 255, 200, 100), 2);
            graphics.DrawRectangle(&btnBorder, btnRect);
        } else {
            Pen btnBorder(COLOR_BORDER, 1);
            graphics.DrawRectangle(&btnBorder, btnRect);
        }

        // Button text
        wchar_t btnText[4];
        swprintf_s(btnText, L"%d", i + 1);
        graphics.DrawString(btnText, -1, &presetFont, btnRect, &sfCenter, &textBrush);
    }

    currentY += PRESET_BAR_HEIGHT;

    // ===== Search bar =====
    SolidBrush searchBgBrush(COLOR_SEARCH_BG);
    RectF searchRect(PADDING, currentY, width - PADDING * 2, SEARCH_HEIGHT);
    graphics.FillRectangle(&searchBgBrush, searchRect);

    // Search text or placeholder
    RectF searchTextRect(PADDING + 8, currentY, width - PADDING * 2 - 16, SEARCH_HEIGHT);
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
            RectF msgRect(PADDING, currentY, width - PADDING * 2, ITEM_HEIGHT);
            graphics.DrawString(L"No matching effects", -1, &itemFont, msgRect, &sf, &dimBrush);
            return;
        }

        for (int i = 0; i < visibleCount; i++) {
            const auto& item = g_searchResults[i];
            RectF itemRect(PADDING, currentY, width - PADDING * 2, ITEM_HEIGHT);

            // Highlight selected/hover
            if (i == g_selectedIndex) {
                SolidBrush selectedBrush(COLOR_ITEM_SELECTED);
                graphics.FillRectangle(&selectedBrush, itemRect);
            } else if (i == g_hoverIndex) {
                SolidBrush hoverBrush(COLOR_ITEM_HOVER);
                graphics.FillRectangle(&hoverBrush, itemRect);
            }

            // Effect name
            RectF nameRect(PADDING + 8, currentY, width - PADDING * 2 - 100, ITEM_HEIGHT);
            graphics.DrawString(item.name, -1, &itemFont, nameRect, &sf, &textBrush);

            // Category
            RectF catRect(width - PADDING - 90, currentY, 80, ITEM_HEIGHT);
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
            RectF msgRect(PADDING, currentY, width - PADDING * 2, ITEM_HEIGHT);
            graphics.DrawString(L"No effects on layer", -1, &itemFont, msgRect, &sf, &dimBrush);
            return;
        }

        for (int i = 0; i < visibleCount; i++) {
            const auto& item = g_layerEffects[i];
            RectF itemRect(PADDING, currentY, width - PADDING * 2, ITEM_HEIGHT);

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
            RectF nameRect(PADDING + 44, currentY, width - PADDING * 2 - 80, ITEM_HEIGHT);
            graphics.DrawString(item.name, -1, &itemFont, nameRect, &sf, &textBrush);

            // Delete button [x]
            int btnX = width - PADDING - ACTION_BUTTON_SIZE - 4;
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

        case WM_KEYDOWN: {
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
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            RECT rc;
            GetClientRect(hwnd, &rc);
            bool needRedraw = false;

            if (g_panelMode == ControlUI::MODE_SEARCH) {
                // Mode 1: Search mode
                int headerHeight = SEARCH_HEIGHT;
                int startY = PADDING + headerHeight + PADDING;

                // Check close button hover
                int closeBtnX = rc.right - PADDING - CLOSE_BUTTON_SIZE;
                int closeBtnY = PADDING + (headerHeight - CLOSE_BUTTON_SIZE) / 2;
                bool wasCloseHover = g_closeButtonHover;
                g_closeButtonHover = (x >= closeBtnX && x < closeBtnX + CLOSE_BUTTON_SIZE &&
                                      y >= closeBtnY && y < closeBtnY + CLOSE_BUTTON_SIZE);
                if (wasCloseHover != g_closeButtonHover) needRedraw = true;

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

                // Check close button hover (in header)
                int closeBtnX = rc.right - PADDING - CLOSE_BUTTON_SIZE;
                int closeBtnY = headerY + (HEADER_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
                bool wasCloseHover = g_closeButtonHover;
                g_closeButtonHover = (x >= closeBtnX && x < closeBtnX + CLOSE_BUTTON_SIZE &&
                                      y >= closeBtnY && y < closeBtnY + CLOSE_BUTTON_SIZE);
                if (wasCloseHover != g_closeButtonHover) needRedraw = true;

                // Check new EC button hover [+]
                int newECBtnX = closeBtnX - NEW_EC_BUTTON_SIZE - 4;
                int newECBtnY = headerY + (HEADER_HEIGHT - NEW_EC_BUTTON_SIZE) / 2;
                bool wasNewECHover = g_newECButtonHover;
                g_newECButtonHover = (x >= newECBtnX && x < newECBtnX + NEW_EC_BUTTON_SIZE &&
                                      y >= newECBtnY && y < newECBtnY + NEW_EC_BUTTON_SIZE);
                if (wasNewECHover != g_newECButtonHover) needRedraw = true;

                // Check save button hover [S]
                int saveBtnX = newECBtnX - SAVE_BUTTON_WIDTH - 4;
                int saveBtnY = headerY + (HEADER_HEIGHT - SAVE_BUTTON_HEIGHT) / 2;
                bool wasSaveHover = g_saveButtonHover;
                g_saveButtonHover = (x >= saveBtnX && x < saveBtnX + SAVE_BUTTON_WIDTH &&
                                     y >= saveBtnY && y < saveBtnY + SAVE_BUTTON_HEIGHT);
                if (wasSaveHover != g_saveButtonHover) needRedraw = true;

                // Check preset button hover
                int presetBtnSpacing = 4;
                int totalPresetWidth = 3 * PRESET_BUTTON_WIDTH + 2 * presetBtnSpacing;
                int presetStartX = (rc.right - totalPresetWidth) / 2;
                int oldHoveredPreset = g_hoveredPresetButton;
                g_hoveredPresetButton = -1;

                for (int i = 0; i < 3; i++) {
                    int btnX = presetStartX + i * (PRESET_BUTTON_WIDTH + presetBtnSpacing);
                    int btnY = presetBarY + (PRESET_BAR_HEIGHT - PRESET_BUTTON_HEIGHT) / 2;
                    if (x >= btnX && x < btnX + PRESET_BUTTON_WIDTH &&
                        y >= btnY && y < btnY + PRESET_BUTTON_HEIGHT) {
                        g_hoveredPresetButton = i;
                        break;
                    }
                }
                if (oldHoveredPreset != g_hoveredPresetButton) needRedraw = true;

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
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            RECT rc;
            GetClientRect(hwnd, &rc);

            if (g_panelMode == ControlUI::MODE_SEARCH) {
                // Mode 1: Search mode
                int headerHeight = SEARCH_HEIGHT;
                int startY = PADDING + headerHeight + PADDING;

                // Check close button click
                int closeBtnX = rc.right - PADDING - CLOSE_BUTTON_SIZE;
                int closeBtnY = PADDING + (headerHeight - CLOSE_BUTTON_SIZE) / 2;
                if (x >= closeBtnX && x < closeBtnX + CLOSE_BUTTON_SIZE &&
                    y >= closeBtnY && y < closeBtnY + CLOSE_BUTTON_SIZE) {
                    g_result.cancelled = true;
                    ControlUI::HidePanel();
                    return 0;
                }

                // Check item click
                if (y >= startY) {
                    int idx = (y - startY) / ITEM_HEIGHT;
                    if (idx >= 0 && idx < (int)g_searchResults.size()) {
                        g_result.effectSelected = true;
                        g_result.selectedEffect = g_searchResults[idx];
                        wcscpy_s(g_result.searchQuery, g_searchQuery);
                        ControlUI::HidePanel();
                    }
                }
            } else {
                // Mode 2: Effects panel with preset buttons
                // Layout: Header -> Presets -> Search -> Items
                int headerY = PADDING;
                int presetBarY = headerY + HEADER_HEIGHT + 4;
                int searchBarY = presetBarY + PRESET_BAR_HEIGHT;
                int itemsStartY = searchBarY + SEARCH_HEIGHT + PADDING;

                // Check close button click (in header)
                int closeBtnX = rc.right - PADDING - CLOSE_BUTTON_SIZE;
                int closeBtnY = headerY + (HEADER_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
                if (x >= closeBtnX && x < closeBtnX + CLOSE_BUTTON_SIZE &&
                    y >= closeBtnY && y < closeBtnY + CLOSE_BUTTON_SIZE) {
                    g_result.cancelled = true;
                    ControlUI::HidePanel();
                    return 0;
                }

                // Check new EC button click [+]
                int newECBtnX = closeBtnX - NEW_EC_BUTTON_SIZE - 4;
                int newECBtnY = headerY + (HEADER_HEIGHT - NEW_EC_BUTTON_SIZE) / 2;
                if (x >= newECBtnX && x < newECBtnX + NEW_EC_BUTTON_SIZE &&
                    y >= newECBtnY && y < newECBtnY + NEW_EC_BUTTON_SIZE) {
                    g_result.action = ControlUI::ACTION_NEW_EC_WINDOW;
                    ControlUI::HidePanel();
                    return 0;
                }

                // Check save button click [S]
                int saveBtnX = newECBtnX - SAVE_BUTTON_WIDTH - 4;
                int saveBtnY = headerY + (HEADER_HEIGHT - SAVE_BUTTON_HEIGHT) / 2;
                if (x >= saveBtnX && x < saveBtnX + SAVE_BUTTON_WIDTH &&
                    y >= saveBtnY && y < saveBtnY + SAVE_BUTTON_HEIGHT) {
                    // Toggle save mode
                    g_saveMode = !g_saveMode;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }

                // Check preset button click
                int presetBtnSpacing = 4;
                int totalPresetWidth = 3 * PRESET_BUTTON_WIDTH + 2 * presetBtnSpacing;
                int presetStartX = (rc.right - totalPresetWidth) / 2;

                for (int i = 0; i < 3; i++) {
                    int btnX = presetStartX + i * (PRESET_BUTTON_WIDTH + presetBtnSpacing);
                    int btnY = presetBarY + (PRESET_BAR_HEIGHT - PRESET_BUTTON_HEIGHT) / 2;
                    if (x >= btnX && x < btnX + PRESET_BUTTON_WIDTH &&
                        y >= btnY && y < btnY + PRESET_BUTTON_HEIGHT) {
                        // Preset button clicked
                        if (g_saveMode) {
                            // Save mode: save current effects to this slot
                            g_result.action = ControlUI::ACTION_SAVE_PRESET;
                            g_result.presetSlotIndex = i;
                            g_result.effectSelected = true;  // Signal that action is valid
                            g_saveMode = false;  // Exit save mode
                            ControlUI::HidePanel();
                        } else {
                            // Normal mode: always try to apply preset
                            // (SnapPlugin will check if file exists and show "slot empty" if not)
                            g_result.action = ControlUI::ACTION_APPLY_PRESET;
                            g_result.presetSlotIndex = i;
                            g_result.effectSelected = true;  // Signal that action is valid
                            ControlUI::HidePanel();
                        }
                        return 0;
                    }
                }

                // Check effect item click
                if (y >= itemsStartY) {
                    int idx = (y - itemsStartY) / ITEM_HEIGHT;
                    if (idx >= 0 && idx < (int)g_layerEffects.size()) {
                        int itemY = itemsStartY + idx * ITEM_HEIGHT;
                        int btnX = rc.right - PADDING - ACTION_BUTTON_SIZE - 4;
                        int btnY = itemY + (ITEM_HEIGHT - ACTION_BUTTON_SIZE) / 2;

                        // Check if clicked on delete button
                        if (x >= btnX && x < btnX + ACTION_BUTTON_SIZE &&
                            y >= btnY && y < btnY + ACTION_BUTTON_SIZE) {
                            g_result.effectSelected = true;
                            g_result.selectedEffect = g_layerEffects[idx];
                            g_result.action = ControlUI::ACTION_DELETE;
                            g_result.effectIndex = g_layerEffects[idx].index;
                            ControlUI::HidePanel();
                        } else {
                            // Clicked on effect name -> expand this effect
                            g_result.effectSelected = true;
                            g_result.selectedEffect = g_layerEffects[idx];
                            g_result.action = ControlUI::ACTION_EXPAND;
                            g_result.effectIndex = g_layerEffects[idx].index;
                            ControlUI::HidePanel();
                        }
                    }
                }
            }
            return 0;
        }

        case WM_KILLFOCUS: {
            // Don't close if we're being dragged
            if (g_isDragging) {
                return 0;
            }
            // Check where focus went - if it's an AE window, don't close
            HWND newFocus = (HWND)wParam;
            if (newFocus) {
                char className[64] = {0};
                GetClassNameA(newFocus, className, sizeof(className));
                // AE windows have class names starting with "AE_"
                if (_strnicmp(className, "AE_", 3) == 0) {
                    // Focus went to AE window, don't close
                    return 0;
                }
            }
            // Focus went elsewhere, close panel
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
            RECT rc;
            GetClientRect(hwnd, &rc);

            // Calculate close button position
            int closeBtnX = rc.right - PADDING - CLOSE_BUTTON_SIZE;
            int closeBtnY = PADDING + (SEARCH_HEIGHT - CLOSE_BUTTON_SIZE) / 2;

            // Check if on close button
            if (pt.x >= closeBtnX && pt.x <= rc.right - PADDING &&
                pt.y >= closeBtnY && pt.y <= closeBtnY + CLOSE_BUTTON_SIZE) {
                return HTCLIENT;  // Let click through for close button
            }

            if (g_panelMode == ControlUI::MODE_SEARCH) {
                // Mode 1: Search mode
                int searchBottom = PADDING + SEARCH_HEIGHT;
                int itemsStartY = searchBottom + PADDING;

                // Search input area (excluding close button area)
                if (pt.y >= PADDING && pt.y < searchBottom && pt.x < closeBtnX) {
                    return HTCLIENT;  // Text input area - no drag
                }

                // Result items area
                int visibleCount = min((int)g_searchResults.size(), MAX_VISIBLE_ITEMS);
                int itemsEndY = itemsStartY + visibleCount * ITEM_HEIGHT;
                if (pt.y >= itemsStartY && pt.y < itemsEndY &&
                    pt.x >= PADDING && pt.x <= rc.right - PADDING) {
                    return HTCLIENT;  // Item area - clickable
                }
            } else {
                // Mode 2: Effects list mode
                int headerBottom = PADDING + HEADER_HEIGHT;
                int presetBottom = headerBottom + PRESET_BAR_HEIGHT;
                int searchBarY = presetBottom + 4;
                int searchBottom = searchBarY + SEARCH_HEIGHT;
                int itemsStartY = searchBottom + PADDING;

                // Preset buttons area (check individual buttons)
                if (pt.y >= headerBottom && pt.y < presetBottom) {
                    int totalPresetWidth = g_settings.slotCount * (PRESET_BUTTON_WIDTH + 4) + SAVE_BUTTON_WIDTH + 8;
                    int presetStartX = (rc.right - totalPresetWidth) / 2;
                    int presetEndX = presetStartX + totalPresetWidth;
                    if (pt.x >= presetStartX && pt.x < presetEndX) {
                        return HTCLIENT;  // Preset buttons - clickable
                    }
                }

                // Search input area
                if (pt.y >= searchBarY && pt.y < searchBottom &&
                    pt.x >= PADDING && pt.x <= rc.right - PADDING) {
                    return HTCLIENT;  // Text input area - no drag
                }

                // New EC button (in header)
                int newECBtnX = rc.right - PADDING - NEW_EC_BUTTON_SIZE;
                int newECBtnY = PADDING + (HEADER_HEIGHT - NEW_EC_BUTTON_SIZE) / 2;
                if (pt.x >= newECBtnX && pt.x <= newECBtnX + NEW_EC_BUTTON_SIZE &&
                    pt.y >= newECBtnY && pt.y <= newECBtnY + NEW_EC_BUTTON_SIZE) {
                    return HTCLIENT;  // New EC button - clickable
                }

                // Effect items area
                int visibleCount = min((int)(wcslen(g_searchQuery) > 0 ? g_searchResults.size() : g_layerEffects.size()), MAX_VISIBLE_ITEMS);
                int itemsEndY = itemsStartY + visibleCount * ITEM_HEIGHT;
                if (pt.y >= itemsStartY && pt.y < itemsEndY &&
                    pt.x >= PADDING && pt.x <= rc.right - PADDING) {
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
    if (slotIndex >= 0 && slotIndex < 3) {
        g_presetSlots[slotIndex] = filled ? slotIndex : -1;
    }
}

bool ControlUI::IsPresetSlotFilled(int slotIndex) {
    if (slotIndex >= 0 && slotIndex < 3) {
        return g_presetSlots[slotIndex] >= 0;
    }
    return false;
}

#else // macOS stub

namespace ControlUI {

void Initialize() {}
void Shutdown() {}
void ShowPanel() {}
void HidePanel() {}
bool IsVisible() { return false; }
ControlResult GetResult() { return ControlResult(); }
ControlSettings& GetSettings() { static ControlSettings s; return s; }
void UpdateSearch(const wchar_t*) {}
void SetPresetSlotFilled(int, bool) {}
bool IsPresetSlotFilled(int) { return false; }

} // namespace ControlUI

#endif // MSWindows
