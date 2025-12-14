/*****************************************************************************
 * NativeUI.cpp
 *
 * Native Windows UI for Anchor Grid
 * Uses GDI+ for anti-aliasing, alpha blending, and smooth rendering
 *
 * Layout:
 *   [Left Icons]  [Grid]  [Right Icons]
 *   - Left: Custom Anchor 1, 2, 3
 *   - Right: Comp Mode, Mask Mode, Settings
 *****************************************************************************/

#include "NativeUI.h"

#ifdef MSWindows

// GDI+ includes - DO NOT MODIFY ORDER (see GdiPlusIncludes.h)
#include "GdiPlusIncludes.h"

#include <cmath>
#include <string>

// GDI+ token for startup/shutdown
static ULONG_PTR g_gdiplusToken = 0;

// Window class name
static const wchar_t *GRID_CLASS_NAME = L"AnchorGridClass";

// Color palette - Selection Mode (Cyan/Teal)
#define COLOR_BG RGB(1, 1, 1)         // Transparent key (unique dark color)
#define COLOR_CELL_BG RGB(35, 35, 40) // Cell background
#define COLOR_GRID_LINE RGB(90, 140, 170)
#define COLOR_GLOW_INNER RGB(74, 207, 255)
#define COLOR_GLOW_MID RGB(42, 122, 154)
#define COLOR_GLOW_OUTER RGB(42, 90, 110)
#define COLOR_ICON_NORMAL RGB(90, 140, 170)
#define COLOR_ICON_HOVER RGB(74, 207, 255)
#define COLOR_ICON_ACTIVE RGB(100, 220, 255)
#define COLOR_BLUE RGB(74, 158, 255)    // Selection, Mask ON
#define COLOR_ORANGE RGB(255, 180, 74)  // Composition mode
#define COLOR_DARK_GRAY RGB(70, 70, 70) // Inactive/OFF state

// Color palette - Comp Mode (Orange/Warm)
#define COLOR_CELL_BG_COMP RGB(35, 25, 15)
#define COLOR_GRID_LINE_COMP RGB(90, 70, 42)
#define COLOR_GLOW_INNER_COMP RGB(255, 180, 74)
#define COLOR_GLOW_MID_COMP RGB(154, 100, 42)
#define COLOR_GLOW_OUTER_COMP RGB(110, 80, 42)

// Side panel dimensions
#define SIDE_PANEL_WIDTH 52
#define ICON_SIZE 34
#define ICON_SPACING 16

// Global state
static HWND g_gridWnd = NULL;
static HINSTANCE g_hInstance = NULL;
static bool g_initialized = false;
static NativeUI::GridConfig g_config;
static NativeUI::GridSettings g_settings;
static int g_windowX = 0;
static int g_windowY = 0;
static int g_windowWidth = 0;
static int g_windowHeight = 0;
static int g_hoverCellX = -1;
static int g_hoverCellY = -1;
static NativeUI::ExtendedOption g_hoverExtOption = NativeUI::OPT_NONE;

// Forward declarations
static LRESULT CALLBACK GridWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam);
static void DrawGrid(HDC hdc);
static void DrawSidePanels(HDC hdc);
static void DrawIcon(HDC hdc, int cx, int cy, NativeUI::ExtendedOption type,
                     bool hover, bool active);
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
    g_initialized = false;
  }
  if (g_gdiplusToken != 0) {
    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    g_gdiplusToken = 0;
  }
}

