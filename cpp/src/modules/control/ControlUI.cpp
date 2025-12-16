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

// Current mode
static ControlUI::PanelMode g_currentMode = ControlUI::MODE_SEARCH;

// Effect Controls tracking
static HWND g_attachedWindow = NULL;         // Effect Controls window we're attached to
static HWINEVENTHOOK g_winEventHook = NULL;  // Hook for position/close tracking
static bool g_isCollapsed = false;           // Thin bar collapsed state
static const int COLLAPSED_HEIGHT = 6;       // Height when collapsed

// Forward declaration for event hook callback
void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);

// Search state
static wchar_t g_searchQuery[256] = {0};
static std::vector<ControlUI::EffectItem> g_searchResults;
static std::vector<ControlUI::EffectItem> g_layerEffects;  // Effects on current layer
static int g_selectedIndex = 0;
static int g_hoverIndex = -1;

// Settings
static ControlUI::ControlSettings g_settings;

// Result
static ControlUI::ControlResult g_result;

// UI Constants
static const int WINDOW_WIDTH = 320;
static const int SEARCH_HEIGHT = 36;
static const int ITEM_HEIGHT = 32;
static const int PADDING = 8;
static const int MAX_VISIBLE_ITEMS = 8;
static const int CLOSE_BUTTON_SIZE = 20;  // Close button [x] size

// Hover state for close button
static bool g_hoverCloseButton = false;

// Colors
static const Color COLOR_BG(240, 28, 28, 32);
static const Color COLOR_SEARCH_BG(255, 40, 40, 48);
static const Color COLOR_ITEM_HOVER(255, 60, 80, 100);
static const Color COLOR_ITEM_SELECTED(255, 74, 158, 255);
static const Color COLOR_TEXT(255, 220, 220, 220);
static const Color COLOR_TEXT_DIM(255, 140, 140, 140);
static const Color COLOR_ACCENT(255, 74, 207, 255);
static const Color COLOR_BORDER(255, 60, 60, 70);

// Built-in effects list (common effects for search)
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
void PerformSearch(const wchar_t* query);

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
    // Unhook event listener
    if (g_winEventHook) {
        UnhookWinEvent(g_winEventHook);
        g_winEventHook = NULL;
    }

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

// Attach to Effect Controls window and start tracking
void AttachToEffectControls(HWND effectControlsWnd) {
    if (!effectControlsWnd || g_attachedWindow == effectControlsWnd) return;

    // Unhook previous if any
    if (g_winEventHook) {
        UnhookWinEvent(g_winEventHook);
        g_winEventHook = NULL;
    }

    g_attachedWindow = effectControlsWnd;

    // Get the thread ID of the Effect Controls window
    DWORD threadId = GetWindowThreadProcessId(effectControlsWnd, NULL);

    // Set up event hook for location change and destroy events
    g_winEventHook = SetWinEventHook(
        EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_DESTROY,
        NULL, WinEventProc,
        0, threadId,
        WINEVENT_OUTOFCONTEXT
    );
}

// Position search bar above the Effect Controls window
void PositionAboveEffectControls() {
    if (!g_hwnd || !g_attachedWindow || !IsWindow(g_attachedWindow)) return;

    RECT ecRect;
    GetWindowRect(g_attachedWindow, &ecRect);

    // Get current window height
    RECT myRect;
    GetWindowRect(g_hwnd, &myRect);
    int myHeight = myRect.bottom - myRect.top;

    // Position above Effect Controls, same width
    int newX = ecRect.left;
    int newY = ecRect.top - myHeight;
    int newWidth = ecRect.right - ecRect.left;
    if (newWidth < WINDOW_WIDTH) newWidth = WINDOW_WIDTH;

    SetWindowPos(g_hwnd, HWND_TOPMOST, newX, newY, newWidth, myHeight, SWP_NOACTIVATE);
}

