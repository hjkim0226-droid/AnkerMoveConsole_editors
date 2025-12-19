/*****************************************************************************
 * KeyframeUI.h
 *
 * Native Windows UI for Anchor Snap - Keyframe Module
 * Provides velocity-based keyframe easing with visual curve editor
 * Trigger: K key 0.4s hold
 *****************************************************************************/

#ifndef KEYFRAMEUI_H
#define KEYFRAMEUI_H

namespace KeyframeUI {

// Velocity curve presets
enum VelocityPreset {
    PRESET_LINEAR = 0,      // Constant velocity
    PRESET_EASE_IN,         // Accelerating (slow to fast)
    PRESET_EASE_OUT,        // Decelerating (fast to slow)
    PRESET_EASE_IN_OUT,     // Slow-fast-slow
    PRESET_EASE_OUT_IN,     // Fast-slow-fast (bounce-like)
    PRESET_CUSTOM           // User-defined curve
};

// Bezier control points for velocity curve
// Normalized to [0,1] range
struct VelocityCurve {
    float p0_x, p0_y;  // First control point (after start)
    float p1_x, p1_y;  // Second control point (before end)
};

// Keyframe info from After Effects
struct KeyframeInfo {
    wchar_t propName[128];          // Property display name
    wchar_t propMatchName[128];     // Property match name
    int keyIndex1;                  // First selected keyframe index
    int keyIndex2;                  // Second selected keyframe index
    float time1, time2;             // Keyframe times
    float value1, value2;           // Keyframe values
    // Current easing
    float outSpeed, outInfluence;   // Out ease at key1
    float inSpeed, inInfluence;     // In ease at key2
};

// Result after panel closes
struct KeyframeResult {
    bool cancelled = false;         // True if ESC or clicked outside
    bool applied = false;           // True if easing should be applied
    VelocityPreset preset = PRESET_LINEAR;
    VelocityCurve customCurve;

    // Calculated AE KeyframeEase values
    float outSpeed = 0.0f;          // Speed at first keyframe
    float outInfluence = 33.33f;    // Influence at first keyframe (%)
    float inSpeed = 0.0f;           // Speed at second keyframe
    float inInfluence = 33.33f;     // Influence at second keyframe (%)

    // Display values (from integration)
    float velocityAtStart = 0.0f;   // Velocity at curve start
    float velocityAtEnd = 0.0f;     // Velocity at curve end
    float accelerationMax = 0.0f;   // Maximum acceleration

    // Preset slot for save/load
    int presetSlot = -1;            // -1 if not using preset slot
};

// Keyframe module settings
struct KeyframeSettings {
    int slotCount = 3;              // Number of preset slots
    bool showVelocityGraph = true;  // Show velocity visualization
    bool showNumericValues = true;  // Show speed/influence numbers
};

// Initialize the Keyframe UI system
void Initialize();

// Cleanup
void Shutdown();

// Show the keyframe panel at specified position
// Returns immediately, panel shown modeless
void ShowPanel(int screenX, int screenY);

// Update hover state (call on mouse move)
void UpdateHover(int screenX, int screenY);

// Hide panel and get result
KeyframeResult HidePanel();

// Get result (for toggle mode - call after IsVisible() becomes false)
KeyframeResult GetResult();

// Check if panel is visible
bool IsVisible();

// Set keyframe info from After Effects
// infoJson format: JSON array from getSelectedKeyframeInfo()
void SetKeyframeInfo(const wchar_t* infoJson);

// Get current curve values (for preview)
VelocityCurve GetCurrentCurve();

// Calculate AE ease values from velocity curve
// Uses Simpson's rule integration
void CalculateAEEase(const VelocityCurve& curve,
                     float& outSpeed, float& outInfluence,
                     float& inSpeed, float& inInfluence);

// Preset management
void SavePresetToSlot(int slot, const VelocityCurve& curve);
bool LoadPresetFromSlot(int slot, VelocityCurve& curve);
bool PresetSlotExists(int slot);

// Get settings reference
KeyframeSettings& GetSettings();

} // namespace KeyframeUI

#endif // KEYFRAMEUI_H
