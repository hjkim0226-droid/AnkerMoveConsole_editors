# 텍스트 입력 시 키보드 후킹 방지

## 개요

플러그인의 키보드 단축키(Y, D, Shift+E 등)가 텍스트 입력 중에 작동하면 안 됩니다.
사용자가 텍스트를 입력할 때 이 키들이 후킹되면 의도치 않게 모듈이 열리는 문제가 발생합니다.

## 현재 상태: ✅ 해결됨

**해결 방법**: UpdateMenuHook 기반 텍스트 편집 감지 (2024-12-25)

---

## 해결책: UpdateMenuHook 방식

### 발견 경위

AE 내부 로그(`Help > Reveal Logging File`)에서 `UpdateMenuHook` 패턴 발견:
```
DynamicLinkPlugin Plugin::UpdateMenuHook windowtype=2
```

**핵심 발견**: 텍스트 편집 중에는 이 훅이 호출되지 않음!

### 동작 원리

1. `AEGP_RegisterUpdateMenuHook`은 **키보드 입력 시** 호출됨 (주기적 타이머 아님!)
2. **텍스트 편집 중에는 키 입력이 에디터로 가서** → 훅 미호출
3. "같은 키 입력으로 훅이 호출됐다 = 텍스트 편집 모드 아님" 공식 성립

### 구현

**위치**: `cpp/src/core/SnapPlugin.cpp`

```cpp
// 전역 변수
static auto g_lastMenuHookTime = std::chrono::steady_clock::now();
static const int MENU_HOOK_THRESHOLD_MS = 50;  // 50ms 이내 = 편집 모드 아님

// UpdateMenuHook 콜백 등록 (GPMain에서)
ERR(suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(
    aegp_plugin_id, UpdateMenuHook, nullptr));

// UpdateMenuHook 콜백
static A_Err UpdateMenuHook(
    AEGP_GlobalRefcon plugin_refconP,
    AEGP_UpdateMenuRefcon refconP,
    AEGP_WindowType active_window) {
  g_lastMenuHookTime = std::chrono::steady_clock::now();
  return A_Err_NONE;
}

// 헬퍼 함수
static bool IsMenuHookRecent() {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - g_lastMenuHookTime).count();
  return elapsed < MENU_HOOK_THRESHOLD_MS;
}

// 사용 예시 (모든 키보드 단축키에 적용)
if (d_key_held && IsMenuHookRecent() && ...) {
  DMenuUI::ShowMenu(x, y);
}
```

### 장점

| 항목 | 이전 방식 | UpdateMenuHook 방식 |
|------|----------|-------------------|
| Windows API 의존 | ✅ 필요 (imm32.lib) | ❌ 불필요 |
| AE 내부 텍스트 에디터 감지 | ❌ 불가능 | ✅ 가능 |
| AE 포그라운드 체크 | 별도 함수 필요 | 자동 포함 |
| 코드 복잡도 | 높음 (~100줄) | 낮음 (~20줄) |
| 반응 속도 | 느림 (WinAPI 호출) | 빠름 (50ms threshold) |

### Threshold 설정

- **50ms**: 같은 키 입력 이벤트 처리에 충분한 여유 시간
- 키 입력 → MenuHook(0ms) → IdleHook(~10ms) 순서로 처리됨
- 50ms = "같은 키 입력에서 왔다"고 판단하는 기준

---

## 이전 방식들 (제거됨)

### 1. IsTextInputFocused() - Windows API 기반

```cpp
// 제거됨
bool IsTextInputFocused() {
  // Windows 캐럿, IME, Edit 컨트롤 확인
  // 문제: AE 컴포지션 뷰어의 텍스트 에디터는 감지 불가
}
```

**문제점**: AE 뷰어는 자체 렌더링을 사용하여 Windows 캐럿/IME 미사용

### 2. CommandHook 기반 (CMD 2136/2004)

```cpp
// 제거됨
if (cmd_id == 2136) {
  g_textEditingMode = true;  // 텍스트 편집 진입
}
if (cmd_id == 2004) {
  g_textEditingMode = false; // 레이어 선택
}
```

**문제점**: 특정 상황에서만 호출되어 신뢰도 낮음

### 3. IsAfterEffectsForeground()

```cpp
// 제거됨
bool IsAfterEffectsForeground() {
  HWND fg = GetForegroundWindow();
  // AE 클래스명 확인
}
```

**문제점**: UpdateMenuHook이 AE 비활성 시 호출 안 됨으로 자동 포함

---

## 적용된 단축키

| 키 | 모듈 | IsMenuHookRecent() 체크 |
|----|------|----------------------|
| Y (hold) | Grid | ✅ |
| Shift+E | Control | ✅ |
| D | DMenu | ✅ |
| Right Shift+K | Keyframe | ✅ |

---

## AEGP_WindowType 참고

UpdateMenuHook의 `active_window` 파라미터:

```cpp
enum AEGP_WindowType {
  AEGP_WindType_NONE = 0,
  AEGP_WindType_PROJECT = 1,
  AEGP_WindType_COMP = 2,
  AEGP_WindType_TIME_LAYOUT = 3,
  AEGP_WindType_LAYER = 4,
  AEGP_WindType_FOOTAGE = 5,
  AEGP_WindType_RENDER_QUEUE = 6,
  AEGP_WindType_QT = 7,
  AEGP_WindType_DIALOG = 8,
  AEGP_WindType_FLOWCHART = 9,
  AEGP_WindType_EFFECT = 10,
  AEGP_WindType_OTHER = 11
};
```

---

## 관련 파일

- `cpp/src/core/SnapPlugin.cpp`: UpdateMenuHook 구현 및 IsMenuHookRecent()
- `CLAUDE.md`: "텍스트 편집 중 키보드 후킹 방지" 섹션