void ShowPanel(PanelMode mode) {
    if (g_isVisible) return;

    // Set mode
    g_currentMode = mode;

    // Reset state
    g_searchQuery[0] = L'\0';
    g_searchResults.clear();
    g_layerEffects.clear();
    g_selectedIndex = 0;
    g_hoverIndex = -1;
    g_result = ControlResult();

    // Get mouse position
    POINT pt;
    GetCursorPos(&pt);

    int windowHeight;

    if (mode == MODE_SEARCH) {
        // Mode 1: Search mode - show search bar and results
        PerformSearch(L"");
        int itemCount = min((int)g_searchResults.size(), MAX_VISIBLE_ITEMS);
        windowHeight = SEARCH_HEIGHT + PADDING * 2 + itemCount * ITEM_HEIGHT + PADDING;
    } else {
        // Mode 2: Effects list mode - show presets and layer effects
        // TODO: Load layer effects from ExtendScript
        int presetHeight = ITEM_HEIGHT;  // Preset row
        int itemCount = min((int)g_layerEffects.size(), MAX_VISIBLE_ITEMS);
        int buttonsHeight = ITEM_HEIGHT;  // All on/off buttons
        windowHeight = PADDING + presetHeight + PADDING + itemCount * ITEM_HEIGHT + buttonsHeight + PADDING;
        if (windowHeight < 120) windowHeight = 120;  // Minimum height
    }

    // Create window
    if (!g_hwnd) {
        g_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            CONTROL_CLASS_NAME,
            mode == MODE_SEARCH ? L"Effect Search" : L"Effect Manager",
            WS_POPUP,
            pt.x - WINDOW_WIDTH / 2,
            pt.y - SEARCH_HEIGHT / 2,
            WINDOW_WIDTH,
            windowHeight,
            NULL, NULL,
            GetModuleHandle(NULL),
            NULL
        );

        // Set layered window for transparency
        SetLayeredWindowAttributes(g_hwnd, 0, 245, LWA_ALPHA);
    } else {
        SetWindowPos(g_hwnd, HWND_TOPMOST,
                     pt.x - WINDOW_WIDTH / 2,
                     pt.y - SEARCH_HEIGHT / 2,
                     WINDOW_WIDTH, windowHeight,
                     SWP_SHOWWINDOW);
    }

    ShowWindow(g_hwnd, SW_SHOW);
    SetFocus(g_hwnd);
    g_isVisible = true;

    InvalidateRect(g_hwnd, NULL, TRUE);
}

PanelMode GetCurrentMode() {
    return g_currentMode;
}

void HidePanel() {
    if (!g_isVisible) return;

    // Unhook event listener
    if (g_winEventHook) {
        UnhookWinEvent(g_winEventHook);
        g_winEventHook = NULL;
    }
    g_attachedWindow = NULL;
    g_isCollapsed = false;

    ShowWindow(g_hwnd, SW_HIDE);
    g_isVisible = false;
}

// Toggle collapsed state (thin bar)
void SetCollapsed(bool collapsed) {
    if (g_isCollapsed == collapsed) return;
    g_isCollapsed = collapsed;

    if (g_hwnd) {
        RECT rc;
        GetWindowRect(g_hwnd, &rc);

        int newHeight;
        if (collapsed) {
            newHeight = COLLAPSED_HEIGHT;
        } else {
            int itemCount = min((int)g_searchResults.size(), MAX_VISIBLE_ITEMS);
            newHeight = SEARCH_HEIGHT + PADDING * 2 + itemCount * ITEM_HEIGHT + PADDING;
        }

        SetWindowPos(g_hwnd, NULL, 0, 0, rc.right - rc.left, newHeight, SWP_NOMOVE | SWP_NOZORDER);
        InvalidateRect(g_hwnd, NULL, TRUE);
    }
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

    // Resize window
    if (g_hwnd) {
        int itemCount = min((int)g_searchResults.size(), MAX_VISIBLE_ITEMS);
        int windowHeight = SEARCH_HEIGHT + PADDING * 2 + itemCount * ITEM_HEIGHT + PADDING;

        RECT rc;
        GetWindowRect(g_hwnd, &rc);
        SetWindowPos(g_hwnd, NULL, 0, 0, WINDOW_WIDTH, windowHeight,
                     SWP_NOMOVE | SWP_NOZORDER);
    }

    InvalidateRect(g_hwnd, NULL, TRUE);
}

} // namespace ControlUI

// WinEvent callback for tracking Effect Controls window position and close
void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
                           LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
    // Only process events for the attached window
    if (hwnd != g_attachedWindow) return;

    switch (event) {
        case EVENT_OBJECT_LOCATIONCHANGE:
            // Effect Controls window moved - reposition our search bar
            if (g_isVisible && g_currentMode == ControlUI::MODE_SEARCH) {
                ControlUI::PositionAboveEffectControls();
            }
            break;

        case EVENT_OBJECT_DESTROY:
            // Effect Controls window closed - close our search bar
            if (g_isVisible) {
                g_result.cancelled = true;
                ControlUI::HidePanel();
            }
            break;
    }
}

