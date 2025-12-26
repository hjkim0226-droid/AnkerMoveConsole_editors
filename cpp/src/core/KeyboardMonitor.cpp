/*****************************************************************************
 * KeyboardMonitor.cpp
 *
 * OS-level keyboard state monitoring implementation
 * Uses CoreGraphics on macOS, Win32 API on Windows
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │ AEGP API 대체 가능성: 없음                                               │
 * │                                                                         │
 * │ AEGP SDK에는 키보드/마우스 상태 조회 API가 없습니다.                      │
 * │ - IdleHook에서 OS API로 직접 상태 조회 필요                              │
 * │ - UpdateMenuHook: 키 입력 시 호출되지만 어떤 키인지 알 수 없음            │
 * │                                                                         │
 * │ 사용 중인 OS API:                                                        │
 * │ - Windows: GetAsyncKeyState(), GetCursorPos()                           │
 * │ - macOS: CGEventSourceKeyState(), CGEventGetLocation()                  │
 * └─────────────────────────────────────────────────────────────────────────┘
 *****************************************************************************/

#include "KeyboardMonitor.h"

#ifdef MSWindows
#include <windows.h>
#endif

namespace KeyboardMonitor {

#ifdef MAC_ENV

/**
 * IsKeyHeld
 * Uses CGEventSourceKeyState to check if a key is currently pressed
 * This works regardless of which application has focus
 *
 * AEGP 대체: 불가능 - AEGP에 키 상태 조회 API 없음
 * 참고: CGEventSourceKeyState는 시스템 전역 키 상태 반환
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
// ═══════════════════════════════════════════════════════════════════════════
// Windows implementation
// AEGP 대체: 불가능 - AEGP에 키보드/마우스 상태 조회 API 없음
// ═══════════════════════════════════════════════════════════════════════════

/**
 * IsKeyHeld
 * Win32 API: GetAsyncKeyState()
 *
 * AEGP 대체: 불가능
 * 이유: AEGP SDK에 키 상태 조회 API 없음
 * 참고: UpdateMenuHook은 키 입력 시 호출되지만 어떤 키인지 알 수 없음
 */
bool IsKeyHeld(KeyCode keyCode) {
  // GetAsyncKeyState checks if key is currently up or down
  // The most significant bit is set if the key is down
  return (GetAsyncKeyState(keyCode) & 0x8000) != 0;
}

bool IsShiftHeld() { return IsKeyHeld(KEY_SHIFT); }
bool IsCtrlHeld() { return IsKeyHeld(KEY_CTRL); }
bool IsAltHeld() { return IsKeyHeld(KEY_ALT); }
bool IsCmdHeld() { return IsKeyHeld(KEY_CMD); }

/**
 * GetMousePosition
 * Win32 API: GetCursorPos()
 *
 * AEGP 대체: 불가능
 * 이유: AEGP SDK에 마우스 위치 조회 API 없음
 * 참고: 스크린 좌표 반환 (모니터 기준)
 */
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

bool IsMouseButtonPressed() {
#ifdef MSWindows
  return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
#else
  // macOS
  return CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonLeft);
#endif
}

} // namespace KeyboardMonitor
