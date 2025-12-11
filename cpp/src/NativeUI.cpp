/*****************************************************************************
 * NativeUI.cpp
 *
 * Native Windows UI for Anchor Grid
 * Custom-drawn popup window with blue highlight for grid cells
 *****************************************************************************/

#include "NativeUI.h"

#ifdef MSWindows

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <string>
#include <windows.h>

// Window class name
static const wchar_t *GRID_CLASS_NAME = L"AnchorGridClass";

// Colors (using explicit values to avoid macro issues)
static const COLORREF COLOR_BACKGROUND =
    0x001E1E1E; // Dark gray (BGR: 30,30,30)
static const COLORREF COLOR_CELL_NORMAL =
    0x003C3C3C; // Medium gray (BGR: 60,60,60)
static const COLORREF COLOR_CELL_HOVER =
    0x00CC6633; // Blue highlight (BGR: 204,102,51)
static const COLORREF COLOR_BORDER = 0x00505050; // Border (BGR: 80,80,80)

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
  wc.hbrBackground = CreateSolidBrush(COLOR_BACKGROUND);
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

  // Calculate window size
  int cellTotal = config.cellSize + config.spacing;
  int gridPixels = config.gridSize * cellTotal - config.spacing;
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

    // Make window opaque but with no shadow
    SetLayeredWindowAttributes(g_gridWnd, 0, 255, LWA_ALPHA);
  }

  // Initial hover update
  UpdateHoverFromMouse(mouseX, mouseY);
  InvalidateRect(g_gridWnd, NULL, TRUE);
}

GridResult HideGrid(int mouseX, int mouseY) {
  GridResult result;

  if (g_gridWnd) {
    // Calculate which cell was under the mouse
    UpdateHoverFromMouse(mouseX, mouseY);
    result.gridX = g_hoverCellX;
    result.gridY = g_hoverCellY;
    result.cancelled = (g_hoverCellX < 0 || g_hoverCellY < 0);

    // Hide window (don't destroy, reuse later)
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

    // Redraw if hover changed
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

  int cellX = relX / cellTotal;
  int cellY = relY / cellTotal;

  // Check if within grid bounds
  if (cellX >= 0 && cellX < g_config.gridSize && cellY >= 0 &&
      cellY < g_config.gridSize && relX >= 0 && relY >= 0) {
    g_hoverCellX = cellX;
    g_hoverCellY = cellY;
  } else {
    g_hoverCellX = -1;
    g_hoverCellY = -1;
  }
}

// Draw the grid
static void DrawGrid(HDC hdc) {
  // Create brushes
  HBRUSH brushNormal = CreateSolidBrush(COLOR_CELL_NORMAL);
  HBRUSH brushHover = CreateSolidBrush(COLOR_CELL_HOVER);
  HPEN penBorder = CreatePen(PS_SOLID, 1, COLOR_BORDER);

  SelectObject(hdc, penBorder);

  int cellTotal = g_config.cellSize + g_config.spacing;

  for (int y = 0; y < g_config.gridSize; y++) {
    for (int x = 0; x < g_config.gridSize; x++) {
      int left = g_config.margin + x * cellTotal;
      int top = g_config.margin + y * cellTotal;
      int right = left + g_config.cellSize;
      int bottom = top + g_config.cellSize;

      // Select brush based on hover state
      HBRUSH brush =
          (x == g_hoverCellX && y == g_hoverCellY) ? brushHover : brushNormal;
      SelectObject(hdc, brush);

      // Draw cell rectangle
      Rectangle(hdc, left, top, right, bottom);
    }
  }

  // Cleanup
  DeleteObject(brushNormal);
  DeleteObject(brushHover);
  DeleteObject(penBorder);
}

// Window procedure
static LRESULT CALLBACK GridWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam) {
  switch (msg) {
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // Double buffer to avoid flicker
    RECT rect;
    GetClientRect(hwnd, &rect);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
    SelectObject(memDC, memBitmap);

    // Fill background
    HBRUSH bgBrush = CreateSolidBrush(COLOR_BACKGROUND);
    FillRect(memDC, &rect, bgBrush);
    DeleteObject(bgBrush);

    // Draw grid
    DrawGrid(memDC);

    // Copy to screen
    BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

    // Cleanup
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
