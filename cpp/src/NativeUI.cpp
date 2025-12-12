/*****************************************************************************
 * NativeUI.cpp
 *
 * Native Windows UI for Anchor Grid
 * Uses GDI+ for anti-aliasing, alpha blending, and smooth rendering
 *****************************************************************************/

#include "NativeUI.h"

#ifdef MSWindows

// Windows SDK 10.0.26100.0 has a bug where certain META_* macros are missing.
// Define them before including any Windows headers.
#ifndef META_SETBKCOLOR
#define META_SETBKCOLOR 0x0201
#define META_SETBKMODE 0x0102
#define META_SETMAPMODE 0x0103
#define META_SETROP2 0x0104
#define META_SETRELABS 0x0105
#define META_SETPOLYFILLMODE 0x0106
#define META_SETSTRETCHBLTMODE 0x0107
#define META_SETTEXTCHAREXTRA 0x0108
#define META_SETTEXTCOLOR 0x0209
#define META_SETTEXTJUSTIFICATION 0x020A
#define META_SETWINDOWORG 0x020B
#define META_SETWINDOWEXT 0x020C
#define META_SETVIEWPORTORG 0x020D
#define META_SETVIEWPORTEXT 0x020E
#define META_OFFSETWINDOWORG 0x020F
#define META_SCALEWINDOWEXT 0x0410
#define META_OFFSETVIEWPORTORG 0x0211
#define META_SCALEVIEWPORTEXT 0x0412
#define META_LINETO 0x0213
#define META_MOVETO 0x0214
#define META_EXCLUDECLIPRECT 0x0415
#define META_INTERSECTCLIPRECT 0x0416
#define META_ARC 0x0817
#define META_ELLIPSE 0x0418
#define META_FLOODFILL 0x0419
#define META_PIE 0x081A
#define META_RECTANGLE 0x041B
#define META_ROUNDRECT 0x061C
#define META_PATBLT 0x061D
#define META_SAVEDC 0x001E
#define META_SETPIXEL 0x041F
#define META_OFFSETCLIPRGN 0x0220
#define META_TEXTOUT 0x0521
#define META_BITBLT 0x0922
#define META_STRETCHBLT 0x0B23
#define META_POLYGON 0x0324
#define META_POLYLINE 0x0325
#define META_ESCAPE 0x0626
#define META_RESTOREDC 0x0127
#define META_FILLREGION 0x0228
#define META_FRAMEREGION 0x0429
#define META_INVERTREGION 0x012A
#define META_PAINTREGION 0x012B
#define META_SELECTCLIPREGION 0x012C
#define META_SELECTOBJECT 0x012D
#define META_SETTEXTALIGN 0x012E
#define META_CHORD 0x0830
#define META_SETMAPPERFLAGS 0x0231
#define META_EXTTEXTOUT 0x0A32
#define META_SETDIBTODEV 0x0D33
#define META_SELECTPALETTE 0x0234
#define META_REALIZEPALETTE 0x0035
#define META_ANIMATEPALETTE 0x0436
#define META_SETPALENTRIES 0x0037
#define META_POLYPOLYGON 0x0538
#define META_RESIZEPALETTE 0x0139
#define META_DIBBITBLT 0x0940
#define META_DIBSTRETCHBLT 0x0B41
#define META_DIBCREATEPATTERNBRUSH 0x0142
#define META_STRETCHDIB 0x0F43
#define META_EXTFLOODFILL 0x0548
#define META_DELETEOBJECT 0x01F0
#define META_CREATEPALETTE 0x00F7
#define META_CREATEBRUSH 0x00F8
#define META_CREATEPATTERNBRUSH 0x01F9
#define META_CREATEPENINDIRECT 0x02FA
#define META_CREATEFONTINDIRECT 0x02FB
#define META_CREATEBRUSHINDIRECT 0x02FC
#define META_CREATEBITMAPINDIRECT 0x02FD
#define META_CREATEBITMAP 0x06FE
#define META_CREATEREGION 0x06FF
#endif

// GDI+ requires Windows headers in this specific order
#include <gdiplus.h>
#include <objidl.h>
#include <windows.h>
#pragma comment(lib, "gdiplus.lib")

#include <string>

// GDI+ token for startup/shutdown
static ULONG_PTR g_gdiplusToken = 0;

// Window class name
static const wchar_t *GRID_CLASS_NAME = L"AnchorGridClass";

// Color palette - Selection Mode (Cyan/Teal)
#define COLOR_BG RGB(0, 0, 0)         // Transparent (color keyed out)
#define COLOR_CELL_BG RGB(20, 20, 20) // Cell background (~30% opacity effect)
#define COLOR_GRID_LINE                                                        \
  RGB(90, 140, 170)                  // Brighter teal marks (when not hovering)
