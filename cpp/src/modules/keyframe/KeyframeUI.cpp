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
    KeyframeUI::VelocityCurve& curve)
{
    // Handle edge case: no value change (avgSpeed = 0)
    // In this case, show a flat line (linear)
    if (fabs(avgSpeed) < 0.0001f) {
        curve.p0_x = 0.25f;
        curve.p0_y = 0.25f;
        curve.p1_x = 0.75f;
        curve.p1_y = 0.75f;
        return;
    }

    // Clamp influence to valid range
    outInfluence = max(0.1f, min(100.0f, outInfluence));
    inInfluence = max(0.1f, min(100.0f, inInfluence));

    // Normalized speeds (relative to average)
    float normalizedOutSpeed = outSpeed / avgSpeed;
    float normalizedInSpeed = inSpeed / avgSpeed;

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

    // Clamp influence to valid range
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

    // Convert back to actual speeds
    outSpeed = normalizedOutSpeed * avgSpeed;
    inSpeed = normalizedInSpeed * avgSpeed;
}

// GDI+ token
static ULONG_PTR g_gdiplusToken = 0;

// Window class name
static const wchar_t* KEYFRAME_CLASS_NAME = L"AnchorSnapKeyframeClass";

// Window handle
static HWND g_hwnd = NULL;
static bool g_isVisible = false;

// UI Constants
static const int WINDOW_WIDTH = 360;
static const int WINDOW_HEIGHT = 400;
static const int HEADER_HEIGHT = 32;
static const int GRAPH_HEIGHT = 180;
static const int PRESET_BAR_HEIGHT = 40;
static const int INFO_HEIGHT = 60;
static const int SLOT_BAR_HEIGHT = 36;
static const int PADDING = 10;
static const int CLOSE_BUTTON_SIZE = 20;
static const int PRESET_BUTTON_WIDTH = 60;
static const int PRESET_BUTTON_HEIGHT = 28;
static const int SLOT_BUTTON_WIDTH = 40;
static const int SLOT_BUTTON_HEIGHT = 28;

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

// Handle dragging
static int g_draggingHandle = -1;  // -1=none, 0=first handle (p0), 1=second handle (p1)
static bool g_handleHover[2] = {false, false};

// Graph area (calculated on draw)
static RECT g_graphRect = {0};

// Forward declarations
LRESULT CALLBACK KeyframeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void DrawKeyframePanel(HDC hdc, int width, int height);
void DrawVelocityGraph(Graphics& graphics, int x, int y, int width, int height);
void DrawBezierCurve(Graphics& graphics, const KeyframeUI::VelocityCurve& curve, int x, int y, int w, int h);
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
    // Don't reset g_keepPanelOpen - preserve pin state across sessions

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

    ShowWindow(g_hwnd, SW_HIDE);
    g_isVisible = false;
    g_saveMode = false;
    g_draggingHandle = -1;
    g_hasKeyframeInfo = false;  // Reset for next open
    g_pinButtonHover = false;

    return g_result;
}

bool IsVisible() {
    return g_isVisible;
}

