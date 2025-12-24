# Module Feature Implementation Status

## Overview

| Module | Trigger | Type | Status |
|--------|---------|------|--------|
| Grid | Y (0.4s hold) | Popup | Production |
| DMenu | D (0.4s hold) | Popup | Production |
| Control | Shift+E | Tool | Production |
| Align | D → A | Tool | Production |
| Keyframe | D → K | Tool | Production |
| Text | D → T | Tool | Production |
| Layer | D → C | Tool | Production |

---

## Common Features Matrix

| Feature | Grid | DMenu | Control | Align | Keyframe | Text | Layer |
|---------|:----:|:-----:|:-------:|:-----:|:--------:|:----:|:-----:|
| GDI+ Rendering | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Double Buffering | ✅ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ |
| ESC Close | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Window Focus | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Click Outside Close | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Pin Button | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Window Drag | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Scale Factor | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Ctrl+Z Undo | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Ctrl+Shift+Z Redo | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ |

---

## Grid Module (Y key)

| Feature | Status | Notes |
|---------|:------:|-------|
| 3x3 Anchor Grid | ✅ | Click to set anchor point |
| Custom Anchor Slots (3) | ✅ | Left panel icons |
| Comp Mode Toggle | ✅ | Right panel - orange theme |
| Mask Mode Toggle | ✅ | Right panel |
| Settings Button | ✅ | Opens CEP panel |
| Glow Effect on Hover | ✅ | Cyan (selection) / Orange (comp) |
| Scale Factor Support | ✅ | From CEP settings |
| Copy/Paste Anchor | ✅ | Clipboard support |

---

## DMenu Module (D key)

| Feature | Status | Notes |
|---------|:------:|-------|
| Quick Menu Popup | ✅ | 4 menu items |
| [A] Align | ✅ | Opens Align module |
| [T] Text | ✅ | Opens Text module |
| [K] Keyframe | ✅ | Opens Keyframe module |
| [C] Layer | ✅ | Opens Layer module |
| Keyboard Shortcuts | ✅ | A, T, K, C keys |
| Mouse Hover Highlight | ✅ | Visual feedback |
| Focus Grace Period | ✅ | 300ms ignore focus loss |

---

## Control Module (Shift+E)

| Feature | Status | Notes |
|---------|:------:|-------|
| **Mode 1: Effect Search** | | |
| Search Bar | ✅ | Real-time filtering |
| Effect List | ✅ | From AE effect list |
| Apply Effect | ✅ | Click or Enter |
| Category Display | ✅ | Shows effect category |
| **Mode 2: Layer Effects** | | |
| Layer Effects List | ✅ | When Effect Controls focused |
| Effect Selection | ✅ | Click to select |
| Effect Deletion | 🚧 | UI ready, script pending |
| **Common** | | |
| Pin Button | ✅ | Keep panel open |
| Window Drag | ✅ | Header area |
| Presets | 🚧 | Planned |

---

## Align Module (D → A)

| Feature | Status | Notes |
|---------|:------:|-------|
| **Align Operations** | | |
| Align Left | ✅ | |
| Align Center H | ✅ | |
| Align Right | ✅ | |
| Align Top | ✅ | |
| Align Center V | ✅ | |
| Align Bottom | ✅ | |
| **Distribute Operations** | | |
| Distribute H | ✅ | |
| Distribute V | ✅ | |
| Distribute Both | ✅ | |
| **Align Target** | | |
| Selection | ✅ | Default |
| Comp | ✅ | Toggle available |
| **Common** | | |
| Pin Button | ✅ | |
| Window Drag | ✅ | |
| Keyboard Shortcuts | ✅ | 1-9 keys |

---

## Keyframe Module (D → K)

| Feature | Status | Notes |
|---------|:------:|-------|
| **Curve Editor** | | |
| Bezier Curve Display | ✅ | Interactive graph |
| Drag Handles | ✅ | P0, P1 control points |
| Live Preview | ✅ | Updates AE in real-time |
| **Presets** | | |
| Built-in Presets (6) | ✅ | Linear, Ease In/Out, etc. |
| Custom Presets (6) | ✅ | Editable slots |
| Save Current Curve | ✅ | To custom slot |
| Mini Bezier Preview | ✅ | In preset buttons |
| **Multi-Keyframe** | | |
| Multiple Pairs View | ✅ | Navigate between pairs |
| Lock Handles Toggle | ✅ | Sync In/Out |
| Navigation Buttons | ✅ | Prev/Next pair |
| **AE Integration** | | |
| Speed/Influence Conversion | ✅ | Bidirectional |
| Keyframe Type Detection | ✅ | Linear/Bezier/Hold |
| **Common** | | |
| Pin Button | ✅ | |
| Window Drag | ✅ | |
| Apply Button | ✅ | Manual apply |
| Load Button | ✅ | Reload from AE |

---

## Text Module (D → T)

