/*****************************************************************************
 * ControlUI.h
 *
 * Native Windows UI for Anchor Snap - Control Module
 * Provides effect search and quick apply functionality
 *
 * Two modes:
 * - MODE_SEARCH: Search bar + results (Viewer/Timeline context)
 * - MODE_EFFECTS_LIST: Presets + layer effects list (Effect Controls context)
 *****************************************************************************/

#ifndef CONTROLUI_H
#define CONTROLUI_H

#ifdef MSWindows
#include <windows.h>
#endif

namespace ControlUI {

// Panel display modes
enum PanelMode {
    MODE_SEARCH = 0,        // Search bar + search results
    MODE_EFFECTS_LIST = 1   // Presets + layer effects list
};

// Search result item
struct EffectItem {
    wchar_t name[128];          // Effect display name
    wchar_t matchName[128];     // Effect match name for applying
    wchar_t category[64];       // Effect category
    int index;                  // Index in results
    bool enabled;               // Effect enabled state (for layer effects)
};

// Control panel result
struct ControlResult {
    bool cancelled = false;
    bool effectSelected = false;
    EffectItem selectedEffect;
    wchar_t searchQuery[256];
};

// Control panel settings
struct ControlSettings {
    int maxResults = 10;        // Max search results to show
    bool showCategories = true; // Show effect categories
    int presetSlotCount = 3;    // Number of preset slots
};

// Initialize the Control UI system
void Initialize();

// Cleanup
void Shutdown();

// Show the control panel at mouse position with specified mode
void ShowPanel(PanelMode mode = MODE_SEARCH);

// Hide the control panel
void HidePanel();

// Check if panel is visible
bool IsVisible();

// Get the result after panel closes
ControlResult GetResult();

// Get settings reference
ControlSettings& GetSettings();

// Update search results
void UpdateSearch(const wchar_t* query);

// Get current panel mode
PanelMode GetCurrentMode();

#ifdef MSWindows
// Attach to Effect Controls window for position tracking
void AttachToEffectControls(HWND effectControlsWnd);

// Position search bar above the Effect Controls window
void PositionAboveEffectControls();

// Set collapsed state (thin bar mode)
void SetCollapsed(bool collapsed);
#endif

} // namespace ControlUI

#endif // CONTROLUI_H
