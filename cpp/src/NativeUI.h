/*****************************************************************************
 * NativeUI.h
 *
 * Native Windows UI for Anchor Grid
 * Provides a custom-drawn popup window for the grid interface
 *****************************************************************************/

#ifndef NATIVEUI_H
#define NATIVEUI_H

#ifdef MSWindows
#include <windows.h>
#endif

namespace NativeUI {

// Grid configuration
struct GridConfig {
  int gridSize = 3;  // 3x3, 5x5, or 7x7
  int cellSize = 40; // pixels per cell
  int spacing = 1;   // pixels between cells
  int margin = 2;    // window margin
};

// Grid result
struct GridResult {
  int gridX = -1;
  int gridY = -1;
  bool cancelled = false;
};

// Initialize native UI system
bool Initialize();

// Cleanup native UI system
void Cleanup();

// Show grid at mouse position, returns selected cell
// Window stays open until HideGrid() is called
void ShowGrid(int mouseX, int mouseY, const GridConfig &config);

// Hide grid and get the result based on current mouse position
GridResult HideGrid(int mouseX, int mouseY);

// Check if grid is currently visible
bool IsGridVisible();

// Update hover state based on mouse position (call from IdleHook)
void UpdateHover(int mouseX, int mouseY);

// Get current hover cell
void GetHoverCell(int *outX, int *outY);

} // namespace NativeUI

#endif // NATIVEUI_H
