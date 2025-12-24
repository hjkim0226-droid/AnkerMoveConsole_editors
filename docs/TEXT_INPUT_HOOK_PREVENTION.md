# 텍스트 입력 시 키보드 후킹 방지

## 개요

플러그인의 키보드 단축키(Y, D, Shift+E 등)가 텍스트 입력 중에 작동하면 안 됩니다.
사용자가 텍스트를 입력할 때 이 키들이 후킹되면 의도치 않게 모듈이 열리는 문제가 발생합니다.

## 현재 상태: 미해결

**문제**: AE에서 텍스트 레이어 편집 중 영어 `D` 키 입력 시 D메뉴가 열림

---

## 키보드 후킹 방지 조건들

### 1. `IsTextInputFocused()` - Windows 텍스트 입력 감지

**위치**: `SnapPlugin.cpp:76-163`

```cpp
bool IsTextInputFocused() {
  // Method 1: Windows 캐럿 확인
  if (gti.hwndCaret != NULL) return true;

  // Method 2: 캐럿 깜빡임 플래그
  if (gti.flags & GUI_CARETBLINKING) return true;

  // Method 3: IME 컨텍스트 확인 (한글/일본어/중국어)
  HIMC hImc = ImmGetContext(focused);
  if (hImc) {
    // IME 조합 문자열 확인
    LONG compLen = ImmGetCompositionStringW(hImc, GCS_COMPSTR, NULL, 0);
    if (compLen > 0) return true;

    // IME 열림 상태 + AE 뷰어 확인
    if (ImmGetOpenStatus(hImc) && strstr(className, "Afx")) return true;
  }

  // Method 4: 표준 텍스트 컨트롤 클래스명
  if (_strnicmp(className, "Edit", 4) == 0) return true;
  if (_strnicmp(className, "RichEdit", 8) == 0) return true;

  // Method 5: Adobe 특수 컨트롤
  if (strstr(className, "TextField")) return true;
  if (strstr(className, "Afx")) return true;
}
```

**감지 가능한 경우**:
| 상황 | 감지 여부 | 이유 |
|------|----------|------|
| Windows Edit 컨트롤 | ✅ 감지됨 | 클래스명 "Edit" |
| 대화상자 텍스트 필드 | ✅ 감지됨 | hwndCaret 존재 |
| 한글 입력 (IME) | ✅ 감지됨 | ImmGetCompositionString |
| **AE 컴포지션 뷰어에서 영어 입력** | ❌ 감지 안됨 | 아래 설명 참조 |

### 2. `IsTextToolActive()` - AE 텍스트 도구 감지

**위치**: `SnapPlugin.cpp:226-238`

```cpp
bool IsTextToolActive() {
  ExecuteScript(
    "var t=app.project.toolType;"
    "if(t===ToolType.Tool_TextH||t===ToolType.Tool_TextV)return '1';"
  );
}
```

**문제점**:
- 이 함수는 현재 D키 조건에서 **사용되지 않음**
- 사용하면 텍스트 도구 선택 시 항상 D키 차단 (편집 안 하고 있어도)

---

## AE 텍스트 편집이 감지 안 되는 이유

### 문제 상황
1. AE 컴포지션 뷰어에서 텍스트 레이어 더블클릭 → 텍스트 편집 모드 진입
2. 영어 `D` 입력 → **D메뉴가 열림** (버그)

### 원인 분석

| 감지 방법 | AE 텍스트 편집에서 작동? | 이유 |
|-----------|------------------------|------|
| hwndCaret | ❌ | AE는 자체 렌더링으로 커서 표시, Windows 캐럿 안 씀 |
| GUI_CARETBLINKING | ❌ | 동일한 이유 |
| IME 조합 문자열 | ❌ | 영어는 IME 조합 안 함 |
| ImmGetOpenStatus | ❌ | 영어 입력 시 IME 비활성 |
| 클래스명 "Edit" | ❌ | AE 뷰어는 Edit 컨트롤 아님 |

