/*****************************************************************************
 * ControlUI.h
 *
 * Native Windows UI for Anchor Snap - Control Module
 * Provides effect search and quick apply functionality
 * Mode 1: Search effects (default)
 * Mode 2: Layer effects list (when Effect Controls focused)
 *****************************************************************************/

#ifndef CONTROLUI_H
#define CONTROLUI_H

namespace ControlUI {

// Panel modes
enum PanelMode {
    MODE_SEARCH = 0,    // Default: Search and add effects
    MODE_EFFECTS = 1    // Effect Controls focused: Show layer effects
};

// Search result item
struct EffectItem {
    wchar_t name[128];          // Effect display name
    wchar_t matchName[128];     // Effect match name for applying
    wchar_t category[64];       // Effect category
    int index;                  // Index in results (or effect index on layer)
    bool isLayerEffect;         // True if this is an existing effect on layer
};

// Action for layer effects
enum EffectAction {
    ACTION_NONE = 0,
    ACTION_SELECT,      // Select effect in Effect Controls
    ACTION_DELETE,      // Delete effect from layer
    ACTION_DUPLICATE,   // Duplicate effect
    ACTION_MOVE_UP,     // Move effect up
    ACTION_MOVE_DOWN,   // Move effect down
    ACTION_EXPAND,      // Expand effect (collapse others)
    ACTION_APPLY_PRESET, // Apply preset from quick slot
    ACTION_NEW_EC_WINDOW // Open new locked EC window
};

// Control panel result
struct ControlResult {
    bool cancelled = false;
    bool effectSelected = false;
    EffectItem selectedEffect;
    wchar_t searchQuery[256];
    EffectAction action = ACTION_NONE;
    int effectIndex = -1;       // For layer effect actions
    int presetSlotIndex = -1;   // For preset quick slot (0-2)
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

// Set panel mode before showing
void SetMode(PanelMode mode);

// Get current mode
PanelMode GetMode();

// Show the control panel at mouse position
void ShowPanel();

// Show the control panel at specific position (for Effect Controls tracking)
void ShowPanelAt(int x, int y);

// Hide the control panel
void HidePanel();

// Check if panel is visible
bool IsVisible();

// Get the result after panel closes
ControlResult GetResult();

// Get settings reference
ControlSettings& GetSettings();

// Update search results (Mode 1)
void UpdateSearch(const wchar_t* query);

// Set layer effects list (Mode 2)
// effectList format: "name1|matchName1;name2|matchName2;..."
void SetLayerEffects(const wchar_t* effectList);

// Clear layer effects
void ClearLayerEffects();

} // namespace ControlUI

#endif // CONTROLUI_H