#define COLOR_CIRCLE RGB(42, 74, 90) // Normal circle
#define COLOR_GLOW_INNER RGB(74, 207, 255) // Bright cyan glow
#define COLOR_GLOW_MID RGB(42, 122, 154)   // Medium glow
#define COLOR_GLOW_OUTER RGB(42, 90, 110)  // Outer glow
#define COLOR_CORNER RGB(42, 74, 90)       // Corner L-marks
#define COLOR_EXT_HOVER RGB(74, 207, 255)  // Extended option hover
#define COLOR_EXT_TEXT RGB(120, 160, 180)  // Extended option text

// Color palette - Comp Mode (Orange/Warm)
#define COLOR_CELL_BG_COMP RGB(35, 25, 15)   // Semi-transparent cell bg (warm)
#define COLOR_GRID_LINE_COMP RGB(90, 70, 42) // Warm orange grid lines
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
    35; // Extended menu area size (pixels from grid edge)

// Forward declarations
static LRESULT CALLBACK GridWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam);
static void DrawGrid(HDC hdc, Gdiplus::Graphics *g);
static void DrawExtendedMenu(HDC hdc, Gdiplus::Graphics *g, int windowSize);
static void UpdateHoverFromMouse(int screenX, int screenY);

namespace NativeUI {

bool Initialize() {
  if (g_initialized)
    return true;

  // Initialize GDI+
  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL) !=
      Gdiplus::Ok) {
    return false;
  }

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
    Gdiplus::GdiplusShutdown(g_gdiplusToken);
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
    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    g_gdiplusToken = 0;
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

  // Transparency: use color-key for background (black = transparent)
  // LWA_COLORKEY makes COLOR_BG (black) transparent
  BYTE alpha = g_settings.transparentMode ? 200 : 255;

  // Create or reposition window
  if (g_gridWnd) {
    SetWindowPos(g_gridWnd, HWND_TOPMOST, g_windowX, g_windowY, windowSize,
                 windowSize, SWP_SHOWWINDOW);
    // Use color key for transparent background + alpha for overall transparency
    SetLayeredWindowAttributes(g_gridWnd, COLOR_BG, alpha,
                               LWA_COLORKEY | LWA_ALPHA);
  } else {
    g_gridWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED, GRID_CLASS_NAME, NULL,
        WS_POPUP | WS_VISIBLE, g_windowX, g_windowY, windowSize, windowSize,
        NULL, NULL, g_hInstance, NULL);

    SetLayeredWindowAttributes(g_gridWnd, COLOR_BG, alpha,
                               LWA_COLORKEY | LWA_ALPHA);
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

  // Check extended menu regions first (outside grid area)
  // Grid area is from (0,0) to (gridPixels, gridPixels) in relative coords
  if (relY < 0 && relX >= 0 && relX < gridPixels) {
    g_hoverExtOption = NativeUI::OPT_SELECTION_MODE; // Top
    return;
  }
  if (relY >= gridPixels && relX >= 0 && relX < gridPixels) {
    g_hoverExtOption = NativeUI::OPT_SETTINGS; // Bottom (now Setting)
    return;
  }
  if (relX < 0 && relY >= 0 && relY < gridPixels) {
    g_hoverExtOption =
        NativeUI::OPT_CUSTOM_ANCHOR; // Left (unused, but keeping)
    return;
  }
  if (relX >= gridPixels && relY >= 0 && relY < gridPixels) {
    g_hoverExtOption = NativeUI::OPT_TRANSPARENT; // Right (unused, but keeping)
    return;
  }

  // Check grid cells (rectangle hit test for entire cell area)
  for (int y = 0; y < g_config.gridSize; y++) {
    for (int x = 0; x < g_config.gridSize; x++) {
      int cellLeft = x * cellTotal;
      int cellTop = y * cellTotal;
      // Hover area extends 2px on each side (cellSize + 4)
      int cellRight = cellLeft + g_config.cellSize + 4;
      int cellBottom = cellTop + g_config.cellSize + 4;

      if (relX >= cellLeft && relX < cellRight && relY >= cellTop &&
          relY < cellBottom) {
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

// Draw extended menu options (text only, using GDI+ for smooth text)
static void DrawExtendedMenu(HDC hdc, Gdiplus::Graphics *g, int windowSize) {
  int cellTotal = g_config.cellSize + g_config.spacing;
  int gridPixels = g_config.gridSize * cellTotal;
  int extOffset = g_extThreshold;
  int gridStart = g_config.margin + extOffset;
  int gridEnd = gridStart + gridPixels;

  // Mode-specific colors
  bool compMode = g_settings.useCompMode;
  Gdiplus::Color lineColor =
      compMode ? Gdiplus::Color(255, 200, 140, 80) : // Orange for comp mode
          Gdiplus::Color(255, 90, 140, 170);         // Teal for selection mode
  Gdiplus::Color glowColor =
      compMode ? Gdiplus::Color(255, 255, 180, 80) : // Bright orange
          Gdiplus::Color(255, 74, 207, 255);         // Bright cyan

  // Create font
  Gdiplus::FontFamily fontFamily(L"Segoe UI");
  Gdiplus::Font font(&fontFamily, 14, Gdiplus::FontStyleBold,
                     Gdiplus::UnitPixel);
  Gdiplus::StringFormat format;
  format.SetAlignment(Gdiplus::StringAlignmentCenter);
  format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

  // Top: Current Mode (Selection or Composition)
  {
    bool hover = (g_hoverExtOption == NativeUI::OPT_SELECTION_MODE);
    Gdiplus::SolidBrush brush(hover ? glowColor : lineColor);
    const wchar_t *text =
        g_settings.useCompMode ? L"Composition" : L"Selection";
    Gdiplus::RectF rc(0, 2, (Gdiplus::REAL)windowSize,
                      (Gdiplus::REAL)(gridStart - 4));
    g->DrawString(text, -1, &font, rc, &format, &brush);
  }

  // Bottom: Setting
  {
    bool hover = (g_hoverExtOption == NativeUI::OPT_SETTINGS);
    Gdiplus::SolidBrush brush(hover ? glowColor : lineColor);
    Gdiplus::RectF rc(0, (Gdiplus::REAL)(gridEnd + 2),
                      (Gdiplus::REAL)windowSize,
                      (Gdiplus::REAL)(windowSize - gridEnd - 4));
    g->DrawString(L"Setting", -1, &font, rc, &format, &brush);
  }
}

// Draw the grid with L/T/+ marks and glow (using GDI+ for anti-aliasing)
static void DrawGrid(HDC hdc, Gdiplus::Graphics *g) {
  int cellTotal = g_config.cellSize + g_config.spacing;
  int gridPixels = g_config.gridSize * cellTotal - g_config.spacing;
  int extOffset = g_extThreshold;
  int margin = g_config.margin + extOffset;
  int radius = g_config.cellSize / 12;
  int hoverRadius = g_config.cellSize / 8;
  int len = (int)(cellTotal * 0.4);

  // Mode-specific colors
  bool compMode = g_settings.useCompMode;

  // Grid background (black 20% opacity)
  Gdiplus::Color gridBgColor(51, 0, 0, 0); // 20% opacity black (255*0.2 = 51)

  // Cell background (gray 20% opacity)
  Gdiplus::Color cellBgColor(51, 128, 128, 128); // 20% opacity gray

  // Mark colors
  Gdiplus::Color lineColor = compMode ? Gdiplus::Color(255, 200, 140, 80)
                                      :                             // Orange
                                 Gdiplus::Color(255, 90, 140, 170); // Teal
  Gdiplus::Color glowInner =
      compMode ? Gdiplus::Color(255, 255, 180, 80) : // Bright orange
          Gdiplus::Color(255, 74, 207, 255);         // Bright cyan
  Gdiplus::Color glowMid = compMode ? Gdiplus::Color(180, 200, 120, 60)
                                    : Gdiplus::Color(180, 42, 122, 154);
  Gdiplus::Color glowOuter = compMode ? Gdiplus::Color(100, 150, 90, 40)
                                      : Gdiplus::Color(100, 42, 90, 110);

  // Draw grid background first (black 20%)
  Gdiplus::SolidBrush gridBrush(gridBgColor);
  g->FillRectangle(&gridBrush, margin, margin, gridPixels, gridPixels);

  // Draw cell backgrounds on top (gray 20%)
  Gdiplus::SolidBrush cellBrush(cellBgColor);
  for (int y = 0; y < g_config.gridSize; y++) {
    for (int x = 0; x < g_config.gridSize; x++) {
      int cellLeft = margin + x * cellTotal;
      int cellTop = margin + y * cellTotal;
      g->FillRectangle(&cellBrush, cellLeft, cellTop, g_config.cellSize,
                       g_config.cellSize);
    }
  }

  // Second pass: draw marks and anchor circles
  for (int y = 0; y < g_config.gridSize; y++) {
    for (int x = 0; x < g_config.gridSize; x++) {
      int cx = margin + x * cellTotal + cellTotal / 2;
      int cy = margin + y * cellTotal + cellTotal / 2;
      bool isHover = (x == g_hoverCellX && y == g_hoverCellY);

      // Determine mark type
      bool isLeft = (x == 0);
      bool isRight = (x == g_config.gridSize - 1);
      bool isTop = (y == 0);
      bool isBottom = (y == g_config.gridSize - 1);
      bool isCorner = (isLeft || isRight) && (isTop || isBottom);
      bool isEdge = (isLeft || isRight || isTop || isBottom) && !isCorner;

      // Calculate mark position
      int edgeOffset = cellTotal / 4;
      int markX = cx, markY = cy;
      if (isCorner || isEdge) {
        if (isLeft)
          markX -= edgeOffset;
        if (isRight)
          markX += edgeOffset;
        if (isTop)
          markY -= edgeOffset;
        if (isBottom)
          markY += edgeOffset;
      }

      // Choose color based on hover
      Gdiplus::Color markColor = isHover ? glowInner : lineColor;
      Gdiplus::Pen markPen(markColor, 2.0f);

      // Draw marks
      if (isCorner) {
        if (isTop && isLeft) {
          g->DrawLine(&markPen, markX, markY + len, markX, markY);
          g->DrawLine(&markPen, markX, markY, markX + len, markY);
        } else if (isTop && isRight) {
          g->DrawLine(&markPen, markX - len, markY, markX, markY);
          g->DrawLine(&markPen, markX, markY, markX, markY + len);
        } else if (isBottom && isLeft) {
          g->DrawLine(&markPen, markX, markY - len, markX, markY);
          g->DrawLine(&markPen, markX, markY, markX + len, markY);
        } else {
          g->DrawLine(&markPen, markX - len, markY, markX, markY);
          g->DrawLine(&markPen, markX, markY, markX, markY - len);
        }
      } else if (isEdge) {
        if (isTop) {
          g->DrawLine(&markPen, markX - len, markY, markX + len, markY);
          g->DrawLine(&markPen, markX, markY, markX, markY + len);
        } else if (isBottom) {
          g->DrawLine(&markPen, markX - len, markY, markX + len, markY);
          g->DrawLine(&markPen, markX, markY - len, markX, markY);
        } else if (isLeft) {
          g->DrawLine(&markPen, markX, markY - len, markX, markY + len);
          g->DrawLine(&markPen, markX, markY, markX + len, markY);
        } else {
          g->DrawLine(&markPen, markX, markY - len, markX, markY + len);
          g->DrawLine(&markPen, markX - len, markY, markX, markY);
        }
      } else {
        // Center cross
        g->DrawLine(&markPen, cx - len, cy, cx + len, cy);
        g->DrawLine(&markPen, cx, cy - len, cx, cy + len);
      }

      // Anchor position
      int anchorX = (isCorner || isEdge) ? markX : cx;
      int anchorY = (isCorner || isEdge) ? markY : cy;

      // Draw glow effect when hovering
      if (isHover) {
        Gdiplus::SolidBrush glowBrush3(glowOuter);
        g->FillEllipse(&glowBrush3, anchorX - hoverRadius * 2,
                       anchorY - hoverRadius * 2, hoverRadius * 4,
                       hoverRadius * 4);

        Gdiplus::SolidBrush glowBrush2(glowMid);
        g->FillEllipse(&glowBrush2, anchorX - hoverRadius - 2,
                       anchorY - hoverRadius - 2, hoverRadius * 2 + 4,
                       hoverRadius * 2 + 4);

        Gdiplus::SolidBrush glowBrush1(glowInner);
        g->FillEllipse(&glowBrush1, anchorX - hoverRadius,
                       anchorY - hoverRadius, hoverRadius * 2, hoverRadius * 2);
      }

      // Draw anchor circle
      Gdiplus::SolidBrush circleBrush(isHover ? glowInner : lineColor);
      g->FillEllipse(&circleBrush, anchorX - radius, anchorY - radius,
                     radius * 2, radius * 2);
    }
  }
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

    // Create memory DC for double buffering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
    SelectObject(memDC, memBitmap);

    // Create GDI+ Graphics from memory DC
    Gdiplus::Graphics graphics(memDC);

    // Enable anti-aliasing for smooth lines and circles
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    // Fill background with color key (will be transparent)
    graphics.Clear(Gdiplus::Color(0, 0, 0));

    // Draw grid and menu using GDI+
    DrawGrid(memDC, &graphics);
    DrawExtendedMenu(memDC, &graphics, rect.right);

    // Copy to screen
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
