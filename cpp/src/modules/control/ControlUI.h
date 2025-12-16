/*****************************************************************************
 * ControlUI.h
 *
 * Native Windows UI for Anchor Snap - Control Module
 * Provides effect search and quick apply functionality
 *****************************************************************************/

#ifndef CONTROLUI_H
#define CONTROLUI_H

namespace ControlUI {

// Search result item
struct EffectItem {
    wchar_t name[128];          // Effect display name
    wchar_t matchName[128];     // Effect match name for applying
    wchar_t category[64];       // Effect category
    int index;                  // Index in results
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
    int slotCount = 6;          // Number of quick slots
};

// Initialize the Control UI system
void Initialize();

// Cleanup
void Shutdown();

// Show the control panel at mouse position
void ShowPanel();

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

} // namespace ControlUI

#endif // CONTROLUI_H