void ShowGrid(int mouseX, int mouseY, const GridConfig &config) {
  if (!g_initialized && !Initialize())
    return;

  g_config = config;
  g_hoverCellX = -1;
  g_hoverCellY = -1;
  g_hoverExtOption = OPT_NONE;

  // Fixed grid size target (constant regardless of cell count)
  const int TARGET_GRID_SIZE = 150; // pixels

  // Calculate cellSize to maintain constant grid size
  int maxDim = (config.gridWidth > config.gridHeight) ? config.gridWidth
                                                      : config.gridHeight;
  g_config.cellSize =
      (TARGET_GRID_SIZE - (maxDim - 1) * config.spacing) / maxDim;

  // Calculate window size
  int cellTotal = g_config.cellSize + config.spacing;
  int gridPixelsW = config.gridWidth * cellTotal;
  int gridPixelsH = config.gridHeight * cellTotal;

  // Window: [left panel] [grid] [right panel]
  g_windowWidth =
      SIDE_PANEL_WIDTH + gridPixelsW + config.margin * 2 + SIDE_PANEL_WIDTH;

  // Minimum height to prevent icon clipping (3 icons + spacing)
  int minHeight = ICON_SIZE * 3 + ICON_SPACING * 2 + 20;
  g_windowHeight = (gridPixelsH + config.margin * 2 > minHeight)
                       ? gridPixelsH + config.margin * 2
                       : minHeight;

  // Center window on mouse
  g_windowX = mouseX - g_windowWidth / 2;
  g_windowY = mouseY - g_windowHeight / 2;

  // Apply grid opacity setting (0-100 -> 0-255)
  BYTE alpha = (BYTE)(g_settings.gridOpacity * 255 / 100);
  if (alpha < 100)
    alpha = 100; // Minimum visibility

  if (g_gridWnd) {
    SetWindowPos(g_gridWnd, HWND_TOPMOST, g_windowX, g_windowY, g_windowWidth,
                 g_windowHeight, SWP_SHOWWINDOW);
    SetLayeredWindowAttributes(g_gridWnd, COLOR_BG, alpha,
                               LWA_COLORKEY | LWA_ALPHA);
  } else {
    g_gridWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED, GRID_CLASS_NAME, NULL,
        WS_POPUP | WS_VISIBLE, g_windowX, g_windowY, g_windowWidth,
        g_windowHeight, NULL, NULL, g_hInstance, NULL);

    SetLayeredWindowAttributes(g_gridWnd, COLOR_BG, alpha,
                               LWA_COLORKEY | LWA_ALPHA);
  }

  UpdateHoverFromMouse(mouseX, mouseY);
  InvalidateRect(g_gridWnd, NULL, FALSE); // FALSE to prevent flickering
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
      InvalidateRect(g_gridWnd, NULL, FALSE); // FALSE to prevent flickering
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
  int gridPixelsW = g_config.gridWidth * cellTotal;
  int gridPixelsH = g_config.gridHeight * cellTotal;
  int gridStartX = SIDE_PANEL_WIDTH + g_config.margin;
  int gridStartY = g_config.margin;

  int relX = screenX - g_windowX;
  int relY = screenY - g_windowY;

  g_hoverCellX = -1;
  g_hoverCellY = -1;
  g_hoverExtOption = NativeUI::OPT_NONE;

  // Check left panel (Custom Anchors)
  if (relX < SIDE_PANEL_WIDTH && relX >= 0) {
    int iconY = (g_windowHeight - (ICON_SIZE * 3 + ICON_SPACING * 2)) / 2;
    for (int i = 0; i < 3; i++) {
      int top = iconY + i * (ICON_SIZE + ICON_SPACING);
      if (relY >= top && relY < top + ICON_SIZE) {
        g_hoverExtOption =
            (NativeUI::ExtendedOption)(NativeUI::OPT_CUSTOM_1 + i);
        return;
      }
    }
    return;
  }

  // Check right panel (Mode controls)
  if (relX >= g_windowWidth - SIDE_PANEL_WIDTH) {
    int iconY = (g_windowHeight - (ICON_SIZE * 3 + ICON_SPACING * 2)) / 2;
    for (int i = 0; i < 3; i++) {
      int top = iconY + i * (ICON_SIZE + ICON_SPACING);
      if (relY >= top && relY < top + ICON_SIZE) {
        switch (i) {
        case 0:
          g_hoverExtOption = NativeUI::OPT_COMP_MODE;
          break;
        case 1:
          g_hoverExtOption = NativeUI::OPT_MASK_MODE;
          break;
        case 2:
          g_hoverExtOption = NativeUI::OPT_SETTINGS;
          break;
        }
        return;
      }
    }
    return;
  }

  // Check grid cells
  int gridRelX = relX - gridStartX;
  int gridRelY = relY - gridStartY;

  if (gridRelX >= 0 && gridRelX < gridPixelsW && gridRelY >= 0 &&
      gridRelY < gridPixelsH) {
    int cellX = gridRelX / cellTotal;
    int cellY = gridRelY / cellTotal;
    if (cellX < g_config.gridWidth && cellY < g_config.gridHeight) {
      g_hoverCellX = cellX;
      g_hoverCellY = cellY;
    }
  }
}

// Draw hover background (unified square area)
static void DrawIconBackground(HDC hdc, int cx, int cy, bool hover) {
  if (hover) {
    int halfSize = ICON_SIZE / 2;
    HBRUSH hoverBrush = CreateSolidBrush(RGB(50, 60, 70));
    RECT hoverRect = {cx - halfSize, cy - halfSize, cx + halfSize,
                      cy + halfSize};
    FillRect(hdc, &hoverRect, hoverBrush);
    DeleteObject(hoverBrush);
  }
}

