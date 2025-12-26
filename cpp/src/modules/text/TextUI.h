/*****************************************************************************
 * TextUI.h
 *
 * Native Windows UI for Anchor Snap - Text Options Module
 * Provides quick text layer property adjustment
 * Trigger: D → T sequence (D key + T key within 500ms)
 *
 * Features:
 * - Drag scrubbing for numeric values (Size, Tracking, Leading, StrokeWidth)
 * - Double-click for direct input (inline edit with style toggle)
 * - Color picker for Fill/Stroke
 * - Font family/style dropdown
 * - Paragraph alignment buttons
 * - Pin button to keep panel open
 * - ESC or click outside to close
 *****************************************************************************/

#ifndef TEXTUI_H
#define TEXTUI_H

namespace TextUI {

// Drag/Edit target values
enum ValueTarget {
    TARGET_NONE = -1,
    TARGET_SIZE = 0,
    TARGET_TRACKING,
    TARGET_LEADING,
    TARGET_STROKE_WIDTH
};

// Paragraph justification (matches AE ParagraphJustification enum)
enum Justification {
    JUST_LEFT = 0,           // LEFT_JUSTIFY
    JUST_CENTER,             // CENTER_JUSTIFY
    JUST_RIGHT,              // RIGHT_JUSTIFY
    JUST_FULL_LEFT,          // FULL_JUSTIFY_LASTLINE_LEFT
    JUST_FULL_CENTER,        // FULL_JUSTIFY_LASTLINE_CENTER
    JUST_FULL_RIGHT,         // FULL_JUSTIFY_LASTLINE_RIGHT
    JUST_FULL_FULL           // FULL_JUSTIFY_LASTLINE_FULL
};

// Current text layer information (read from AE)
struct TextInfo {
    wchar_t font[128];
    wchar_t fontStyle[64];
    float fontSize;
    float tracking;
    float leading;          // 0 = Auto
    float strokeWidth;
    float fillColor[3];     // RGB 0-1
    float strokeColor[3];   // RGB 0-1
    bool applyFill;
    bool applyStroke;
    Justification justify;
    wchar_t layerName[256];
};

// Result structure
struct TextResult {
    bool cancelled = false;
    bool applied = false;
};

// Initialize the Text UI system
void Initialize();

// Cleanup
void Shutdown();

// Show the text panel at specified position
void ShowPanel(int x, int y);

// Hide the panel and return result
TextResult HidePanel();

// Check if panel is visible
bool IsVisible();

// Update hover state (call periodically with mouse position)
void UpdateHover(int mouseX, int mouseY);

// Get the result after panel closes
TextResult GetResult();

// Set current text info from ExtendScript JSON
void SetTextInfo(const wchar_t* jsonInfo);

// Refresh request - returns true if refresh is needed, then clears the flag
bool NeedsRefresh();

// Color picker
void ShowColorPicker(bool forStroke, int x, int y);
void HideColorPicker();
bool IsColorPickerVisible();

} // namespace TextUI

#endif // TEXTUI_H
