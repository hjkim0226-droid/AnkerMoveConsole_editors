/*****************************************************************************
 * NativeUI.h
 *
 * Native Windows UI for Anchor Grid
 * Provides a custom-drawn popup window for the grid interface
 *****************************************************************************/

#ifndef NATIVEUI_H
#define NATIVEUI_H

namespace NativeUI {

// Grid configuration
struct GridConfig {
  int gridWidth = 3;  // Horizontal cells (3-7)
  int gridHeight = 3; // Vertical cells (3-7)
  int cellSize = 40;  // pixels per cell
  int spacing = 4;    // pixels between cells (4px gap)
  int margin = 2;     // window margin
};

// Extended menu options (side panels)
enum ExtendedOption {
  OPT_NONE = 0, // Normal grid selection
  // Left side: Custom anchor presets
  OPT_CUSTOM_1, // Custom anchor preset 1
  OPT_CUSTOM_2, // Custom anchor preset 2
  OPT_CUSTOM_3, // Custom anchor preset 3
  // Right side: Mode controls
  OPT_COMP_MODE, // Toggle Composition mode
  OPT_MASK_MODE, // Toggle Mask recognition
  OPT_SETTINGS,  // Open settings panel
  // Bottom: Extended features
  OPT_COPY_ANCHOR, // Copy anchor ratio
  OPT_PASTE_ANCHOR // Paste anchor ratio
};

// Grid result
struct GridResult {
  int gridX = -1;
  int gridY = -1;
  bool cancelled = false;
  ExtendedOption extendedOption = OPT_NONE;
};

// Current mode settings
struct GridSettings {
  bool useCompMode = false;       // false = per-selection, true = whole comp
  bool useMaskRecognition = true; // true = use mask bounds
  bool transparentMode = false;   // transparent background
  float gridOpacity = 0.75f;      // Grid background opacity
  float cellOpacity = 0.50f;      // Cell background opacity
  // Custom anchor presets (x, y ratios 0-1)
  float customAnchor1X = 0.0f, customAnchor1Y = 0.0f;
  float customAnchor2X = 0.5f, customAnchor2Y = 0.0f;
  float customAnchor3X = 1.0f, customAnchor3Y = 0.0f;
};

// Initialize native UI system
bool Initialize();

// Cleanup native UI system
void Cleanup();

// Show grid at mouse position
void ShowGrid(int mouseX, int mouseY, const GridConfig &config);

// Hide grid and get the result based on current mouse position
GridResult HideGrid(int mouseX, int mouseY);

// Check if grid is currently visible
bool IsGridVisible();

// Update hover state based on mouse position (call from IdleHook)
void UpdateHover(int mouseX, int mouseY);

// Get current hover cell
void GetHoverCell(int *outX, int *outY);

// Get/Set settings
GridSettings &GetSettings();

// Clipboard anchor functions for Copy/Paste
bool HasClipboardAnchor();
void GetClipboardAnchor(float *outX, float *outY);
void SetClipboardAnchor(float x, float y);

} // namespace NativeUI

#endif // NATIVEUI_H
