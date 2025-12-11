/*****************************************************************************
 * NativeUI.cpp
 *
 * Native Windows UI for Anchor Grid
 * Custom-drawn popup window with circular buttons, glow effect, and extended
 *menu
 *****************************************************************************/

#include "NativeUI.h"

#ifdef MSWindows

// Include Windows SDK first
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

// Window class name
static const wchar_t *GRID_CLASS_NAME = L"AnchorGridClass";

// Color palette - Selection Mode (Cyan/Teal)
#define COLOR_BG RGB(26, 26, 26)           // Dark background
#define COLOR_GRID_LINE RGB(42, 74, 90)    // Dark teal grid lines
#define COLOR_CIRCLE RGB(42, 74, 90)       // Normal circle
#define COLOR_GLOW_INNER RGB(74, 207, 255) // Bright cyan glow
#define COLOR_GLOW_MID RGB(42, 122, 154)   // Medium glow
#define COLOR_GLOW_OUTER RGB(42, 90, 110)  // Outer glow
#define COLOR_CORNER RGB(42, 74, 90)       // Corner L-marks
#define COLOR_EXT_HOVER RGB(74, 207, 255)  // Extended option hover
#define COLOR_EXT_TEXT RGB(120, 160, 180)  // Extended option text

// Color palette - Comp Mode (Orange/Warm)
#define COLOR_GRID_LINE_COMP RGB(90, 70, 42)    // Warm orange grid lines
#define COLOR_GLOW_INNER_COMP RGB(255, 180, 74) // Bright orange glow
#define COLOR_GLOW_MID_COMP RGB(154, 100, 42)   // Medium orange glow
#define COLOR_GLOW_OUTER_COMP RGB(110, 80, 42)  // Outer orange glow
#define COLOR_CORNER_COMP RGB(90, 70, 42)       // Orange corner L-marks

// Global state
static HWND g_gridWnd = NULL;
static HINSTANCE g_hInstance = NULL;
static bool g_initialized = false;
static NativeUI::GridConfig g_config;
static NativeUI::GridSettings g_settings;
static int g_windowX = 0;
static int g_windowY = 0;
static int g_hoverCellX = -1;
static int g_hoverCellY = -1;
static NativeUI::ExtendedOption g_hoverExtOption = NativeUI::OPT_NONE;
static int g_extThreshold =
    30; // Distance from grid edge to trigger extended menu

// Forward declarations
static LRESULT CALLBACK GridWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam);
static void DrawGrid(HDC hdc);
static void DrawExtendedMenu(HDC hdc, int windowSize);
static void UpdateHoverFromMouse(int screenX, int screenY);

namespace NativeUI {

bool Initialize() {
  if (g_initialized)
    return true;

  g_hInstance = GetModuleHandle(NULL);

  // Register window class
  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = GridWndProc;
  wc.hInstance = g_hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = CreateSolidBrush(COLOR_BG);
  wc.lpszClassName = GRID_CLASS_NAME;

  if (!RegisterClassExW(&wc)) {
    return false;
  }

  g_initialized = true;
  return true;
}

void Cleanup() {
  if (g_gridWnd) {
    DestroyWindow(g_gridWnd);
    g_gridWnd = NULL;
  }
  if (g_initialized) {
    UnregisterClassW(GRID_CLASS_NAME, g_hInstance);
    g_initialized = false;
  }
}

void ShowGrid(int mouseX, int mouseY, const GridConfig &config) {
  if (!g_initialized && !Initialize())
    return;

  g_config = config;
  g_hoverCellX = -1;
  g_hoverCellY = -1;
  g_hoverExtOption = OPT_NONE;

  // Calculate window size (with extended menu area)
  int cellTotal = config.cellSize + config.spacing;
  int gridPixels = config.gridSize * cellTotal;
  int extendedSize = g_extThreshold * 2; // Extra space for extended menu
  int windowSize = gridPixels + config.margin * 2 + extendedSize;

  // Center window on mouse
  g_windowX = mouseX - windowSize / 2;
  g_windowY = mouseY - windowSize / 2;

  // Create or reposition window
  if (g_gridWnd) {
    SetWindowPos(g_gridWnd, HWND_TOPMOST, g_windowX, g_windowY, windowSize,
                 windowSize, SWP_SHOWWINDOW);
  } else {
    g_gridWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED, GRID_CLASS_NAME, NULL,
        WS_POPUP | WS_VISIBLE, g_windowX, g_windowY, windowSize, windowSize,
        NULL, NULL, g_hInstance, NULL);

    SetLayeredWindowAttributes(g_gridWnd, 0, 255, LWA_ALPHA);
  }

