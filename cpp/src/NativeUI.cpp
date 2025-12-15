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
// Grid offset for centering smaller grids within max dimension area
static int g_gridOffsetX = 0;
static int g_gridOffsetY = 0;

// Copy/Paste anchor clipboard
static bool g_hasClipboardAnchor = false;
static float g_clipboardAnchorX = 0.5f;
static float g_clipboardAnchorY = 0.5f;

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

  // FIXED window size based on 3x3 grid with current scale
  // This stays constant regardless of actual grid dimensions
  int cellTotal = g_config.cellSize + g_config.spacing;
  int fixedGridPixels = 3 * cellTotal; // Always 3x3 for window frame

  // Actual grid pixels (may vary based on width/height settings)
  int gridPixelsW = g_config.gridWidth * cellTotal;
  int gridPixelsH = g_config.gridHeight * cellTotal;

  // Window: FIXED size based on 3x3
  g_windowWidth = SIDE_PANEL_WIDTH + fixedGridPixels + g_config.margin * 2 +
                  SIDE_PANEL_WIDTH;

  int minHeight = ICON_SIZE * 3 + ICON_SPACING * 2 + 20;
  int bottomButtonsHeight = ICON_SIZE + 10;
  int gridAreaHeight = fixedGridPixels + g_config.margin * 2;
  int baseHeight = (gridAreaHeight > minHeight) ? gridAreaHeight : minHeight;
  g_windowHeight = baseHeight + bottomButtonsHeight;

  // Grid offset: center the actual grid within the fixed 3x3 area
  g_gridOffsetX = (fixedGridPixels - gridPixelsW) / 2;
  g_gridOffsetY = (fixedGridPixels - gridPixelsH) / 2;

  // Mouse at WINDOW CENTER (not grid center)
  g_windowX = mouseX - g_windowWidth / 2;
  g_windowY = mouseY - (g_windowHeight - bottomButtonsHeight) / 2;

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

bool HasClipboardAnchor() { return g_hasClipboardAnchor; }

void GetClipboardAnchor(float *outX, float *outY) {
  if (outX)
    *outX = g_clipboardAnchorX;
  if (outY)
    *outY = g_clipboardAnchorY;
}

void SetClipboardAnchor(float x, float y) {
  g_hasClipboardAnchor = true;
  g_clipboardAnchorX = x;
  g_clipboardAnchorY = y;
}

void InvalidateGrid() {
  if (g_gridWnd && IsWindow(g_gridWnd)) {
    InvalidateRect(g_gridWnd, NULL, FALSE);
  }
}

ExtendedOption GetHoverExtOption() { return g_hoverExtOption; }

} // namespace NativeUI

