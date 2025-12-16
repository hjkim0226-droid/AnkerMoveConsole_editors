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

// Search state
static wchar_t g_searchQuery[256] = {0};
static std::vector<ControlUI::EffectItem> g_searchResults;
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

void ShowPanel() {
    if (g_isVisible) return;

    // Reset state
    g_searchQuery[0] = L'\0';
    g_searchResults.clear();
    g_selectedIndex = 0;
    g_hoverIndex = -1;
    g_result = ControlResult();

    // Initial search (show all)
    PerformSearch(L"");

    // Get mouse position
    POINT pt;
    GetCursorPos(&pt);

    // Calculate window height based on results
    int itemCount = min((int)g_searchResults.size(), MAX_VISIBLE_ITEMS);
    int windowHeight = SEARCH_HEIGHT + PADDING * 2 + itemCount * ITEM_HEIGHT + PADDING;

    // Create window
    if (!g_hwnd) {
        g_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            CONTROL_CLASS_NAME,
            L"Effect Search",
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

void HidePanel() {
    if (!g_isVisible) return;

    ShowWindow(g_hwnd, SW_HIDE);
    g_isVisible = false;
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
    RectF searchRect(PADDING, PADDING, width - PADDING * 2, SEARCH_HEIGHT);
    graphics.FillRectangle(&searchBgBrush, searchRect);

    // Search text
    FontFamily fontFamily(L"Segoe UI");
    Font searchFont(&fontFamily, 14, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);

    RectF textRect(PADDING + 8, PADDING + 8, width - PADDING * 2 - 16, SEARCH_HEIGHT - 16);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentNear);
    sf.SetLineAlignment(StringAlignmentCenter);

    if (wcslen(g_searchQuery) > 0) {
        graphics.DrawString(g_searchQuery, -1, &searchFont, textRect, &sf, &textBrush);
    } else {
        graphics.DrawString(L"Search effects...", -1, &searchFont, textRect, &sf, &dimBrush);
    }

    // Cursor blink (simple)
    if (wcslen(g_searchQuery) > 0 || true) {
        // Measure text width
        RectF bounds;
        graphics.MeasureString(g_searchQuery, -1, &searchFont, textRect, &sf, &bounds);

        Pen cursorPen(COLOR_ACCENT, 2);
        float cursorX = PADDING + 8 + bounds.Width + 2;
        graphics.DrawLine(&cursorPen, cursorX, PADDING + 10, cursorX, PADDING + SEARCH_HEIGHT - 10);
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
            int y = HIWORD(lParam);
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

        case WM_LBUTTONDOWN: {
            int y = HIWORD(lParam);
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
void ShowPanel() {}
void HidePanel() {}
bool IsVisible() { return false; }
ControlResult GetResult() { return ControlResult(); }
ControlSettings& GetSettings() { static ControlSettings s; return s; }
void UpdateSearch(const wchar_t*) {}

} // namespace ControlUI

#endif // MSWindows
