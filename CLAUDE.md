# CLAUDE.md - Project Guidelines for Claude Code

## Model Usage (비용 최적화)

### Sonnet 사용 (빠르고 저렴한 작업)
- `git add`, `git commit`, `git push` 등 Git 명령어
- GitHub Actions 빌드 트리거 및 상태 확인
- 단순 파일 읽기/검색
- 빌드 로그 확인
- 간단한 오타 수정

### Opus 사용 (복잡한 작업)
- 새로운 기능 구현
- 버그 디버깅 및 분석
- 코드 아키텍처 설계
- 복잡한 문제 해결
- 코드 리뷰 및 리팩토링

## Build Commands

```bash
# GitHub Actions 빌드 상태 확인
gh run list --limit 1

# 빌드 로그 보기
gh run view <run-id> --log

# Artifact 다운로드 링크
# https://github.com/hjkim0226-droid/AnkerMoveConsole_editors/actions
```

## Project Structure

```
cpp/
├── src/
│   ├── core/           # 공통 모듈 (SnapPlugin, KeyboardMonitor, CEPBridge)
│   └── modules/
│       ├── grid/       # Y key - Anchor Grid
│       └── control/    # E key - Effect Search
├── include/AE_SDK/     # After Effects SDK
└── resources/          # PiPL, RC files

cep/                    # CEP Panel (Settings UI)
```

## Key Bindings
- **Y key (0.4s hold)**: Anchor Grid popup
- **Shift+E**: Effect Search popup (Control module)
  - Mode 1: Search & add effects (when Viewer/Timeline focused)
  - Mode 2: Layer effects list (when Effect Controls focused)

## Install Paths (Windows)
- Plugin: `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\AnchorSnap.aex`
- CEP: `C:\Program Files (x86)\Common Files\Adobe\CEP\extensions\com.anchor.snap\`

## Language
- 한국어로 대화

---

## Module UI Guidelines (모듈 UI 가이드라인)

### 모듈 타입별 UI 요소

| 요소 | 도구 모듈 | 팝업 메뉴 |
|-----|----------|----------|
| **핀 버튼** | ✅ 필수 | ❌ 없음 |
| **X 버튼** | ❌ 없음 | ❌ 없음 |
| **창 이동** | ✅ 헤더 드래그 | ❌ 없음 |
| **ESC 닫기** | ✅ 필수 | ✅ 필수 |

**도구 모듈**: Grid, Control, Keyframe, Align, Text
**팝업 메뉴**: DMenu

### 공통 상수 (표준)
```cpp
// === 레이아웃 ===
static const int WINDOW_WIDTH = 320;          // 창 너비 (고정)
static const int PADDING = 8;                 // 기본 여백
static const int HEADER_HEIGHT = 32;          // 헤더 높이
static const int SEARCH_HEIGHT = 36;          // 검색창 높이
static const int ITEM_HEIGHT = 32;            // 리스트 아이템 높이
static const int MAX_VISIBLE_ITEMS = 8;       // 최대 표시 아이템

// === 버튼 ===
static const int PIN_BUTTON_SIZE = 20;        // 핀/닫기 버튼
static const int ACTION_BUTTON_SIZE = 20;     // 액션 버튼
static const int PRESET_BUTTON_SIZE = 28;     // 프리셋 버튼
static const int BUTTON_MARGIN = 4;           // 버튼 여백

// === 아이콘 ===
static const int ICON_SIZE_SMALL = 16;        // 작은 아이콘
static const int ICON_SIZE_MEDIUM = 20;       // 중간 아이콘
static const int ICON_SIZE_LARGE = 34;        // 큰 아이콘
```

### 색상 팔레트 (표준)
```cpp
// === 배경 ===
static const Color COLOR_BG(240, 28, 28, 32);             // 패널 배경
static const Color COLOR_HEADER_BG(255, 40, 40, 48);      // 헤더/검색창
static const Color COLOR_CELL_BG(255, 35, 35, 40);        // 셀/아이템

// === 텍스트 ===
static const Color COLOR_TEXT(255, 220, 220, 220);        // 기본 텍스트
static const Color COLOR_TEXT_DIM(255, 140, 140, 140);    // 보조 텍스트