// Calculate hover cell or extended option from screen coordinates
static void UpdateHoverFromMouse(int screenX, int screenY) {
  int cellTotal = g_config.cellSize + g_config.spacing;
  int gridPixelsW = g_config.gridWidth * cellTotal;
  int gridPixelsH = g_config.gridHeight * cellTotal;
  // Apply grid offset for centered rectangular grids
  int gridStartX = SIDE_PANEL_WIDTH + g_config.margin + g_gridOffsetX;
  int gridStartY = g_config.margin + g_gridOffsetY;

  int relX = screenX - g_windowX;
  int relY = screenY - g_windowY;

  g_hoverCellX = -1;
  g_hoverCellY = -1;
  g_hoverExtOption = NativeUI::OPT_NONE;

  // Check left panel (Custom Anchors only)
  if (relX < SIDE_PANEL_WIDTH && relX >= 0) {
    int bottomButtonsHeight = ICON_SIZE + 10;
    int gridAreaHeight = g_windowHeight - bottomButtonsHeight;
    int iconY = (gridAreaHeight - (ICON_SIZE * 3 + ICON_SPACING * 2)) / 2;

    // Check custom anchor icons
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

  // Check right panel (Mode controls) - limit to panel width only
  if (relX >= g_windowWidth - SIDE_PANEL_WIDTH && relX < g_windowWidth) {
    int bottomButtonsHeight = ICON_SIZE + 10;
    int gridAreaHeight = g_windowHeight - bottomButtonsHeight;
    int iconY = (gridAreaHeight - (ICON_SIZE * 3 + ICON_SPACING * 2)) / 2;
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

  // Check Copy/Paste buttons (below grid area, centered in window)
  int bottomButtonsHeight = ICON_SIZE + 10;
  // Use FIXED 3x3 grid size for button positioning (same as window layout)
  int fixedGridPixels = 3 * cellTotal;
  // Center buttons in the window (not based on actual grid size)
  int windowCenterX = SIDE_PANEL_WIDTH + g_config.margin + fixedGridPixels / 2;
  int gridBottomY = g_config.margin + fixedGridPixels + ICON_SIZE / 2 + 5;

  if (relY >= gridBottomY - ICON_SIZE / 2 &&
      relY < gridBottomY + ICON_SIZE / 2) {
    int copyX = windowCenterX - ICON_SIZE / 2 - 5;
    int pasteX = windowCenterX + ICON_SIZE / 2 + 5;

    // Copy button
    if (relX >= copyX - ICON_SIZE / 2 && relX < copyX + ICON_SIZE / 2) {
      g_hoverExtOption = NativeUI::OPT_COPY_ANCHOR;
      return;
    }
    // Paste button
    if (relX >= pasteX - ICON_SIZE / 2 && relX < pasteX + ICON_SIZE / 2) {
      g_hoverExtOption = NativeUI::OPT_PASTE_ANCHOR;
      return;
    }
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
    // Default: dark gray, hover: blue (like selection mode)
    COLORREF colorRef =
        hover ? COLOR_ICON_HOVER : (active ? COLOR_BLUE : COLOR_DARK_GRAY);
    Color color = toColor(colorRef);
    // Use hover background color when hovering, otherwise transparent key
    COLORREF bgColorRef = hover ? RGB(50, 60, 70) : COLOR_BG;
    Color bgColor = toColor(bgColorRef);
    Pen pen(color, 2.0f);
    SolidBrush bgBrush(bgColor);

    // +10% larger overall
    int iconR = (int)(r * 1.1f); // 10% larger radius

    // Crosshair lines (with gap for center circle)
    int gap = 9; // Slightly larger gap
    graphics.DrawLine(&pen, cx - iconR, cy, cx - gap, cy);
    graphics.DrawLine(&pen, cx + gap, cy, cx + iconR, cy);
    graphics.DrawLine(&pen, cx, cy - iconR, cx, cy - gap);
    graphics.DrawLine(&pen, cx, cy + gap, cx, cy + iconR);

    // Center circle with fill (AE anchor style) - 10% larger
    int circleR = 9; // Increased from 8 to 9 (10% larger)
    graphics.FillEllipse(&bgBrush, cx - circleR, cy - circleR, circleR * 2,
                         circleR * 2);
    graphics.DrawEllipse(&pen, cx - circleR, cy - circleR, circleR * 2,
                         circleR * 2);

    // Draw preset number inside circle - 20% larger font
    FontFamily fontFamily(L"Segoe UI");
    Font font(&fontFamily, 12, FontStyleBold, UnitPixel);
    SolidBrush textBrush(color);
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    wchar_t num[2] = {
        static_cast<wchar_t>(L'1' + (type - NativeUI::OPT_CUSTOM_1)), 0};
    RectF textRect((REAL)(cx - circleR), (REAL)(cy - circleR),
                   (REAL)(circleR * 2), (REAL)(circleR * 2));
    graphics.DrawString(num, 1, &font, textRect, &format, &textBrush);
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
    // Use hover background color when hovering for circle cutout
    COLORREF bgColorRef = hover ? RGB(50, 60, 70) : COLOR_BG;
    Color bgColor = toColor(bgColorRef);
    SolidBrush bgBrush(bgColor);

    // Horizontal rectangle (20% taller - 10% more than before)
    int w = r + 3;
    int h = (int)((r - 3) * 1.2f); // 20% taller total
    graphics.FillRectangle(&brush, cx - w, cy - h, w * 2, h * 2);
    // Circle cutout (30% larger than original)
    int circleR = (int)(h / 2 * 1.3f); // 30% larger
    graphics.FillEllipse(&bgBrush, cx - circleR, cy - circleR, circleR * 2,
                         circleR * 2);
    break;
  }

  case NativeUI::OPT_SETTINGS: {
    // Default: dark gray, hover: blue
    COLORREF colorRef = hover ? COLOR_BLUE : COLOR_DARK_GRAY;
    Color color = toColor(colorRef);
    SolidBrush brush(color);

    // User-style gear: 6 wide teeth, large center hole - 40% larger
    int baseR = (int)(r * 1.4f);        // 40% larger
    int outerR = baseR;                 // Outer radius (teeth tips)
    int midR = (int)(baseR * 0.7f);     // Mid radius (teeth base) - 70%
    int centerR = (int)(baseR * 0.35f); // Center hole - 35%

    // Draw gear using polygon with 24 points (6 teeth, 4 points each)
    Point gearPoints[24];
    for (int i = 0; i < 6; i++) {
      double baseAngle = i * 3.14159 * 2. / 6. - 3.14159 / 2.; // Start from top
      double toothWidth = 3.14159 / 7.5;                       // Wider teeth

      // Tooth left edge (inner to outer)
      gearPoints[i * 4 + 0].X = cx + (int)(midR * cos(baseAngle - toothWidth));
      gearPoints[i * 4 + 0].Y = cy + (int)(midR * sin(baseAngle - toothWidth));
      // Tooth left tip
      gearPoints[i * 4 + 1].X =
          cx + (int)(outerR * cos(baseAngle - toothWidth / 2));
      gearPoints[i * 4 + 1].Y =
          cy + (int)(outerR * sin(baseAngle - toothWidth / 2));
      // Tooth right tip
      gearPoints[i * 4 + 2].X =
          cx + (int)(outerR * cos(baseAngle + toothWidth / 2));
      gearPoints[i * 4 + 2].Y =
          cy + (int)(outerR * sin(baseAngle + toothWidth / 2));
      // Tooth right edge (outer to inner)
      gearPoints[i * 4 + 3].X = cx + (int)(midR * cos(baseAngle + toothWidth));
      gearPoints[i * 4 + 3].Y = cy + (int)(midR * sin(baseAngle + toothWidth));
    }
    graphics.FillPolygon(&brush, gearPoints, 24);

    // Fill inner body circle
    graphics.FillEllipse(&brush, cx - midR, cy - midR, midR * 2, midR * 2);

    // Cut out center circle (use bg color)
    COLORREF bgColorRef = hover ? RGB(50, 60, 70) : COLOR_BG;
    Color bgColor = toColor(bgColorRef);
    SolidBrush bgBrush(bgColor);
    graphics.FillEllipse(&bgBrush, cx - centerR, cy - centerR, centerR * 2,
                         centerR * 2);
    break;
  }

  case NativeUI::OPT_COPY_ANCHOR: {
    // Orange if has clipboard data, blue if hover, else dark gray
    COLORREF colorRef =
        hover ? COLOR_ICON_HOVER
              : (g_hasClipboardAnchor ? COLOR_ORANGE : COLOR_DARK_GRAY);
    Color color = toColor(colorRef);
    Pen pen(color, 2.0f);
    SolidBrush brush(color);

    // Two overlapping rectangles - 25% larger
    int size = (int)(r * 1.25f) - 2;
    int offset = 4;
    // Back rectangle (outline only)
    graphics.DrawRectangle(&pen, cx - size + offset, cy - size + offset,
                           size * 2 - offset * 2, size * 2 - offset * 2);
    // Front rectangle (filled)
    graphics.FillRectangle(&brush, cx - size, cy - size, size * 2 - offset * 2,
                           size * 2 - offset * 2);
    break;
  }

  case NativeUI::OPT_PASTE_ANCHOR: {
    // Dim if no clipboard data
    COLORREF colorRef =
        hover ? COLOR_ICON_HOVER
              : (g_hasClipboardAnchor ? COLOR_BLUE : COLOR_DARK_GRAY);
    Color color = toColor(colorRef);
    Pen pen(color, 2.0f);
    SolidBrush brush(color);

    // Clipboard icon - smaller (was 1.2, now ~1.0)
    int w = (int)(r * 1.0f) - 2;
    int h = (int)(r * 1.0f) + 2;
    // Main board
    graphics.DrawRectangle(&pen, cx - w, cy - h + 4, w * 2, h * 2 - 4);
    // Clip at top
    int clipW = w / 2;
    graphics.FillRectangle(&brush, cx - clipW, cy - h, clipW * 2, 6);
    break;
  }

  default:
    break;
  }
}

// Draw side panels with icons
static void DrawSidePanels(HDC hdc) {
  // Calculate icon Y based on grid area only (exclude bottom buttons)
  int bottomButtonsHeight = ICON_SIZE + 10;
  int gridAreaHeight = g_windowHeight - bottomButtonsHeight;
  int iconY = (gridAreaHeight - (ICON_SIZE * 3 + ICON_SPACING * 2)) / 2;
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

  // Copy/Paste buttons below grid (centered based on FIXED 3x3 grid size)
  int cellTotal = g_config.cellSize + g_config.spacing;
  int fixedGridPixels = 3 * cellTotal; // Always 3x3 for consistent positioning
  int windowCenterX = SIDE_PANEL_WIDTH + g_config.margin + fixedGridPixels / 2;
  int gridBottomY = g_config.margin + fixedGridPixels + ICON_SIZE / 2 + 5;

  // Copy icon
  bool copyHover = (g_hoverExtOption == NativeUI::OPT_COPY_ANCHOR);
  DrawIcon(hdc, windowCenterX - ICON_SIZE / 2 - 5, gridBottomY,
           NativeUI::OPT_COPY_ANCHOR, copyHover, false);

  // Paste icon
  bool pasteHover = (g_hoverExtOption == NativeUI::OPT_PASTE_ANCHOR);
  DrawIcon(hdc, windowCenterX + ICON_SIZE / 2 + 5, gridBottomY,
           NativeUI::OPT_PASTE_ANCHOR, pasteHover, g_hasClipboardAnchor);
}

// Draw the grid with marks and glow using GDI+
static void DrawGrid(HDC hdc) {
  using namespace Gdiplus;

  Graphics graphics(hdc);
  graphics.SetSmoothingMode(SmoothingModeNone);
  graphics.SetPixelOffsetMode(PixelOffsetModeHalf);

  // Use cellSize directly (no spacing - grid lines will separate)
  int cellTotal = g_config.cellSize; // No spacing now
  // Apply grid offset for centered rectangular grids
  int gridStartX = SIDE_PANEL_WIDTH + g_config.margin + g_gridOffsetX;
  int gridStartY = g_config.margin + g_gridOffsetY;
  int radius = g_config.cellSize / 10;     // Slightly larger dots
  int hoverRadius = g_config.cellSize / 8; // Larger hover glow
  int len = (int)(cellTotal * 0.3);        // Shorter marks (was 0.4)

  bool compMode = g_settings.useCompMode;

  // Helper to convert COLORREF to GDI+ Color
  auto toColor = [](COLORREF c) {
    return Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
  };

  Color cellBgColor = toColor(COLOR_CELL_BG);

  // Apply gridOpacity (mark opacity) to line/mark colors
  BYTE markAlpha = (BYTE)(g_settings.gridOpacity * 255 / 100);
  COLORREF lineColorRef = compMode ? COLOR_GRID_LINE_COMP : COLOR_GRID_LINE;
  Color lineColor(markAlpha, GetRValue(lineColorRef), GetGValue(lineColorRef),
                  GetBValue(lineColorRef));

  COLORREF glowInnerRef = compMode ? COLOR_GLOW_INNER_COMP : COLOR_GLOW_INNER;
  Color glowInnerColor(255, GetRValue(glowInnerRef), GetGValue(glowInnerRef),
                       GetBValue(glowInnerRef)); // Glow stays full
  COLORREF glowMidRef = compMode ? COLOR_GLOW_MID_COMP : COLOR_GLOW_MID;
  Color glowMidColor(255, GetRValue(glowMidRef), GetGValue(glowMidRef),
                     GetBValue(glowMidRef));
  COLORREF glowOuterRef = compMode ? COLOR_GLOW_OUTER_COMP : COLOR_GLOW_OUTER;
  Color glowOuterColor(255, GetRValue(glowOuterRef), GetGValue(glowOuterRef),
                       GetBValue(glowOuterRef));

  // Draw cell backgrounds with opacity
  BYTE cellAlpha = (BYTE)(g_settings.cellOpacity * 255 / 100);
  Color cellBgWithAlpha(cellAlpha, GetRValue(COLOR_CELL_BG),
                        GetGValue(COLOR_CELL_BG), GetBValue(COLOR_CELL_BG));
  SolidBrush cellBrush(cellBgWithAlpha);

  // Fill entire grid area first
  int gridWidth = g_config.gridWidth * cellTotal;
  int gridHeight = g_config.gridHeight * cellTotal;
  graphics.FillRectangle(&cellBrush, gridStartX, gridStartY, gridWidth,
                         gridHeight);

  // Draw gray grid lines (separators between cells)
  Color gridLineColor(180, 80, 80, 80); // Semi-transparent gray
  Pen gridPen(gridLineColor, 1.0f);

  // Vertical lines
  for (int x = 1; x < g_config.gridWidth; x++) {
    int lineX = gridStartX + x * cellTotal;
    graphics.DrawLine(&gridPen, lineX, gridStartY, lineX,
                      gridStartY + gridHeight);
  }
  // Horizontal lines
  for (int y = 1; y < g_config.gridHeight; y++) {
    int lineY = gridStartY + y * cellTotal;
    graphics.DrawLine(&gridPen, gridStartX, lineY, gridStartX + gridWidth,
                      lineY);
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

      // Reduced offset to move marks closer to center (was cellTotal/4)
      int edgeOffset = cellTotal / 6;
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

      // Different lengths for corner, edge, and center marks
      int cornerLen = (int)(cellTotal * 0.35); // Longer for corners
      int edgeLen = (int)(cellTotal * 0.25);   // Shorter for edges
      int centerLen = (int)(cellTotal * 0.25); // Same as edges for center

      Color markColor = isHover ? glowInnerColor : lineColor;
      Pen linePen(markColor, 2.0f);

      if (isCorner) {
        int L = cornerLen;
        if (isTop && isLeft) {
          graphics.DrawLine(&linePen, markX, markY + L, markX, markY);
          graphics.DrawLine(&linePen, markX, markY, markX + L, markY);
        } else if (isTop && isRight) {
          graphics.DrawLine(&linePen, markX - L, markY, markX, markY);
          graphics.DrawLine(&linePen, markX, markY, markX, markY + L);
        } else if (isBottom && isLeft) {
          graphics.DrawLine(&linePen, markX, markY - L, markX, markY);
          graphics.DrawLine(&linePen, markX, markY, markX + L, markY);
        } else {
          graphics.DrawLine(&linePen, markX - L, markY, markX, markY);
          graphics.DrawLine(&linePen, markX, markY, markX, markY - L);
        }
      } else if (isEdge) {
        int L = edgeLen;
        if (isTop) {
          graphics.DrawLine(&linePen, markX - L, markY, markX + L, markY);
          graphics.DrawLine(&linePen, markX, markY, markX, markY + L);
        } else if (isBottom) {
          graphics.DrawLine(&linePen, markX - L, markY, markX + L, markY);
          graphics.DrawLine(&linePen, markX, markY - L, markX, markY);
        } else if (isLeft) {
          graphics.DrawLine(&linePen, markX, markY - L, markX, markY + L);
          graphics.DrawLine(&linePen, markX, markY, markX + L, markY);
        } else {
          graphics.DrawLine(&linePen, markX, markY - L, markX, markY + L);
          graphics.DrawLine(&linePen, markX - L, markY, markX, markY);
        }
      } else {
        int L = centerLen;
        graphics.DrawLine(&linePen, cx - L, cy, cx + L, cy);
        graphics.DrawLine(&linePen, cx, cy - L, cx, cy + L);
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