// Draw an icon based on type using pure GDI+
static void DrawIcon(HDC hdc, int cx, int cy, NativeUI::ExtendedOption type,
                     bool hover, bool active) {
  using namespace Gdiplus;

  // Draw hover background first
  DrawIconBackground(hdc, cx, cy, hover);

  // Create GDI+ Graphics with anti-aliasing
  Graphics graphics(hdc);
  graphics.SetSmoothingMode(SmoothingModeNone);
  graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
  graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

  int r = ICON_SIZE / 2 - 6;
  int s = 3;

  // Helper to convert COLORREF to GDI+ Color
  auto toColor = [](COLORREF c) {
    return Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
  };

  switch (type) {
  case NativeUI::OPT_CUSTOM_1:
  case NativeUI::OPT_CUSTOM_2:
  case NativeUI::OPT_CUSTOM_3: {
    COLORREF colorRef =
        hover ? COLOR_ICON_HOVER : (active ? COLOR_BLUE : COLOR_ICON_NORMAL);
    Color color = toColor(colorRef);
    Color bgColor = toColor(COLOR_BG);
    Pen pen(color, 2.0f);
    SolidBrush bgBrush(bgColor);

    // Crosshair lines (with gap for center circle)
    int gap = 8;
    graphics.DrawLine(&pen, cx - r, cy, cx - gap, cy);
    graphics.DrawLine(&pen, cx + gap, cy, cx + r, cy);
    graphics.DrawLine(&pen, cx, cy - r, cx, cy - gap);
    graphics.DrawLine(&pen, cx, cy + gap, cx, cy + r);

    // Center circle with fill (AE anchor style) - 70% larger
    int circleR = 7;
    graphics.FillEllipse(&bgBrush, cx - circleR, cy - circleR, circleR * 2,
                         circleR * 2);
    graphics.DrawEllipse(&pen, cx - circleR, cy - circleR, circleR * 2,
                         circleR * 2);

    // Draw preset number
    FontFamily fontFamily(L"Segoe UI");
    Font font(&fontFamily, 9, FontStyleBold, UnitPixel);
    SolidBrush textBrush(color);
    wchar_t num[2] = {
        static_cast<wchar_t>(L'1' + (type - NativeUI::OPT_CUSTOM_1)), 0};
    graphics.DrawString(num, 1, &font,
                        PointF((REAL)(cx + r - 8), (REAL)(cy + r - 12)),
                        &textBrush);
    break;
  }

  case NativeUI::OPT_COMP_MODE: {
    bool isCompMode = g_settings.useCompMode;
    COLORREF colorRef =
        hover ? COLOR_ICON_HOVER : (isCompMode ? COLOR_ORANGE : COLOR_BLUE);
    Color color = toColor(colorRef);
    Pen pen(color, 2.0f);
    SolidBrush brush(color);

    if (isCompMode) {
      // Composition: wide rectangle with + in center
      int w = r + 2;
      int h = r - 2;
      graphics.DrawRectangle(&pen, cx - w, cy - h, w * 2, h * 2);
      // Plus sign
      int ps = 3;
      graphics.DrawLine(&pen, cx - ps, cy, cx + ps, cy);
      graphics.DrawLine(&pen, cx, cy - ps, cx, cy + ps);
    } else {
      // Selection: square with corner squares (centered on corners)
      graphics.DrawRectangle(&pen, cx - r, cy - r, r * 2, r * 2);
      // Corner squares - centered on each corner
      int cs = s + 1; // corner square size
      int half = cs / 2;
      graphics.FillRectangle(&brush, cx - r - half, cy - r - half, cs,
                             cs); // top-left
      graphics.FillRectangle(&brush, cx + r - half, cy - r - half, cs,
                             cs); // top-right
      graphics.FillRectangle(&brush, cx - r - half, cy + r - half, cs,
                             cs); // bottom-left
      graphics.FillRectangle(&brush, cx + r - half, cy + r - half, cs,
                             cs); // bottom-right
    }
    break;
  }

  case NativeUI::OPT_MASK_MODE: {
    bool maskOn = g_settings.useMaskRecognition;
    COLORREF colorRef =
        hover ? COLOR_ICON_HOVER : (maskOn ? COLOR_BLUE : COLOR_DARK_GRAY);
    Color color = toColor(colorRef);
    SolidBrush brush(color);
    Color bgColor = toColor(COLOR_BG);
    SolidBrush bgBrush(bgColor);

    // Horizontal rectangle (wider than tall)
    int w = r + 3;
    int h = r - 3;
    graphics.FillRectangle(&brush, cx - w, cy - h, w * 2, h * 2);
    // Circle cutout
    int circleR = h / 2;
    graphics.FillEllipse(&bgBrush, cx - circleR, cy - circleR, circleR * 2,
                         circleR * 2);
    break;
  }

  case NativeUI::OPT_SETTINGS: {
    COLORREF colorRef = hover ? COLOR_BLUE : COLOR_ICON_NORMAL;
    Color color = toColor(colorRef);

    // Larger center circle (main part of gear)
    int innerR = r - 4; // Bigger circle
    Pen circlePen(color, 2.5f);
    graphics.DrawEllipse(&circlePen, cx - innerR, cy - innerR, innerR * 2,
                         innerR * 2);

    // 6 short gear teeth
    Pen teethPen(color, 3.0f);
    int teethInner = innerR - 1; // Start from circle edge
    int teethOuter = innerR + 4; // Short teeth
    for (int i = 0; i < 6; i++) {
      double angle = i * 3.14159 / 3;
      int x1 = cx + (int)(teethInner * cos(angle));
      int y1 = cy + (int)(teethInner * sin(angle));
      int x2 = cx + (int)(teethOuter * cos(angle));
      int y2 = cy + (int)(teethOuter * sin(angle));
      graphics.DrawLine(&teethPen, x1, y1, x2, y2);
    }
    break;
  }

  default:
    break;
  }
}

