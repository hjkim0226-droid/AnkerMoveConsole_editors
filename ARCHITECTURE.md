# Anchor Grid Architecture

## 시스템 개요

```
┌─────────────────────────────────────────────────────────────┐
│                        CEP 패널                              │
│                   (설정의 메인 컨트롤러)                       │
│                                                             │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│   │ Grid Size   │  │ Transparency│  │ Custom      │        │
│   │ W × H       │  │ Mark/Cell   │  │ Anchors     │        │
│   └─────────────┘  └─────────────┘  └─────────────┘        │
│                                                             │
│   ┌─────────────────────────────────────────────┐          │
│   │         Mode Buttons                        │          │
│   │   [Selection/Composition] [Mask ON/OFF]     │          │
│   └─────────────────────────────────────────────┘          │
└───────────────────────────┬─────────────────────────────────┘
                            │
                   CSXSEvent│ (실시간 동기화)
                    저장/로드│
                            ▼
                    ┌───────────────┐
                    │ settings.json │
                    │               │
                    │ - gridWidth   │
                    │ - gridHeight  │
                    │ - gridScale   │
                    │ - useCompMode │
                    │ - useMask...  │
                    │ - opacity...  │
                    │ - customAnch..│
                    └───────────────┘
                            ▲
                       읽기 │
┌───────────────────────────┴─────────────────────────────────┐
│                     Native UI 팝업                           │
│                  (Y키 홀드 시 표시)                           │
│                                                             │
│   ┌─────────┐  ┌─────────────────────┐  ┌─────────┐        │
│   │ Custom  │  │                     │  │  Comp   │        │
│   │ Anchor  │  │     Anchor Grid     │  │  Mode   │        │
│   │   1     │  │       (N×M)         │  │ Toggle  │        │
│   │   2     │  │                     │  │  Mask   │        │
│   │   3     │  │                     │  │ Toggle  │        │
│   └─────────┘  └─────────────────────┘  │Settings │        │
│                                         └─────────┘        │
│   ┌─────────────────────────────────┐                      │
│   │      [Copy]    [Paste]          │                      │
│   └─────────────────────────────────┘                      │
└─────────────────────────────────────────────────────────────┘
```

## 데이터 플로우

### 1. 설정 변경

```
CEP에서 설정 변경
    ↓
localStorage 저장
    ↓
settings.json 파일 저장 (saveToFile)
    ↓
C++ 그리드 호출 시 파일에서 로드 (LoadSettingsFromFile)
```

### 2. 모드 토글 (팝업에서)

```
팝업에서 모드 토글 클릭
    ↓
C++ 메모리 값 변경
    ↓
settings.json 파일 저장 (SaveSettingsToFile)
    ↓
ExtendScript 호출 (notifyModeChange)
    ↓
CSXSEvent 발송 (anchorGridModeChanged)
    ↓
CEP 리스너에서 즉시 UI 업데이트
```

### 3. 앵커 적용

```
그리드 셀 선택 (마우스 릴리즈)
    ↓
C++에서 ExtendScript 호출
    ↓
anchor.jsx 함수 실행 (setLayerAnchor 등)
    ↓
After Effects 레이어 앵커 포인트 수정
```

## 파일 구조

```
ae-anchor-radial-menu/
├── cpp/src/
│   ├── AnchorRadialMenu.cpp   # 메인 AEGP 플러그인
│   ├── NativeUI.cpp           # GDI+ 기반 팝업 UI
│   ├── NativeUI.h             # UI 인터페이스
│   └── KeyboardMonitor.h      # Y키 감지
│
├── cep/
│   ├── index.html             # CEP 패널 HTML
│   ├── manifest.xml           # CEP 설정
│   ├── css/style.css          # 스타일
│   ├── js/
│   │   ├── main.js            # 초기화 + 이벤트 리스닝
│   │   ├── settings.js        # 설정 관리 (저장/로드)
│   │   ├── custom-anchor.js   # 커스텀 앵커 에디터
│   │   ├── grid.js            # 그리드 프리뷰
│   │   └── i18n.js            # 다국어 지원
│   └── jsx/
│       └── anchor.jsx         # ExtendScript (AE 조작)
│
└── settings.json              # 공유 설정 파일
    (AppData/Adobe/CEP/extensions/com.anchor.grid/)
```

## 주요 기능

| 기능 | 위치 | 설명 |
|------|------|------|
| Y키 홀드 감지 | C++ | IdleHook에서 키 상태 모니터링 |
| 그리드 팝업 표시 | C++ | GDI+ Layered Window |
| 앵커 적용 | ExtendScript | sourceRectAtTime, fromComp 등 |
| 설정 저장 | CEP + C++ | settings.json 공유 |
| 실시간 동기화 | CSXSEvent | 토글 시 CEP 즉시 업데이트 |

## 모드 설명

### Selection Mode (기본)
- `sourceRectAtTime()`으로 레이어 바운딩 박스 계산
- 레이어 로컬 좌표 기준

### Composition Mode
- 컴포지션 width/height 기준 좌표 계산
- 레이어 transform 역산하여 로컬 좌표로 변환

### Mask Recognition
- 마스크가 있으면 마스크 바운드 우선
- 없으면 sourceRectAtTime 폴백
