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

// IMPORTANT: GDI+ Include Order
// 1. windows.h FIRST (provides META_*, EMR_*, CALLBACK, etc.)
// 2. objidl.h (provides IStream for GDI+)
// 3. gdiplus.h LAST (uses all types from above)
// DO NOT define WIN32_LEAN_AND_MEAN - it excludes GDI headers needed by GDI+
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include <cmath>
#include <string>

// GDI+ token for startup/shutdown
static ULONG_PTR g_gdiplusToken = 0;

// Window class name
static const wchar_t *GRID_CLASS_NAME = L"AnchorGridClass";

// Color palette - Selection Mode (Cyan/Teal)
#define COLOR_BG RGB(0, 0, 0)         // Transparent (color keyed out)
#define COLOR_CELL_BG RGB(20, 20, 20) // Cell background
#define COLOR_GRID_LINE RGB(90, 140, 170)
#define COLOR_GLOW_INNER RGB(74, 207, 255)
#define COLOR_GLOW_MID RGB(42, 122, 154)
#define COLOR_GLOW_OUTER RGB(42, 90, 110)
#define COLOR_ICON_NORMAL RGB(90, 140, 170)
#define COLOR_ICON_HOVER RGB(74, 207, 255)
#define COLOR_ICON_ACTIVE RGB(100, 220, 255)
#define COLOR_BLUE RGB(74, 158, 255)      // Selection, Mask ON
#define COLOR_ORANGE RGB(255, 180, 74)    // Composition mode
#define COLOR_DARK_GRAY RGB(70, 70, 70)   // Inactive/OFF state

// Color palette - Comp Mode (Orange/Warm)
#define COLOR_CELL_BG_COMP RGB(35, 25, 15)
#define COLOR_GRID_LINE_COMP RGB(90, 70, 42)
#define COLOR_GLOW_INNER_COMP RGB(255, 180, 74)
#define COLOR_GLOW_MID_COMP RGB(154, 100, 42)
#define COLOR_GLOW_OUTER_COMP RGB(110, 80, 42)

// Side panel dimensions
#define SIDE_PANEL_WIDTH 44
#define ICON_SIZE 28
#define ICON_SPACING 8

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

  // Calculate window size
  int cellTotal = config.cellSize + config.spacing;
  int gridPixelsW = config.gridWidth * cellTotal;
  int gridPixelsH = config.gridHeight * cellTotal;

  // Window: [left panel] [grid] [right panel]
  g_windowWidth =
      SIDE_PANEL_WIDTH + gridPixelsW + config.margin * 2 + SIDE_PANEL_WIDTH;
  g_windowHeight = gridPixelsH + config.margin * 2;

  // Center window on mouse
  g_windowX = mouseX - g_windowWidth / 2;
  g_windowY = mouseY - g_windowHeight / 2;

  BYTE alpha = g_settings.transparentMode ? 200 : 255;

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
    RECT hoverRect = {cx - halfSize, cy - halfSize, cx + halfSize, cy + halfSize};
    FillRect(hdc, &hoverRect, hoverBrush);
    DeleteObject(hoverBrush);
  }
}

