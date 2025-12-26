# Windows API → AEGP API 매핑

## 개요

이 문서는 ae-anchor-radial-menu 플러그인에서 사용하는 Windows API와 AEGP SDK API 간의 대체 가능성을 정리합니다.

---

## API 매핑 표

### 키보드/마우스 상태 조회

| Windows API | 사용 위치 | AEGP 대체 | 비고 |
|------------|----------|----------|-----|
| `GetAsyncKeyState()` | KeyboardMonitor.cpp:82 | ❌ 없음 | **필수** - AEGP에 키 상태 조회 API 없음 |
| `GetCursorPos()` | KeyboardMonitor.cpp:101 | ❌ 없음 | **필수** - AEGP에 마우스 위치 API 없음 |

### 윈도우 관리

| Windows API | 사용 위치 | AEGP 대체 | 비고 |
|------------|----------|----------|-----|
| `GetForegroundWindow()` | SnapPlugin.cpp:1456 | ⚠️ 부분 대체 | AEGP_GetMainHWND와 조합하여 사용 |
| `GetAncestor()` | SnapPlugin.cpp:1467 | ❌ 없음 | 자식 윈도우 확인용 |
| `GetWindow()` | SnapPlugin.cpp:1478 | ❌ 없음 | 소유자 체인 탐색용 |
| `CreateWindowExW()` | 모든 UI 모듈 | ❌ 없음 | **필수** - AEGP에 윈도우 생성 API 없음 |

### 삭제된 함수 (미사용)

| Windows API | 원래 위치 | 상태 | 비고 |
|------------|----------|------|-----|
| `EnumWindows()` | FindEffectControlsWindow (삭제됨) | ✅ 삭제 | IsEffectControlsFocused()가 ExtendScript로 대체 |
| `GetWindowTextW()` | FindEffectControlsWindow (삭제됨) | ✅ 삭제 | 상동 |
| `IsWindowVisible()` | FindEffectControlsWindow (삭제됨) | ✅ 삭제 | 상동 |
| `GetWindowRect()` | FindEffectControlsWindow (삭제됨) | ✅ 삭제 | 상동 |

---

## AEGP API 사용 현황

### 사용 중인 AEGP Suites

| Suite | 함수 | 사용 위치 | 용도 |
|-------|------|----------|-----|
| `AEGP_UtilitySuite6` | `AEGP_GetMainHWND()` | IsAEForeground() | AE 메인 윈도우 핸들 |
| `AEGP_UtilitySuite6` | `AEGP_ExecuteScript()` | ExecuteScript() | ExtendScript 실행 |
| `AEGP_UtilitySuite6` | `AEGP_WriteToDebugLog()` | 로깅 | 디버그 로그 |
| `AEGP_RegisterSuite5` | `AEGP_RegisterIdleHook()` | GPMain() | 주기적 키 상태 확인 |
| `AEGP_RegisterSuite5` | `AEGP_RegisterUpdateMenuHook()` | GPMain() | 텍스트 편집 감지 |

### ExtendScript로 구현된 기능

| 기능 | ExtendScript 함수 | 이유 |
|-----|------------------|-----|
| Effect Controls 포커스 감지 | `IsEffectControlsFocused()` | AEGP 대체보다 간단하고 신뢰적 |
| 텍스트 도구 활성화 감지 | `IsTextToolActive()` | AEGP TextDocumentSuite보다 간단 |
| 이펙트 추가/삭제 | `addEffectToLayer()`, `removeEffectFromLayer()` | AEGP EffectSuite보다 간단 |
| 레이어 이펙트 목록 | `getLayerEffects()` | 상동 |

---

## 결론

### 필수 Windows API (대체 불가)

1. **GetAsyncKeyState()** - 키 상태 조회
2. **GetCursorPos()** - 마우스 위치 조회
3. **CreateWindowExW()** - UI 윈도우 생성

### AEGP로 개선된 부분

1. **IsAEForeground()** - AEGP_GetMainHWND + GetForegroundWindow 조합으로 더 정확
2. **텍스트 편집 감지** - UpdateMenuHook으로 AE 내부 상태와 동기화

### ExtendScript로 유지하는 부분

1. 레이어/이펙트 조작 - AEGP Suite보다 코드 복잡도 낮음
2. 패널 포커스 감지 - AEGP API보다 간단하고 신뢰적

---

## 참고

- [AEGP Suites Reference](https://ae-plugins.docsforadobe.dev/aegps/aegp-suites.html)
- [After Effects Scripting Guide](https://ae-scripting.docsforadobe.dev/)
