/*****************************************************************************
 * KeyframeUI.cpp
 *
 * Native Windows UI for Anchor Snap - Keyframe Module
 * Velocity-based keyframe easing with visual curve editor
 *****************************************************************************/

#include "KeyframeUI.h"

#ifdef MSWindows

#include "GdiPlusIncludes.h"
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cwchar>

using namespace Gdiplus;

// =========================================================
// JSON Helper Functions (Simple Manual Parser)
// =========================================================

// Extract a float value from JSON string like "key": 123.45
static float JsonGetFloat(const wchar_t* json, const wchar_t* key, float defaultVal = 0.0f) {
    wchar_t searchKey[64];
    swprintf_s(searchKey, L"\"%s\"", key);

    const wchar_t* pos = wcsstr(json, searchKey);
    if (!pos) return defaultVal;

    // Find the colon after the key
    pos = wcschr(pos, L':');
    if (!pos) return defaultVal;
    pos++;

    // Skip whitespace
    while (*pos == L' ' || *pos == L'\t' || *pos == L'\n' || *pos == L'\r') pos++;

    // Parse the number
    return (float)_wtof(pos);
}

// Extract a string value from JSON like "key": "value"
static void JsonGetString(const wchar_t* json, const wchar_t* key, wchar_t* out, size_t outSize) {
    out[0] = L'\0';
    wchar_t searchKey[64];
    swprintf_s(searchKey, L"\"%s\"", key);

    const wchar_t* pos = wcsstr(json, searchKey);
    if (!pos) return;

    // Find the colon
    pos = wcschr(pos, L':');
    if (!pos) return;
    pos++;

    // Skip whitespace
    while (*pos == L' ' || *pos == L'\t' || *pos == L'\n' || *pos == L'\r') pos++;

    // Expect opening quote
    if (*pos != L'"') return;
    pos++;

    // Copy until closing quote
    size_t i = 0;
    while (*pos && *pos != L'"' && i < outSize - 1) {
        out[i++] = *pos++;
    }
    out[i] = L'\0';
}

// =========================================================
// AE <-> Bezier Conversion Functions
// =========================================================

// Convert After Effects speed/influence to bezier control points
// Formula based on AE's KeyframeEase system:
//   avgSpeed = valueChange / duration
//   normalizedSpeed = speed / avgSpeed
//   P1.x = outInfluence / 100
//   P1.y = P1.x * normalizedOutSpeed
//   P2.x = 1 - (inInfluence / 100)
//   P2.y = 1 - (1 - P2.x) * normalizedInSpeed
static void ConvertAEToBezier(
    float outSpeed, float outInfluence,
    float inSpeed, float inInfluence,
    float avgSpeed,
    KeyframeUI::KeyframeType outType,
    KeyframeUI::KeyframeType inType,
    KeyframeUI::VelocityCurve& curve)
{
    // Handle edge case: no value change (avgSpeed = 0)
    // In this case, show a flat line
    if (fabs(avgSpeed) < 0.0001f) {
        curve.p0_x = 0.25f;
        curve.p0_y = 0.25f;
        curve.p1_x = 0.75f;
        curve.p1_y = 0.75f;
        return;
    }

    // Handle special keyframe types
    // Hold keyframe: horizontal line (instant value jump at start/end)
    if (outType == KeyframeUI::KEYFRAME_HOLD) {
        // Out from hold: horizontal line at start
        curve.p0_x = 0.33f;
        curve.p0_y = 0.0f;  // Flat at bottom
        curve.p1_x = 0.67f;
        curve.p1_y = 1.0f;  // Jump to top at end
        return;
    }
    if (inType == KeyframeUI::KEYFRAME_HOLD) {
        // In to hold: horizontal line at end
        curve.p0_x = 0.33f;
        curve.p0_y = 0.0f;  // Start at bottom
        curve.p1_x = 0.67f;
        curve.p1_y = 1.0f;  // Jump at end
        return;
    }

    // Linear keyframe: diagonal line (constant velocity)
    if (outType == KeyframeUI::KEYFRAME_LINEAR && inType == KeyframeUI::KEYFRAME_LINEAR) {
        curve.p0_x = 0.33f;
        curve.p0_y = 0.33f;
        curve.p1_x = 0.67f;
        curve.p1_y = 0.67f;
        return;
    }

    // Bezier keyframe: use speed/influence to calculate control points
    // Clamp influence to valid range
    outInfluence = max(0.1f, min(100.0f, outInfluence));
    inInfluence = max(0.1f, min(100.0f, inInfluence));

    // Normalized speeds (relative to average)
    float normalizedOutSpeed = outSpeed / avgSpeed;
    float normalizedInSpeed = inSpeed / avgSpeed;

    // Handle linear out with bezier in (or vice versa)
    if (outType == KeyframeUI::KEYFRAME_LINEAR) {
        normalizedOutSpeed = 1.0f;  // Linear = same as average speed
        outInfluence = 33.33f;
    }
    if (inType == KeyframeUI::KEYFRAME_LINEAR) {
        normalizedInSpeed = 1.0f;
        inInfluence = 33.33f;
    }

    // First control point (affects "out" from first keyframe)
    curve.p0_x = outInfluence / 100.0f;
    curve.p0_y = curve.p0_x * normalizedOutSpeed;

    // Second control point (affects "in" to second keyframe)
    curve.p1_x = 1.0f - (inInfluence / 100.0f);
    curve.p1_y = 1.0f - ((1.0f - curve.p1_x) * normalizedInSpeed);

    // Clamp Y values to reasonable range (-0.5 to 1.5 for overshoot effects)
    curve.p0_y = max(-0.5f, min(1.5f, curve.p0_y));
    curve.p1_y = max(-0.5f, min(1.5f, curve.p1_y));
}

// Convert bezier control points back to AE speed/influence
// Reverse of ConvertAEToBezier
static void ConvertBezierToAE(
    const KeyframeUI::VelocityCurve& curve,
    float avgSpeed,
    float& outSpeed, float& outInfluence,
    float& inSpeed, float& inInfluence)
{
    // Influence from X coordinates
    outInfluence = curve.p0_x * 100.0f;
    inInfluence = (1.0f - curve.p1_x) * 100.0f;

    // Clamp influence to valid range (AE requires 0.1-100)
    outInfluence = max(0.1f, min(100.0f, outInfluence));
    inInfluence = max(0.1f, min(100.0f, inInfluence));

    // Calculate normalized speeds from control points
    // normalizedOutSpeed = P1.y / P1.x (when P1.x != 0)
    float normalizedOutSpeed = 1.0f;  // Default: linear
    if (fabs(curve.p0_x) > 0.001f) {
        normalizedOutSpeed = curve.p0_y / curve.p0_x;
    }

    // normalizedInSpeed = (1 - P2.y) / (1 - P2.x) (when P2.x != 1)
    float normalizedInSpeed = 1.0f;  // Default: linear
    if (fabs(1.0f - curve.p1_x) > 0.001f) {
        normalizedInSpeed = (1.0f - curve.p1_y) / (1.0f - curve.p1_x);
    }

    // If avgSpeed is 0 or very small, use default speed of 1.0
    // This happens when value change is 0 (no animation)
    float safeAvgSpeed = (fabs(avgSpeed) < 0.0001f) ? 1.0f : avgSpeed;

    // Clamp normalized speeds first (prevent huge values)
    normalizedOutSpeed = max(-100.0f, min(100.0f, normalizedOutSpeed));
    normalizedInSpeed = max(-100.0f, min(100.0f, normalizedInSpeed));

    // Convert back to actual speeds
    outSpeed = normalizedOutSpeed * safeAvgSpeed;
    inSpeed = normalizedInSpeed * safeAvgSpeed;

    // Clamp speeds to prevent NaN/inf in script (reasonable range: 0-10000)
    outSpeed = max(0.0f, min(10000.0f, outSpeed));
    inSpeed = max(0.0f, min(10000.0f, inSpeed));

    // Final NaN and infinity check
    // Finite numbers satisfy: (x - x == 0) and (x == x)
    if (outSpeed != outSpeed || (outSpeed - outSpeed) != 0.0f) outSpeed = safeAvgSpeed;
    if (inSpeed != inSpeed || (inSpeed - inSpeed) != 0.0f) inSpeed = safeAvgSpeed;
    if (outInfluence != outInfluence || (outInfluence - outInfluence) != 0.0f) outInfluence = 33.33f;
    if (inInfluence != inInfluence || (inInfluence - inInfluence) != 0.0f) inInfluence = 33.33f;
}

// GDI+ token
static ULONG_PTR g_gdiplusToken = 0;

// Window class name
static const wchar_t* KEYFRAME_CLASS_NAME = L"AnchorSnapKeyframeClass";

// Window handle
static HWND g_hwnd = NULL;
static bool g_isVisible = false;

// UI Constants
static const int WINDOW_WIDTH = 480;   // Widened for Multi-View mode
static const int WINDOW_HEIGHT = 416;  // Compact layout (lock buttons removed)
static const int HEADER_HEIGHT = 32;
static const int GRAPH_HEIGHT = 180;
static const int PRESET_BAR_HEIGHT = 40;
static const int INFO_HEIGHT = 60;
static const int SLOT_BAR_HEIGHT = 36;
static const int PADDING = 10;
static const int CLOSE_BUTTON_SIZE = 20;
static const int PRESET_BUTTON_WIDTH = 40;   // Square for graph preview
static const int PRESET_BUTTON_HEIGHT = 40;
static const int SLOT_BUTTON_WIDTH = 40;
static const int SLOT_BUTTON_HEIGHT = 40;
static const int SLOT_BUTTON_SIZE = 40;  // Alias for square buttons
static const int NUM_TOTAL_SLOTS = 10;   // 5 presets + 5 custom slots

// Colors (matching ControlUI style)
static const Color COLOR_BG(240, 28, 28, 32);
static const Color COLOR_HEADER_BG(255, 40, 40, 48);
static const Color COLOR_GRAPH_BG(255, 20, 20, 25);
static const Color COLOR_GRAPH_GRID(255, 50, 50, 60);
static const Color COLOR_CURVE(255, 74, 207, 255);
static const Color COLOR_CURVE_DIM(180, 74, 207, 255);
static const Color COLOR_HANDLE(255, 255, 180, 0);
static const Color COLOR_HANDLE_HOVER(255, 255, 220, 100);
static const Color COLOR_TEXT(255, 220, 220, 220);
static const Color COLOR_TEXT_DIM(255, 140, 140, 140);
static const Color COLOR_ACCENT(255, 74, 207, 255);
static const Color COLOR_BORDER(255, 60, 60, 70);
static const Color COLOR_CLOSE_HOVER(255, 200, 60, 60);
static const Color COLOR_PRESET_BG(255, 50, 50, 60);
static const Color COLOR_PRESET_HOVER(255, 70, 100, 130);
static const Color COLOR_PRESET_ACTIVE(255, 74, 158, 255);
static const Color COLOR_SAVE_MODE(255, 200, 120, 0);

