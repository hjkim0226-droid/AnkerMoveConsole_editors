# Module Classification & Terminology

## Module Types

### Tool Modules (도구 모듈)
복잡한 작업을 수행하는 패널 형태의 UI. 핀 버튼으로 고정 가능.

| Module | Trigger | Description |
|--------|---------|-------------|
| **Grid** | Y 0.4s hold | Anchor Point 그리드 선택 |
| **Control** | Shift+E | Effect 검색/관리 |
| **Keyframe** | Right Shift+K 또는 D→K | 키프레임 이징 커브 편집 |
| **Align** | D→A | 레이어 정렬 |
| **Text** | D→T | 텍스트 속성 편집 |

**공통 특성:**
- 핀 버튼: 창 고정 (외부 클릭시 닫히지 않음)
- 헤더 드래그: 창 이동
- ESC: 닫기
- 토글 모드: 같은 키로 열기/닫기

### Popup Menus (팝업 메뉴)
단순 선택만 제공하는 일시적 UI.

| Menu | Trigger | Description |
|------|---------|-------------|
| **DMenu** | D | 모듈 선택 메뉴 (A/T/K) |

**공통 특성:**
- 핀 버튼 없음
- 포커스 잃으면 닫힘
- 키보드 단축키로 선택

---

## Terminology (용어 정의)

### Trigger Types (트리거 방식)

| 용어 | 설명 | 예시 |
|------|------|------|
| **Hold** | 일정 시간 키를 누르고 있으면 실행 | Y 0.4s hold |
| **Instant** | 키 누르는 즉시 실행 | Shift+E |
| **Combo** | 수식키 + 키 조합 | Right Shift+K |
| **Menu** | 메뉴를 통해 선택 | D→K (D 누르고 K 선택) |

### UI Elements (UI 요소)

| 용어 | 설명 |
|------|------|
| **Pin Button** | 창 고정 토글 버튼 (압정 아이콘) |
| **Close Button** | 창 닫기 버튼 (X 아이콘) |
| **Apply Button** | 변경사항 적용 버튼 |
| **Load Button** | 현재 선택된 데이터 새로고침 |
| **Save Button** | 프리셋 저장 모드 진입 |
| **Slot** | 프리셋 저장 슬롯 |

### View Modes (뷰 모드)

| 용어 | 설명 | 적용 모듈 |
|------|------|----------|
| **Solo View** | 키프레임 2개 선택 (단일 구간) | Keyframe |
| **Multi View** | 키프레임 3개+ 선택 (다중 구간) | Keyframe |
| **Sync Toggle** | 중간 키프레임의 In/Out 동기화 | Keyframe (Multi View) |

### Data Flow (데이터 흐름)

| 용어 | 설명 |
|------|------|
| **Parse** | JSON 문자열을 구조체로 변환 |
| **Load** | ExtendScript로 AE에서 데이터 가져오기 |
| **Apply** | 수정된 데이터를 AE에 적용 |
| **Preset** | 미리 정의된 설정값 (Built-in 또는 Custom) |

---

## Module Criteria (모듈 기준)

### Tool Module이 되기 위한 조건
1. **복잡한 상호작용**: 단순 선택 이상의 조작 필요
2. **상태 유지**: 여러 옵션/값을 편집
3. **미리보기**: 적용 전 결과 확인 가능
4. **프리셋 시스템**: 저장/불러오기 기능

### Popup Menu가 되기 위한 조건
1. **단순 선택**: 옵션 중 하나 선택
2. **일시적 표시**: 선택 후 즉시 닫힘
3. **빠른 접근**: 최소한의 조작

---

## Architecture (아키텍처)

```
cpp/src/
├── core/
│   ├── SnapPlugin.cpp      # 메인 플러그인 진입점 + 키 이벤트 처리
│   ├── KeyboardMonitor.cpp # 키보드 상태 감시 (Hook)
│   └── CEPBridge.cpp       # ExtendScript 통신
│
└── modules/
    ├── grid/               # Y key - Anchor Grid
    │   ├── GridUI.h
    │   └── GridUI.cpp
    ├── control/            # Shift+E - Effect Search
    │   ├── ControlUI.h
    │   └── ControlUI.cpp
    ├── keyframe/           # Right Shift+K - Keyframe Easing
    │   ├── KeyframeUI.h
    │   └── KeyframeUI.cpp
    ├── align/              # D→A - Layer Align
    │   ├── AlignUI.h
    │   └── AlignUI.cpp
    ├── text/               # D→T - Text Properties
    │   ├── TextUI.h
    │   └── TextUI.cpp
    └── dmenu/              # D key - Quick Menu
        ├── DMenuUI.h
        └── DMenuUI.cpp
```

---

## Naming Conventions (네이밍 규칙)

### Files
- `ModuleUI.h/cpp`: 각 모듈의 UI 구현
- `GdiPlusIncludes.h`: 공통 GDI+ 헤더

### Variables
- `g_moduleName`: 글로벌 상태 변수
- `g_isVisible`: 표시 상태
- `g_result`: 결과 구조체
- `g_hwnd`: 윈도우 핸들

### Functions
- `Initialize()`: 모듈 초기화
- `Shutdown()`: 모듈 정리
- `ShowPanel()`: 패널 표시
- `HidePanel()`: 패널 숨김
- `GetResult()`: 결과 반환
- `UpdateHover()`: 마우스 호버 업데이트

---

## Key Bindings Summary (단축키 요약)

| Key | Action |
|-----|--------|
| **Y (0.4s hold)** | Grid 패널 열기 |
| **Shift+E** | Control 패널 열기/닫기 |
| **Right Shift+K** | Keyframe 패널 열기/닫기 |
| **D** | DMenu 열기 |
| **D→A** | Align 패널 열기 |
| **D→T** | Text 패널 열기 |
| **D→K** | Keyframe 패널 열기 |
| **ESC** | 현재 패널 닫기 |

---

## Version History

- **v1.0.0**: Grid 모듈 완성
- **v1.1.0**: Control 모듈 (Shift+E)
- **v1.2.0**: Keyframe 모듈 (Right Shift+K, Load 버튼)