// === 상호작용 ===
static const Color COLOR_HOVER(255, 60, 80, 100);         // 호버 배경
static const Color COLOR_SELECTED(255, 74, 158, 255);     // 선택됨 (파란색)
static const Color COLOR_ACCENT(255, 74, 207, 255);       // 강조 (시안)
static const Color COLOR_BORDER(255, 60, 60, 70);         // 테두리

// === 특수 ===
static const Color COLOR_DELETE_HOVER(255, 200, 80, 80);  // 삭제 호버
static const Color COLOR_SAVE_ACTIVE(255, 255, 180, 0);   // 저장 모드
```

### 폰트 (표준)
```cpp
static const wchar_t* FONT_FAMILY = L"Segoe UI";
static const int FONT_SIZE_HEADER = 12;     // 헤더, 타이틀 (Bold)
static const int FONT_SIZE_ITEM = 12;       // 리스트 아이템
static const int FONT_SIZE_SEARCH = 14;     // 검색창
static const int FONT_SIZE_SMALL = 10;      // 카테고리, 인덱스

// 렌더링
graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
graphics.SetSmoothingMode(SmoothingModeAntiAlias);
```

### 더블 버퍼링 (필수)
```cpp
case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 더블 버퍼링
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

    // memDC에 그리기...

    // 화면에 복사
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    // 정리
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    EndPaint(hwnd, &ps);
    return 0;
}
```

### 핀 버튼 구현
```cpp
// 헤더 오른쪽에 위치
int pinBtnX = width - PADDING - CLOSE_BUTTON_SIZE;
int pinBtnY = PADDING + (HEADER_HEIGHT - CLOSE_BUTTON_SIZE) / 2;

// 핀 아이콘: 원 + 선 (압정 모양)
float pinCx = pinBtnX + CLOSE_BUTTON_SIZE / 2.0f;
float pinCy = pinBtnY + CLOSE_BUTTON_SIZE / 2.0f;
graphics.FillEllipse(&pinBrush, pinCx - 4, pinCy - 6, 8, 8);  // 머리
graphics.DrawLine(&pinPen, pinCx, pinCy + 2, pinCx, pinCy + 8); // 바늘
```

### 창 드래그 이동 구현
```cpp
// 변수
static bool g_windowDragging = false;
static POINT g_windowDragOffset = {0, 0};

// WM_LBUTTONDOWN (헤더 영역, 버튼 제외)
if (y >= PADDING && y < PADDING + HEADER_HEIGHT && x < pinBtnX) {
    g_windowDragging = true;
    SetCapture(hwnd);
    // 오프셋 계산...
}