// Draw side panels with icons
static void DrawSidePanels(HDC hdc) {
  int iconY = (g_windowHeight - (ICON_SIZE * 3 + ICON_SPACING * 2)) / 2;
  int leftCx = SIDE_PANEL_WIDTH / 2;
  int rightCx = g_windowWidth - SIDE_PANEL_WIDTH / 2;

  // Left panel: Custom anchors 1, 2, 3
  for (int i = 0; i < 3; i++) {
    int cy = iconY + i * (ICON_SIZE + ICON_SPACING) + ICON_SIZE / 2;
    NativeUI::ExtendedOption opt =
        (NativeUI::ExtendedOption)(NativeUI::OPT_CUSTOM_1 + i);
    bool hover = (g_hoverExtOption == opt);
    DrawIcon(hdc, leftCx, cy, opt, hover, false);
  }

  // Right panel: Comp mode, Mask mode, Settings
  NativeUI::ExtendedOption rightOpts[] = {
      NativeUI::OPT_COMP_MODE, NativeUI::OPT_MASK_MODE, NativeUI::OPT_SETTINGS};
  bool activeStates[] = {g_settings.useCompMode, g_settings.useMaskRecognition,
                         false};

  for (int i = 0; i < 3; i++) {
    int cy = iconY + i * (ICON_SIZE + ICON_SPACING) + ICON_SIZE / 2;
    bool hover = (g_hoverExtOption == rightOpts[i]);
    DrawIcon(hdc, rightCx, cy, rightOpts[i], hover, activeStates[i]);
  }
}