// Current state
static KeyframeUI::VelocityPreset g_currentPreset = KeyframeUI::PRESET_LINEAR;
static KeyframeUI::VelocityCurve g_currentCurve = {0.25f, 0.25f, 0.75f, 0.75f};
static KeyframeUI::KeyframeResult g_result;
static KeyframeUI::KeyframeSettings g_settings;

// Keyframe info from AE
static KeyframeUI::KeyframeInfo g_keyframeInfo = {};
static bool g_hasKeyframeInfo = false;
static float g_avgSpeed = 0.0f;  // Cached average speed for conversions

// Multi-View mode: support multiple keyframe pairs
static const int MAX_KEYFRAME_PAIRS = 10;  // Max supported pairs
struct KeyframePairInfo {
    KeyframeUI::KeyframeInfo info;
    KeyframeUI::VelocityCurve curve;
    float avgSpeed;
    bool isMiddleKeyframe;  // K2 in K1-K2-K3 has both in and out handles
};
static KeyframePairInfo g_keyframePairs[MAX_KEYFRAME_PAIRS];
static int g_numKeyframePairs = 0;
static int g_currentPairIndex = 0;  // Currently selected/viewed pair
static bool g_multiViewMode = false;  // True if more than 2 keyframes selected

// Navigation button states
static bool g_navPrevHover = false;
static bool g_navNextHover = false;
static bool g_pressedNavPrev = false;
static bool g_pressedNavNext = false;
static const int NAV_BUTTON_SIZE = 28;

// Preset curves (control points for cubic bezier)
static KeyframeUI::VelocityCurve g_presetCurves[] = {
    {0.25f, 0.25f, 0.75f, 0.75f},   // LINEAR: constant velocity
    {0.42f, 0.0f, 1.0f, 1.0f},      // EASE_IN: accelerating
    {0.0f, 0.0f, 0.58f, 1.0f},      // EASE_OUT: decelerating
    {0.42f, 0.0f, 0.58f, 1.0f},     // EASE_IN_OUT: slow-fast-slow
    {0.0f, 1.0f, 1.0f, 0.0f},       // EASE_OUT_IN: fast-slow-fast
};

// Custom curve slots (4 slots)
static const int NUM_CUSTOM_SLOTS = 4;
static KeyframeUI::VelocityCurve g_customSlots[NUM_CUSTOM_SLOTS] = {
    {0.25f, 0.25f, 0.75f, 0.75f},
    {0.25f, 0.25f, 0.75f, 0.75f},
    {0.25f, 0.25f, 0.75f, 0.75f},
    {0.25f, 0.25f, 0.75f, 0.75f}
};
static bool g_slotFilled[NUM_CUSTOM_SLOTS] = {false, false, false, false};
// Slot icons: 0=Empty, 1=Star, 2=Circle, 3=Wave, 4=Diamond
static int g_slotIcons[NUM_CUSTOM_SLOTS] = {0, 0, 0, 0};

// UI interaction state
static bool g_closeButtonHover = false;
static int g_hoveredPresetButton = -1;  // 0-4 for presets, -1 for none
static int g_hoveredSlotButton = -1;    // 0-3 for slots, -1 for none
static bool g_saveButtonHover = false;
static bool g_saveMode = false;
static bool g_pinButtonHover = false;
static bool g_keepPanelOpen = false;  // Pin mode
static bool g_applyButtonHover = false;  // Apply button hover state
static bool g_loadButtonHover = false;   // Load button hover state

// Handle dragging
static int g_draggingHandle = -1;  // -1=none, 0=first handle (p0), 1=second handle (p1)
static bool g_handleHover[2] = {false, false};

// Graph area (calculated on draw)
static RECT g_graphRect = {0};

// Click feedback animation
static const UINT_PTR CLICK_FEEDBACK_TIMER_ID = 1001;
static const int CLICK_FEEDBACK_DURATION_MS = 100;  // 100ms feedback duration
static int g_pressedPresetButton = -1;    // Which preset button is pressed (-1 = none)
static int g_pressedSlotButton = -1;      // Which slot button is pressed (-1 = none)
static bool g_pressedSaveButton = false;  // Save button pressed state
static bool g_pressedApplyButton = false; // Apply button pressed state
static bool g_pressedLoadButton = false;  // Load button pressed state
static bool g_pressedPinButton = false;   // Pin button pressed state
static bool g_pressedCloseButton = false; // Close button pressed state


// Window dragging
static bool g_windowDragging = false;
static POINT g_windowDragOffset = {0, 0};

// Lock toggle for multi-view mode only (middle keyframe In/Out sync)
static bool g_lockHandles = false;      // Sync In/Out handles for middle keyframes
static bool g_lockHandlesHover = false;
static bool g_pressedLockHandles = false;
static const int LOCK_BUTTON_SIZE = 24;

// Forward declarations
LRESULT CALLBACK KeyframeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void DrawKeyframePanel(HDC hdc, int width, int height);
void DrawVelocityGraph(Graphics& graphics, int x, int y, int width, int height);
void DrawBezierCurve(Graphics& graphics, const KeyframeUI::VelocityCurve& curve, int x, int y, int w, int h);
void DrawMiniBezier(Graphics& graphics, const KeyframeUI::VelocityCurve& curve, int x, int y, int size, bool active);
PointF EvalCubicBezier(float t, float p0, float p1, float p2, float p3);
float IntegrateVelocityCurve(const KeyframeUI::VelocityCurve& curve, float t);

// Draw slot icon
// iconType: 0=Empty/Number, 1=Star, 2=Circle, 3=Wave, 4=Diamond
void DrawSlotIcon(Graphics& graphics, int iconType, float cx, float cy, float size, const Color& color) {
    SolidBrush brush(color);
    Pen pen(color, 1.5f);
    float r = size / 2.0f;

    switch (iconType) {
        case 1: {  // Star (5-pointed)
            PointF starPoints[10];
            for (int i = 0; i < 10; i++) {
                float angle = -90.0f + i * 36.0f;
                float rad = angle * 3.14159f / 180.0f;
                float dist = (i % 2 == 0) ? r : r * 0.4f;
                starPoints[i].X = cx + cosf(rad) * dist;
                starPoints[i].Y = cy + sinf(rad) * dist;
            }
            graphics.FillPolygon(&brush, starPoints, 10);
            break;
        }
        case 2: {  // Circle
            graphics.FillEllipse(&brush, cx - r * 0.7f, cy - r * 0.7f, r * 1.4f, r * 1.4f);
            break;
        }
        case 3: {  // Wave (sine wave symbol)
            GraphicsPath path;
            float waveR = r * 0.8f;
            for (int i = 0; i <= 20; i++) {
                float t = (float)i / 20.0f;
                float x = cx - waveR + t * waveR * 2.0f;
                float y = cy + sinf(t * 3.14159f * 2.0f) * (r * 0.5f);
                if (i == 0) path.StartFigure();
                else {
                    float prevT = (float)(i - 1) / 20.0f;
                    float prevX = cx - waveR + prevT * waveR * 2.0f;
                    float prevY = cy + sinf(prevT * 3.14159f * 2.0f) * (r * 0.5f);
                    path.AddLine(prevX, prevY, x, y);
                }
            }
            Pen wavePen(color, 2.0f);
            graphics.DrawPath(&wavePen, &path);
            break;
        }
        case 4: {  // Diamond
            PointF diamondPoints[4] = {
                PointF(cx, cy - r),
                PointF(cx + r * 0.7f, cy),
                PointF(cx, cy + r),
                PointF(cx - r * 0.7f, cy)
            };
            graphics.FillPolygon(&brush, diamondPoints, 4);
            break;
        }
        default:
            break;  // No icon for 0
    }
}

// Draw mini bezier curve in a slot button
void DrawMiniBezier(Graphics& graphics, const KeyframeUI::VelocityCurve& curve, int x, int y, int size, bool active) {
    int padding = 4;
    int drawX = x + padding;
    int drawY = y + padding;
    int drawSize = size - padding * 2;

    // Background
    SolidBrush bgBrush(active ? Color(255, 40, 60, 40) : Color(255, 30, 30, 35));
    graphics.FillRectangle(&bgBrush, x, y, size, size);

    // Draw curve
    Pen curvePen(active ? COLOR_CURVE : Color(180, 74, 207, 255), 1.5f);

    // Bezier curve points (normalized 0-1 to screen space)
    std::vector<PointF> points;
    for (int i = 0; i <= 20; i++) {
        float t = (float)i / 20.0f;

        // Cubic bezier: P = (1-t)^3*P0 + 3*(1-t)^2*t*P1 + 3*(1-t)*t^2*P2 + t^3*P3
        float px = (1-t)*(1-t)*(1-t)*0.0f +
                   3*(1-t)*(1-t)*t*curve.p0_x +
                   3*(1-t)*t*t*curve.p1_x +
                   t*t*t*1.0f;
        float py = (1-t)*(1-t)*(1-t)*0.0f +
                   3*(1-t)*(1-t)*t*curve.p0_y +
                   3*(1-t)*t*t*curve.p1_y +
                   t*t*t*1.0f;

        // Map to screen (Y inverted)
        float screenX = drawX + px * drawSize;
        float screenY = drawY + (1.0f - py) * drawSize;
        points.push_back(PointF(screenX, screenY));
    }

    // Draw curve segments
    for (size_t i = 1; i < points.size(); i++) {
        graphics.DrawLine(&curvePen, points[i-1], points[i]);
    }

    // Border
    Pen borderPen(active ? COLOR_PRESET_ACTIVE : COLOR_BORDER, 1);
    graphics.DrawRectangle(&borderPen, x, y, size, size);
}

namespace KeyframeUI {

void Initialize() {
    if (g_gdiplusToken == 0) {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    }

    // Register window class
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = KeyframeWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = KEYFRAME_CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);
}

void Shutdown() {
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }
    UnregisterClassW(KEYFRAME_CLASS_NAME, GetModuleHandle(NULL));

    if (g_gdiplusToken) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

void ShowPanel(int screenX, int screenY) {
    if (g_isVisible) return;

    // Reset state
    g_result = KeyframeResult();
    // Only reset to linear preset if no keyframe info was provided
    if (!g_hasKeyframeInfo) {
        g_currentPreset = PRESET_LINEAR;
        g_currentCurve = g_presetCurves[PRESET_LINEAR];
    }
    g_hoveredPresetButton = -1;
    g_hoveredSlotButton = -1;
    g_saveMode = false;
    g_draggingHandle = -1;
    g_pinButtonHover = false;
    g_applyButtonHover = false;
    // Don't reset g_keepPanelOpen - preserve pin state across sessions

    // Reset all pressed states
    g_pressedPresetButton = -1;
    g_pressedSlotButton = -1;
    g_pressedSaveButton = false;
    g_pressedApplyButton = false;
    g_pressedPinButton = false;
    g_pressedCloseButton = false;
    g_pressedLockHandles = false;
    g_lockHandlesHover = false;
    // Don't reset g_lockHandles - preserve across sessions

    // Reset navigation states
    g_navPrevHover = false;
    g_navNextHover = false;
    g_pressedNavPrev = false;
    g_pressedNavNext = false;
    g_currentPairIndex = 0;  // Start from first pair

    // Create or reposition window
    if (!g_hwnd) {
        g_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            KEYFRAME_CLASS_NAME,
            L"Keyframe Easing",
            WS_POPUP,
            screenX - WINDOW_WIDTH / 2,
            screenY - WINDOW_HEIGHT / 2,
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            NULL, NULL,
            GetModuleHandle(NULL),
            NULL
        );
        SetLayeredWindowAttributes(g_hwnd, 0, 245, LWA_ALPHA);
    } else {
        SetWindowPos(g_hwnd, HWND_TOPMOST,
                     screenX - WINDOW_WIDTH / 2,
                     screenY - WINDOW_HEIGHT / 2,
                     WINDOW_WIDTH, WINDOW_HEIGHT,
                     SWP_SHOWWINDOW);
    }

    ShowWindow(g_hwnd, SW_SHOW);
    SetFocus(g_hwnd);
    g_isVisible = true;

    InvalidateRect(g_hwnd, NULL, TRUE);
}

