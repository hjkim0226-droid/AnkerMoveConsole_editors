---
name: module-ui-guide
enabled: true
event: file
conditions:
  - field: file_path
    operator: regex_match
    pattern: modules/.*UI\.cpp$
  - field: new_text
    operator: regex_match
    pattern: WM_PAINT|DrawKeyframe|DrawControl|DrawGrid
---

**Module UI Guidelines Check**

You are editing a module UI file. Please follow these guidelines from CLAUDE.md:

**Tool Module Requirements** (Grid, Control, Keyframe, Align, Text):
- Pin button (header right side)
- Header drag to move window
- ESC key to close
- NO X button

**Popup Menu Requirements** (DMenu):
- NO pin button
- NO window drag
- ESC key to close
- NO X button

**Common Constants**:
```cpp
static const int PADDING = 10;
static const int HEADER_HEIGHT = 28;
static const int CLOSE_BUTTON_SIZE = 20;
```

Refer to `CLAUDE.md > Module UI Guidelines` for implementation details.