void SetKeyframeInfo(const wchar_t* infoJson) {
    if (!infoJson || wcslen(infoJson) == 0) {
        g_hasKeyframeInfo = false;
        return;
    }

    // Parse JSON from ExtendScript
    // Expected format:
    // {
    //   "propName": "Position",
    //   "propMatchName": "ADBE Position",
    //   "keyIndex1": 1, "keyIndex2": 2,
    //   "time1": 0.0, "time2": 1.0,
    //   "value1": 0, "value2": 100,
    //   "outSpeed": 0, "outInfluence": 33.33,
    //   "inSpeed": 0, "inInfluence": 33.33,
    //   "avgSpeed": 100
    // }

    // Extract property name
    JsonGetString(infoJson, L"propName", g_keyframeInfo.propName, 128);
    JsonGetString(infoJson, L"propMatchName", g_keyframeInfo.propMatchName, 128);

    // Extract keyframe indices
    g_keyframeInfo.keyIndex1 = (int)JsonGetFloat(infoJson, L"keyIndex1", 1);
    g_keyframeInfo.keyIndex2 = (int)JsonGetFloat(infoJson, L"keyIndex2", 2);

    // Extract times and values
    g_keyframeInfo.time1 = JsonGetFloat(infoJson, L"time1", 0.0f);
    g_keyframeInfo.time2 = JsonGetFloat(infoJson, L"time2", 1.0f);
    g_keyframeInfo.value1 = JsonGetFloat(infoJson, L"value1", 0.0f);
    g_keyframeInfo.value2 = JsonGetFloat(infoJson, L"value2", 0.0f);

    // Extract easing values
    g_keyframeInfo.outSpeed = JsonGetFloat(infoJson, L"outSpeed", 0.0f);
    g_keyframeInfo.outInfluence = JsonGetFloat(infoJson, L"outInfluence", 33.33f);
    g_keyframeInfo.inSpeed = JsonGetFloat(infoJson, L"inSpeed", 0.0f);
    g_keyframeInfo.inInfluence = JsonGetFloat(infoJson, L"inInfluence", 33.33f);

    // Get average speed (may be calculated in ExtendScript or here)
    g_avgSpeed = JsonGetFloat(infoJson, L"avgSpeed", 0.0f);

    // If avgSpeed not provided, calculate it
    if (g_avgSpeed == 0.0f) {
        float duration = g_keyframeInfo.time2 - g_keyframeInfo.time1;
        float valueChange = g_keyframeInfo.value2 - g_keyframeInfo.value1;
        if (fabs(duration) > 0.0001f) {
            g_avgSpeed = fabs(valueChange / duration);
        }
    }

    g_hasKeyframeInfo = true;

    // Convert AE easing to bezier control points
    ConvertAEToBezier(
        g_keyframeInfo.outSpeed,
        g_keyframeInfo.outInfluence,
        g_keyframeInfo.inSpeed,
        g_keyframeInfo.inInfluence,
        g_avgSpeed,
        g_currentCurve
    );

    // Set preset to custom since we're using real keyframe data
    g_currentPreset = PRESET_CUSTOM;

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
void DrawBezierCurve(Graphics& graphics, const KeyframeUI::VelocityCurve& curve,
                     int x, int y, int w, int h) {
    // Draw the velocity curve as a bezier
    std::vector<PointF> points;
    const int segments = 50;

    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments;
        PointF pt = EvalCubicBezier(t, 0, 1, curve.p0_x, 1 - curve.p0_y,
                                    curve.p1_x, 1 - curve.p1_y, 1, 0);
        // Map to graph coordinates (Y inverted for screen)
        float px = x + pt.X * w;
        float py = y + pt.Y * h;
        points.push_back(PointF(px, py));
    }

    // Draw curve
    Pen curvePen(COLOR_CURVE, 2.5f);
    if (points.size() > 1) {
        graphics.DrawLines(&curvePen, points.data(), (INT)points.size());
    }

    // Draw control point handles
    float p0_screenX = x + curve.p0_x * w;
    float p0_screenY = y + (1 - curve.p0_y) * h;
    float p1_screenX = x + curve.p1_x * w;
    float p1_screenY = y + (1 - curve.p1_y) * h;

    // Lines from endpoints to handles
    Pen handleLinePen(COLOR_CURVE_DIM, 1);
    graphics.DrawLine(&handleLinePen, (REAL)x, (REAL)(y + h), p0_screenX, p0_screenY);
    graphics.DrawLine(&handleLinePen, (REAL)(x + w), (REAL)y, p1_screenX, p1_screenY);

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
        (REAL)x - endpointRadius, (REAL)(y + h) - endpointRadius,
        endpointRadius * 2, endpointRadius * 2);
    graphics.FillEllipse(&endpointBrush,
        (REAL)(x + w) - endpointRadius, (REAL)y - endpointRadius,
        endpointRadius * 2, endpointRadius * 2);
}