void UpdateHover(int screenX, int screenY) {
    if (!g_isVisible || !g_hwnd) return;

    // Convert to client coordinates
    POINT pt = {screenX, screenY};
    ScreenToClient(g_hwnd, &pt);

    // This would normally trigger hit testing
    // For now, we rely on WM_MOUSEMOVE in the window proc
}

KeyframeResult HidePanel() {
    if (!g_isVisible) return g_result;

    // Kill any active timer
    if (g_hwnd) {
        KillTimer(g_hwnd, CLICK_FEEDBACK_TIMER_ID);
    }

    ShowWindow(g_hwnd, SW_HIDE);
    g_isVisible = false;
    g_saveMode = false;
    g_draggingHandle = -1;
    g_hasKeyframeInfo = false;  // Reset for next open
    g_pinButtonHover = false;

    // Reset all pressed states
    g_pressedPresetButton = -1;
    g_pressedSlotButton = -1;
    g_pressedSaveButton = false;
    g_pressedApplyButton = false;
    g_pressedPinButton = false;
    g_pressedCloseButton = false;
    g_pressedLockHandles = false;
    g_lockHandlesHover = false;

    // Reset navigation states
    g_navPrevHover = false;
    g_navNextHover = false;
    g_pressedNavPrev = false;
    g_pressedNavNext = false;
    g_numKeyframePairs = 0;
    g_multiViewMode = false;

    return g_result;
}

KeyframeResult GetResult() {
    return g_result;
}

bool IsVisible() {
    return g_isVisible;
}

// Helper: Parse a single keyframe pair object from JSON
static void ParseSingleKeyframePair(const wchar_t* json, KeyframePairInfo& pair) {
    // Extract property name
    JsonGetString(json, L"propName", pair.info.propName, 128);
    JsonGetString(json, L"propMatchName", pair.info.propMatchName, 128);

    // Extract keyframe indices
    pair.info.keyIndex1 = (int)JsonGetFloat(json, L"keyIndex1", 1);
    pair.info.keyIndex2 = (int)JsonGetFloat(json, L"keyIndex2", 2);

    // Extract times and values
    pair.info.time1 = JsonGetFloat(json, L"time1", 0.0f);
    pair.info.time2 = JsonGetFloat(json, L"time2", 1.0f);
    pair.info.value1 = JsonGetFloat(json, L"value1", 0.0f);
    pair.info.value2 = JsonGetFloat(json, L"value2", 0.0f);

    // Extract easing values
    pair.info.outSpeed = JsonGetFloat(json, L"outSpeed", 0.0f);
    pair.info.outInfluence = JsonGetFloat(json, L"outInfluence", 33.33f);
    pair.info.inSpeed = JsonGetFloat(json, L"inSpeed", 0.0f);
    pair.info.inInfluence = JsonGetFloat(json, L"inInfluence", 33.33f);

    // Sanitize parsed values (prevent NaN/inf)
    auto sanitizeFloat = [](float& val, float defaultVal, float minVal, float maxVal) {
        if (val != val || (val - val) != 0.0f) val = defaultVal;  // NaN/inf check
        val = max(minVal, min(maxVal, val));
    };
    sanitizeFloat(pair.info.outSpeed, 0.0f, 0.0f, 10000.0f);
    sanitizeFloat(pair.info.outInfluence, 33.33f, 0.1f, 100.0f);
    sanitizeFloat(pair.info.inSpeed, 0.0f, 0.0f, 10000.0f);
    sanitizeFloat(pair.info.inInfluence, 33.33f, 0.1f, 100.0f);

    // Extract keyframe types (1=linear, 2=bezier, 3=hold)
    int outTypeInt = (int)JsonGetFloat(json, L"outType", 2);
    int inTypeInt = (int)JsonGetFloat(json, L"inType", 2);
    pair.info.outType = (KeyframeUI::KeyframeType)outTypeInt;
    pair.info.inType = (KeyframeUI::KeyframeType)inTypeInt;

    // Get average speed
    pair.avgSpeed = JsonGetFloat(json, L"avgSpeed", 0.0f);

    // If avgSpeed not provided, calculate it
    if (pair.avgSpeed == 0.0f) {
        float duration = pair.info.time2 - pair.info.time1;
        float valueChange = pair.info.value2 - pair.info.value1;
        if (fabs(duration) > 0.0001f) {
            pair.avgSpeed = fabs(valueChange / duration);
        }
    }

    // Sanitize avgSpeed (prevent NaN/inf from breaking script)
    if (pair.avgSpeed != pair.avgSpeed || (pair.avgSpeed - pair.avgSpeed) != 0.0f) {
        pair.avgSpeed = 1.0f;  // Default to 1.0 for invalid values
    }
    pair.avgSpeed = max(0.0f, min(10000.0f, pair.avgSpeed));

    // Convert AE easing to bezier control points
    ConvertAEToBezier(
        pair.info.outSpeed,
        pair.info.outInfluence,
        pair.info.inSpeed,
        pair.info.inInfluence,
        pair.avgSpeed,
        pair.info.outType,
        pair.info.inType,
        pair.curve
    );

    pair.isMiddleKeyframe = false;  // Will be set later based on context
}

void SetKeyframeInfo(const wchar_t* infoJson) {
    if (!infoJson || wcslen(infoJson) == 0) {
        g_hasKeyframeInfo = false;
        g_numKeyframePairs = 0;
        g_multiViewMode = false;
        return;
    }

    // Reset state
    g_numKeyframePairs = 0;
    g_currentPairIndex = 0;
    g_multiViewMode = false;

    // Check if this is a JSON array (starts with '[')
    const wchar_t* ptr = infoJson;
    while (*ptr == L' ' || *ptr == L'\t' || *ptr == L'\n' || *ptr == L'\r') ptr++;

    if (*ptr == L'[') {
        // Parse JSON array - multiple keyframe pairs
        ptr++;  // Skip '['

        while (*ptr && g_numKeyframePairs < MAX_KEYFRAME_PAIRS) {
            // Skip whitespace
            while (*ptr == L' ' || *ptr == L'\t' || *ptr == L'\n' || *ptr == L'\r' || *ptr == L',') ptr++;

            if (*ptr == L']') break;  // End of array

            if (*ptr == L'{') {
                // Find the matching closing brace
                const wchar_t* objStart = ptr;
                int braceCount = 1;
                ptr++;

                while (*ptr && braceCount > 0) {
                    if (*ptr == L'{') braceCount++;
                    else if (*ptr == L'}') braceCount--;
                    ptr++;
                }

                if (braceCount == 0) {
                    // Extract this object as a substring
                    size_t objLen = ptr - objStart;
                    wchar_t* objStr = new wchar_t[objLen + 1];
                    wcsncpy_s(objStr, objLen + 1, objStart, objLen);
                    objStr[objLen] = L'\0';

                    // Parse this object
                    ParseSingleKeyframePair(objStr, g_keyframePairs[g_numKeyframePairs]);

                    // Mark middle keyframes (not first, not last will be determined later)
                    g_numKeyframePairs++;

                    delete[] objStr;
                }
            } else {
                ptr++;  // Skip unknown character
            }
        }

        // Mark middle keyframes (keyframes that appear in both pairs)
        // For pairs: [K1-K2, K2-K3, K3-K4], K2 and K3 are middle keyframes
        for (int i = 0; i < g_numKeyframePairs; i++) {
            if (i > 0) {
                // Check if this pair's first keyframe matches previous pair's second
                if (g_keyframePairs[i].info.keyIndex1 == g_keyframePairs[i-1].info.keyIndex2) {
                    g_keyframePairs[i].isMiddleKeyframe = true;
                }
            }
        }

        g_multiViewMode = (g_numKeyframePairs > 1);

    } else if (*ptr == L'{') {
        // Single object - parse directly
        ParseSingleKeyframePair(infoJson, g_keyframePairs[0]);
        g_numKeyframePairs = 1;
        g_multiViewMode = false;
    } else {
        // Invalid JSON
        g_hasKeyframeInfo = false;
        return;
    }

    // Initialize with first pair
    if (g_numKeyframePairs > 0) {
        g_keyframeInfo = g_keyframePairs[0].info;
        g_currentCurve = g_keyframePairs[0].curve;
        g_avgSpeed = g_keyframePairs[0].avgSpeed;
        g_hasKeyframeInfo = true;
        g_currentPreset = PRESET_CUSTOM;
    } else {
        g_hasKeyframeInfo = false;
    }

    // Reset loadRequested flag (load operation complete)
    g_result.loadRequested = false;

    // Redraw if visible
    if (g_hwnd && g_isVisible) {
        InvalidateRect(g_hwnd, NULL, TRUE);
    }
}

VelocityCurve GetCurrentCurve() {
    return g_currentCurve;
}

void CalculateAEEase(const VelocityCurve& curve,
                     float& outSpeed, float& outInfluence,
                     float& inSpeed, float& inInfluence) {
    // Use the proper reverse conversion function
    // This converts bezier control points back to AE speed/influence values
    ConvertBezierToAE(curve, g_avgSpeed, outSpeed, outInfluence, inSpeed, inInfluence);
}

void SavePresetToSlot(int slot, const VelocityCurve& curve) {
    if (slot < 0 || slot >= NUM_CUSTOM_SLOTS) return;
    g_customSlots[slot] = curve;
    g_slotFilled[slot] = true;
    // Auto-assign icon based on slot index + 1 (1=Star, 2=Circle, etc.)
    if (g_slotIcons[slot] == 0) {
        g_slotIcons[slot] = slot + 1;
    }
}

bool LoadPresetFromSlot(int slot, VelocityCurve& curve) {
    if (slot < 0 || slot >= NUM_CUSTOM_SLOTS) return false;
    if (!g_slotFilled[slot]) return false;
    curve = g_customSlots[slot];
    return true;
}

bool PresetSlotExists(int slot) {
    if (slot < 0 || slot >= NUM_CUSTOM_SLOTS) return false;
    return g_slotFilled[slot];
}