  UpdateHoverFromMouse(mouseX, mouseY);
  InvalidateRect(g_gridWnd, NULL, TRUE);
}

GridResult HideGrid(int mouseX, int mouseY) {
  GridResult result;

  if (g_gridWnd) {
    UpdateHoverFromMouse(mouseX, mouseY);
    result.gridX = g_hoverCellX;
    result.gridY = g_hoverCellY;
    result.extendedOption = g_hoverExtOption;
    result.cancelled =
        (g_hoverCellX < 0 && g_hoverCellY < 0 && g_hoverExtOption == OPT_NONE);

    ShowWindow(g_gridWnd, SW_HIDE);
  } else {
    result.cancelled = true;
  }

  return result;
}

bool IsGridVisible() { return g_gridWnd && IsWindowVisible(g_gridWnd); }

void UpdateHover(int mouseX, int mouseY) {
  if (IsGridVisible()) {
    int oldX = g_hoverCellX;
    int oldY = g_hoverCellY;
    ExtendedOption oldExt = g_hoverExtOption;
    UpdateHoverFromMouse(mouseX, mouseY);

    if (oldX != g_hoverCellX || oldY != g_hoverCellY ||
        oldExt != g_hoverExtOption) {
      InvalidateRect(g_gridWnd, NULL, TRUE);
    }
  }
}

void GetHoverCell(int *outX, int *outY) {
  if (outX)
    *outX = g_hoverCellX;
  if (outY)
    *outY = g_hoverCellY;
}

GridSettings &GetSettings() { return g_settings; }

} // namespace NativeUI

// Calculate hover cell or extended option from screen coordinates
static void UpdateHoverFromMouse(int screenX, int screenY) {
  int cellTotal = g_config.cellSize + g_config.spacing;
  int gridPixels = g_config.gridSize * cellTotal;
  int extOffset = g_extThreshold; // Extended menu offset
  int gridStart = g_config.margin + extOffset;

  int relX = screenX - g_windowX - gridStart;
  int relY = screenY - g_windowY - gridStart;
  int radius = g_config.cellSize / 4;

  // Reset
  g_hoverCellX = -1;
  g_hoverCellY = -1;
  g_hoverExtOption = NativeUI::OPT_NONE;

  // Check extended menu regions first
  if (relY < -g_extThreshold / 2 && relX >= 0 && relX < gridPixels) {
    g_hoverExtOption = NativeUI::OPT_SELECTION_MODE; // Top
    return;
  }
  if (relY > gridPixels + g_extThreshold / 2 - cellTotal && relX >= 0 &&
      relX < gridPixels) {
    g_hoverExtOption = NativeUI::OPT_CUSTOM_ANCHOR; // Bottom
    return;
  }
  if (relX < -g_extThreshold / 2 && relY >= 0 && relY < gridPixels) {
    g_hoverExtOption = NativeUI::OPT_SETTINGS; // Left
    return;
  }
  if (relX > gridPixels + g_extThreshold / 2 - cellTotal && relY >= 0 &&
      relY < gridPixels) {
    g_hoverExtOption = NativeUI::OPT_TRANSPARENT; // Right
    return;
  }

  // Check grid cells
  for (int y = 0; y < g_config.gridSize; y++) {
    for (int x = 0; x < g_config.gridSize; x++) {
      int cx = x * cellTotal + cellTotal / 2;
      int cy = y * cellTotal + cellTotal / 2;

      int dx = relX - cx;
      int dy = relY - cy;
      int dist = dx * dx + dy * dy;

      if (dist < radius * radius * 4) {
        g_hoverCellX = x;
        g_hoverCellY = y;
        return;
      }
    }
  }
}