// Draw an icon based on type
static void DrawIcon(HDC hdc, int cx, int cy, NativeUI::ExtendedOption type,
                     bool hover, bool active) {
  // Draw hover background first
  DrawIconBackground(hdc, cx, cy, hover);
  
  int r = ICON_SIZE / 2 - 6;
  int s = 3; // Small corner square size
  
  switch (type) {
  case NativeUI::OPT_CUSTOM_1:
  case NativeUI::OPT_CUSTOM_2:
  case NativeUI::OPT_CUSTOM_3: {
    // AE-style anchor point indicator with number
    COLORREF color = hover ? COLOR_ICON_HOVER : (active ? COLOR_BLUE : COLOR_ICON_NORMAL);
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    SelectObject(hdc, pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    
    // Crosshair lines
    MoveToEx(hdc, cx - r, cy, NULL);
    LineTo(hdc, cx - 4, cy);
    MoveToEx(hdc, cx + 4, cy, NULL);
    LineTo(hdc, cx + r, cy);
    MoveToEx(hdc, cx, cy - r, NULL);
    LineTo(hdc, cx, cy - 4);
    MoveToEx(hdc, cx, cy + 4, NULL);
    LineTo(hdc, cx, cy + r);
    
    // Center circle
    Ellipse(hdc, cx - 4, cy - 4, cx + 4, cy + 4);
    
    // Draw number in corner
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    HFONT hFont = CreateFontW(11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    wchar_t num[2] = {L'1' + (type - NativeUI::OPT_CUSTOM_1), 0};
    TextOutW(hdc, cx + r - 6, cy + r - 10, num, 1);
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
    DeleteObject(pen);
    break;
  }

  case NativeUI::OPT_COMP_MODE: {
    // Selection or Composition icon based on current mode
    bool isCompMode = g_settings.useCompMode;
    COLORREF color = hover ? COLOR_ICON_HOVER : (isCompMode ? COLOR_ORANGE : COLOR_BLUE);
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    SelectObject(hdc, pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    
    if (isCompMode) {
      // Composition: wide rectangle with + in center
      int w = r + 2;
      int h = r - 2;
      Rectangle(hdc, cx - w, cy - h, cx + w, cy + h);
      // Plus sign in center
      int ps = 3;
      MoveToEx(hdc, cx - ps, cy, NULL);
      LineTo(hdc, cx + ps + 1, cy);
      MoveToEx(hdc, cx, cy - ps, NULL);
      LineTo(hdc, cx, cy + ps + 1);
    } else {
      // Selection: square with small squares at corners
      Rectangle(hdc, cx - r, cy - r, cx + r, cy + r);
      // Corner squares (filled)
      HBRUSH cornerBrush = CreateSolidBrush(color);
      RECT tl = {cx - r - 1, cy - r - 1, cx - r + s, cy - r + s};
      RECT tr = {cx + r - s, cy - r - 1, cx + r + 1, cy - r + s};
      RECT bl = {cx - r - 1, cy + r - s, cx - r + s, cy + r + 1};
      RECT br = {cx + r - s, cy + r - s, cx + r + 1, cy + r + 1};
      FillRect(hdc, &tl, cornerBrush);
      FillRect(hdc, &tr, cornerBrush);
      FillRect(hdc, &bl, cornerBrush);
      FillRect(hdc, &br, cornerBrush);
      DeleteObject(cornerBrush);
    }
    DeleteObject(pen);
    break;
  }

  case NativeUI::OPT_MASK_MODE: {
    // Mask icon: filled rectangle with transparent circle in center
    bool maskOn = g_settings.useMaskRecognition;
    COLORREF color = hover ? COLOR_ICON_HOVER : (maskOn ? COLOR_BLUE : COLOR_DARK_GRAY);
    
    // Filled rectangle background
    HBRUSH fillBrush = CreateSolidBrush(color);
    RECT maskRect = {cx - r, cy - r, cx + r, cy + r};
    FillRect(hdc, &maskRect, fillBrush);
    DeleteObject(fillBrush);
    
    // Transparent circle in center (cut out using background color)
    HBRUSH bgBrush = CreateSolidBrush(COLOR_BG);
    HPEN bgPen = CreatePen(PS_SOLID, 1, COLOR_BG);
    SelectObject(hdc, bgPen);
    SelectObject(hdc, bgBrush);
    int circleR = r / 2;
    Ellipse(hdc, cx - circleR, cy - circleR, cx + circleR, cy + circleR);
    DeleteObject(bgBrush);
    DeleteObject(bgPen);
    break;
  }

  case NativeUI::OPT_SETTINGS: {
    // Gear icon: filled with hollow center
    COLORREF color = hover ? COLOR_BLUE : COLOR_ICON_NORMAL;
    HBRUSH gearBrush = CreateSolidBrush(color);
    HPEN gearPen = CreatePen(PS_SOLID, 1, color);
    SelectObject(hdc, gearPen);
    SelectObject(hdc, gearBrush);
    
    // Draw gear teeth as filled polygon (simplified: octagon)
    int outerR = r;
    int innerR = r - 4;
    POINT teeth[16];
    for (int i = 0; i < 8; i++) {
      double angle1 = i * 3.14159 / 4 - 0.2;
      double angle2 = i * 3.14159 / 4 + 0.2;
      teeth[i * 2].x = cx + (int)(outerR * cos(angle1));
      teeth[i * 2].y = cy + (int)(outerR * sin(angle1));
      teeth[i * 2 + 1].x = cx + (int)(outerR * cos(angle2));
      teeth[i * 2 + 1].y = cy + (int)(outerR * sin(angle2));
    }
    Polygon(hdc, teeth, 16);
    
    // Hollow center circle
    HBRUSH bgBrush = CreateSolidBrush(COLOR_BG);
    SelectObject(hdc, bgBrush);
    int centerR = 4;
    Ellipse(hdc, cx - centerR, cy - centerR, cx + centerR, cy + centerR);
    
    DeleteObject(bgBrush);
    DeleteObject(gearBrush);
    DeleteObject(gearPen);
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

// Draw the grid with marks and glow
static void DrawGrid(HDC hdc) {
  int cellTotal = g_config.cellSize + g_config.spacing;
  int gridStartX = SIDE_PANEL_WIDTH + g_config.margin;
  int gridStartY = g_config.margin;
  int radius = g_config.cellSize / 12;
  int hoverRadius = g_config.cellSize / 12;
  int len = (int)(cellTotal * 0.4);

  bool compMode = g_settings.useCompMode;
  COLORREF cellBgColor = COLOR_CELL_BG;
  COLORREF lineColor = compMode ? COLOR_GRID_LINE_COMP : COLOR_GRID_LINE;
  COLORREF glowInner = compMode ? COLOR_GLOW_INNER_COMP : COLOR_GLOW_INNER;
  COLORREF glowMid = compMode ? COLOR_GLOW_MID_COMP : COLOR_GLOW_MID;
  COLORREF glowOuter = compMode ? COLOR_GLOW_OUTER_COMP : COLOR_GLOW_OUTER;

  // Draw cell backgrounds
  for (int y = 0; y < g_config.gridHeight; y++) {
    for (int x = 0; x < g_config.gridWidth; x++) {
      int cellLeft = gridStartX + x * cellTotal;
      int cellTop = gridStartY + y * cellTotal;
      RECT cellRect = {cellLeft, cellTop, cellLeft + g_config.cellSize,
                       cellTop + g_config.cellSize};
      HBRUSH cellBrush = CreateSolidBrush(cellBgColor);
      FillRect(hdc, &cellRect, cellBrush);
      DeleteObject(cellBrush);
    }
  }

  // Draw marks and dots
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

      COLORREF markColor = isHover ? glowInner : lineColor;
      HPEN linePen = CreatePen(PS_SOLID, 2, markColor);
      SelectObject(hdc, linePen);
      SelectObject(hdc, GetStockObject(NULL_BRUSH));

      if (isCorner) {
        if (isTop && isLeft) {
          MoveToEx(hdc, markX, markY + len, NULL);
          LineTo(hdc, markX, markY);
          LineTo(hdc, markX + len, markY);
        } else if (isTop && isRight) {
          MoveToEx(hdc, markX - len, markY, NULL);
          LineTo(hdc, markX, markY);
          LineTo(hdc, markX, markY + len);
        } else if (isBottom && isLeft) {
          MoveToEx(hdc, markX, markY - len, NULL);
          LineTo(hdc, markX, markY);
          LineTo(hdc, markX + len, markY);
        } else {
          MoveToEx(hdc, markX - len, markY, NULL);
          LineTo(hdc, markX, markY);
          LineTo(hdc, markX, markY - len);
        }
      } else if (isEdge) {
        if (isTop) {
          MoveToEx(hdc, markX - len, markY, NULL);
          LineTo(hdc, markX + len, markY);
          MoveToEx(hdc, markX, markY, NULL);
          LineTo(hdc, markX, markY + len);
        } else if (isBottom) {
          MoveToEx(hdc, markX - len, markY, NULL);
          LineTo(hdc, markX + len, markY);
          MoveToEx(hdc, markX, markY - len, NULL);
          LineTo(hdc, markX, markY);
        } else if (isLeft) {
          MoveToEx(hdc, markX, markY - len, NULL);
          LineTo(hdc, markX, markY + len);
          MoveToEx(hdc, markX, markY, NULL);
          LineTo(hdc, markX + len, markY);
        } else {
          MoveToEx(hdc, markX, markY - len, NULL);
          LineTo(hdc, markX, markY + len);
          MoveToEx(hdc, markX - len, markY, NULL);
          LineTo(hdc, markX, markY);
        }
      } else {
        MoveToEx(hdc, cx - len, cy, NULL);
        LineTo(hdc, cx + len, cy);
        MoveToEx(hdc, cx, cy - len, NULL);
        LineTo(hdc, cx, cy + len);
      }
      DeleteObject(linePen);

      int anchorX = (isCorner || isEdge) ? markX : cx;
      int anchorY = (isCorner || isEdge) ? markY : cy;

      HPEN circlePen = CreatePen(PS_SOLID, 1, lineColor);
      HBRUSH circleBrush = CreateSolidBrush(lineColor);
      SelectObject(hdc, circlePen);
      SelectObject(hdc, circleBrush);
      Ellipse(hdc, anchorX - radius, anchorY - radius, anchorX + radius,
              anchorY + radius);
      DeleteObject(circlePen);
      DeleteObject(circleBrush);

      if (isHover) {
        HPEN glowPen3 = CreatePen(PS_SOLID, 1, glowOuter);
        HBRUSH glowBrush3 = CreateSolidBrush(glowOuter);
        SelectObject(hdc, glowPen3);
        SelectObject(hdc, glowBrush3);
        Ellipse(hdc, anchorX - hoverRadius * 2, anchorY - hoverRadius * 2,
                anchorX + hoverRadius * 2, anchorY + hoverRadius * 2);
        DeleteObject(glowPen3);
        DeleteObject(glowBrush3);

        HPEN glowPen2 = CreatePen(PS_SOLID, 1, glowMid);
        HBRUSH glowBrush2 = CreateSolidBrush(glowMid);
        SelectObject(hdc, glowPen2);
        SelectObject(hdc, glowBrush2);
        Ellipse(hdc, anchorX - hoverRadius - 3, anchorY - hoverRadius - 3,
                anchorX + hoverRadius + 3, anchorY + hoverRadius + 3);
        DeleteObject(glowPen2);
        DeleteObject(glowBrush2);

        HPEN glowPen1 = CreatePen(PS_SOLID, 2, glowInner);
        HBRUSH glowBrush1 = CreateSolidBrush(glowInner);
        SelectObject(hdc, glowPen1);
        SelectObject(hdc, glowBrush1);
        Ellipse(hdc, anchorX - hoverRadius, anchorY - hoverRadius,
                anchorX + hoverRadius, anchorY + hoverRadius);
        DeleteObject(glowPen1);
        DeleteObject(glowBrush1);
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

    HBRUSH bgBrush = CreateSolidBrush(COLOR_BG);
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