void SetSlotIcon(int slot, int iconType) {
    if (slot < 0 || slot >= NUM_CUSTOM_SLOTS) return;
    g_slotIcons[slot] = iconType;
}

int GetSlotIcon(int slot) {
    if (slot < 0 || slot >= NUM_CUSTOM_SLOTS) return 0;
    return g_slotIcons[slot];
}

KeyframeSettings& GetSettings() {
    return g_settings;
}

} // namespace KeyframeUI

// Evaluate cubic bezier at parameter t
PointF EvalCubicBezier(float t, float p0x, float p0y, float p1x, float p1y,
                       float p2x, float p2y, float p3x, float p3y) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    PointF result;
    result.X = uuu * p0x + 3 * uu * t * p1x + 3 * u * tt * p2x + ttt * p3x;
    result.Y = uuu * p0y + 3 * uu * t * p1y + 3 * u * tt * p2y + ttt * p3y;
    return result;
}

// Integrate velocity curve using Simpson's rule
float IntegrateVelocityCurve(const KeyframeUI::VelocityCurve& curve, float t) {
    const int N = 100;
    float h = t / N;
    float sum = 0;

    for (int i = 0; i <= N; i++) {
        float ti = (float)i * h;
        // Evaluate bezier Y at this t (velocity)
        PointF pt = EvalCubicBezier(ti, 0, 0, curve.p0_x, curve.p0_y,
                                    curve.p1_x, curve.p1_y, 1, 1);
        float vi = pt.Y;

        if (i == 0 || i == N) sum += vi;
        else if (i % 2 == 0) sum += 2 * vi;
        else sum += 4 * vi;
    }

    return (h / 3.0f) * sum;
}

// Draw the bezier curve in the graph area
// Graph padding constant for overshoot visualization
static const float GRAPH_Y_PADDING = 0.2f;  // 20% padding above and below

// Helper: Map curve Y value (with overshoot) to screen Y
inline float CurveYToScreen(float curveY, int graphY, int graphH) {
    // curveY range: -0.2 to 1.2 (with padding)
    // Screen: top = 1.2, bottom = -0.2
    float normalizedY = (1.0f + GRAPH_Y_PADDING - curveY) / (1.0f + GRAPH_Y_PADDING * 2);
    return graphY + normalizedY * graphH;
}

void DrawBezierCurve(Graphics& graphics, const KeyframeUI::VelocityCurve& curve,
                     int x, int y, int w, int h) {
    // Draw the velocity curve as a bezier with overshoot support
    std::vector<PointF> points;
    const int segments = 50;

    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments;
        PointF pt = EvalCubicBezier(t, 0, 1, curve.p0_x, 1 - curve.p0_y,
                                    curve.p1_x, 1 - curve.p1_y, 1, 0);
        // Map to graph coordinates with padding for overshoot
        float px = x + pt.X * w;
        // pt.Y is inverted (0=bottom, 1=top in curve space)
        float curveY = 1.0f - pt.Y;  // Convert back to curve Y
        float py = CurveYToScreen(curveY, y, h);
        points.push_back(PointF(px, py));
    }

    // Draw curve
    Pen curvePen(COLOR_CURVE, 2.5f);
    if (points.size() > 1) {
        graphics.DrawLines(&curvePen, points.data(), (INT)points.size());
    }

    // Draw control point handles (with overshoot support)
    float p0_screenX = x + curve.p0_x * w;
    float p0_screenY = CurveYToScreen(curve.p0_y, y, h);
    float p1_screenX = x + curve.p1_x * w;
    float p1_screenY = CurveYToScreen(curve.p1_y, y, h);

    // Endpoint screen positions (0 and 1 in curve space)
    float startY = CurveYToScreen(0.0f, y, h);  // Bottom endpoint
    float endY = CurveYToScreen(1.0f, y, h);    // Top endpoint

    // Lines from endpoints to handles
    Pen handleLinePen(COLOR_CURVE_DIM, 1);
    graphics.DrawLine(&handleLinePen, (REAL)x, startY, p0_screenX, p0_screenY);
    graphics.DrawLine(&handleLinePen, (REAL)(x + w), endY, p1_screenX, p1_screenY);

    // Handle circles
    const float handleRadius = 6.0f;
    Color h0Color = g_handleHover[0] ? COLOR_HANDLE_HOVER : COLOR_HANDLE;
    Color h1Color = g_handleHover[1] ? COLOR_HANDLE_HOVER : COLOR_HANDLE;

    SolidBrush h0Brush(h0Color);
    SolidBrush h1Brush(h1Color);

    graphics.FillEllipse(&h0Brush,
        p0_screenX - handleRadius, p0_screenY - handleRadius,
        handleRadius * 2, handleRadius * 2);
    graphics.FillEllipse(&h1Brush,
        p1_screenX - handleRadius, p1_screenY - handleRadius,
        handleRadius * 2, handleRadius * 2);

    // Start/end points
    SolidBrush endpointBrush(COLOR_TEXT);
    const float endpointRadius = 4.0f;
    graphics.FillEllipse(&endpointBrush,
        (REAL)x - endpointRadius, startY - endpointRadius,
        endpointRadius * 2, endpointRadius * 2);
    graphics.FillEllipse(&endpointBrush,
        (REAL)(x + w) - endpointRadius, endY - endpointRadius,
        endpointRadius * 2, endpointRadius * 2);
}

// Draw the velocity graph with grid
void DrawVelocityGraph(Graphics& graphics, int x, int y, int width, int height) {
    // Background
    SolidBrush bgBrush(COLOR_GRAPH_BG);
    graphics.FillRectangle(&bgBrush, x, y, width, height);

    // Overshoot areas (dimmer background)
    float line0Y = CurveYToScreen(0.0f, y, height);  // Y=0 line
    float line1Y = CurveYToScreen(1.0f, y, height);  // Y=1 line
    SolidBrush overshootBrush(Color(40, 255, 100, 100));  // Subtle red tint
    // Top overshoot area (above Y=1)
    graphics.FillRectangle(&overshootBrush, x, y, width, (int)(line1Y - y));
    // Bottom overshoot area (below Y=0)
    graphics.FillRectangle(&overshootBrush, x, (int)line0Y, width, (int)(y + height - line0Y));

    // Border
    Pen borderPen(COLOR_BORDER, 1);
    graphics.DrawRectangle(&borderPen, x, y, width - 1, height - 1);

    // Grid lines (within 0-1 range)
    Pen gridPen(COLOR_GRAPH_GRID, 1);
    const int gridLines = 4;
    for (int i = 1; i < gridLines; i++) {
        float ratio = (float)i / gridLines;
        int gx = x + (int)(ratio * width);
        // Vertical grid lines span full height
        graphics.DrawLine(&gridPen, gx, y, gx, y + height);
        // Horizontal grid lines at 0.25, 0.5, 0.75 in curve space
        float lineY = CurveYToScreen(ratio, y, height);
        graphics.DrawLine(&gridPen, x, (int)lineY, x + width, (int)lineY);
    }

    // Boundary lines at Y=0 and Y=1 (more visible)
    Pen boundaryPen(Color(180, 100, 180, 255), 1);  // Purple-ish
    graphics.DrawLine(&boundaryPen, x, (int)line0Y, x + width, (int)line0Y);
    graphics.DrawLine(&boundaryPen, x, (int)line1Y, x + width, (int)line1Y);

    // Diagonal reference line (linear) - from (0,0) to (1,1) in curve space
    Pen refPen(COLOR_GRAPH_GRID, 1);
    refPen.SetDashStyle(DashStyleDash);
    graphics.DrawLine(&refPen, (REAL)x, line0Y, (REAL)(x + width), line1Y);

    // Store graph rect for hit testing
    g_graphRect.left = x;
    g_graphRect.top = y;
    g_graphRect.right = x + width;
    g_graphRect.bottom = y + height;

    // Draw current curve
    DrawBezierCurve(graphics, g_currentCurve, x, y, width, height);

    // Axis labels
    FontFamily fontFamily(L"Segoe UI");
    Font labelFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
    SolidBrush labelBrush(COLOR_TEXT_DIM);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);

    // Labels removed for cleaner look
}