// Search implementation
void PerformSearch(const wchar_t* query) {
    g_searchResults.clear();

    std::wstring q(query);
    // Convert to lowercase for case-insensitive search
    std::transform(q.begin(), q.end(), q.begin(), ::towlower);

    int idx = 0;
    for (int i = 0; BUILTIN_EFFECTS[i][0] != NULL; i++) {
        std::wstring name(BUILTIN_EFFECTS[i][0]);
        std::wstring nameLower = name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);

        // Match if query is empty or found in name
        if (q.empty() || nameLower.find(q) != std::wstring::npos) {
            ControlUI::EffectItem item;
            wcscpy_s(item.name, BUILTIN_EFFECTS[i][0]);
            wcscpy_s(item.matchName, BUILTIN_EFFECTS[i][1]);
            wcscpy_s(item.category, BUILTIN_EFFECTS[i][2]);
            item.index = idx++;
            g_searchResults.push_back(item);

            if (g_searchResults.size() >= 20) break; // Limit results
        }
    }

    g_selectedIndex = 0;
}

// Draw search mode UI
void DrawSearchMode(Graphics& graphics, int width, int height) {
    // If collapsed, just draw a thin accent bar
    if (g_isCollapsed) {
        SolidBrush accentBrush(COLOR_ACCENT);
        graphics.FillRectangle(&accentBrush, 0, 0, width, COLLAPSED_HEIGHT);
        return;
    }

    FontFamily fontFamily(L"Segoe UI");
    Font searchFont(&fontFamily, 14, FontStyleRegular, UnitPixel);
    Font itemFont(&fontFamily, 12, FontStyleRegular, UnitPixel);
    Font categoryFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    Font closeFont(&fontFamily, 14, FontStyleBold, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentNear);
    sf.SetLineAlignment(StringAlignmentCenter);

    // Close button [x] on the right
    int closeX = width - PADDING - CLOSE_BUTTON_SIZE;
    int closeY = PADDING + (SEARCH_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
    RectF closeRect(closeX, closeY, CLOSE_BUTTON_SIZE, CLOSE_BUTTON_SIZE);

    // Close button background (highlight on hover)
    if (g_hoverCloseButton) {
        SolidBrush hoverBrush(Color(255, 180, 60, 60));
        graphics.FillRectangle(&hoverBrush, closeRect);
    }

    // Draw X
    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);
    graphics.DrawString(L"×", -1, &closeFont, closeRect, &sfCenter, &textBrush);

    // Search box background (leave space for close button)
    SolidBrush searchBgBrush(COLOR_SEARCH_BG);
    RectF searchRect(PADDING, PADDING, width - PADDING * 3 - CLOSE_BUTTON_SIZE, SEARCH_HEIGHT);
    graphics.FillRectangle(&searchBgBrush, searchRect);

    // Search text
    RectF textRect(PADDING + 8, PADDING + 8, width - PADDING * 3 - CLOSE_BUTTON_SIZE - 16, SEARCH_HEIGHT - 16);

    if (wcslen(g_searchQuery) > 0) {
        graphics.DrawString(g_searchQuery, -1, &searchFont, textRect, &sf, &textBrush);
    } else {
        graphics.DrawString(L"Search effects...", -1, &searchFont, textRect, &sf, &dimBrush);
    }

    // Cursor
    RectF bounds;
    graphics.MeasureString(g_searchQuery, -1, &searchFont, textRect, &sf, &bounds);
    Pen cursorPen(COLOR_ACCENT, 2);
    REAL cursorX = (REAL)(PADDING + 8) + bounds.Width + 2.0f;
    REAL cursorY1 = (REAL)(PADDING + 10);
    REAL cursorY2 = (REAL)(PADDING + SEARCH_HEIGHT - 10);
    graphics.DrawLine(&cursorPen, cursorX, cursorY1, cursorX, cursorY2);

    // Results
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

// Draw effects list mode UI (presets + layer effects)
void DrawEffectsListMode(Graphics& graphics, int width, int height) {
    FontFamily fontFamily(L"Segoe UI");
    Font titleFont(&fontFamily, 12, FontStyleBold, UnitPixel);
    Font itemFont(&fontFamily, 12, FontStyleRegular, UnitPixel);
    Font smallFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);
    SolidBrush accentBrush(COLOR_ACCENT);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentNear);
    sf.SetLineAlignment(StringAlignmentCenter);

    int y = PADDING;

    // Preset slots row
    int slotWidth = 30;
    int slotSpacing = 8;
    int presetStartX = PADDING;

    // Draw 3 preset slots (★)
    for (int i = 0; i < 3; i++) {
        RectF slotRect(presetStartX + i * (slotWidth + slotSpacing), y, slotWidth, ITEM_HEIGHT);
        SolidBrush slotBg(Color(255, 50, 50, 60));
        graphics.FillRectangle(&slotBg, slotRect);

        // Star icon
        StringFormat sfCenter;
        sfCenter.SetAlignment(StringAlignmentCenter);
        sfCenter.SetLineAlignment(StringAlignmentCenter);
        graphics.DrawString(L"★", -1, &titleFont, slotRect, &sfCenter, &dimBrush);
    }

    // [+Save] button
    int saveX = presetStartX + 3 * (slotWidth + slotSpacing) + 20;
    RectF saveRect(saveX, y, 60, ITEM_HEIGHT);
    SolidBrush saveBg(Color(255, 60, 80, 60));
    graphics.FillRectangle(&saveBg, saveRect);
    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);
    graphics.DrawString(L"+Save", -1, &smallFont, saveRect, &sfCenter, &textBrush);

    y += ITEM_HEIGHT + PADDING;

    // Separator line
    Pen sepPen(COLOR_BORDER, 1);
    graphics.DrawLine(&sepPen, PADDING, y, width - PADDING, y);
    y += PADDING;

    // Layer effects list
    int visibleCount = min((int)g_layerEffects.size(), MAX_VISIBLE_ITEMS);

    if (visibleCount == 0) {
        RectF msgRect(PADDING, y, width - PADDING * 2, ITEM_HEIGHT);
        graphics.DrawString(L"No effects on layer", -1, &itemFont, msgRect, &sf, &dimBrush);
        y += ITEM_HEIGHT;
    } else {
        for (int i = 0; i < visibleCount; i++) {
            const auto& item = g_layerEffects[i];
            RectF itemRect(PADDING, y, width - PADDING * 2, ITEM_HEIGHT);

            // Highlight hover
            if (i == g_hoverIndex) {
                SolidBrush hoverBrush(COLOR_ITEM_HOVER);
                graphics.FillRectangle(&hoverBrush, itemRect);
            }

            // Expand/collapse icon
            graphics.DrawString(L"▼", -1, &smallFont,
                RectF(PADDING + 4, y, 16, ITEM_HEIGHT), &sf, &dimBrush);

            // Effect name
            SolidBrush* nameBrush = item.enabled ? &textBrush : &dimBrush;
            RectF nameRect(PADDING + 24, y, width - PADDING * 2 - 60, ITEM_HEIGHT);
            graphics.DrawString(item.name, -1, &itemFont, nameRect, &sf, nameBrush);

            // Enable/disable toggle [👁]
            RectF toggleRect(width - PADDING - 32, y + 4, 24, ITEM_HEIGHT - 8);
            graphics.DrawString(item.enabled ? L"👁" : L"○", -1, &itemFont, toggleRect, &sfCenter,
                item.enabled ? &accentBrush : &dimBrush);

            y += ITEM_HEIGHT;
        }
    }

    y += PADDING;

    // Bottom buttons: [All On] [All Off]
    int btnWidth = (width - PADDING * 3) / 2;

    RectF allOnRect(PADDING, y, btnWidth, ITEM_HEIGHT - 4);
    SolidBrush btnBg(Color(255, 50, 70, 50));
    graphics.FillRectangle(&btnBg, allOnRect);
    graphics.DrawString(L"All On", -1, &smallFont, allOnRect, &sfCenter, &textBrush);

    RectF allOffRect(PADDING * 2 + btnWidth, y, btnWidth, ITEM_HEIGHT - 4);
    SolidBrush btnBgOff(Color(255, 70, 50, 50));
    graphics.FillRectangle(&btnBgOff, allOffRect);
    graphics.DrawString(L"All Off", -1, &smallFont, allOffRect, &sfCenter, &textBrush);
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

    // Draw based on mode
    if (g_currentMode == ControlUI::MODE_SEARCH) {
        DrawSearchMode(graphics, width, height);
    } else {
        DrawEffectsListMode(graphics, width, height);
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

            DrawControlPanel(memDC, rc.right, rc.bottom);

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CHAR: {
            wchar_t ch = (wchar_t)wParam;
            size_t len = wcslen(g_searchQuery);

            if (ch == VK_BACK) {
                // Backspace
                if (len > 0) {
                    g_searchQuery[len - 1] = L'\0';
                    ControlUI::UpdateSearch(g_searchQuery);
                }
            } else if (ch == VK_RETURN) {
                // Enter - select current
                if (!g_searchResults.empty() && g_selectedIndex < (int)g_searchResults.size()) {
                    g_result.effectSelected = true;
                    g_result.selectedEffect = g_searchResults[g_selectedIndex];
                    wcscpy_s(g_result.searchQuery, g_searchQuery);
                    ControlUI::HidePanel();
                }
            } else if (ch == VK_ESCAPE) {
                // Escape - cancel
                g_result.cancelled = true;
                ControlUI::HidePanel();
            } else if (ch >= 32 && ch < 127) {
                // Normal character
                if (len < 254) {
                    g_searchQuery[len] = ch;
                    g_searchQuery[len + 1] = L'\0';
                    ControlUI::UpdateSearch(g_searchQuery);
                }
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_UP) {
                if (g_selectedIndex > 0) {
                    g_selectedIndex--;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (wParam == VK_DOWN) {
                if (g_selectedIndex < (int)g_searchResults.size() - 1) {
                    g_selectedIndex++;
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
            int width = rc.right;

            // Track mouse leave for collapse feature
            TRACKMOUSEEVENT tme = {0};
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);

            // If collapsed, expand on mouse enter
            if (g_isCollapsed) {
                ControlUI::SetCollapsed(false);
                return 0;
            }

            // Check close button hover (in search mode)
            if (g_currentMode == ControlUI::MODE_SEARCH) {
                int closeX = width - PADDING - CLOSE_BUTTON_SIZE;
                int closeY = PADDING + (SEARCH_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
                bool wasHover = g_hoverCloseButton;
                g_hoverCloseButton = (x >= closeX && x <= closeX + CLOSE_BUTTON_SIZE &&
                                      y >= closeY && y <= closeY + CLOSE_BUTTON_SIZE);
                if (wasHover != g_hoverCloseButton) {
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }

            // Check result items hover
            int startY = PADDING + SEARCH_HEIGHT + PADDING;
            if (y >= startY) {
                int idx = (y - startY) / ITEM_HEIGHT;
                if (idx >= 0 && idx < (int)g_searchResults.size()) {
                    if (g_hoverIndex != idx) {
                        g_hoverIndex = idx;
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                }
            } else {
                if (g_hoverIndex != -1) {
                    g_hoverIndex = -1;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            // Collapse when mouse leaves (only in search mode with attached window)
            if (g_currentMode == ControlUI::MODE_SEARCH && g_attachedWindow) {
                ControlUI::SetCollapsed(true);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            RECT rc;
            GetClientRect(hwnd, &rc);
            int width = rc.right;

            // Check close button click (in search mode)
            if (g_currentMode == ControlUI::MODE_SEARCH) {
                int closeX = width - PADDING - CLOSE_BUTTON_SIZE;
                int closeY = PADDING + (SEARCH_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
                if (x >= closeX && x <= closeX + CLOSE_BUTTON_SIZE &&
                    y >= closeY && y <= closeY + CLOSE_BUTTON_SIZE) {
                    g_result.cancelled = true;
                    ControlUI::HidePanel();
                    return 0;
                }
            }

            // Check result item click
            int startY = PADDING + SEARCH_HEIGHT + PADDING;
            if (y >= startY) {
                int idx = (y - startY) / ITEM_HEIGHT;
                if (idx >= 0 && idx < (int)g_searchResults.size()) {
                    g_result.effectSelected = true;
                    g_result.selectedEffect = g_searchResults[idx];
                    wcscpy_s(g_result.searchQuery, g_searchQuery);
                    ControlUI::HidePanel();
                }
            }
            return 0;
        }

        case WM_KILLFOCUS: {
            // Close when losing focus
            g_result.cancelled = true;
            ControlUI::HidePanel();
            return 0;
        }

        case WM_DESTROY:
            g_hwnd = NULL;
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

#else // macOS stub

namespace ControlUI {

void Initialize() {}
void Shutdown() {}
void ShowPanel(PanelMode) {}
void HidePanel() {}
bool IsVisible() { return false; }
ControlResult GetResult() { return ControlResult(); }
ControlSettings& GetSettings() { static ControlSettings s; return s; }
void UpdateSearch(const wchar_t*) {}
PanelMode GetCurrentMode() { return MODE_SEARCH; }

} // namespace ControlUI

#endif // MSWindows