// Draw the grid with marks and glow using GDI+
static void DrawGrid(HDC hdc) {
  using namespace Gdiplus;

  Graphics graphics(hdc);
  graphics.SetSmoothingMode(SmoothingModeNone);
  graphics.SetPixelOffsetMode(PixelOffsetModeHalf);

  int cellTotal = g_config.cellSize + g_config.spacing;
  int gridStartX = SIDE_PANEL_WIDTH + g_config.margin;
  int gridStartY = g_config.margin;
  int radius = g_config.cellSize / 12;
  int hoverRadius = g_config.cellSize / 12;
  int len = (int)(cellTotal * 0.4);

  bool compMode = g_settings.useCompMode;

  // Helper to convert COLORREF to GDI+ Color
  auto toColor = [](COLORREF c) {
    return Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
  };

  Color cellBgColor = toColor(COLOR_CELL_BG);
  Color lineColor = toColor(compMode ? COLOR_GRID_LINE_COMP : COLOR_GRID_LINE);
  Color glowInnerColor =
      toColor(compMode ? COLOR_GLOW_INNER_COMP : COLOR_GLOW_INNER);
  Color glowMidColor = toColor(compMode ? COLOR_GLOW_MID_COMP : COLOR_GLOW_MID);
  Color glowOuterColor =
      toColor(compMode ? COLOR_GLOW_OUTER_COMP : COLOR_GLOW_OUTER);

  // Draw cell backgrounds with opacity
  BYTE cellAlpha = (BYTE)(g_settings.cellOpacity * 255 / 100);
  Color cellBgWithAlpha(cellAlpha, GetRValue(COLOR_CELL_BG),
                        GetGValue(COLOR_CELL_BG), GetBValue(COLOR_CELL_BG));
  SolidBrush cellBrush(cellBgWithAlpha);
  for (int y = 0; y < g_config.gridHeight; y++) {
    for (int x = 0; x < g_config.gridWidth; x++) {
      int cellLeft = gridStartX + x * cellTotal;
      int cellTop = gridStartY + y * cellTotal;
      graphics.FillRectangle(&cellBrush, cellLeft, cellTop, g_config.cellSize,
                             g_config.cellSize);
    }
  }

  // Draw marks and dots using GDI+
  Pen markPen(lineColor, 2.0f);
  SolidBrush dotBrush(lineColor);

  for (int y = 0; y < g_config.gridHeight; y++) {
    for (int x = 0; x < g_config.gridWidth; x++) {
      int cx = gridStartX + x * cellTotal + cellTotal / 2;
      int cy = gridStartY + y * cellTotal + cellTotal / 2;

      bool isHover = (x == g_hoverCellX && y == g_hoverCellY);

      bool isLeft = (x == 0);
      bool isRight = (x == g_config.gridWidth - 1);
      bool isTop = (y == 0);
      bool isBottom = (y == g_config.gridHeight - 1);
      bool isCorner = (isLeft || isRight) && (isTop || isBottom);
      bool isEdge = (isLeft || isRight || isTop || isBottom) && !isCorner;

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

      Color markColor = isHover ? glowInnerColor : lineColor;
      Pen linePen(markColor, 2.0f);

      if (isCorner) {
        if (isTop && isLeft) {
          graphics.DrawLine(&linePen, markX, markY + len, markX, markY);
          graphics.DrawLine(&linePen, markX, markY, markX + len, markY);
        } else if (isTop && isRight) {
          graphics.DrawLine(&linePen, markX - len, markY, markX, markY);
          graphics.DrawLine(&linePen, markX, markY, markX, markY + len);
        } else if (isBottom && isLeft) {
          graphics.DrawLine(&linePen, markX, markY - len, markX, markY);
          graphics.DrawLine(&linePen, markX, markY, markX + len, markY);
        } else {
          graphics.DrawLine(&linePen, markX - len, markY, markX, markY);
          graphics.DrawLine(&linePen, markX, markY, markX, markY - len);
        }
      } else if (isEdge) {
        if (isTop) {
          graphics.DrawLine(&linePen, markX - len, markY, markX + len, markY);
          graphics.DrawLine(&linePen, markX, markY, markX, markY + len);
        } else if (isBottom) {
          graphics.DrawLine(&linePen, markX - len, markY, markX + len, markY);
          graphics.DrawLine(&linePen, markX, markY - len, markX, markY);
        } else if (isLeft) {
          graphics.DrawLine(&linePen, markX, markY - len, markX, markY + len);
          graphics.DrawLine(&linePen, markX, markY, markX + len, markY);
        } else {
          graphics.DrawLine(&linePen, markX, markY - len, markX, markY + len);
          graphics.DrawLine(&linePen, markX - len, markY, markX, markY);
        }
      } else {
        graphics.DrawLine(&linePen, cx - len, cy, cx + len, cy);
        graphics.DrawLine(&linePen, cx, cy - len, cx, cy + len);
      }

      int anchorX = (isCorner || isEdge) ? markX : cx;
      int anchorY = (isCorner || isEdge) ? markY : cy;

      // Draw center dot
      SolidBrush dotBrush(lineColor);
      graphics.FillEllipse(&dotBrush, anchorX - radius, anchorY - radius,
                           radius * 2, radius * 2);

      // Draw hover glow
      if (isHover) {
        SolidBrush glowBrush3(glowOuterColor);
        graphics.FillEllipse(&glowBrush3, anchorX - hoverRadius * 2,
                             anchorY - hoverRadius * 2, hoverRadius * 4,
                             hoverRadius * 4);

        SolidBrush glowBrush2(glowMidColor);
        graphics.FillEllipse(&glowBrush2, anchorX - hoverRadius - 3,
                             anchorY - hoverRadius - 3, (hoverRadius + 3) * 2,
                             (hoverRadius + 3) * 2);

        SolidBrush glowBrush1(glowInnerColor);
        graphics.FillEllipse(&glowBrush1, anchorX - hoverRadius,
                             anchorY - hoverRadius, hoverRadius * 2,
                             hoverRadius * 2);
      }
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
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
    SelectObject(memDC, memBitmap);

    // Fill with exact color key for clean transparency
    HBRUSH bgBrush = CreateSolidBrush(RGB(1, 1, 1)); // Must match COLOR_BG
    FillRect(memDC, &rect, bgBrush);
    DeleteObject(bgBrush);

    DrawGrid(memDC);
    DrawSidePanels(memDC);

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