// Draw the velocity graph with grid
void DrawVelocityGraph(Graphics& graphics, int x, int y, int width, int height) {
    // Background
    SolidBrush bgBrush(COLOR_GRAPH_BG);
    graphics.FillRectangle(&bgBrush, x, y, width, height);

    // Border
    Pen borderPen(COLOR_BORDER, 1);
    graphics.DrawRectangle(&borderPen, x, y, width - 1, height - 1);

    // Grid lines
    Pen gridPen(COLOR_GRAPH_GRID, 1);
    const int gridLines = 4;
    for (int i = 1; i < gridLines; i++) {
        float ratio = (float)i / gridLines;
        int gx = x + (int)(ratio * width);
        int gy = y + (int)(ratio * height);
        graphics.DrawLine(&gridPen, gx, y, gx, y + height);
        graphics.DrawLine(&gridPen, x, gy, x + width, gy);
    }

    // Diagonal reference line (linear)
    Pen refPen(COLOR_GRAPH_GRID, 1);
    refPen.SetDashStyle(DashStyleDash);
    graphics.DrawLine(&refPen, x, y + height, x + width, y);

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

    RectF bottomLabel((REAL)x, (REAL)(y + height + 2), (REAL)width, 14);
    graphics.DrawString(L"Time", -1, &labelFont, bottomLabel, &sf, &labelBrush);

    // Rotated "Velocity" label would go on left, but keeping it simple
    RectF leftLabel((REAL)(x - 45), (REAL)(y + height / 2 - 7), 40, 14);
    StringFormat sfRight;
    sfRight.SetAlignment(StringAlignmentFar);
    graphics.DrawString(L"Velocity", -1, &labelFont, leftLabel, &sfRight, &labelBrush);
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

    Color pinColor = g_keepPanelOpen ? COLOR_ACCENT : (g_pinButtonHover ? COLOR_PRESET_HOVER : COLOR_PRESET_BG);
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

    if (g_closeButtonHover) {
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

    // ===== Velocity Graph =====
    int graphX = PADDING + 50;  // Leave room for Y axis label
    int graphY = currentY;
    int graphW = width - PADDING * 2 - 50;
    int graphH = GRAPH_HEIGHT;

    DrawVelocityGraph(graphics, graphX, graphY, graphW, graphH);

    currentY += GRAPH_HEIGHT + PADDING + 16;  // Extra space for axis label

    // ===== Preset buttons =====
    const wchar_t* presetNames[] = {L"Linear", L"Ease In", L"Ease Out", L"In-Out", L"Out-In"};
    int presetCount = 5;
    int btnSpacing = 4;
    int totalBtnWidth = presetCount * PRESET_BUTTON_WIDTH + (presetCount - 1) * btnSpacing;
    int presetStartX = (width - totalBtnWidth) / 2;

    for (int i = 0; i < presetCount; i++) {
        int btnX = presetStartX + i * (PRESET_BUTTON_WIDTH + btnSpacing);
        int btnY = currentY;

        RectF btnRect((REAL)btnX, (REAL)btnY,
                      (REAL)PRESET_BUTTON_WIDTH, (REAL)PRESET_BUTTON_HEIGHT);

        // Button color
        Color btnColor;
        if (i == (int)g_currentPreset) {
            btnColor = COLOR_PRESET_ACTIVE;
        } else if (i == g_hoveredPresetButton) {
            btnColor = COLOR_PRESET_HOVER;
        } else {
            btnColor = COLOR_PRESET_BG;
        }

        SolidBrush btnBrush(btnColor);
        graphics.FillRectangle(&btnBrush, btnRect);

        Pen btnBorder(COLOR_BORDER, 1);
        graphics.DrawRectangle(&btnBorder, btnRect);

        graphics.DrawString(presetNames[i], -1, &presetFont, btnRect, &sfCenter, &textBrush);
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
    int totalSlotWidth = NUM_CUSTOM_SLOTS * SLOT_BUTTON_WIDTH + (NUM_CUSTOM_SLOTS - 1) * slotBtnSpacing + slotBtnSpacing + saveBtnWidth;
    int slotStartX = (width - totalSlotWidth) / 2;

    // "Slots:" label
    RectF presetsLabelRect((REAL)(slotStartX - 50), (REAL)currentY, 45, (REAL)SLOT_BUTTON_HEIGHT);
    graphics.DrawString(L"Slots:", -1, &labelFont, presetsLabelRect, &sf, &dimBrush);

    for (int i = 0; i < NUM_CUSTOM_SLOTS; i++) {
        int btnX = slotStartX + i * (SLOT_BUTTON_WIDTH + slotBtnSpacing);
        int btnY = currentY;

        RectF btnRect((REAL)btnX, (REAL)btnY,
                      (REAL)SLOT_BUTTON_WIDTH, (REAL)SLOT_BUTTON_HEIGHT);

        // Button color - different in save mode
        Color btnColor;
        if (g_saveMode) {
            // Save mode - orange glow
            btnColor = (i == g_hoveredSlotButton) ? Color(255, 255, 180, 0) : COLOR_SAVE_MODE;
        } else {
            // Normal mode
            btnColor = (i == g_hoveredSlotButton) ? COLOR_PRESET_HOVER :
                       g_slotFilled[i] ? COLOR_PRESET_ACTIVE : COLOR_PRESET_BG;
        }

        SolidBrush btnBrush(btnColor);
        graphics.FillRectangle(&btnBrush, btnRect);

        // Border - thicker in save mode
        if (g_saveMode) {
            Pen btnBorder(Color(255, 255, 200, 100), 2);
            graphics.DrawRectangle(&btnBorder, btnRect);
        } else {
            Pen btnBorder(COLOR_BORDER, 1);
            graphics.DrawRectangle(&btnBorder, btnRect);
        }

        // Draw icon or number
        float iconCx = (float)btnX + SLOT_BUTTON_WIDTH / 2.0f;
        float iconCy = (float)btnY + SLOT_BUTTON_HEIGHT / 2.0f;

        if (g_slotFilled[i] && g_slotIcons[i] > 0) {
            // Draw icon
            DrawSlotIcon(graphics, g_slotIcons[i], iconCx, iconCy, 16.0f, COLOR_TEXT);
        } else {
            // Draw slot number
            wchar_t slotText[4];
            swprintf_s(slotText, L"%d", i + 1);
            graphics.DrawString(slotText, -1, &presetFont, btnRect, &sfCenter,
                               g_slotFilled[i] ? &textBrush : &dimBrush);
        }
    }

    // Save button (floppy disk icon area)
    int saveBtnX = slotStartX + NUM_CUSTOM_SLOTS * (SLOT_BUTTON_WIDTH + slotBtnSpacing);
    RectF saveBtnRect((REAL)saveBtnX, (REAL)currentY,
                      (REAL)saveBtnWidth, (REAL)SLOT_BUTTON_HEIGHT);

    Color saveBtnColor = g_saveMode ? COLOR_PRESET_ACTIVE :
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

            // Handle dragging
            if (g_draggingHandle >= 0) {
                // Convert screen pos to curve coordinates
                float nx = (float)(x - g_graphRect.left) / (g_graphRect.right - g_graphRect.left);
                float ny = 1.0f - (float)(y - g_graphRect.top) / (g_graphRect.bottom - g_graphRect.top);

                // Clamp
                nx = max(0.0f, min(1.0f, nx));
                ny = max(0.0f, min(1.0f, ny));

                if (g_draggingHandle == 0) {
                    g_currentCurve.p0_x = nx;
                    g_currentCurve.p0_y = ny;
                } else {
                    g_currentCurve.p1_x = nx;
                    g_currentCurve.p1_y = ny;
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

            // Check preset button hover
            int currentY = PADDING + HEADER_HEIGHT + PADDING + GRAPH_HEIGHT + PADDING + 16;
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
            int totalSlotWidth = NUM_CUSTOM_SLOTS * SLOT_BUTTON_WIDTH + (NUM_CUSTOM_SLOTS - 1) * slotBtnSpacing + slotBtnSpacing + saveBtnWidth;
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

            // Check handle hover (when not dragging)
            if (g_draggingHandle < 0) {
                float p0_screenX = g_graphRect.left + g_currentCurve.p0_x * (g_graphRect.right - g_graphRect.left);
                float p0_screenY = g_graphRect.top + (1 - g_currentCurve.p0_y) * (g_graphRect.bottom - g_graphRect.top);
                float p1_screenX = g_graphRect.left + g_currentCurve.p1_x * (g_graphRect.right - g_graphRect.left);
                float p1_screenY = g_graphRect.top + (1 - g_currentCurve.p1_y) * (g_graphRect.bottom - g_graphRect.top);

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
                g_keepPanelOpen = !g_keepPanelOpen;
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }

            // Check close button click
            int closeBtnX = rc.right - PADDING - CLOSE_BUTTON_SIZE;
            int closeBtnY = PADDING + (HEADER_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
            if (x >= closeBtnX && x < closeBtnX + CLOSE_BUTTON_SIZE &&
                y >= closeBtnY && y < closeBtnY + CLOSE_BUTTON_SIZE) {
                g_result.cancelled = true;
                KeyframeUI::HidePanel();
                return 0;
            }

            // Check preset button click
            int currentY = PADDING + HEADER_HEIGHT + PADDING + GRAPH_HEIGHT + PADDING + 16;
            int presetCount = 5;
            int btnSpacing = 4;
            int totalBtnWidth = presetCount * PRESET_BUTTON_WIDTH + (presetCount - 1) * btnSpacing;
            int presetStartX = (rc.right - totalBtnWidth) / 2;

            for (int i = 0; i < presetCount; i++) {
                int btnX = presetStartX + i * (PRESET_BUTTON_WIDTH + btnSpacing);
                if (x >= btnX && x < btnX + PRESET_BUTTON_WIDTH &&
                    y >= currentY && y < currentY + PRESET_BUTTON_HEIGHT) {
                    g_currentPreset = (KeyframeUI::VelocityPreset)i;
                    g_currentCurve = g_presetCurves[i];
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }
            }

            // Check slot button click
            int slotY = currentY + PRESET_BUTTON_HEIGHT + PADDING + 24 + 30;
            int slotBtnSpacing = 6;
            int saveBtnWidth = 44;
            int totalSlotWidth = NUM_CUSTOM_SLOTS * SLOT_BUTTON_WIDTH + (NUM_CUSTOM_SLOTS - 1) * slotBtnSpacing + slotBtnSpacing + saveBtnWidth;
            int slotStartX = (rc.right - totalSlotWidth) / 2;

            for (int i = 0; i < NUM_CUSTOM_SLOTS; i++) {
                int btnX = slotStartX + i * (SLOT_BUTTON_WIDTH + slotBtnSpacing);
                if (x >= btnX && x < btnX + SLOT_BUTTON_WIDTH &&
                    y >= slotY && y < slotY + SLOT_BUTTON_HEIGHT) {
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
                    return 0;
                }
            }

            // Check save button click
            int saveBtnX = slotStartX + NUM_CUSTOM_SLOTS * (SLOT_BUTTON_WIDTH + slotBtnSpacing);
            if (x >= saveBtnX && x < saveBtnX + saveBtnWidth &&
                y >= slotY && y < slotY + SLOT_BUTTON_HEIGHT) {
                g_saveMode = !g_saveMode;
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }

            // Check handle click for dragging
            float p0_screenX = g_graphRect.left + g_currentCurve.p0_x * (g_graphRect.right - g_graphRect.left);
            float p0_screenY = g_graphRect.top + (1 - g_currentCurve.p0_y) * (g_graphRect.bottom - g_graphRect.top);
            float p1_screenX = g_graphRect.left + g_currentCurve.p1_x * (g_graphRect.right - g_graphRect.left);
            float p1_screenY = g_graphRect.top + (1 - g_currentCurve.p1_y) * (g_graphRect.bottom - g_graphRect.top);

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
            if (g_draggingHandle >= 0) {
                g_draggingHandle = -1;
                ReleaseCapture();
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_KILLFOCUS: {
            // If pin mode is active, don't close on focus loss
            if (g_keepPanelOpen) {
                return 0;
            }

            // Check where focus went
            HWND newFocus = (HWND)wParam;
            if (newFocus) {
                char className[64] = {0};
                GetClassNameA(newFocus, className, sizeof(className));
                if (_strnicmp(className, "AE_", 3) == 0) {
                    return 0;
                }
            }
            g_result.cancelled = true;
            KeyframeUI::HidePanel();
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
