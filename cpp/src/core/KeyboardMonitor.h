/*****************************************************************************
 * KeyboardMonitor.h
 *
 * OS-level keyboard state monitoring for macOS
 * Uses CoreGraphics to detect key held state
 *****************************************************************************/

#pragma once

#ifdef MAC_ENV
#include <Carbon/Carbon.h>
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace KeyboardMonitor {

#ifdef MAC_ENV
// Key codes for macOS (from Events.h / Carbon)
enum KeyCode {
  KEY_Y = 0x10,         // Y key
  KEY_E = 0x0E,         // E key
  KEY_K = 0x28,         // K key (kVK_ANSI_K)
  KEY_SEMICOLON = 0x29, // ; key
  KEY_SHIFT = 0x38,     // Shift key
  KEY_CTRL = 0x3B,      // Control key
  KEY_ALT = 0x3A,       // Option/Alt key
  KEY_CMD = 0x37,       // Command key
};
#else
// Key codes for Windows (Virtual Key Codes)
enum KeyCode {
  KEY_Y = 0x59,         // 'Y' key (VK_Y is same as ASCII 'Y')
  KEY_E = 0x45,         // 'E' key
  KEY_K = 0x4B,         // 'K' key (VK_K)
  KEY_SEMICOLON = 0xBA, // ';' key (VK_OEM_1)
  KEY_SHIFT = 0x10,     // VK_SHIFT
  KEY_CTRL = 0x11,      // VK_CONTROL
  KEY_ALT = 0x12,       // VK_MENU (Alt)
  KEY_CMD = 0x5B,       // VK_LWIN (Windows Key)
};
#endif

/**
 * Check if a specific key is currently being held down
 * @param keyCode The key code to check (from KeyCode enum)
 * @return true if the key is currently held down
 */
bool IsKeyHeld(KeyCode keyCode);

/**
 * Check if modifier keys are held
 */
bool IsShiftHeld();
bool IsCtrlHeld();
bool IsAltHeld();
bool IsCmdHeld();

/**
 * Get current mouse position on screen
 * @param x Output: X coordinate
 * @param y Output: Y coordinate
 */
void GetMousePosition(int *x, int *y);

/**
 * Check if mouse left button is currently pressed
 */
bool IsMouseButtonPressed();

} // namespace KeyboardMonitor
