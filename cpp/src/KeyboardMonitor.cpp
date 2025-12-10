/*****************************************************************************
 * KeyboardMonitor.cpp
 *
 * OS-level keyboard state monitoring implementation for macOS
 * Uses CoreGraphics CGEventSourceKeyState for real-time key state
 *****************************************************************************/

#include "KeyboardMonitor.h"

namespace KeyboardMonitor {

#ifdef MAC_ENV

/**
 * IsKeyHeld
 * Uses CGEventSourceKeyState to check if a key is currently pressed
 * This works regardless of which application has focus
 */
bool IsKeyHeld(KeyCode keyCode) {
  // kCGEventSourceStateCombinedSessionState checks the combined state
  // of all event sources in the current login session
  return CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState,
                               static_cast<CGKeyCode>(keyCode));
}

bool IsShiftHeld() { return IsKeyHeld(KEY_SHIFT); }

bool IsCtrlHeld() { return IsKeyHeld(KEY_CTRL); }

bool IsAltHeld() { return IsKeyHeld(KEY_ALT); }

bool IsCmdHeld() { return IsKeyHeld(KEY_CMD); }

/**
 * GetMousePosition
 * Gets the current position of the mouse cursor on screen
 */
void GetMousePosition(int *x, int *y) {
  CGEventRef event = CGEventCreate(NULL);
  CGPoint cursor = CGEventGetLocation(event);
  CFRelease(event);

  if (x)
    *x = static_cast<int>(cursor.x);
  if (y)
    *y = static_cast<int>(cursor.y);
}

#else
// Windows implementation
#include <windows.h>

bool IsKeyHeld(KeyCode keyCode) {
  // GetAsyncKeyState checks if key is currently up or down
  // The most significant bit is set if the key is down
  return (GetAsyncKeyState(keyCode) & 0x8000) != 0;
}

bool IsShiftHeld() { return IsKeyHeld(KEY_SHIFT); }
bool IsCtrlHeld() { return IsKeyHeld(KEY_CTRL); }
bool IsAltHeld() { return IsKeyHeld(KEY_ALT); }
bool IsCmdHeld() { return IsKeyHeld(KEY_CMD); }

void GetMousePosition(int *x, int *y) {
  POINT p;
  if (GetCursorPos(&p)) {
    if (x)
      *x = p.x;
    if (y)
      *y = p.y;
  } else {
    if (x)
      *x = 0;
    if (y)
      *y = 0;
  }
}

#endif

} // namespace KeyboardMonitor