### 핵심 문제
> **AE 컴포지션 뷰어의 텍스트 편집은 AE 내부적으로 처리되며,
> Windows API로 감지할 수 있는 표준 메커니즘을 사용하지 않음**

---

## 가능한 해결 방안

### 방안 1: `IsTextToolActive()` 사용
```cpp
// D키 조건에 추가
if (!IsTextToolActive() && !IsTextInputFocused() && ...) {
  DMenuUI::ShowMenu();
}
```
- **장점**: 텍스트 도구 선택 시 D키 차단
- **단점**: 텍스트 편집 안 하고 도구만 선택해도 차단됨

### 방안 2: TextLayer 선택 상태 확인
```cpp
// ExtendScript로 TextLayer 선택 확인
bool IsTextLayerSelected() {
  ExecuteScript(
    "var layers=app.project.activeItem.selectedLayers;"
    "for(var i=0;i<layers.length;i++){"
    "  if(layers[i] instanceof TextLayer)return '1';"
    "}"
  );
}
```
- **단점**: TextLayer 선택만 해도 차단 (편집 안 하고 있어도)

### 방안 3: AE 단축키 동작 확인 (실험적)
사용자 힌트: "텍스트 편집 중에는 선택 도구(V), 앵커 도구(Y) 단축키가 안 먹힘"

AE가 내부적으로 "텍스트 편집 모드"를 알고 있다는 뜻.
하지만 이 상태를 외부에서 확인할 API가 없음.

### 방안 4: 특정 AE 창 클래스 확인
AE 텍스트 편집 시 특정 자식 창이 생성되는지 Spy++로 확인 필요.

---

## 현재 구현된 코드

### D키 후킹 조건 (SnapPlugin.cpp:2072-2075)
```cpp
if (d_key_held && !g_dKeyWasHeld &&
    !alt_held && !shift_held && !ctrl_held &&
    !IsTextInputFocused() &&           // 텍스트 입력 감지
    IsAfterEffectsForeground() &&
    !g_globals.menu_visible &&
    !g_controlVisible &&
    !g_keyframeVisible &&
    !g_alignVisible &&
    !g_textVisible &&
    !g_dMenuVisible) {
  DMenuUI::ShowMenu(mouseX, mouseY);
}
```

### 다른 단축키도 동일한 문제 있음
- **Y키** (Grid 모듈): `!IsTextInputFocused()` 체크함 → 영어 입력 감지 안됨
- **Shift+E** (Control 모듈): `!IsTextInputFocused()` 체크함 → 동일

---

## ExtendScript API 조사 결과

### 확인한 문서
- `general/application`: app 객체 속성/메서드
- `other/viewer`: Viewer 객체 (패널 타입만 확인 가능)
- `layer/textlayer`: TextLayer 속성
- `text/textdocument`: 텍스트 속성 (폰트, 크기, 색상 등)

### 결론
> **AE ExtendScript에는 "텍스트 편집 중인지" 확인하는 API가 존재하지 않음**

- `app.project.toolType`: 도구 선택 상태만 확인 (편집 모드 아님)
- `TextDocument`: 텍스트 속성만 제공
- `Viewer.type`: 패널 종류만 (COMPOSITION/LAYER/FOOTAGE)

---

## 테스트 필요 사항

1. **Spy++로 AE 텍스트 편집 시 포커스된 창 클래스명 확인**
   - 텍스트 편집 시 특정 자식 창이 생성되는지 확인
   - 포커스된 창의 클래스명이 바뀌는지 확인

2. **AE Command ID 조사**
   - https://hyperbrew.github.io/after-effects-command-ids/
   - 텍스트 편집 모드 진입/종료 관련 Command ID 확인

---

## 관련 파일

- `cpp/src/core/SnapPlugin.cpp`: IsTextInputFocused(), IsTextToolActive()
- `cpp/src/core/KeyboardMonitor.cpp`: 키보드 상태 확인
