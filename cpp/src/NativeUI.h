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
  int gridSize = 3;  // 3x3, 5x5, or 7x7
  int cellSize = 40; // pixels per cell
  int spacing = 2;   // pixels between cells (5% gap)
  int margin = 2;    // window margin
};

// Extended menu options (when mouse moves outside grid)
enum ExtendedOption {
  OPT_NONE = 0,       // Normal grid selection
  OPT_SELECTION_MODE, // Top: Toggle Selection/Comp mode
  OPT_CUSTOM_ANCHOR,  // Bottom: Custom anchor position
  OPT_SETTINGS,       // Left: Open settings panel
  OPT_TRANSPARENT     // Right: Toggle transparent mode
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
  bool useCompMode = false;     // false = per-selection, true = whole comp
  bool transparentMode = false; // transparent background
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

} // namespace NativeUI

#endif // NATIVEUI_H