// Draw corner L-mark
static void DrawCornerMark(HDC hdc, int cx, int cy, int size, int corner) {
  HPEN pen = CreatePen(PS_SOLID, 2, COLOR_CORNER);
  SelectObject(hdc, pen);

  int len = size / 3;

  switch (corner) {
  case 0: // Top-left
    MoveToEx(hdc, cx, cy + len, NULL);
    LineTo(hdc, cx, cy);
    LineTo(hdc, cx + len, cy);
    break;
  case 1: // Top-right
    MoveToEx(hdc, cx - len, cy, NULL);
    LineTo(hdc, cx, cy);
    LineTo(hdc, cx, cy + len);
    break;
  case 2: // Bottom-left
    MoveToEx(hdc, cx, cy - len, NULL);
    LineTo(hdc, cx, cy);
    LineTo(hdc, cx + len, cy);
    break;
  case 3: // Bottom-right
    MoveToEx(hdc, cx - len, cy, NULL);
    LineTo(hdc, cx, cy);
    LineTo(hdc, cx, cy - len);
    break;
  }

  DeleteObject(pen);
}

// Draw extended menu options
static void DrawExtendedMenu(HDC hdc, int windowSize) {
  int cellTotal = g_config.cellSize + g_config.spacing;
  int gridPixels = g_config.gridSize * cellTotal;
  int extOffset = g_extThreshold;
  int gridStart = g_config.margin + extOffset;
  int center = windowSize / 2;

  SetBkMode(hdc, TRANSPARENT);

  // Top: Selection Mode
  {
    bool hover = (g_hoverExtOption == NativeUI::OPT_SELECTION_MODE);
    COLORREF color = hover ? COLOR_EXT_HOVER : COLOR_EXT_TEXT;
    SetTextColor(hdc, color);
    const wchar_t *text = g_settings.useCompMode ? L"Comp" : L"Layer";
    RECT rc = {0, 5, windowSize, gridStart - 5};
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }

  // Bottom: Custom Anchor
  {
    bool hover = (g_hoverExtOption == NativeUI::OPT_CUSTOM_ANCHOR);
    COLORREF color = hover ? COLOR_EXT_HOVER : COLOR_EXT_TEXT;
    SetTextColor(hdc, color);
    RECT rc = {0, gridStart + gridPixels + 5, windowSize, windowSize - 5};
    DrawTextW(hdc, L"Custom", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }

  // Left: Settings
  {
    bool hover = (g_hoverExtOption == NativeUI::OPT_SETTINGS);
    COLORREF color = hover ? COLOR_EXT_HOVER : COLOR_EXT_TEXT;
    SetTextColor(hdc, color);
    RECT rc = {5, gridStart, gridStart - 5, gridStart + gridPixels};
    DrawTextW(hdc, L"SET", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }

  // Right: Transparent
  {
    bool hover = (g_hoverExtOption == NativeUI::OPT_TRANSPARENT);
    COLORREF color = hover ? COLOR_EXT_HOVER : COLOR_EXT_TEXT;
    SetTextColor(hdc, color);
    const wchar_t *text = g_settings.transparentMode ? L"[T]" : L"T";
    RECT rc = {gridStart + gridPixels + 5, gridStart, windowSize - 5,
               gridStart + gridPixels};
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }
}

// Draw the grid with circles and glow (mode-specific colors)
static void DrawGrid(HDC hdc) {
  int cellTotal = g_config.cellSize + g_config.spacing;
  int extOffset = g_extThreshold;
  int margin = g_config.margin + extOffset;
  int radius = g_config.cellSize / 5;

  // Select colors based on mode
  bool compMode = g_settings.useCompMode;
  COLORREF lineColor = compMode ? COLOR_GRID_LINE_COMP : COLOR_GRID_LINE;
  COLORREF cornerColor = compMode ? COLOR_CORNER_COMP : COLOR_CORNER;
  COLORREF glowInner = compMode ? COLOR_GLOW_INNER_COMP : COLOR_GLOW_INNER;
  COLORREF glowMid = compMode ? COLOR_GLOW_MID_COMP : COLOR_GLOW_MID;
  COLORREF glowOuter = compMode ? COLOR_GLOW_OUTER_COMP : COLOR_GLOW_OUTER;

  // Draw grid lines
  HPEN linePen = CreatePen(PS_SOLID, 2, lineColor);
  SelectObject(hdc, linePen);
  SelectObject(hdc, GetStockObject(NULL_BRUSH));

  for (int y = 0; y < g_config.gridSize; y++) {
    for (int x = 0; x < g_config.gridSize; x++) {
      int cx = margin + x * cellTotal + cellTotal / 2;
      int cy = margin + y * cellTotal + cellTotal / 2;
      int len = cellTotal / 3;

      bool isHover = (x == g_hoverCellX && y == g_hoverCellY);
      bool isCorner = (x == 0 || x == g_config.gridSize - 1) &&
                      (y == 0 || y == g_config.gridSize - 1);

      if (isCorner) {
        // Draw corner with mode-specific color
        HPEN cornerPen = CreatePen(PS_SOLID, 2, cornerColor);
        SelectObject(hdc, cornerPen);
        int corner = (y == 0 ? 0 : 2) + (x == g_config.gridSize - 1 ? 1 : 0);
        int cLen = cellTotal / 3;
        switch (corner) {
        case 0:
          MoveToEx(hdc, cx, cy + cLen, NULL);
          LineTo(hdc, cx, cy);
          LineTo(hdc, cx + cLen, cy);
          break;
        case 1:
          MoveToEx(hdc, cx - cLen, cy, NULL);
          LineTo(hdc, cx, cy);
          LineTo(hdc, cx, cy + cLen);
          break;
        case 2:
          MoveToEx(hdc, cx, cy - cLen, NULL);
          LineTo(hdc, cx, cy);
          LineTo(hdc, cx + cLen, cy);
          break;
        case 3:
          MoveToEx(hdc, cx - cLen, cy, NULL);
          LineTo(hdc, cx, cy);
          LineTo(hdc, cx, cy - cLen);
          break;
        }
        DeleteObject(cornerPen);
      } else {
        SelectObject(hdc, linePen);
        MoveToEx(hdc, cx - len, cy, NULL);
        LineTo(hdc, cx + len, cy);
        MoveToEx(hdc, cx, cy - len, NULL);
        LineTo(hdc, cx, cy + len);
      }

      if (isHover) {
        // Outer glow
        HPEN glowPen3 = CreatePen(PS_SOLID, 1, glowOuter);
        HBRUSH glowBrush3 = CreateSolidBrush(glowOuter);
        SelectObject(hdc, glowPen3);
        SelectObject(hdc, glowBrush3);
        Ellipse(hdc, cx - radius * 2, cy - radius * 2, cx + radius * 2,
                cy + radius * 2);
        DeleteObject(glowPen3);
        DeleteObject(glowBrush3);

        // Middle glow
        HPEN glowPen2 = CreatePen(PS_SOLID, 1, glowMid);
        HBRUSH glowBrush2 = CreateSolidBrush(glowMid);
        SelectObject(hdc, glowPen2);
        SelectObject(hdc, glowBrush2);
        Ellipse(hdc, cx - radius - 4, cy - radius - 4, cx + radius + 4,
                cy + radius + 4);
        DeleteObject(glowPen2);
        DeleteObject(glowBrush2);

        // Inner bright circle
        HPEN glowPen1 = CreatePen(PS_SOLID, 2, glowInner);
        HBRUSH glowBrush1 = CreateSolidBrush(glowInner);
        SelectObject(hdc, glowPen1);
        SelectObject(hdc, glowBrush1);
        Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
        DeleteObject(glowPen1);
        DeleteObject(glowBrush1);
      }
    }
  }

  DeleteObject(linePen);
}

// Window procedure
static LRESULT CALLBACK GridWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam) {
  switch (msg) {
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rect;
    GetClientRect(hwnd, &rect);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
    SelectObject(memDC, memBitmap);

    HBRUSH bgBrush = CreateSolidBrush(COLOR_BG);
    FillRect(memDC, &rect, bgBrush);
    DeleteObject(bgBrush);

    DrawGrid(memDC);
    DrawExtendedMenu(memDC, rect.right);

    BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

    DeleteObject(memBitmap);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
    return 0;
  }

  case WM_MOUSEMOVE: {
    POINT pt;
    GetCursorPos(&pt);
    NativeUI::UpdateHover(pt.x, pt.y);
    return 0;
  }

  case WM_DESTROY:
    g_gridWnd = NULL;
    return 0;
  }

  return DefWindowProc(hwnd, msg, wParam, lParam);
}

#else // macOS stub

namespace NativeUI {
bool Initialize() { return false; }
void Cleanup() {}
void ShowGrid(int, int, const GridConfig &) {}
GridResult HideGrid(int, int) { return GridResult{-1, -1, true, OPT_NONE}; }
bool IsGridVisible() { return false; }
void UpdateHover(int, int) {}
void GetHoverCell(int *, int *) {}
GridSettings &GetSettings() {
  static GridSettings s;
  return s;
}
} // namespace NativeUI

#endif // MSWindows