// WM_MOUSEMOVE
if (g_windowDragging) {
    POINT pt;
    GetCursorPos(&pt);
    SetWindowPos(hwnd, NULL, pt.x - offset.x, pt.y - offset.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

// WM_LBUTTONUP
if (g_windowDragging) {
    g_windowDragging = false;
    ReleaseCapture();
}
```

### WM_ACTIVATE 처리 (핀 모드 지원)
```cpp
case WM_ACTIVATE:
    if (LOWORD(wParam) == WA_INACTIVE && !g_keepPanelOpen) {
        // 창 닫기
        HidePanel();
    }
    return 0;
```

---

## Troubleshooting (문제 해결 가이드)

### 플러그인이 키 입력에 반응하지 않을 때

1. **설치 경로 확인** (가장 흔한 원인)
   - ✅ 올바른 경로: `MediaCore/AnchorSnap.aex`
   - ❌ 잘못된 경로: `After Effects/AnchorSnap.aex`
   - `install_windows.bat`에서 경로 확인

2. **CMake 캐시 문제**
   - 코드 변경이 반영 안 될 때 `--clean-first` 플래그 사용
   - `.github/workflows/build-windows.yml`에 이미 적용됨

3. **빌드 아티팩트 크기 동일한 경우**
   - 코드가 실제로 컴파일되지 않았을 수 있음
   - GitHub Actions에서 `--clean-first` 확인

### CEP 패널 문제

- Debug mode 활성화 필요:
  ```
  HKCU\Software\Adobe\CSXS.11\PlayerDebugMode = 1
  HKCU\Software\Adobe\CSXS.12\PlayerDebugMode = 1
  ```

---

## Development Notes (개발 노트)

### Control 모듈 (Shift+E) - 진행 상황

- [x] Shift+E 즉시 트리거 (입력 방지를 위해 hold에서 변경)
- [x] Mode 1 (Search): Viewer/Timeline 포커스 → Search bar
- [x] Mode 2 (Effects List): Effect Controls 포커스 → 레이어 이펙트 목록
- [x] 닫기 버튼 [x] 및 ESC 키
- [ ] 이펙트 실제 삭제 연결 (ExtendScript)
- [ ] 이펙트 검색 결과에서 실제 적용
- [ ] Effect Controls 닫히면 자동 닫힘

### 중요한 교훈

1. **MediaCore vs After Effects 폴더**
   - MediaCore 폴더에 설치해야 키보드 훅이 정상 작동
   - After Effects 폴더는 Effect 플러그인용

2. **CMake --clean-first**
   - CI/CD에서 항상 clean build 강제
   - 로컬에서도 문제 시 build 폴더 삭제 후 재빌드

3. **Git diff로 비교**
   - 작동하던 커밋과 현재 코드 비교: `git diff <working-commit> HEAD -- <file>`

---

## Git Workflow

### 브랜치 전략
```
main        ← 안정된 릴리즈 버전만
  │
  └── dev   ← 개발 작업은 여기서
```

### 버전 태그
```bash
# 현재 태그 확인
git tag -l

# 태그 생성 (특정 커밋에)
git tag -a v1.0.0 <commit-hash> -m "Release message"

# 태그 푸시
git push origin v1.0.0
```

### 버전 네이밍
```
v1.0.0-alpha.1  → 초기 개발
v1.0.0-beta.1   → 베타 테스트
v1.0.0-rc.1     → 릴리즈 후보 (Release Candidate)
v1.0.0          → 정식 릴리즈
```

### 현재 버전
- **v1.0.0** - Grid 모듈 완성 (커밋: 6dbc6fc)
- **dev** - Control 모듈 개발 중

### Commit Convention
```
feat: 새로운 기능 추가
fix: 버그 수정
docs: 문서 변경
refactor: 코드 리팩토링
ci: CI/CD 설정 변경
```

### 개발 Workflow

1. `dev` 브랜치에서 작업
2. `git add . && git commit -m "..."` → 자동 빌드 트리거
3. `gh run list --limit 1` → 빌드 상태 확인
4. GitHub Actions에서 Artifact 다운로드
5. `install_windows.bat` 실행 (관리자 권한)
6. After Effects 재시작
7. 테스트 완료 후 `main`에 병합 + 태그

### main에 병합하기
```bash
git checkout main
git merge dev
git tag -a v1.1.0 -m "v1.1.0 - Control module"
git push origin main --tags
git checkout dev
```

---

## ExtendScript API Reference

### 이펙트 추가
```javascript
// matchName으로 이펙트 추가
layer.Effects.addProperty("ADBE Gaussian Blur 2");

// 추가 전 확인
if (layer.Effects.canAddProperty("ADBE Gaussian Blur 2")) {
    layer.Effects.addProperty("ADBE Gaussian Blur 2");
}
```

### 이펙트 삭제
```javascript
// 인덱스로 삭제 (1-based)
layer.Effects.property(1).remove();

// 이름으로 삭제
layer.Effects.property("Gaussian Blur").remove();
```

### 이펙트 목록 가져오기
```javascript
var fx = layer.Effects;
for (var i = 1; i <= fx.numProperties; i++) {
    var e = fx.property(i);
    // e.name - 표시 이름
    // e.matchName - 매치 이름 (스크립트용)
}
```

### Effect Controls 패널 감지
```javascript
var v = app.activeViewer;
if (v && v.type == ViewerType.VIEWER_EFFECT_CONTROLS) {
    // Effect Controls 패널이 활성화됨
}
```

---

## 참고 문서 링크

### Adobe 공식 문서
- [After Effects Scripting Guide](https://ae-scripting.docsforadobe.dev/)
- [First-Party Effect Match Names](https://ae-scripting.docsforadobe.dev/matchnames/effects/firstparty/)
- [PropertyBase (remove 메서드)](https://ae-scripting.docsforadobe.dev/property/propertybase/)
- [PropertyGroup (addProperty 메서드)](https://ae-scripting.docsforadobe.dev/property/propertygroup/)

### CEP 개발
- [Adobe CEP Resources](https://github.com/Adobe-CEP/CEP-Resources)
- [Adobe CEP Samples](https://github.com/Adobe-CEP/Samples)
- [After Effects Panel Sample](https://github.com/Adobe-CEP/Samples/tree/master/AfterEffectsPanel)
