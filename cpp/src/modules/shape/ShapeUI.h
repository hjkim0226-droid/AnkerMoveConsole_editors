/*****************************************************************************
 * ShapeUI.h
 *
 * Native Windows UI for Anchor Snap - Shape Layer Module
 * Provides quick shape layer property adjustment
 * Trigger: D → S sequence (D key + S key within 500ms)
 *
 * Features:
 * - Appearance: Fill/Stroke color, Width, Opacity
 * - Transform: Size W/H, Roundness, Shape Anchor (mini grid via Y key)
 * - Path Operations: Trim, Repeater, Wiggle, Offset, etc.
 * - Color presets save/load
 * - Accordion sections for organized UI
 * - Pin button to keep panel open
 * - ESC or click outside to close
 *****************************************************************************/

#ifndef SHAPEUI_H
#define SHAPEUI_H

namespace ShapeUI {

// UI Sections (accordion)
enum Section {
    SECTION_APPEARANCE = 0,
    SECTION_TRANSFORM,
    SECTION_PATH_OPS,
    SECTION_COUNT
};

// Drag/Edit target values
enum ValueTarget {
    TARGET_NONE = -1,
    TARGET_STROKE_WIDTH = 0,
    TARGET_OPACITY,
    TARGET_SIZE_W,
    TARGET_SIZE_H,
    TARGET_ROUNDNESS,
    TARGET_COUNT
};

// Path operation types
enum PathOpType {
    PATH_OP_TRIM = 0,
    PATH_OP_REPEATER,
    PATH_OP_WIGGLE_PATH,
    PATH_OP_WIGGLE_TRANSFORM,
    PATH_OP_OFFSET,
    PATH_OP_ROUND_CORNERS,
    PATH_OP_ZIGZAG,
    PATH_OP_TWIST,
    PATH_OP_PUCKER_BLOAT,
    PATH_OP_COUNT
};

// Shape group information (read from AE)
struct ShapeInfo {
    wchar_t layerName[256];
    wchar_t shapeName[128];      // Shape group name (e.g., "Rectangle 1")
    wchar_t shapeType[64];       // "Rectangle", "Ellipse", "Path", etc.

    // Appearance
    float fillColor[3];          // RGB 0-1
    float strokeColor[3];        // RGB 0-1
    float strokeWidth;
    float opacity;               // 0-100
    bool hasFill;
    bool hasStroke;

    // Transform (for parametric shapes)
    float sizeW;                 // Width
    float sizeH;                 // Height
    float roundness;             // Corner roundness
    float anchorX;               // Shape anchor X
    float anchorY;               // Shape anchor Y
    float rotation;              // Shape rotation

    // Bounds (for anchor calculation)
    float boundsLeft;
    float boundsTop;
    float boundsWidth;
    float boundsHeight;

    // State
    bool isParametric;           // true = Rectangle/Ellipse, false = Path
    bool sizeLinkEnabled;        // W/H proportional link
};

// Result structure
struct ShapeResult {
    bool cancelled = false;
    bool applied = false;
};

// Color preset
struct ColorPreset {
    float fillColor[3];
    float strokeColor[3];
    float strokeWidth;
    bool hasFill;
    bool hasStroke;
};

// Initialize the Shape UI system
void Initialize();

// Cleanup
void Shutdown();

// Show the shape panel at specified position
void ShowPanel(int x, int y);

// Hide the panel and return result
ShapeResult HidePanel();

// Check if panel is visible
bool IsVisible();

// Update hover state (call periodically with mouse position)
void UpdateHover(int mouseX, int mouseY);

// Get the result after panel closes
ShapeResult GetResult();

// Set current shape info from ExtendScript JSON
void SetShapeInfo(const wchar_t* jsonInfo);

// Refresh request - returns true if refresh is needed, then clears the flag
bool NeedsRefresh();

// Color picker
void ShowColorPicker(bool forStroke, int x, int y);
void HideColorPicker();
bool IsColorPickerVisible();

// Mini anchor grid (triggered by Y key while panel is open)
void ShowAnchorGrid(int x, int y);
void HideAnchorGrid();
bool IsAnchorGridVisible();

// Section toggle
void ToggleSection(Section section);
bool IsSectionExpanded(Section section);

} // namespace ShapeUI

#endif // SHAPEUI_H