| Feature | Status | Notes |
|---------|:------:|-------|
| **Font** | | |
| Font Selection | ✅ | Dropdown with search |
| Font Search | ✅ | Real-time filtering |
| Font Preview | 🚧 | Planned |
| **Text Properties** | | |
| Font Size | ✅ | Drag or type |
| Tracking | ✅ | Drag or type |
| Leading | ✅ | Drag or type, Auto support |
| Stroke Width | ✅ | Drag or type |
| **Color** | | |
| Fill Color | ✅ | Color box display |
| Stroke Color | ✅ | Color box display |
| Color Picker | 🚧 | Planned (click cycles presets) |
| **Alignment** | | |
| Left | ✅ | |
| Center | ✅ | |
| Right | ✅ | |
| Justify Left | ✅ | |
| Justify Center | ✅ | |
| Justify Right | ✅ | |
| Justify Full | ✅ | |
| **Presets** | | |
| Save Style | ✅ | Star button |
| Apply Preset | ✅ | From dropdown |
| Delete Preset | ✅ | Hover X button |
| Preset Persistence | 🚧 | File save pending |
| **Common** | | |
| Pin Button | ✅ | |
| Window Drag | ✅ | |
| Value Drag | ✅ | Shift for fine adjust |
| Double-click Edit | ✅ | Inline editing |

---

## Layer Module (D → C)

| Feature | Status | Notes |
|---------|:------:|-------|
| **Layer Detection** | | |
| Text Layer | ✅ | Orange-yellow indicator |
| Shape Layer | ✅ | Blue indicator |
| Solid Layer | ✅ | Gray indicator |
| Adjustment Layer | ✅ | Gray indicator |
| Null Layer | ✅ | Red indicator |
| Footage Layer | ✅ | Green indicator |
| Pre-comp Layer | ✅ | Green indicator |
| Camera Layer | ✅ | Purple indicator |
| Light Layer | ✅ | Yellow-white indicator |
| **Text Layer Actions** | | |
| Typewriter Animator | 🚧 | Button ready |
| Fade In Animator | 🚧 | Button ready |
| Scale Animator | 🚧 | Button ready |
| Blur Animator | 🚧 | Button ready |
| Tracking Animator | 🚧 | Button ready |
| **Shape Layer Actions** | | |
| Trim Path | 🚧 | Button ready |
| Repeater | 🚧 | Button ready |
| Wiggle Path | 🚧 | Button ready |
| Wiggle Transform | 🚧 | Button ready |
| **Solid Layer Actions** | | |
| Change Color | 🚧 | Button ready |
| Fit to Comp | 🚧 | Button ready |
| Reset Transform | 🚧 | Button ready |
| **Footage Layer Actions** | | |
| Loop (Cycle) | 🚧 | Button ready |
| Loop (Ping Pong) | 🚧 | Button ready |
| Last Frame Hold | 🚧 | Button ready |
| Reset Transform | 🚧 | Button ready |
| **Common Actions** | | |
| Reset Position | 🚧 | For Camera/Light |
| Reset Transform | 🚧 | Parent-aware for Null |
| **Common** | | |
| Pin Button | ✅ | |
| Window Drag | ✅ | |
| Number Key Shortcuts | ✅ | 1-9 keys |

---

## Legend

| Symbol | Meaning |
|:------:|---------|
| ✅ | Implemented and working |
| 🚧 | In progress / UI ready but script pending |
| ❌ | Not implemented / Not applicable |

---

## Module Type Definitions

### Popup Menu
- No pin button (always closes on action or click outside)
- No window drag
- Focus on show for keyboard input

### Tool Module
- Has pin button (keep panel open)
- Has window drag (header area)
- Ctrl+Z/Redo forwarding to AE
- Focus on show for keyboard input

---

## Technical Notes

### Window Focus Pattern (Tool Modules)
```cpp
// ShowPanel()
ShowWindow(g_hwnd, SW_SHOW);
SetForegroundWindow(g_hwnd);
SetFocus(g_hwnd);
```

### Undo/Redo Forwarding Pattern
```cpp
static void ForwardUndoRedoToAE(bool isRedo) {
    HWND aeWnd = FindAfterEffectsWindow();
    if (aeWnd) {
        SetForegroundWindow(aeWnd);
        keybd_event(VK_CONTROL, 0, 0, 0);
        if (isRedo) keybd_event(VK_SHIFT, 0, 0, 0);
        keybd_event('Z', 0, 0, 0);
        keybd_event('Z', 0, KEYEVENTF_KEYUP, 0);
        if (isRedo) keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        Sleep(30);
        SetForegroundWindow(g_hwnd);
        SetFocus(g_hwnd);
    }
}
```

### WM_ACTIVATE Handler (Tool Modules)
```cpp
case WM_ACTIVATE:
    if (LOWORD(wParam) == WA_INACTIVE && !g_keepPanelOpen) {
        HidePanel();
    }
    return 0;
```

---

*Last updated: 2024-12-24*
