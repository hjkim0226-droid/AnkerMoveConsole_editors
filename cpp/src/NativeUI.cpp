/*****************************************************************************
 * NativeUI.cpp
 *
 * Native Windows UI for Anchor Grid
 * Custom-drawn popup window with circular buttons and glow effect
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

// Color palette (matching reference design)
#define COLOR_BG RGB(26, 26, 26)           // Dark background
#define COLOR_GRID_LINE RGB(42, 74, 90)    // Dark teal grid lines
#define COLOR_CIRCLE RGB(42, 74, 90)       // Normal circle
#define COLOR_GLOW_INNER RGB(74, 207, 255) // Bright cyan glow
#define COLOR_GLOW_MID RGB(42, 122, 154)   // Medium glow
#define COLOR_GLOW_OUTER RGB(42, 90, 110)  // Outer glow
#define COLOR_CORNER RGB(42, 74, 90)       // Corner L-marks

// Global state
static HWND g_gridWnd = NULL;
static HINSTANCE g_hInstance = NULL;
static bool g_initialized = false;
static NativeUI::GridConfig g_config;
static int g_windowX = 0;
static int g_windowY = 0;
static int g_hoverCellX = -1;
static int g_hoverCellY = -1;

// Forward declarations
static LRESULT CALLBACK GridWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam);
static void DrawGrid(HDC hdc);
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

  // Calculate window size (larger for new design)
  int cellTotal = config.cellSize + config.spacing;
  int gridPixels = config.gridSize * cellTotal;
  int windowSize = gridPixels + config.margin * 2;

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

    // Make window opaque
    SetLayeredWindowAttributes(g_gridWnd, 0, 255, LWA_ALPHA);
  }

  // Initial hover update
  UpdateHoverFromMouse(mouseX, mouseY);
  InvalidateRect(g_gridWnd, NULL, TRUE);
}

GridResult HideGrid(int mouseX, int mouseY) {
  GridResult result;

  if (g_gridWnd) {
    UpdateHoverFromMouse(mouseX, mouseY);
    result.gridX = g_hoverCellX;
    result.gridY = g_hoverCellY;
    result.cancelled = (g_hoverCellX < 0 || g_hoverCellY < 0);

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
    UpdateHoverFromMouse(mouseX, mouseY);

    if (oldX != g_hoverCellX || oldY != g_hoverCellY) {
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

} // namespace NativeUI

// Calculate hover cell from screen coordinates
static void UpdateHoverFromMouse(int screenX, int screenY) {
  int relX = screenX - g_windowX - g_config.margin;
  int relY = screenY - g_windowY - g_config.margin;

  int cellTotal = g_config.cellSize + g_config.spacing;
  int radius = g_config.cellSize / 4; // Circle hit radius

  // Check each intersection point
  g_hoverCellX = -1;
  g_hoverCellY = -1;

  for (int y = 0; y < g_config.gridSize; y++) {
    for (int x = 0; x < g_config.gridSize; x++) {
      int cx = x * cellTotal + cellTotal / 2;
      int cy = y * cellTotal + cellTotal / 2;

      int dx = relX - cx;
      int dy = relY - cy;
      int dist = dx * dx + dy * dy;

      if (dist < radius * radius * 4) { // Within circle
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

// Draw the grid with circles and glow
static void DrawGrid(HDC hdc) {
  int cellTotal = g_config.cellSize + g_config.spacing;
  int margin = g_config.margin;
  int radius = g_config.cellSize / 5;

  // Draw grid lines (horizontal and vertical cross marks)
  HPEN linePen = CreatePen(PS_SOLID, 2, COLOR_GRID_LINE);
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

      // Draw corner L-marks for corners
      if (isCorner) {
        int corner = (y == 0 ? 0 : 2) + (x == g_config.gridSize - 1 ? 1 : 0);
        DrawCornerMark(hdc, cx, cy, cellTotal, corner);
      } else {
        // Draw cross marks for middle buttons
        // Horizontal line
        MoveToEx(hdc, cx - len, cy, NULL);
        LineTo(hdc, cx + len, cy);
        // Vertical line
        MoveToEx(hdc, cx, cy - len, NULL);
        LineTo(hdc, cx, cy + len);
      }

      // Draw glow effect for hovered cell
      if (isHover) {
        // Outer glow (larger, semi-transparent feel via color)
        HPEN glowPen3 = CreatePen(PS_SOLID, 1, COLOR_GLOW_OUTER);
        HBRUSH glowBrush3 = CreateSolidBrush(COLOR_GLOW_OUTER);
        SelectObject(hdc, glowPen3);
        SelectObject(hdc, glowBrush3);
        Ellipse(hdc, cx - radius * 2, cy - radius * 2, cx + radius * 2,
                cy + radius * 2);
        DeleteObject(glowPen3);
        DeleteObject(glowBrush3);

        // Middle glow
        HPEN glowPen2 = CreatePen(PS_SOLID, 1, COLOR_GLOW_MID);
        HBRUSH glowBrush2 = CreateSolidBrush(COLOR_GLOW_MID);
        SelectObject(hdc, glowPen2);
        SelectObject(hdc, glowBrush2);
        Ellipse(hdc, cx - radius - 4, cy - radius - 4, cx + radius + 4,
                cy + radius + 4);
        DeleteObject(glowPen2);
        DeleteObject(glowBrush2);

        // Inner bright circle
        HPEN glowPen1 = CreatePen(PS_SOLID, 2, COLOR_GLOW_INNER);
        HBRUSH glowBrush1 = CreateSolidBrush(COLOR_GLOW_INNER);
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
GridResult HideGrid(int, int) { return GridResult{-1, -1, true}; }
bool IsGridVisible() { return false; }
void UpdateHover(int, int) {}
void GetHoverCell(int *, int *) {}
} // namespace NativeUI

#endif // MSWindows
