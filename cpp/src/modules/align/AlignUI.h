/*****************************************************************************
 * AlignUI.h
 *
 * Native Windows UI for Anchor Snap - Align/Distribute Module
 * Provides quick layer alignment and distribution functionality
 * Trigger: D → A sequence (D key + A key within 500ms)
 *
 * Modes:
 * - Function: Align (6 directions) / Distribute (H/V)
 * - Reference: Selection (layer-based) / Composition (comp bounds)
 *****************************************************************************/

#ifndef ALIGNUI_H
#define ALIGNUI_H

namespace AlignUI {

// Function mode (left side of header)
enum FunctionMode {
    FUNC_ALIGN = 0,
    FUNC_DISTRIBUTE
};

// Reference mode (right side of header)
enum ReferenceMode {
    REF_SELECTION = 0,
    REF_COMPOSITION
};

// Align direction (6 buttons)
enum AlignDirection {
    ALIGN_LEFT = 0,
    ALIGN_CENTER_H,
    ALIGN_RIGHT,
    ALIGN_TOP,
    ALIGN_MIDDLE_V,
    ALIGN_BOTTOM
};

// Distribute direction (2 buttons)
enum DistributeDirection {
    DIST_HORIZONTAL = 0,
    DIST_VERTICAL
};

// Result structure
struct AlignResult {
    bool cancelled = false;
    bool applied = false;
    FunctionMode funcMode = FUNC_ALIGN;
    ReferenceMode refMode = REF_SELECTION;
    AlignDirection alignDir = ALIGN_LEFT;      // Valid when FUNC_ALIGN
    DistributeDirection distDir = DIST_HORIZONTAL; // Valid when FUNC_DISTRIBUTE
};

// Initialize the Align UI system
void Initialize();

// Cleanup
void Shutdown();

// Show the align panel at specified position
void ShowPanel(int x, int y);

// Hide the panel and return result
AlignResult HidePanel();

// Check if panel is visible
bool IsVisible();

// Update hover state (call periodically with mouse position)
void UpdateHover(int mouseX, int mouseY);

// Get the result after panel closes
AlignResult GetResult();

// Get current function mode
FunctionMode GetFunctionMode();

// Get current reference mode
ReferenceMode GetReferenceMode();

// Set function mode
void SetFunctionMode(FunctionMode mode);

// Set reference mode
void SetReferenceMode(ReferenceMode mode);

} // namespace AlignUI

#endif // ALIGNUI_H
