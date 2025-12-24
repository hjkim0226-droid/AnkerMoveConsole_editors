# Layer & Text Module Enhancement Plan

## Overview

1. **Text 모듈 (D → T)**: UI 정리 및 간소화
2. **Layer 모듈 - Composition**: Un-Precompose, Deep Copy, Fit to Layers 구현
3. **Layer Preset System**: 레이어 프리셋 저장/적용 시스템

---

## Part 1: Research Summary (경쟁 플러그인 조사)

### Layer Preset 관련 플러그인

| Plugin | Key Features | Price |
|--------|-------------|-------|
| [Motion Bro](https://motionbro.com/) | 프리셋 브라우저, 실시간 미리보기, AI 통합, 1400+ 무료 프리셋 | Free / Paid packs |
| [FX Console](https://www.videocopilot.net/) | 핫키 검색, 이펙트 즐겨찾기, 빠른 적용 | Free |
| [Animation Composer](https://misterhorse.com/) | 100+ 무료 프리셋, 카테고리 정리, 드래그 적용 | Free / Paid packs |
| AE Native FFX | `.ffx` 파일 저장/적용, 키프레임+표현식 포함 | Built-in |

**핵심 인사이트:**
- 프리셋 미리보기가 사용자 경험에 중요
- 카테고리/태그 기반 정리 필요
- 검색 기능 필수

### Composition 관련 스크립트

| Script | Features | Price |
|--------|----------|-------|
| [True Comp Duplicator](https://aescripts.com/true-comp-duplicator/) | 컴프 계층 전체 복제, 폴더 구조 유지 | Pay What You Want |
| [Un-PreCompose](https://aescripts.com/un-precompose/) | 프리컴프에서 레이어 추출, 속성 유지 | Free |
| [Trim Comp to Contents](https://aescripts.com/trim-to-comp-contents/) | 컴프 길이를 레이어 범위에 맞춤 | $9.99 |
| rd_CompSetter | 여러 컴프 길이 일괄 조정 | Free |

**핵심 인사이트:**
- True Comp Duplicator: 중첩 컴프까지 완전 독립 복제
- Un-PreCompose: 레이어 위치/속성 유지하며 추출
- Trim to Contents: Work Area가 아닌 실제 레이어 기준

---

## Part 2: Text Module (D → T) UI Redesign

### Current State (문제점)

```
┌────────────────────────────────────────┐  340 x 260
│ Text Options    [layer name]   [📌][X] │  32px 헤더
├────────────────────────────────────────┤
│ Font                                   │  ← 라벨
│ [Font Dropdown────────────────▼] [★]  │  ← 폰트 + 프리셋
│ Color                                  │  ← 라벨
│ Fill [■] Stroke [■] Width [──100──]   │  ← 3개가 한 줄에
│ Size [──72pt──]  Tracking [──0──]     │  ← 2개가 한 줄에
│ Leading [──Auto──]                     │  ← 혼자
│ Align                                  │  ← 라벨
│ [L][C][R][JL][JC][JR][JF]             │  ← 7개 버튼
└────────────────────────────────────────┘
```

**문제점:**
1. 섹션 라벨(Font, Color, Align)이 각각 한 줄 차지 → 공간 낭비
2. 요소들의 정렬이 불규칙 (어떤 건 2개, 어떤 건 3개)
3. Leading이 혼자 떨어져 있어 어색
4. 전체적으로 밀도가 낮고 산만한 느낌

### New Design (제안)

```
┌────────────────────────────────────────┐  320 x 200
│ TEXT ─ [layer name]            [📌][★] │  28px 헤더
├────────────────────────────────────────┤
│                                        │
│  Font  [Noto Sans KR Regular───────▼] │  ← 라벨+필드 같은 줄
│                                        │
│  ┌────────┐ ┌────────┐ ┌────────┐     │
│  │  Size  │ │Tracking│ │Leading │     │  ← 3열 균등 배치
│  │  72pt  │ │   0    │ │  Auto  │     │
│  └────────┘ └────────┘ └────────┘     │
│                                        │
│  Fill [■]  Stroke [■] [─2.0px─]       │  ← 색상 + 스트로크 폭
│                                        │
│  ┌──┐┌──┐┌──┐   ┌──┐┌──┐┌──┐┌──┐     │
│  │◀ ││≡ ││▶ │   │◀≡││≡≡││≡▶││≡≡│     │  ← 정렬 아이콘
│  └──┘└──┘└──┘   └──┘└──┘└──┘└──┘     │
│                                        │
└────────────────────────────────────────┘
```

**개선사항:**
1. 창 크기 축소 (340x260 → 320x200)
2. 섹션 라벨 제거, 필드 자체가 의미 전달
3. Size/Tracking/Leading을 3열 균등 배치
4. 정렬 버튼 아이콘화 (7개 → 시각적으로 명확)
5. 프리셋 버튼(★)을 헤더로 이동

### Layout Constants (변경)

```cpp
// Before
static const int WINDOW_WIDTH = 340;
static const int WINDOW_HEIGHT = 260;
static const int HEADER_HEIGHT = 32;
static const int SECTION_HEIGHT = 28;

// After
static const int WINDOW_WIDTH = 320;
static const int WINDOW_HEIGHT = 200;
static const int HEADER_HEIGHT = 28;
static const int ROW_HEIGHT = 24;
static const int VALUE_BOX_WIDTH = 90;  // 3열 균등
```

---

## Part 3: Layer Module - Composition UI

### Pre-comp Layer UI (NEW!)

```
┌────────────────────────────────────────────────┐
│ PRECOMP ─ [Comp Name]                    [📌] │
├────────────────────────────────────────────────┤
│                                                │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ │
│  │    Un-     │ │    Deep    │ │  Fit Comp  │ │
│  │ Precompose │ │    Copy    │ │ to Layers  │ │
│  └─────1──────┘ └─────2──────┘ └─────3──────┘ │
│                                                │
│  ┌────────────┐                                │
│  │   Reset    │                                │
│  │ Transform  │                                │
│  └─────4──────┘                                │
│                                                │
└────────────────────────────────────────────────┘
```

**기능 설명:**

| 버튼 | 기능 | 설명 |
|-----|------|-----|
| Un-Precompose | 레이어 추출 | 프리컴프 내부 레이어를 현재 컴프로 꺼냄 |
| Deep Copy | 계층 복제 | 컴프 + 모든 서브컴프를 완전히 독립 복제 |
| Fit Comp to Layers | 길이 맞춤 | 컴프 길이를 레이어 범위에 맞춤 (축소/확장) |
| Reset Transform | 초기화 | 위치/스케일/회전 초기화 |

---

## Part 4: Feature Specifications

### 4.1 Layer Preset System

#### 프리셋 저장 (Save Preset)

```javascript
// 저장할 정보
{
    "name": "Custom Typewriter",
    "layerType": "text",
    "category": "animator",
    "properties": {
        // 선택된 속성들의 키프레임, 표현식 등
    },
    "effects": [...],
    "thumbnail": "base64_encoded_preview"
}
```

**저장 방식:**
1. **FFX 연동**: AE 네이티브 프리셋 시스템 활용
2. **JSON 저장**: `~/.ae-anchor/presets/layer/` 폴더에 저장
3. **미리보기 생성**: 썸네일 자동 생성 (옵션)

#### 프리셋 적용 (Apply Preset)

```javascript
// ExtendScript
function applyLayerPreset(presetPath) {
    var layer = app.project.activeItem.selectedLayers[0];
    var presetFile = new File(presetPath);
    if (presetFile.exists) {
        layer.applyPreset(presetFile);
    }
}
```

### 4.2 Composition Features

#### 4.2.1 Un-Precompose (언프리컴프)

**기능:** 선택한 프리컴프 레이어의 내부 레이어들을 현재 컴프로 추출

```javascript
// ExtendScript (개념)
function unPrecompose() {
    var comp = app.project.activeItem;
    var precompLayer = comp.selectedLayers[0];
    var precomp = precompLayer.source;

    // 프리컴프 내부 레이어들을 현재 컴프에 복사
    for (var i = 1; i <= precomp.numLayers; i++) {
        var layer = precomp.layer(i);
        // 레이어 복제 및 위치 조정
        // 프리컴프의 transform 적용
    }

    // 원본 프리컴프 레이어 삭제 (옵션)
}
```

**고려사항:**
- 프리컴프 Transform을 내부 레이어에 적용
- 3D 레이어 처리
- 표현식 참조 조정
- 카메라/라이트 처리

#### 4.2.2 Deep Copy (컴프 계층 복제)

**기능:** True Comp Duplicator 스타일로 컴프와 모든 서브컴프를 완전 독립적으로 복제

```javascript
// ExtendScript (개념)
function deepCopyComp(sourceComp, suffix) {
    suffix = suffix || "_copy";
    var duplicatedComps = {};  // 이미 복제된 컴프 추적

    function duplicateRecursive(comp) {
        if (duplicatedComps[comp.id]) {
            return duplicatedComps[comp.id];
        }

        // 컴프 복제
        var newComp = comp.duplicate();
        newComp.name = comp.name + suffix;
        duplicatedComps[comp.id] = newComp;

        // 내부 프리컴프 레이어들도 재귀적으로 복제
        for (var i = 1; i <= newComp.numLayers; i++) {
            var layer = newComp.layer(i);
            if (layer.source instanceof CompItem) {
                var newSubComp = duplicateRecursive(layer.source);
                layer.replaceSource(newSubComp, false);
            }
        }

        return newComp;
    }

    return duplicateRecursive(sourceComp);
}
```

**옵션:**
- 접미사 지정 (_copy, _v2, etc.)
- 폴더 구조 유지/복제
- 푸티지 참조 유지 vs 복제

#### 4.2.3 Fit Comp to Layers (컴프 길이 맞춤)

**기능:** 컴프 길이를 내부 레이어들의 실제 범위에 맞춤 (축소 + 확장 통합)

```javascript
// ExtendScript (개념)
function fitCompToLayers() {
    var comp = app.project.activeItem;
    if (!comp || !(comp instanceof CompItem)) return;

    // 모든 레이어의 in/out 포인트 수집
    var minIn = Infinity;
    var maxOut = 0;

    for (var i = 1; i <= comp.numLayers; i++) {
        var layer = comp.layer(i);
        minIn = Math.min(minIn, layer.inPoint);
        maxOut = Math.max(maxOut, layer.outPoint);
    }

    if (comp.numLayers === 0) return;  // 레이어 없으면 무시

    // 레이어 시작점이 0이 아니면 모든 레이어를 이동
    if (minIn > 0) {
        for (var i = 1; i <= comp.numLayers; i++) {
            var layer = comp.layer(i);
            layer.startTime -= minIn;
        }
        maxOut -= minIn;
        minIn = 0;
    }

    // 컴프 길이를 레이어 범위에 맞춤
    comp.duration = maxOut;
}
```

**동작:**
- 레이어가 0초 이후에 시작하면 → 모든 레이어를 앞으로 당김
- 레이어가 컴프 끝을 넘어가면 → 컴프 확장
- 컴프가 레이어보다 길면 → 컴프 축소

---

## Part 5: Implementation Plan

### Phase 1: Text 모듈 UI 리팩토링

| Task | Priority | Status |
|------|----------|--------|
| 레이아웃 재설계 (3열 균등) | High | 🔲 |
| Size/Tracking/Leading 한 줄로 | High | 🔲 |
| 섹션 라벨 제거/통합 | Medium | 🔲 |
| 정렬 버튼 아이콘화 | Medium | 🔲 |
| 창 크기 최적화 (320x200) | Medium | 🔲 |

### Phase 2: Layer 모듈 - Composition 기능

| Task | Priority | Status |
|------|----------|--------|
| Pre-comp UI 추가 (CompUI.cpp) | High | 🔲 |
| Un-Precompose ExtendScript | High | 🔲 |
| Deep Copy ExtendScript | High | 🔲 |
| Fit Comp to Layers ExtendScript | High | 🔲 |
| C++ ↔ ExtendScript 연결 | High | 🔲 |

### Phase 3: Layer Preset System (선택)

| Task | Priority | Status |
|------|----------|--------|
| 프리셋 저장 로직 | Medium | 🔲 |
| 프리셋 불러오기 로직 | Medium | 🔲 |
| 프리셋 미리보기 생성 | Low | 🔲 |
| 프리셋 관리 UI | Low | 🔲 |

---

## Part 6: File Structure

```
cpp/src/modules/
├── text/
│   ├── TextUI.h          # 텍스트 모듈 헤더
│   └── TextUI.cpp        # 텍스트 모듈 UI (리팩토링 대상)
├── comp/
│   ├── CompUI.h          # 레이어 모듈 헤더
│   └── CompUI.cpp        # 레이어 모듈 UI (Precomp 기능 추가)

cep/jsx/
├── compOperations.jsx    # Un-Precompose, Deep Copy, Fit to Layers
├── layerDetection.jsx    # 레이어 타입 감지 (기존)
└── layerPresets.jsx      # 레이어 프리셋 저장/적용 (Phase 3)

~/.ae-anchor/
├── presets/
│   ├── layer/            # 레이어 프리셋
│   └── text/             # 텍스트 스타일 프리셋
└── config.json
```

---

## Part 7: Summary

### 핵심 변경사항

| 영역 | 변경 내용 |
|-----|----------|
| **Text 모듈** | UI 간소화, 3열 그리드, 섹션 라벨 제거 |
| **Layer 모듈** | Pre-comp 기능 추가 (Un-Precompose, Deep Copy, Fit) |
| **Preset** | 레이어 프리셋 저장/적용 시스템 (Phase 3) |

### 구현 우선순위

1. **Phase 1**: Text 모듈 UI 리팩토링
2. **Phase 2**: Layer 모듈 Composition 기능 (Un-Precompose, Deep Copy, Fit)
3. **Phase 3**: Preset 시스템 (선택)

### 참고 자료

- [True Comp Duplicator](https://aescripts.com/true-comp-duplicator/)
- [Un-PreCompose](https://aescripts.com/un-precompose/)
- [Trim Comp to Contents](https://aescripts.com/trim-to-comp-contents/)
- [Motion Bro](https://motionbro.com/)
- [AE Animation Presets Guide](https://helpx.adobe.com/after-effects/using/effects-animation-presets-overview.html)

---

## Part 8: Dynamic Action System (동적 액션 시스템)

### 개요

**목표:** 사용자가 레이어 타입별 액션을 커스터마이징 가능하게
- 순서 변경 (자주 쓰는 기능을 1번에)
- 활성화/비활성화 (안 쓰는 기능 숨기기)
- 위치 기반 단축키 (보이는 순서 = 1,2,3...)

### 현재 구조 (하드코딩)

```cpp
// CompUI.cpp - 고정된 배열
static const ButtonInfo TEXT_BUTTONS[] = {
    {L"Typewriter", L"1", ...},  // 항상 1번
    {L"Fade In", L"2", ...},     // 항상 2번
    ...
};
```

**문제점:**
- 순서 고정 (사용자 선호도 무시)
- 기능 추가 시 코드 수정 필요
- 안 쓰는 기능도 항상 표시

### 새로운 구조 (동적)

```
┌─────────────────────────────┐
│ LAYER ─ Text Layer    [📌] │
├─────────────────────────────┤
│ 1  [Fade In           ]     │  ← 사용자가 1번으로 설정
│ 2  [Typewriter        ]     │  ← 2번으로 이동
│ 3  [Tracking          ]     │  ← 3번
│    (Scale, Blur 비활성화)    │
└─────────────────────────────┘
```

### 설정 파일 구조

**위치:** `~/.ae-anchor/layer-actions.json`

```json
{
  "version": 1,
  "layerTypes": {
    "text": {
      "actions": [
        {"id": "fadeIn", "enabled": true},
        {"id": "typewriter", "enabled": true},
        {"id": "tracking", "enabled": true},
        {"id": "scale", "enabled": false},
        {"id": "blur", "enabled": false}
      ]
    },
    "shape": {
      "actions": [
        {"id": "trimPath", "enabled": true},
        {"id": "repeater", "enabled": true},
        {"id": "wigglePath", "enabled": true},
        {"id": "wiggleTransform", "enabled": false}
      ]
    },
    "solid": {
      "actions": [
        {"id": "changeColor", "enabled": true},
        {"id": "fitToComp", "enabled": true},
        {"id": "resetTransform", "enabled": true}
      ]
    },
    "footage": {
      "actions": [
        {"id": "loopCycle", "enabled": true},
        {"id": "loopPingPong", "enabled": true},
        {"id": "lastFrameHold", "enabled": true},
        {"id": "resetTransform", "enabled": true}
      ]
    },
    "precomp": {
      "actions": [
        {"id": "unPrecompose", "enabled": true},
        {"id": "deepCopy", "enabled": true},
        {"id": "fitToLayers", "enabled": true},
        {"id": "resetTransform", "enabled": true}
      ]
    },
    "null": {
      "actions": [
        {"id": "resetTransform", "enabled": true}
      ]
    },
    "camera": {
      "actions": [
        {"id": "resetPosition", "enabled": true}
      ]
    },
    "light": {
      "actions": [
        {"id": "resetPosition", "enabled": true}
      ]
    }
  }
}
```

### 액션 정의 테이블 (C++)

```cpp
// 모든 가능한 액션 정의 (마스터 테이블)
struct ActionDefinition {
    const char* id;           // JSON에서 사용하는 ID
    const wchar_t* label;     // 버튼 텍스트
    const wchar_t* desc;      // 설명
    LayerAction actionEnum;   // 기존 enum 값
    LayerType allowedTypes;   // 허용되는 레이어 타입 (bitflag)
};

static const ActionDefinition ALL_ACTIONS[] = {
    // Text actions
    {"typewriter",     L"Typewriter",      L"Animate text typing",        ACTION_TEXT_ANIMATOR_TYPEWRITER,  LAYER_TEXT},
    {"fadeIn",         L"Fade In",         L"Fade in characters",         ACTION_TEXT_ANIMATOR_FADE,        LAYER_TEXT},
    {"scale",          L"Scale",           L"Scale characters",           ACTION_TEXT_ANIMATOR_SCALE,       LAYER_TEXT},
    {"blur",           L"Blur",            L"Blur characters",            ACTION_TEXT_ANIMATOR_BLUR,        LAYER_TEXT},
    {"tracking",       L"Tracking",        L"Animate tracking",           ACTION_TEXT_ANIMATOR_TRACKING,    LAYER_TEXT},

    // Shape actions
    {"trimPath",       L"Trim Path",       L"Add trim paths",             ACTION_SHAPE_TRIM_PATH,           LAYER_SHAPE},
    {"repeater",       L"Repeater",        L"Add repeater",               ACTION_SHAPE_REPEATER,            LAYER_SHAPE},
    {"wigglePath",     L"Wiggle Path",     L"Add wiggle paths",           ACTION_SHAPE_WIGGLE_PATH,         LAYER_SHAPE},
    {"wiggleTransform",L"Wiggle Transform",L"Add wiggle transform",       ACTION_SHAPE_WIGGLE_TRANSFORM,    LAYER_SHAPE},

    // Solid actions
    {"changeColor",    L"Change Color",    L"Change solid color",         ACTION_SOLID_CHANGE_COLOR,        LAYER_SOLID | LAYER_ADJUSTMENT},
    {"fitToComp",      L"Fit to Comp",     L"Match comp dimensions",      ACTION_SOLID_FIT_TO_COMP,         LAYER_SOLID | LAYER_ADJUSTMENT},

    // Footage/Precomp actions
    {"loopCycle",      L"Loop (Cycle)",    L"Loop with cycle",            ACTION_FOOTAGE_LOOP_CYCLE,        LAYER_FOOTAGE},
    {"loopPingPong",   L"Loop (Ping Pong)",L"Loop back and forth",        ACTION_FOOTAGE_LOOP_PINGPONG,     LAYER_FOOTAGE},
    {"lastFrameHold",  L"Last Frame Hold", L"Freeze last frame",          ACTION_FOOTAGE_LAST_FRAME_HOLD,   LAYER_FOOTAGE},

    // Precomp-specific actions
    {"unPrecompose",   L"Un-Precompose",   L"Extract layers from precomp",ACTION_PRECOMP_UNPRECOMPOSE,      LAYER_PRECOMP},
    {"deepCopy",       L"Deep Copy",       L"Duplicate comp hierarchy",   ACTION_PRECOMP_DEEP_COPY,         LAYER_PRECOMP},
    {"fitToLayers",    L"Fit to Layers",   L"Fit comp to layer range",    ACTION_PRECOMP_FIT_TO_LAYERS,     LAYER_PRECOMP},

    // Common actions
    {"resetTransform", L"Reset Transform", L"Reset pos/scale/rot",        ACTION_RESET_TRANSFORM,           LAYER_ALL},
    {"resetPosition",  L"Reset Position",  L"Reset position only",        ACTION_RESET_POSITION,            LAYER_CAMERA | LAYER_LIGHT},
};
```

### C++ 동적 로딩

```cpp
// 동적 버튼 벡터
static std::vector<const ActionDefinition*> g_activeActions;

// 설정에서 버튼 로드
void LoadActionsFromConfig(LayerType type, const char* jsonConfig) {
    g_activeActions.clear();

    // JSON 파싱해서 해당 레이어 타입의 활성화된 액션만 순서대로 추가
    // ... JSON 파싱 로직 ...

    for (const auto& actionId : enabledActionIds) {
        for (const auto& def : ALL_ACTIONS) {
            if (strcmp(def.id, actionId) == 0 && (def.allowedTypes & type)) {
                g_activeActions.push_back(&def);
                break;
            }
        }
    }
}

// 위치 기반 단축키 처리
void HandleKeyPress(wchar_t key) {
    if (key >= L'1' && key <= L'9') {
        int index = key - L'1';
        if (index < g_activeActions.size()) {
            ExecuteAction(g_activeActions[index]->actionEnum);
        }
    }
}
```

### CEP 설정 UI

```
┌─────────────────────────────────────┐
│ Layer Actions Settings              │
├─────────────────────────────────────┤
│ Layer Type: [Text Layer         ▼] │
├─────────────────────────────────────┤
│                                     │
│ ☑ ≡ Fade In              [↑][↓]   │  1
│ ☑ ≡ Typewriter           [↑][↓]   │  2
│ ☑ ≡ Tracking             [↑][↓]   │  3
│ ☐ ≡ Scale                [↑][↓]   │  (숨김)
│ ☐ ≡ Blur                 [↑][↓]   │  (숨김)
│                                     │
├─────────────────────────────────────┤
│ [Reset to Default]  [Save]          │
└─────────────────────────────────────┘
```

**UI 기능:**
- 드롭다운: 레이어 타입 선택
- 체크박스: 활성화/비활성화
- ↑↓ 버튼: 순서 변경
- 드래그: 순서 변경 (선택)
- Reset: 기본값 복원

### CEP ↔ C++ 통신

```javascript
// CEP에서 설정 변경 시
function saveLayerActionConfig(layerType, actions) {
    // 1. JSON 파일에 저장
    var configPath = getUserDataFolder() + "/layer-actions.json";
    saveJSON(configPath, config);

    // 2. C++ 플러그인에 알림
    csInterface.evalScript('setLayerActionConfig("' + layerType + '", ' + JSON.stringify(actions) + ')');
}

// C++ 측 (SnapPlugin.cpp)
void SetLayerActionConfig(const char* layerType, const char* actionsJson) {
    // 설정 캐시 업데이트
    g_layerActionConfigs[layerType] = ParseActionsJson(actionsJson);

    // CompUI에 알림
    CompUI::RefreshActionsForType(GetLayerTypeEnum(layerType));
}
```

### 구현 단계

| Phase | Task | 상태 |
|-------|------|------|
| 1.1 | 설정 파일 구조 정의 (`layer-actions.json`) | 🔲 |
| 1.2 | 기본 설정 파일 생성 로직 | 🔲 |
| 2.1 | `ActionDefinition` 마스터 테이블 | 🔲 |
| 2.2 | 동적 버튼 벡터 (`g_activeActions`) | 🔲 |
| 2.3 | JSON 파싱 및 로딩 | 🔲 |
| 2.4 | 위치 기반 단축키 | 🔲 |
| 3.1 | CEP 설정 UI 레이아웃 | 🔲 |
| 3.2 | 순서 변경 기능 | 🔲 |
| 3.3 | 활성화/비활성화 토글 | 🔲 |
| 3.4 | 설정 저장/로드 | 🔲 |

### 장점

| 측면 | 효과 |
|------|------|
| **직관성** | 보이는 순서 = 단축키 (1,2,3...) |
| **개인화** | 자주 쓰는 기능을 1번에 배치 |
| **깔끔함** | 안 쓰는 기능 숨기기 가능 |
| **확장성** | 새 기능 추가 시 코드 변경 최소화 |
| **부담 감소** | 개발자: 기능만 추가, 사용자: 원하는 것만 활성화 |

---

*Created: 2024-12-24*
*Updated: 2024-12-24 (동적 액션 시스템 추가)*