// Draw the full keyframe panel
void DrawKeyframePanel(HDC hdc, int width, int height) {
    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Background
    SolidBrush bgBrush(COLOR_BG);
    graphics.FillRectangle(&bgBrush, 0, 0, width, height);

    // Border
    Pen borderPen(COLOR_BORDER, 1);
    graphics.DrawRectangle(&borderPen, 0, 0, width - 1, height - 1);

    // Fonts
    FontFamily fontFamily(L"Segoe UI");
    Font headerFont(&fontFamily, 12, FontStyleBold, UnitPixel);
    Font labelFont(&fontFamily, 11, FontStyleRegular, UnitPixel);
    Font presetFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    Font valueFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(COLOR_TEXT);
    SolidBrush dimBrush(COLOR_TEXT_DIM);
    SolidBrush accentBrush(COLOR_ACCENT);

    StringFormat sf;
    sf.SetAlignment(StringAlignmentNear);
    sf.SetLineAlignment(StringAlignmentCenter);

    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);

    int currentY = PADDING;

    // ===== Header bar =====
    SolidBrush headerBgBrush(COLOR_HEADER_BG);
    RectF headerRect((REAL)PADDING, (REAL)currentY,
                     (REAL)(width - PADDING * 2 - CLOSE_BUTTON_SIZE - 4), (REAL)HEADER_HEIGHT);
    graphics.FillRectangle(&headerBgBrush, headerRect);

    // Title - show property name if available
    RectF titleRect((REAL)(PADDING + 8), (REAL)currentY, (REAL)(width - PADDING * 2 - CLOSE_BUTTON_SIZE - 20), (REAL)HEADER_HEIGHT);
    if (g_hasKeyframeInfo && wcslen(g_keyframeInfo.propName) > 0) {
        // Show property name with accent color
        graphics.DrawString(g_keyframeInfo.propName, -1, &headerFont, titleRect, &sf, &accentBrush);
    } else {
        graphics.DrawString(L"Keyframe Easing", -1, &headerFont, titleRect, &sf, &textBrush);
    }

    // Pin button 📌 (before close button)
    int pinBtnX = width - PADDING - CLOSE_BUTTON_SIZE * 2 - 4;
    int pinBtnY = currentY + (HEADER_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
    RectF pinRect((REAL)pinBtnX, (REAL)pinBtnY,
                  (REAL)CLOSE_BUTTON_SIZE, (REAL)CLOSE_BUTTON_SIZE);

    Color pinColor = g_pressedPinButton ? Color(255, 40, 80, 40) :  // Pressed: darker
                     g_keepPanelOpen ? COLOR_ACCENT : (g_pinButtonHover ? COLOR_PRESET_HOVER : COLOR_PRESET_BG);
    SolidBrush pinBgBrush(pinColor);
    graphics.FillRectangle(&pinBgBrush, pinRect);

    // Draw pin icon (pushpin shape)
    float pinCx = (float)pinBtnX + CLOSE_BUTTON_SIZE / 2.0f;
    float pinCy = (float)pinBtnY + CLOSE_BUTTON_SIZE / 2.0f;
    Color pinIconColor = g_keepPanelOpen ? Color(255, 255, 255, 255) : COLOR_TEXT;
    Pen pinPen(pinIconColor, 1.5f);
    SolidBrush pinBrush(pinIconColor);

    // Pin head (circle)
    graphics.FillEllipse(&pinBrush, pinCx - 3.0f, pinCy - 5.0f, 6.0f, 6.0f);
    // Pin needle (line down)
    graphics.DrawLine(&pinPen, pinCx, pinCy + 1.0f, pinCx, pinCy + 6.0f);
    // Pin base (small triangle)
    PointF pinBase[3] = {
        PointF(pinCx - 2.0f, pinCy + 1.0f),
        PointF(pinCx + 2.0f, pinCy + 1.0f),
        PointF(pinCx, pinCy + 4.0f)
    };
    graphics.FillPolygon(&pinBrush, pinBase, 3);

    // Close button [x]
    int closeBtnX = width - PADDING - CLOSE_BUTTON_SIZE;
    int closeBtnY = currentY + (HEADER_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
    RectF closeRect((REAL)closeBtnX, (REAL)closeBtnY,
                    (REAL)CLOSE_BUTTON_SIZE, (REAL)CLOSE_BUTTON_SIZE);

    if (g_pressedCloseButton) {
        SolidBrush closeBgBrush(Color(255, 150, 40, 40));  // Darker red when pressed
        graphics.FillRectangle(&closeBgBrush, closeRect);
    } else if (g_closeButtonHover) {
        SolidBrush closeBgBrush(COLOR_CLOSE_HOVER);
        graphics.FillRectangle(&closeBgBrush, closeRect);
    }

    Pen xPen(COLOR_TEXT, 2);
    float margin = 5.0f;
    graphics.DrawLine(&xPen,
        (REAL)(closeBtnX + margin), (REAL)(closeBtnY + margin),
        (REAL)(closeBtnX + CLOSE_BUTTON_SIZE - margin), (REAL)(closeBtnY + CLOSE_BUTTON_SIZE - margin));
    graphics.DrawLine(&xPen,
        (REAL)(closeBtnX + CLOSE_BUTTON_SIZE - margin), (REAL)(closeBtnY + margin),
        (REAL)(closeBtnX + margin), (REAL)(closeBtnY + CLOSE_BUTTON_SIZE - margin));

    currentY += HEADER_HEIGHT + PADDING;

    // ===== Multi-View Navigation (if multiple pairs) =====
    if (g_multiViewMode && g_numKeyframePairs > 1) {
        int navY = currentY;
        int navCenterX = width / 2;

        // Previous button [◀]
        int prevBtnX = navCenterX - 80;
        RectF prevBtnRect((REAL)prevBtnX, (REAL)navY,
                          (REAL)NAV_BUTTON_SIZE, (REAL)NAV_BUTTON_SIZE);

        Color prevBtnColor = g_pressedNavPrev ? Color(255, 30, 80, 30) :
                             g_navPrevHover ? COLOR_PRESET_HOVER :
                             (g_currentPairIndex > 0) ? COLOR_PRESET_BG : Color(100, 40, 40, 50);
        SolidBrush prevBtnBrush(prevBtnColor);
        graphics.FillRectangle(&prevBtnBrush, prevBtnRect);
        Pen prevBorder(COLOR_BORDER, 1);
        graphics.DrawRectangle(&prevBorder, prevBtnRect);

        // Draw ◀ arrow
        if (g_currentPairIndex > 0) {
            SolidBrush arrowBrush(COLOR_TEXT);
            PointF leftArrow[3] = {
                PointF((float)prevBtnX + 18, (float)navY + 8),
                PointF((float)prevBtnX + 18, (float)navY + NAV_BUTTON_SIZE - 8),
                PointF((float)prevBtnX + 8, (float)navY + NAV_BUTTON_SIZE / 2)
            };
            graphics.FillPolygon(&arrowBrush, leftArrow, 3);
        } else {
            SolidBrush dimArrowBrush(COLOR_TEXT_DIM);
            PointF leftArrow[3] = {
                PointF((float)prevBtnX + 18, (float)navY + 8),
                PointF((float)prevBtnX + 18, (float)navY + NAV_BUTTON_SIZE - 8),
                PointF((float)prevBtnX + 8, (float)navY + NAV_BUTTON_SIZE / 2)
            };
            graphics.FillPolygon(&dimArrowBrush, leftArrow, 3);
        }

        // Indicator text "1/3" in the middle
        wchar_t navText[16];
        swprintf_s(navText, L"%d / %d", g_currentPairIndex + 1, g_numKeyframePairs);
        RectF navTextRect((REAL)(navCenterX - 30), (REAL)navY, 60, (REAL)NAV_BUTTON_SIZE);
        graphics.DrawString(navText, -1, &labelFont, navTextRect, &sfCenter, &textBrush);

        // Next button [▶]
        int nextBtnX = navCenterX + 80 - NAV_BUTTON_SIZE;
        RectF nextBtnRect((REAL)nextBtnX, (REAL)navY,
                          (REAL)NAV_BUTTON_SIZE, (REAL)NAV_BUTTON_SIZE);

        Color nextBtnColor = g_pressedNavNext ? Color(255, 30, 80, 30) :
                             g_navNextHover ? COLOR_PRESET_HOVER :
                             (g_currentPairIndex < g_numKeyframePairs - 1) ? COLOR_PRESET_BG : Color(100, 40, 40, 50);
        SolidBrush nextBtnBrush(nextBtnColor);
        graphics.FillRectangle(&nextBtnBrush, nextBtnRect);
        Pen nextBorder(COLOR_BORDER, 1);
        graphics.DrawRectangle(&nextBorder, nextBtnRect);

        // Draw ▶ arrow
        if (g_currentPairIndex < g_numKeyframePairs - 1) {
            SolidBrush arrowBrush(COLOR_TEXT);
            PointF rightArrow[3] = {
                PointF((float)nextBtnX + 10, (float)navY + 8),
                PointF((float)nextBtnX + 10, (float)navY + NAV_BUTTON_SIZE - 8),
                PointF((float)nextBtnX + 20, (float)navY + NAV_BUTTON_SIZE / 2)
            };
            graphics.FillPolygon(&arrowBrush, rightArrow, 3);
        } else {
            SolidBrush dimArrowBrush(COLOR_TEXT_DIM);
            PointF rightArrow[3] = {
                PointF((float)nextBtnX + 10, (float)navY + 8),
                PointF((float)nextBtnX + 10, (float)navY + NAV_BUTTON_SIZE - 8),
                PointF((float)nextBtnX + 20, (float)navY + NAV_BUTTON_SIZE / 2)
            };
            graphics.FillPolygon(&dimArrowBrush, rightArrow, 3);
        }

        currentY += NAV_BUTTON_SIZE + 4;
    }

    // ===== Velocity Graph (square aspect ratio) =====
    int graphH = GRAPH_HEIGHT;
    int graphW = graphH;  // Square graph
    int graphX = (width - graphW) / 2;  // Center horizontally
    int graphY = currentY;

    DrawVelocityGraph(graphics, graphX, graphY, graphW, graphH);

    currentY += GRAPH_HEIGHT + 4;  // Small gap after graph

    // ===== Lock toggle (multi-view only) =====
    if (g_multiViewMode) {
        int lockX = graphX;
        int lockY = currentY;
        int lockBtnWidth = LOCK_BUTTON_SIZE * 2 + 30;

        // Lock Handles button
        RectF lockRect((REAL)lockX, (REAL)lockY, (REAL)lockBtnWidth, (REAL)LOCK_BUTTON_SIZE);
        Color lockColor = g_pressedLockHandles ? Color(255, 30, 80, 30) :
                          g_lockHandles ? COLOR_PRESET_ACTIVE :
                          g_lockHandlesHover ? COLOR_PRESET_HOVER : COLOR_PRESET_BG;
        SolidBrush lockBrush(lockColor);
        graphics.FillRectangle(&lockBrush, lockRect);
        Pen lockBorder(COLOR_BORDER, 1);
        graphics.DrawRectangle(&lockBorder, lockRect);

        // Chain icon
        float iconX = (float)lockX + 6;
        float iconY = (float)lockY + LOCK_BUTTON_SIZE / 2.0f;
        Color iconColor = g_lockHandles ? Color(255, 255, 255, 255) : COLOR_TEXT_DIM;
        Pen iconPen(iconColor, 1.5f);
        graphics.DrawEllipse(&iconPen, iconX, iconY - 5.0f, 8.0f, 10.0f);
        graphics.DrawEllipse(&iconPen, iconX + 6.0f, iconY - 5.0f, 8.0f, 10.0f);

        // "Sync" text
        RectF lockTextRect((REAL)(lockX + 22), (REAL)lockY, 40, (REAL)LOCK_BUTTON_SIZE);
        SolidBrush lockTextBrush(g_lockHandles ? Color(255, 255, 255, 255) : COLOR_TEXT_DIM);
        graphics.DrawString(L"Sync", -1, &presetFont, lockTextRect, &sf, &lockTextBrush);

        currentY += LOCK_BUTTON_SIZE + PADDING;
    } else {
        currentY += PADDING;  // Just padding for solo view
    }

    // ===== Preset buttons (with mini graph) =====
    int presetCount = 5;
    int btnSpacing = 4;
    int totalBtnWidth = presetCount * PRESET_BUTTON_WIDTH + (presetCount - 1) * btnSpacing;
    int presetStartX = (width - totalBtnWidth) / 2;

    for (int i = 0; i < presetCount; i++) {
        int btnX = presetStartX + i * (PRESET_BUTTON_WIDTH + btnSpacing);
        int btnY = currentY;

        bool isActive = (i == (int)g_currentPreset);
        bool isHovered = (i == g_hoveredPresetButton);
        bool isPressed = (i == g_pressedPresetButton);

        // Draw mini bezier graph for this preset
        DrawMiniBezier(graphics, g_presetCurves[i], btnX, btnY, PRESET_BUTTON_WIDTH, isActive);

        // Overlay for hover/pressed states
        if (isPressed) {
            SolidBrush pressedOverlay(Color(100, 0, 0, 0));
            graphics.FillRectangle(&pressedOverlay, btnX, btnY, PRESET_BUTTON_WIDTH, PRESET_BUTTON_HEIGHT);
        } else if (isHovered && !isActive) {
            SolidBrush hoverOverlay(Color(60, 255, 255, 255));
            graphics.FillRectangle(&hoverOverlay, btnX, btnY, PRESET_BUTTON_WIDTH, PRESET_BUTTON_HEIGHT);
        }

        // Border
        Pen btnBorder(isActive ? COLOR_PRESET_ACTIVE : COLOR_BORDER, isActive ? 2.0f : 1.0f);
        graphics.DrawRectangle(&btnBorder, btnX, btnY, PRESET_BUTTON_WIDTH, PRESET_BUTTON_HEIGHT);
    }

    currentY += PRESET_BUTTON_HEIGHT + PADDING;

    // ===== Info display =====
    // Calculate current ease values
    float outSpeed, outInfluence, inSpeed, inInfluence;
    KeyframeUI::CalculateAEEase(g_currentCurve, outSpeed, outInfluence, inSpeed, inInfluence);

    // Left column: Out ease
    RectF outLabelRect((REAL)PADDING, (REAL)currentY, 80, 20);
    graphics.DrawString(L"Out Ease:", -1, &labelFont, outLabelRect, &sf, &dimBrush);

    wchar_t outText[64];
    swprintf_s(outText, L"Speed: %.1f  Infl: %.1f%%", outSpeed, outInfluence);
    RectF outValueRect((REAL)(PADDING + 70), (REAL)currentY, 120, 20);
    graphics.DrawString(outText, -1, &valueFont, outValueRect, &sf, &textBrush);

    // Right column: In ease
    RectF inLabelRect((REAL)(width / 2), (REAL)currentY, 80, 20);
    graphics.DrawString(L"In Ease:", -1, &labelFont, inLabelRect, &sf, &dimBrush);

    wchar_t inText[64];
    swprintf_s(inText, L"Speed: %.1f  Infl: %.1f%%", inSpeed, inInfluence);
    RectF inValueRect((REAL)(width / 2 + 60), (REAL)currentY, 120, 20);
    graphics.DrawString(inText, -1, &valueFont, inValueRect, &sf, &textBrush);

    currentY += 24;

    // Integral check (position = integral of velocity)
    float integralValue = IntegrateVelocityCurve(g_currentCurve, 1.0f);
    wchar_t integralText[64];
    swprintf_s(integralText, L"Integral check: %.3f (should be ~0.5)", integralValue);
    RectF integralRect((REAL)PADDING, (REAL)currentY, (REAL)(width - PADDING * 2), 20);
    graphics.DrawString(integralText, -1, &valueFont, integralRect, &sf, &dimBrush);

    currentY += 30;

    // ===== Custom preset slots (4 slots with icons) =====
    int slotBtnSpacing = 6;
    int saveBtnWidth = 44;
    int applyBtnWidth = 60;  // Apply button width
    int loadBtnWidth = 50;   // Load button width
    int totalSlotWidth = NUM_CUSTOM_SLOTS * SLOT_BUTTON_WIDTH + (NUM_CUSTOM_SLOTS - 1) * slotBtnSpacing + slotBtnSpacing * 3 + saveBtnWidth + applyBtnWidth + loadBtnWidth;
    int slotStartX = (width - totalSlotWidth) / 2;

    // "Slots:" label
    RectF presetsLabelRect((REAL)(slotStartX - 50), (REAL)currentY, 45, (REAL)SLOT_BUTTON_HEIGHT);
    graphics.DrawString(L"Slots:", -1, &labelFont, presetsLabelRect, &sf, &dimBrush);

    for (int i = 0; i < NUM_CUSTOM_SLOTS; i++) {
        int btnX = slotStartX + i * (SLOT_BUTTON_WIDTH + slotBtnSpacing);
        int btnY = currentY;

        bool isFilled = g_slotFilled[i];
        bool isHovered = (i == g_hoveredSlotButton);
        bool isPressed = (i == g_pressedSlotButton);

        // Draw mini bezier graph (or empty slot)
        if (isFilled) {
            DrawMiniBezier(graphics, g_customSlots[i], btnX, btnY, SLOT_BUTTON_WIDTH, false);
        } else {
            // Empty slot - dark background with slot number
            SolidBrush emptyBrush(Color(255, 25, 25, 30));
            graphics.FillRectangle(&emptyBrush, btnX, btnY, SLOT_BUTTON_WIDTH, SLOT_BUTTON_HEIGHT);

            wchar_t slotText[4];
            swprintf_s(slotText, L"%d", i + 6);  // 6-9 for custom slots
            RectF textRect((REAL)btnX, (REAL)btnY, (REAL)SLOT_BUTTON_WIDTH, (REAL)SLOT_BUTTON_HEIGHT);
            graphics.DrawString(slotText, -1, &presetFont, textRect, &sfCenter, &dimBrush);
        }

        // Overlay for hover/pressed/save mode
        if (isPressed) {
            SolidBrush pressedOverlay(Color(100, 0, 0, 0));
            graphics.FillRectangle(&pressedOverlay, btnX, btnY, SLOT_BUTTON_WIDTH, SLOT_BUTTON_HEIGHT);
        } else if (g_saveMode) {
            // Save mode - orange border glow
            Pen savePen(Color(255, 255, 180, 0), 2);
            graphics.DrawRectangle(&savePen, btnX, btnY, SLOT_BUTTON_WIDTH, SLOT_BUTTON_HEIGHT);
        } else if (isHovered) {
            SolidBrush hoverOverlay(Color(60, 255, 255, 255));
            graphics.FillRectangle(&hoverOverlay, btnX, btnY, SLOT_BUTTON_WIDTH, SLOT_BUTTON_HEIGHT);
        }

        // Border
        if (!g_saveMode) {
            Pen btnBorder(COLOR_BORDER, 1);
            graphics.DrawRectangle(&btnBorder, btnX, btnY, SLOT_BUTTON_WIDTH, SLOT_BUTTON_HEIGHT);
        }
    }

    // Save button (floppy disk icon area)
    int saveBtnX = slotStartX + NUM_CUSTOM_SLOTS * (SLOT_BUTTON_WIDTH + slotBtnSpacing);
    RectF saveBtnRect((REAL)saveBtnX, (REAL)currentY,
                      (REAL)saveBtnWidth, (REAL)SLOT_BUTTON_HEIGHT);

    Color saveBtnColor = g_pressedSaveButton ? Color(255, 30, 80, 30) :  // Darker when pressed
                         g_saveMode ? COLOR_PRESET_ACTIVE :
                         g_saveButtonHover ? COLOR_PRESET_HOVER : COLOR_PRESET_BG;
    SolidBrush saveBtnBrush(saveBtnColor);
    graphics.FillRectangle(&saveBtnBrush, saveBtnRect);

    Pen saveBorder(COLOR_BORDER, 1);
    graphics.DrawRectangle(&saveBorder, saveBtnRect);

    // Draw floppy disk icon
    float diskCx = (float)saveBtnX + saveBtnWidth / 2.0f;
    float diskCy = (float)currentY + SLOT_BUTTON_HEIGHT / 2.0f;
    float diskSize = 14.0f;
    Color diskColor = g_saveMode ? Color(255, 255, 255, 255) : COLOR_TEXT;

    // Floppy disk outline
    Pen diskPen(diskColor, 1.5f);
    graphics.DrawRectangle(&diskPen, diskCx - diskSize/2, diskCy - diskSize/2, diskSize, diskSize);
    // Label area (top)
    graphics.DrawRectangle(&diskPen, diskCx - diskSize/3, diskCy - diskSize/2, diskSize*2/3, diskSize/3);
    // Metal slide (bottom-right)
    SolidBrush diskBrush(diskColor);
    graphics.FillRectangle(&diskBrush, diskCx - diskSize/6, diskCy + diskSize/6, diskSize/3, diskSize/3);

    // Apply button (green)
    int applyBtnX = saveBtnX + saveBtnWidth + slotBtnSpacing;
    RectF applyBtnRect((REAL)applyBtnX, (REAL)currentY,
                       (REAL)applyBtnWidth, (REAL)SLOT_BUTTON_HEIGHT);

    Color applyBtnColor = g_pressedApplyButton ? Color(255, 30, 100, 30) :  // Darker green when pressed
                          g_applyButtonHover ? Color(255, 80, 180, 80) : Color(255, 50, 140, 50);
    SolidBrush applyBtnBrush(applyBtnColor);
    graphics.FillRectangle(&applyBtnBrush, applyBtnRect);

    Pen applyBorder(Color(255, 100, 200, 100), 1);
    graphics.DrawRectangle(&applyBorder, applyBtnRect);

    // Draw "Apply" text
    SolidBrush applyTextBrush(Color(255, 255, 255, 255));
    graphics.DrawString(L"Apply", -1, &presetFont, applyBtnRect, &sfCenter, &applyTextBrush);

    // Load button (blue - refresh keyframe info)
    int loadBtnX = applyBtnX + applyBtnWidth + slotBtnSpacing;
    RectF loadBtnRect((REAL)loadBtnX, (REAL)currentY,
                      (REAL)loadBtnWidth, (REAL)SLOT_BUTTON_HEIGHT);

    Color loadBtnColor = g_pressedLoadButton ? Color(255, 30, 60, 120) :
                         g_loadButtonHover ? Color(255, 80, 140, 220) : Color(255, 50, 100, 180);
    SolidBrush loadBtnBrush(loadBtnColor);
    graphics.FillRectangle(&loadBtnBrush, loadBtnRect);

    Pen loadBorder(Color(255, 100, 150, 255), 1);
    graphics.DrawRectangle(&loadBorder, loadBtnRect);

    // Draw "Load" text
    SolidBrush loadTextBrush(Color(255, 255, 255, 255));
    graphics.DrawString(L"Load", -1, &presetFont, loadBtnRect, &sfCenter, &loadTextBrush);
}

// Window procedure
LRESULT CALLBACK KeyframeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);

            // Double buffer
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

            DrawKeyframePanel(memDC, rc.right, rc.bottom);

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                g_result.cancelled = true;
                KeyframeUI::HidePanel();
            } else if (wParam == VK_RETURN) {
                // Apply current curve
                g_result.applied = true;
                g_result.preset = g_currentPreset;
                g_result.customCurve = g_currentCurve;
                KeyframeUI::CalculateAEEase(g_currentCurve,
                    g_result.outSpeed, g_result.outInfluence,
                    g_result.inSpeed, g_result.inInfluence);
                KeyframeUI::HidePanel();
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            RECT rc;
            GetClientRect(hwnd, &rc);
            bool needRedraw = false;

            // Window dragging
            if (g_windowDragging) {
                POINT pt;
                GetCursorPos(&pt);
                int newX = pt.x - g_windowDragOffset.x;
                int newY = pt.y - g_windowDragOffset.y;
                SetWindowPos(hwnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                return 0;
            }

            // Handle dragging (independent control - no lock sync)
            if (g_draggingHandle >= 0) {
                // Shift modifier for H/V constraint
                bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

                // Convert screen pos to curve coordinates
                // Graph has 20% padding for overshoot visualization
                float graphPadding = 0.2f;
                float effectiveHeight = 1.0f + graphPadding * 2;  // -0.2 to 1.2 range

                float nx = (float)(x - g_graphRect.left) / (g_graphRect.right - g_graphRect.left);
                float rawY = 1.0f - (float)(y - g_graphRect.top) / (g_graphRect.bottom - g_graphRect.top);
                float ny = rawY * effectiveHeight - graphPadding;  // Map to -0.2 ~ 1.2

                // X is clamped to 0-1 (time can't go backwards)
                nx = max(0.0f, min(1.0f, nx));
                // Y allows overshoot: -0.5 to 1.5 range
                ny = max(-0.5f, min(1.5f, ny));

                // Shift: constrain to H or V movement
                if (shiftHeld) {
                    float oldX = (g_draggingHandle == 0) ? g_currentCurve.p0_x : g_currentCurve.p1_x;
                    float oldY = (g_draggingHandle == 0) ? g_currentCurve.p0_y : g_currentCurve.p1_y;
                    float deltaX = fabsf(nx - oldX);
                    float deltaY = fabsf(ny - oldY);
                    if (deltaX > deltaY) {
                        ny = oldY;  // Horizontal constraint
                    } else {
                        nx = oldX;  // Vertical constraint
                    }
                }

                // Update the dragged handle
                if (g_draggingHandle == 0) {
                    g_currentCurve.p0_x = nx;
                    g_currentCurve.p0_y = ny;

                    // Multi-view Sync: mirror to other handle
                    if (g_multiViewMode && g_lockHandles) {
                        g_currentCurve.p1_x = 1.0f - nx;
                        g_currentCurve.p1_y = 1.0f - ny;
                    }
                } else {
                    g_currentCurve.p1_x = nx;
                    g_currentCurve.p1_y = ny;

                    // Multi-view Sync: mirror to other handle
                    if (g_multiViewMode && g_lockHandles) {
                        g_currentCurve.p0_x = 1.0f - nx;
                        g_currentCurve.p0_y = 1.0f - ny;
                    }
                }

                g_currentPreset = KeyframeUI::PRESET_CUSTOM;
                needRedraw = true;
            }

            // Check pin button hover
            int pinBtnX = rc.right - PADDING - CLOSE_BUTTON_SIZE * 2 - 4;
            int pinBtnY = PADDING + (HEADER_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
            bool wasPinHover = g_pinButtonHover;
            g_pinButtonHover = (x >= pinBtnX && x < pinBtnX + CLOSE_BUTTON_SIZE &&
                                y >= pinBtnY && y < pinBtnY + CLOSE_BUTTON_SIZE);
            if (wasPinHover != g_pinButtonHover) needRedraw = true;

            // Check close button hover
            int closeBtnX = rc.right - PADDING - CLOSE_BUTTON_SIZE;
            int closeBtnY = PADDING + (HEADER_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
            bool wasCloseHover = g_closeButtonHover;
            g_closeButtonHover = (x >= closeBtnX && x < closeBtnX + CLOSE_BUTTON_SIZE &&
                                  y >= closeBtnY && y < closeBtnY + CLOSE_BUTTON_SIZE);
            if (wasCloseHover != g_closeButtonHover) needRedraw = true;

            // Calculate base Y considering navigation bar
            int baseY = PADDING + HEADER_HEIGHT + PADDING;

            // Check navigation buttons hover (if multi-view mode)
            if (g_multiViewMode && g_numKeyframePairs > 1) {
                int navY = PADDING + HEADER_HEIGHT + PADDING;
                int navCenterX = rc.right / 2;
                int prevBtnX = navCenterX - 80;
                int nextBtnX = navCenterX + 80 - NAV_BUTTON_SIZE;

                bool wasNavPrevHover = g_navPrevHover;
                bool wasNavNextHover = g_navNextHover;
                g_navPrevHover = (x >= prevBtnX && x < prevBtnX + NAV_BUTTON_SIZE &&
                                  y >= navY && y < navY + NAV_BUTTON_SIZE);
                g_navNextHover = (x >= nextBtnX && x < nextBtnX + NAV_BUTTON_SIZE &&
                                  y >= navY && y < navY + NAV_BUTTON_SIZE);
                if (wasNavPrevHover != g_navPrevHover || wasNavNextHover != g_navNextHover)
                    needRedraw = true;

                baseY += NAV_BUTTON_SIZE + 4;
            }

            // Check lock button hover (multi-view only)
            int lockY = baseY + GRAPH_HEIGHT + 4;
            if (g_multiViewMode) {
                int graphW = GRAPH_HEIGHT;  // Square
                int lockX = (rc.right - graphW) / 2;
                int lockBtnWidth = LOCK_BUTTON_SIZE * 2 + 30;

                bool wasLockHover = g_lockHandlesHover;
                g_lockHandlesHover = (x >= lockX && x < lockX + lockBtnWidth &&
                                      y >= lockY && y < lockY + LOCK_BUTTON_SIZE);
                if (wasLockHover != g_lockHandlesHover) needRedraw = true;
            }

            // Check preset button hover
            int currentY = g_multiViewMode ? (lockY + LOCK_BUTTON_SIZE + PADDING) : (baseY + GRAPH_HEIGHT + PADDING);
            int presetCount = 5;
            int btnSpacing = 4;
            int totalBtnWidth = presetCount * PRESET_BUTTON_WIDTH + (presetCount - 1) * btnSpacing;
            int presetStartX = (rc.right - totalBtnWidth) / 2;

            int oldHoveredPreset = g_hoveredPresetButton;
            g_hoveredPresetButton = -1;

            for (int i = 0; i < presetCount; i++) {
                int btnX = presetStartX + i * (PRESET_BUTTON_WIDTH + btnSpacing);
                if (x >= btnX && x < btnX + PRESET_BUTTON_WIDTH &&
                    y >= currentY && y < currentY + PRESET_BUTTON_HEIGHT) {
                    g_hoveredPresetButton = i;
                    break;
                }
            }
            if (oldHoveredPreset != g_hoveredPresetButton) needRedraw = true;

            // Check slot button hover
            int slotY = currentY + PRESET_BUTTON_HEIGHT + PADDING + 24 + 30;
            int slotBtnSpacing = 6;
            int saveBtnWidth = 44;
            int applyBtnWidth = 60;
            int loadBtnWidth = 50;
            int totalSlotWidth = NUM_CUSTOM_SLOTS * SLOT_BUTTON_WIDTH + (NUM_CUSTOM_SLOTS - 1) * slotBtnSpacing + slotBtnSpacing * 3 + saveBtnWidth + applyBtnWidth + loadBtnWidth;
            int slotStartX = (rc.right - totalSlotWidth) / 2;

            int oldHoveredSlot = g_hoveredSlotButton;
            g_hoveredSlotButton = -1;

            for (int i = 0; i < NUM_CUSTOM_SLOTS; i++) {
                int btnX = slotStartX + i * (SLOT_BUTTON_WIDTH + slotBtnSpacing);
                if (x >= btnX && x < btnX + SLOT_BUTTON_WIDTH &&
                    y >= slotY && y < slotY + SLOT_BUTTON_HEIGHT) {
                    g_hoveredSlotButton = i;
                    break;
                }
            }
            if (oldHoveredSlot != g_hoveredSlotButton) needRedraw = true;

            // Check save button hover
            int saveBtnX = slotStartX + NUM_CUSTOM_SLOTS * (SLOT_BUTTON_WIDTH + slotBtnSpacing);
            bool wasSaveHover = g_saveButtonHover;
            g_saveButtonHover = (x >= saveBtnX && x < saveBtnX + saveBtnWidth &&
                                 y >= slotY && y < slotY + SLOT_BUTTON_HEIGHT);
            if (wasSaveHover != g_saveButtonHover) needRedraw = true;

            // Check apply button hover
            int applyBtnX = saveBtnX + saveBtnWidth + slotBtnSpacing;
            bool wasApplyHover = g_applyButtonHover;
            g_applyButtonHover = (x >= applyBtnX && x < applyBtnX + applyBtnWidth &&
                                  y >= slotY && y < slotY + SLOT_BUTTON_HEIGHT);
            if (wasApplyHover != g_applyButtonHover) needRedraw = true;

            // Check load button hover
            int loadBtnX = applyBtnX + applyBtnWidth + slotBtnSpacing;
            bool wasLoadHover = g_loadButtonHover;
            g_loadButtonHover = (x >= loadBtnX && x < loadBtnX + loadBtnWidth &&
                                 y >= slotY && y < slotY + SLOT_BUTTON_HEIGHT);
            if (wasLoadHover != g_loadButtonHover) needRedraw = true;

            // Check handle hover (when not dragging)
            if (g_draggingHandle < 0) {
                int graphW = g_graphRect.right - g_graphRect.left;
                int graphH = g_graphRect.bottom - g_graphRect.top;
                float p0_screenX = g_graphRect.left + g_currentCurve.p0_x * graphW;
                float p0_screenY = CurveYToScreen(g_currentCurve.p0_y, g_graphRect.top, graphH);
                float p1_screenX = g_graphRect.left + g_currentCurve.p1_x * graphW;
                float p1_screenY = CurveYToScreen(g_currentCurve.p1_y, g_graphRect.top, graphH);

                const float handleRadius = 10.0f;
                bool oldHover0 = g_handleHover[0];
                bool oldHover1 = g_handleHover[1];

                float d0 = sqrtf((x - p0_screenX) * (x - p0_screenX) + (y - p0_screenY) * (y - p0_screenY));
                float d1 = sqrtf((x - p1_screenX) * (x - p1_screenX) + (y - p1_screenY) * (y - p1_screenY));

                g_handleHover[0] = (d0 <= handleRadius);
                g_handleHover[1] = (d1 <= handleRadius);

                if (oldHover0 != g_handleHover[0] || oldHover1 != g_handleHover[1]) {
                    needRedraw = true;
                }
            }

            if (needRedraw) InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            RECT rc;
            GetClientRect(hwnd, &rc);

            // Check pin button click
            int pinBtnX = rc.right - PADDING - CLOSE_BUTTON_SIZE * 2 - 4;
            int pinBtnY = PADDING + (HEADER_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
            if (x >= pinBtnX && x < pinBtnX + CLOSE_BUTTON_SIZE &&
                y >= pinBtnY && y < pinBtnY + CLOSE_BUTTON_SIZE) {
                g_pressedPinButton = true;
                g_keepPanelOpen = !g_keepPanelOpen;
                InvalidateRect(hwnd, NULL, TRUE);
                SetTimer(hwnd, CLICK_FEEDBACK_TIMER_ID, CLICK_FEEDBACK_DURATION_MS, NULL);
                return 0;
            }

            // Check close button click
            int closeBtnX = rc.right - PADDING - CLOSE_BUTTON_SIZE;
            int closeBtnY = PADDING + (HEADER_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
            if (x >= closeBtnX && x < closeBtnX + CLOSE_BUTTON_SIZE &&
                y >= closeBtnY && y < closeBtnY + CLOSE_BUTTON_SIZE) {
                g_pressedCloseButton = true;
                InvalidateRect(hwnd, NULL, TRUE);
                g_result.cancelled = true;
                KeyframeUI::HidePanel();
                return 0;
            }

            // Check header drag (not on buttons)
            if (y >= PADDING && y < PADDING + HEADER_HEIGHT && x < pinBtnX) {
                // Start window dragging
                g_windowDragging = true;
                SetCapture(hwnd);
                POINT pt = {x, y};
                ClientToScreen(hwnd, &pt);
                RECT windowRect;
                GetWindowRect(hwnd, &windowRect);
                g_windowDragOffset.x = pt.x - windowRect.left;
                g_windowDragOffset.y = pt.y - windowRect.top;
                return 0;
            }

            // Calculate base Y considering navigation bar
            int baseY = PADDING + HEADER_HEIGHT + PADDING;
            if (g_multiViewMode && g_numKeyframePairs > 1) {
                baseY += NAV_BUTTON_SIZE + 4;

                // Check navigation buttons click
                int navY = PADDING + HEADER_HEIGHT + PADDING;
                int navCenterX = rc.right / 2;
                int prevBtnX = navCenterX - 80;
                int nextBtnX = navCenterX + 80 - NAV_BUTTON_SIZE;

                // Previous button
                if (x >= prevBtnX && x < prevBtnX + NAV_BUTTON_SIZE &&
                    y >= navY && y < navY + NAV_BUTTON_SIZE) {
                    if (g_currentPairIndex > 0) {
                        g_pressedNavPrev = true;
                        g_currentPairIndex--;
                        // Update current curve from pair
                        g_currentCurve = g_keyframePairs[g_currentPairIndex].curve;
                        g_avgSpeed = g_keyframePairs[g_currentPairIndex].avgSpeed;
                        InvalidateRect(hwnd, NULL, TRUE);
                        SetTimer(hwnd, CLICK_FEEDBACK_TIMER_ID, CLICK_FEEDBACK_DURATION_MS, NULL);
                    }
                    return 0;
                }

                // Next button
                if (x >= nextBtnX && x < nextBtnX + NAV_BUTTON_SIZE &&
                    y >= navY && y < navY + NAV_BUTTON_SIZE) {
                    if (g_currentPairIndex < g_numKeyframePairs - 1) {
                        g_pressedNavNext = true;
                        g_currentPairIndex++;
                        // Update current curve from pair
                        g_currentCurve = g_keyframePairs[g_currentPairIndex].curve;
                        g_avgSpeed = g_keyframePairs[g_currentPairIndex].avgSpeed;
                        InvalidateRect(hwnd, NULL, TRUE);
                        SetTimer(hwnd, CLICK_FEEDBACK_TIMER_ID, CLICK_FEEDBACK_DURATION_MS, NULL);
                    }
                    return 0;
                }
            }

            // Check lock button click (multi-view only)
            int lockY = baseY + GRAPH_HEIGHT + 4;
            if (g_multiViewMode) {
                int graphW = GRAPH_HEIGHT;  // Square
                int lockX = (rc.right - graphW) / 2;
                int lockBtnWidth = LOCK_BUTTON_SIZE * 2 + 30;

                if (x >= lockX && x < lockX + lockBtnWidth &&
                    y >= lockY && y < lockY + LOCK_BUTTON_SIZE) {
                    g_pressedLockHandles = true;
                    g_lockHandles = !g_lockHandles;
                    InvalidateRect(hwnd, NULL, TRUE);
                    SetTimer(hwnd, CLICK_FEEDBACK_TIMER_ID, CLICK_FEEDBACK_DURATION_MS, NULL);
                    return 0;
                }
            }

            // Check preset button click
            int currentY = g_multiViewMode ? (lockY + LOCK_BUTTON_SIZE + PADDING) : (baseY + GRAPH_HEIGHT + PADDING);
            int presetCount = 5;
            int btnSpacing = 4;
            int totalBtnWidth = presetCount * PRESET_BUTTON_WIDTH + (presetCount - 1) * btnSpacing;
            int presetStartX = (rc.right - totalBtnWidth) / 2;

            for (int i = 0; i < presetCount; i++) {
                int btnX = presetStartX + i * (PRESET_BUTTON_WIDTH + btnSpacing);
                if (x >= btnX && x < btnX + PRESET_BUTTON_WIDTH &&
                    y >= currentY && y < currentY + PRESET_BUTTON_HEIGHT) {
                    g_pressedPresetButton = i;
                    g_currentPreset = (KeyframeUI::VelocityPreset)i;
                    g_currentCurve = g_presetCurves[i];
                    InvalidateRect(hwnd, NULL, TRUE);
                    SetTimer(hwnd, CLICK_FEEDBACK_TIMER_ID, CLICK_FEEDBACK_DURATION_MS, NULL);
                    return 0;
                }
            }

            // Check slot button click
            int slotY = currentY + PRESET_BUTTON_HEIGHT + PADDING + 24 + 30;
            int slotBtnSpacing = 6;
            int saveBtnWidth = 44;
            int applyBtnWidthLocal = 60;
            int loadBtnWidth = 50;
            int totalSlotWidth = NUM_CUSTOM_SLOTS * SLOT_BUTTON_WIDTH + (NUM_CUSTOM_SLOTS - 1) * slotBtnSpacing + slotBtnSpacing * 3 + saveBtnWidth + applyBtnWidthLocal + loadBtnWidth;
            int slotStartX = (rc.right - totalSlotWidth) / 2;

            for (int i = 0; i < NUM_CUSTOM_SLOTS; i++) {
                int btnX = slotStartX + i * (SLOT_BUTTON_WIDTH + slotBtnSpacing);
                if (x >= btnX && x < btnX + SLOT_BUTTON_WIDTH &&
                    y >= slotY && y < slotY + SLOT_BUTTON_HEIGHT) {
                    g_pressedSlotButton = i;
                    if (g_saveMode) {
                        // Save current curve to slot
                        KeyframeUI::SavePresetToSlot(i, g_currentCurve);
                        g_saveMode = false;
                    } else if (g_slotFilled[i]) {
                        // Load curve from slot
                        KeyframeUI::LoadPresetFromSlot(i, g_currentCurve);
                        g_currentPreset = KeyframeUI::PRESET_CUSTOM;
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    SetTimer(hwnd, CLICK_FEEDBACK_TIMER_ID, CLICK_FEEDBACK_DURATION_MS, NULL);
                    return 0;
                }
            }

            // Check save button click
            int saveBtnX = slotStartX + NUM_CUSTOM_SLOTS * (SLOT_BUTTON_WIDTH + slotBtnSpacing);
            if (x >= saveBtnX && x < saveBtnX + saveBtnWidth &&
                y >= slotY && y < slotY + SLOT_BUTTON_HEIGHT) {
                g_pressedSaveButton = true;
                g_saveMode = !g_saveMode;
                InvalidateRect(hwnd, NULL, TRUE);
                SetTimer(hwnd, CLICK_FEEDBACK_TIMER_ID, CLICK_FEEDBACK_DURATION_MS, NULL);
                return 0;
            }

            // Check apply button click
            int applyBtnWidth = 60;
            int applyBtnX = saveBtnX + saveBtnWidth + slotBtnSpacing;
            if (x >= applyBtnX && x < applyBtnX + applyBtnWidth &&
                y >= slotY && y < slotY + SLOT_BUTTON_HEIGHT) {
                g_pressedApplyButton = true;
                InvalidateRect(hwnd, NULL, TRUE);
                // Apply current curve
                g_result.applied = true;
                g_result.preset = g_currentPreset;
                g_result.customCurve = g_currentCurve;
                KeyframeUI::CalculateAEEase(g_currentCurve,
                    g_result.outSpeed, g_result.outInfluence,
                    g_result.inSpeed, g_result.inInfluence);
                KeyframeUI::HidePanel();
                return 0;
            }

            // Check load button click
            int loadBtnX = applyBtnX + applyBtnWidth + slotBtnSpacing;
            if (x >= loadBtnX && x < loadBtnX + loadBtnWidth &&
                y >= slotY && y < slotY + SLOT_BUTTON_HEIGHT) {
                g_pressedLoadButton = true;
                g_result.loadRequested = true;
                InvalidateRect(hwnd, NULL, TRUE);
                SetTimer(hwnd, CLICK_FEEDBACK_TIMER_ID, CLICK_FEEDBACK_DURATION_MS, NULL);
                return 0;
            }

            // Check handle click for dragging
            int graphW = g_graphRect.right - g_graphRect.left;
            int graphH = g_graphRect.bottom - g_graphRect.top;
            float p0_screenX = g_graphRect.left + g_currentCurve.p0_x * graphW;
            float p0_screenY = CurveYToScreen(g_currentCurve.p0_y, g_graphRect.top, graphH);
            float p1_screenX = g_graphRect.left + g_currentCurve.p1_x * graphW;
            float p1_screenY = CurveYToScreen(g_currentCurve.p1_y, g_graphRect.top, graphH);

            const float handleRadius = 10.0f;
            float d0 = sqrtf((float)(x - p0_screenX) * (x - p0_screenX) + (y - p0_screenY) * (y - p0_screenY));
            float d1 = sqrtf((float)(x - p1_screenX) * (x - p1_screenX) + (y - p1_screenY) * (y - p1_screenY));

            if (d0 <= handleRadius) {
                g_draggingHandle = 0;
                SetCapture(hwnd);
            } else if (d1 <= handleRadius) {
                g_draggingHandle = 1;
                SetCapture(hwnd);
            }

            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_windowDragging) {
                g_windowDragging = false;
                ReleaseCapture();
                return 0;
            }
            if (g_draggingHandle >= 0) {
                g_draggingHandle = -1;
                ReleaseCapture();
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_ACTIVATE: {
            // Close when window is deactivated (clicked outside)
            if (LOWORD(wParam) == WA_INACTIVE && !g_keepPanelOpen) {
                g_result.cancelled = true;
                KeyframeUI::HidePanel();
            }
            return 0;
        }

        case WM_KILLFOCUS: {
            // If pin mode is active, don't close on focus loss
            if (g_keepPanelOpen) {
                return 0;
            }
            // Close panel when focus is lost
            g_result.cancelled = true;
            KeyframeUI::HidePanel();
            return 0;
        }

        case WM_TIMER: {
            if (wParam == CLICK_FEEDBACK_TIMER_ID) {
                // Reset all pressed states
                g_pressedPresetButton = -1;
                g_pressedSlotButton = -1;
                g_pressedSaveButton = false;
                g_pressedApplyButton = false;
                g_pressedPinButton = false;
                g_pressedCloseButton = false;
                g_pressedLockHandles = false;
                g_pressedNavPrev = false;
                g_pressedNavNext = false;
                g_pressedLoadButton = false;
                KillTimer(hwnd, CLICK_FEEDBACK_TIMER_ID);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_DESTROY:
            g_hwnd = NULL;
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

#else // macOS stub

namespace KeyframeUI {

void Initialize() {}
void Shutdown() {}
void ShowPanel(int, int) {}
void UpdateHover(int, int) {}
KeyframeResult HidePanel() { return KeyframeResult(); }
KeyframeResult GetResult() { return KeyframeResult(); }
bool IsVisible() { return false; }
void SetKeyframeInfo(const wchar_t*) {}
VelocityCurve GetCurrentCurve() { return VelocityCurve(); }
void CalculateAEEase(const VelocityCurve&, float&, float&, float&, float&) {}
void SavePresetToSlot(int, const VelocityCurve&) {}
bool LoadPresetFromSlot(int, VelocityCurve&) { return false; }
bool PresetSlotExists(int) { return false; }
KeyframeSettings& GetSettings() { static KeyframeSettings s; return s; }

} // namespace KeyframeUI

#endif // MSWindows
