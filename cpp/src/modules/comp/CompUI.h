/*****************************************************************************
 * CompUI.h (Layer Module)
 *
 * Native Windows UI for Anchor Snap - Layer Editor Module
 * Provides layer-type specific operations
 * Trigger: D → C sequence (D key + C key within 500ms)
 *
 * Layer Types and Features:
 * - Text: Animator presets (Typewriter, Fade, Scale, etc.)
 * - Shape: Animation properties (Trim Path, Repeater, Wiggle)
 * - Solid: Color change, Fit to comp, Transform reset
 * - Null: Transform reset (with parent compensation)
 * - Footage: Loop options, Last frame hold, Transform reset
 * - Camera: Position reset
 * - Light: Position reset
 *****************************************************************************/

#ifndef COMPUI_H
#define COMPUI_H

namespace CompUI {

// Layer types
enum LayerType {
    LAYER_UNKNOWN = -1,
    LAYER_NONE = 0,     // No layer selected
    LAYER_TEXT,         // 1
    LAYER_SHAPE,        // 2
    LAYER_SOLID,        // 3
    LAYER_NULL,         // 4
    LAYER_FOOTAGE,      // 5
    LAYER_CAMERA,       // 6
    LAYER_LIGHT,        // 7
    LAYER_ADJUSTMENT,   // 8 - Adjustment layer (special Solid)
    LAYER_PRECOMP       // 9 - Pre-composition
};

// LayerType bitmasks for ActionDef.allowedTypes
constexpr unsigned int LT_TEXT       = (1 << LAYER_TEXT);       // 0x02
constexpr unsigned int LT_SHAPE      = (1 << LAYER_SHAPE);      // 0x04
constexpr unsigned int LT_SOLID      = (1 << LAYER_SOLID);      // 0x08
constexpr unsigned int LT_NULL       = (1 << LAYER_NULL);       // 0x10
constexpr unsigned int LT_FOOTAGE    = (1 << LAYER_FOOTAGE);    // 0x20
constexpr unsigned int LT_CAMERA     = (1 << LAYER_CAMERA);     // 0x40
constexpr unsigned int LT_LIGHT      = (1 << LAYER_LIGHT);      // 0x80
constexpr unsigned int LT_ADJUSTMENT = (1 << LAYER_ADJUSTMENT); // 0x100
constexpr unsigned int LT_PRECOMP    = (1 << LAYER_PRECOMP);    // 0x200

// Common layer type groups
constexpr unsigned int LT_VISUAL = LT_TEXT | LT_SHAPE | LT_SOLID | LT_FOOTAGE | LT_PRECOMP | LT_NULL;
constexpr unsigned int LT_3D_ONLY = LT_CAMERA | LT_LIGHT;

// Action types (layer-type specific)
enum LayerAction {
    ACTION_NONE = -1,

    // Text layer actions
    ACTION_TEXT_ANIMATOR_TYPEWRITER = 100,
    ACTION_TEXT_ANIMATOR_FADE,
    ACTION_TEXT_ANIMATOR_SCALE,
    ACTION_TEXT_ANIMATOR_BLUR,
    ACTION_TEXT_ANIMATOR_TRACKING,

    // Shape layer actions
    ACTION_SHAPE_TRIM_PATH = 200,
    ACTION_SHAPE_REPEATER,
    ACTION_SHAPE_WIGGLE_PATH,
    ACTION_SHAPE_WIGGLE_TRANSFORM,

    // Solid layer actions
    ACTION_SOLID_CHANGE_COLOR = 300,
    ACTION_SOLID_FIT_TO_COMP,

    // Footage layer actions
    ACTION_FOOTAGE_LOOP_CYCLE = 400,
    ACTION_FOOTAGE_LOOP_PINGPONG,
    ACTION_FOOTAGE_LOOP_OFFSET,
    ACTION_FOOTAGE_LAST_FRAME_HOLD,

    // Precomp layer actions
    ACTION_PRECOMP_UNPRECOMPOSE = 450,
    ACTION_PRECOMP_DEEP_COPY,
    ACTION_PRECOMP_FIT_TO_LAYERS,

    // Common actions (multiple layer types)
    ACTION_RESET_TRANSFORM = 500,       // Position, Scale, Rotation reset
    ACTION_RESET_POSITION,              // Position only (Camera, Light)
    ACTION_RESET_SCALE,
    ACTION_RESET_ROTATION
};

// Action definition for master table (used for dynamic action system)
struct ActionDef {
    const char* id;             // JSON identifier (e.g., "typewriter")
    const wchar_t* label;       // Display label
    const wchar_t* desc;        // Description
    LayerAction action;         // Enum value
    unsigned int allowedTypes;  // Bitmask of allowed LayerTypes
};

// Current layer info (read from AE)
struct LayerInfo {
    wchar_t name[256];
    LayerType type;
    int index;              // Layer index in comp
    bool hasParent;         // Has parent layer
    int parentIndex;        // Parent layer index
    bool isSelected;
    // Solid specific
    unsigned int solidColor;  // RGB color for solid
    // Footage specific
    bool isSequence;        // Image sequence
    bool hasTimeRemap;      // Time remap enabled
};

// Result structure
struct CompResult {
    bool cancelled = false;
    bool applied = false;
    LayerAction action = ACTION_NONE;
    LayerType layerType = LAYER_UNKNOWN;
};

// Initialize the Layer UI system
void Initialize();

// Cleanup
void Shutdown();

// Show the layer panel at specified position
void ShowPanel(int x, int y);

// Hide the panel and return result
CompResult HidePanel();

// Check if panel is visible
bool IsVisible();

// Update hover state (call periodically with mouse position)
void UpdateHover(int mouseX, int mouseY);

// Get the result after panel closes
CompResult GetResult();

// Set current layer info from ExtendScript JSON
void SetLayerInfo(const wchar_t* jsonInfo);

// Get current layer type
LayerType GetCurrentLayerType();

// Get selected action
LayerAction GetSelectedAction();

// Load action configuration from CEP settings file
// Returns true if successfully loaded, false uses defaults
bool LoadActionsFromConfig();

// Get pointer to action definition by ID
const ActionDef* GetActionById(const char* id);

} // namespace CompUI

#endif // COMPUI_H
