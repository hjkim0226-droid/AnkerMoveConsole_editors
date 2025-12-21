/*****************************************************************************
 * CompUI.h
 *
 * Native Windows UI for Anchor Snap - Composition Editor Module
 * Provides quick composition editing operations
 * Trigger: D → C sequence (D key + C key within 500ms)
 *
 * Features:
 * - Auto Crop: Fit composition to layer bounds
 * - Full Duplicate: Complete comp duplication with nested comps
 * - Duration Extend/Trim: Adjust comp duration (two modes)
 * - Pre-render: Pre-render composition
 *****************************************************************************/

#ifndef COMPUI_H
#define COMPUI_H

namespace CompUI {

// Action types
enum CompAction {
    ACTION_NONE = -1,
    ACTION_AUTO_CROP = 0,
    ACTION_DUPLICATE,
    ACTION_EXTEND,
    ACTION_TRIM,
    ACTION_PRERENDER
};

// Duration mode
enum DurationMode {
    MODE_EXTEND = 0,  // Extend to layer duration
    MODE_TRIM         // Trim to work area
};

// Current composition info (read from AE)
struct CompInfo {
    wchar_t name[256];
    float width;
    float height;
    float duration;
    float frameRate;
    float workAreaStart;
    float workAreaEnd;
};

// Result structure
struct CompResult {
    bool cancelled = false;
    bool applied = false;
    CompAction action = ACTION_NONE;
};

// Initialize the Comp UI system
void Initialize();

// Cleanup
void Shutdown();

// Show the comp panel at specified position
void ShowPanel(int x, int y);

// Hide the panel and return result
CompResult HidePanel();

// Check if panel is visible
bool IsVisible();

// Update hover state (call periodically with mouse position)
void UpdateHover(int mouseX, int mouseY);

// Get the result after panel closes
CompResult GetResult();

// Set current comp info from ExtendScript JSON
void SetCompInfo(const wchar_t* jsonInfo);

// Get selected action
CompAction GetSelectedAction();

} // namespace CompUI

#endif // COMPUI_H
