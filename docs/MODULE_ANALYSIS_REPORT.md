# AnchorSnap 플러그인 전체 모듈 분석 보고서

**작성일**: 2025-12-27
**버전**: dev (commit 650f992)
**빌드 상태**: 성공

---

## 프로젝트 개요

| 항목 | 값 |
|------|-----|
| **총 코드 라인** | 12,831 줄 |
| **모듈 수** | 8개 (Core + 7개 UI 모듈) |
| **플랫폼** | Windows (AEGP Plugin) |
| **UI 프레임워크** | Win32 + GDI+ |

---

## 모듈별 상세 분석

### 1. Grid 모듈 (982 줄)

**파일**: `modules/grid/GridUI.cpp|.h`
**트리거**: Y 키 0.4초 홀드

| 기능 | 상태 | 설명 |
|------|------|------|
| 3x3 ~ 7x7 그리드 | 정상 | 설정 가능한 그리드 크기 |
| 커스텀 앵커 1/2/3 | 정상 | 왼쪽 사이드 패널 |
| Comp 모드 | 정상 | 오렌지 색상 팔레트 |
| Mask 모드 | 정상 | 마스크 인식 토글 |
| Copy/Paste 앵커 | 정상 | 앵커 비율 복사/붙여넣기 |
| GDI+ 렌더링 | 정상 | 안티앨리어싱, 알파 블렌딩 |

**강점**:
- 투명 배경 지원 (`WS_EX_LAYERED`)
- 멀티 모니터 대응 (`MonitorFromPoint`)
- 스케일 팩터 지원

---

### 2. DMenu 모듈 (440 줄)

**파일**: `modules/dmenu/DMenuUI.cpp|.h`
**트리거**: D 키

| 기능 | 상태 | 설명 |
|------|------|------|
| [A] Align | 정상 | 정렬 모듈 호출 |
| [T] Text | 정상 | 텍스트 모듈 호출 |
| [K] Keyframe | 정상 | 키프레임 모듈 호출 |
| [C] Layer | 정상 | 레이어 모듈 호출 |
| [G] Settings | 정상 | 설정 패널 호출 |
| ESC 닫기 | 수정됨 | `CheckAndCloseLostFocus()` 추가 |
| 외부 클릭 닫기 | 수정됨 | Grace period 적용 |

**최근 수정** (버그 픽스):
- `CheckAndCloseLostFocus()` 함수 추가 - IdleHook에서 fallback 체크
- Grace period (300ms) 적용 - D키 릴리즈 시 닫힘 방지

---

### 3. Align 모듈 (858 줄)

**파일**: `modules/align/AlignUI.cpp|.h`
**트리거**: D → A

| 기능 | 상태 | 설명 |
|------|------|------|
| 정렬 (6방향) | 정상 | Left/Center/Right/Top/Middle/Bottom |
| 분배 (H/V) | 정상 | 수평/수직 분배 |
| Selection 모드 | 정상 | 선택된 레이어 기준 |
| Composition 모드 | 정상 | 컴프 경계 기준 |
| 핀 버튼 | 정상 | 패널 고정 |
| Ctrl+Z 전달 | 정상 | AE에 Undo/Redo 전달 |

**강점**:
- 시각적 아이콘 (GDI+ 벡터 드로잉)
- 키보드 단축키 (1-6, H/V)

---

### 4. Text 모듈 (2,453 줄) - 가장 큰 모듈

**파일**: `modules/text/TextUI.cpp|.h`
**트리거**: D → T

| 기능 | 상태 | 설명 |
|------|------|------|
| Font Size | 정상 | 드래그 스크러빙 + 더블클릭 직접입력 |
| Tracking | 정상 | 자간 조절 |
| Leading | 정상 | 행간 조절 (0=Auto) |
| Stroke Width | 정상 | 획 두께 |
| Fill Color | 정상 | 컬러 피커 팝업 |
| Stroke Color | 정상 | 컬러 피커 팝업 |
| Font Family | 정상 | 드롭다운 + 검색 |
| Justification | 정상 | 7가지 정렬 옵션 |
| **Ctrl+C/V** | **신규** | 스타일 복사/붙여넣기 |
| 프리셋 저장/로드 | 정상 | 드롭다운 메뉴 |
| 핀 버튼 | 정상 | 패널 고정 |

**최근 수정**:
- `NeedsRefresh()` 함수 추가 - 자동 새로고침
- `g_colorPickerOpen` 체크 추가 - 컬러피커 열릴 때 닫힘 방지
- **Ctrl+C/V 스타일 복사/붙여넣기 기능 추가**

**구현 상세** (스타일 복사/붙여넣기):
```
복사되는 속성:
- fontSize, tracking, leading, strokeWidth
- fillColor, strokeColor (applyFill/Stroke 조건부)
- justification
- font (PostScript name 검색으로 정확한 폰트 적용)
```

---

### 5. Keyframe 모듈 (1,984 줄)

**파일**: `modules/keyframe/KeyframeUI.cpp|.h`
**트리거**: D → K 또는 RShift 홀드

| 기능 | 상태 | 설명 |
|------|------|------|
| 베지어 커브 에디터 | 정상 | 드래그로 제어점 조절 |
| Velocity 그래프 | 정상 | 속도 시각화 |
| 프리셋 (5종) | 정상 | Linear/Ease In/Out/InOut/OutIn |
| 커스텀 슬롯 (4개) | 정상 | 사용자 정의 커브 저장 |
| AE ↔ Bezier 변환 | 정상 | Speed/Influence 변환 |
| Hold/Linear 타입 | 정상 | 특수 보간 처리 |

**강점**:
- Simpson's rule 적분으로 정확한 AE 값 계산
- JSON 파서 내장 (외부 라이브러리 불필요)
- 실시간 미리보기

---

### 6. Control 모듈 (1,870 줄)

**파일**: `modules/control/ControlUI.cpp|.h`
**트리거**: Shift+E

| 기능 | 상태 | 설명 |
|------|------|------|
| **Mode 1: 검색** | 정상 | 이펙트 검색 & 추가 |
| **Mode 2: 효과 목록** | 정상 | Effect Controls 포커스 시 |
| 이펙트 삭제 | 정상 | 휴지통 아이콘 클릭 |
| 이펙트 복제 | 정상 | 복제 버튼 |
| 이펙트 이동 | 정상 | 위/아래 이동 |
| 프리셋 슬롯 (6개) | 정상 | 이펙트 조합 저장/로드 |
| 텍스트 커서 | 정상 | 깜빡이는 캐럿 |
| 선택 범위 | 정상 | 드래그로 텍스트 선택 |

**강점**:
- 동적 이펙트 목록 (AE에서 로드, 로컬라이즈 지원)
- 아이콘 선택기 (프리셋 슬롯 커스터마이징)
- 레이어 레이블 색상 표시

---

### 7. Comp/Layer 모듈 (1,032 줄)

**파일**: `modules/comp/CompUI.cpp|.h`
**트리거**: D → C

| 레이어 타입 | 지원 액션 |
|------------|----------|
| **Text** | Typewriter, Fade In, Scale, Blur, Tracking |
| **Shape** | Trim Path, Repeater, Wiggle Path, Wiggle Transform |
| **Solid** | Change Color, Fit to Comp |
| **Footage** | Loop Cycle/PingPong, Last Frame Hold |
| **Precomp** | Un-Precompose, Deep Copy, Fit to Layers |
| **Camera/Light** | Reset Position |

**강점**:
- Master Action Table 기반 확장 가능한 설계
- CEP 설정 파일 연동 (동적 액션 활성화)
- 레이어 타입별 컬러 코딩

---

## Core 모듈

### SnapPlugin.cpp (2,949 줄)

**역할**: 모든 모듈 통합, IdleHook, UpdateMenuHook, ExtendScript 실행

| 기능 | 설명 |
|------|------|
| `IdleHook` | 키보드 상태 폴링, 모듈 상태 관리 |
| `UpdateMenuHook` | 텍스트 편집 모드 감지 |
| `ExecuteScript` | ExtendScript 실행 (AEGP_ExecuteScript) |
| `ApplyText*` | Text 모듈용 헬퍼 함수들 |
| `IsAEForeground` | AE 포그라운드 체크 |
| `IsKeyInputAllowed` | 키 입력 허용 조건 |

### KeyboardMonitor.cpp (127 줄)

**역할**: OS 레벨 키보드/마우스 상태 조회

```
Windows: GetAsyncKeyState(), GetCursorPos()
macOS: CGEventSourceKeyState(), CGEventGetLocation()
```

> **중요**: AEGP에 대체 API가 없어 OS API 사용 필수

---

## 최근 수정된 버그

| 버그 | 원인 | 해결 |
|------|------|------|
| DMenu 안 닫힘 | SetForegroundWindow 실패 가능 | `CheckAndCloseLostFocus()` fallback |
| 컬러피커 → TextUI 닫힘 | WM_KILLFOCUS 미체크 | `g_colorPickerOpen` 조건 추가 |
| 텍스트 정보 갱신 안됨 | 새로고침 메커니즘 없음 | `NeedsRefresh()` + WM_ACTIVATE |
| AE 비활성화 시 키 훅킹 | 핸들 없을 때 true 반환 | false 반환으로 변경 |

---

## 코드 품질 분석

### 장점
1. **일관된 UI 가이드라인** - 색상 팔레트, 상수, 더블 버퍼링 표준화
2. **스케일 팩터 지원** - 모든 모듈에서 DPI 스케일링 지원
3. **메모리 관리** - `new`/`delete[]` 쌍 확인됨
4. **에러 핸들링** - GDI+ 초기화 실패 처리
5. **멀티 모니터** - `MonitorFromPoint` 사용

### 개선 고려사항
1. **macOS 미지원** - 모든 UI 모듈이 `#ifdef MSWindows` 스텁
2. **하드코딩된 문자열** - 일부 이펙트 이름이 영어로 고정
3. **JSON 파서** - 커스텀 구현 (외부 라이브러리 대비 제한적)

---

## 기능 요약 매트릭스

### 기본 UI 기능

| 모듈 | 트리거 | ESC 닫기 | 외부클릭 닫기 | 핀 버튼 | 창 드래그 |
|------|--------|:--------:|:------------:|:-------:|:--------:|
| **Grid** | Y(hold) | O | O | X | X |
| **DMenu** | D | O | O | X | X |
| **Align** | D→A | O | O | O | O |
| **Text** | D→T | O | O | O | O |
| **Keyframe** | D→K/RShift | O | O | O | O |
| **Control** | Shift+E | O | O | O | O |
| **Comp** | D→C | O | O | O | O |

### 입력/편집 기능

| 모듈 | 드래그 스크러빙 | 더블클릭 편집 | 검색 | 드롭다운 | 스크롤 |
|------|:--------------:|:------------:|:----:|:--------:|:------:|
| **Grid** | X | X | X | X | X |
| **DMenu** | X | X | X | X | X |
| **Align** | X | X | X | X | X |
| **Text** | O | O | O (폰트) | O (폰트/프리셋) | O |
| **Keyframe** | O (커브) | X | X | X | X |
| **Control** | X | X | O (이펙트) | X | O |
| **Comp** | X | X | X | X | X |

### 특수 기능

| 모듈 | Ctrl+Z 전달 | Ctrl+C/V | 프리셋 | 컬러피커 | 키보드 단축키 |
|------|:----------:|:--------:|:------:|:--------:|:------------:|
| **Grid** | X | O (앵커) | O (커스텀 1/2/3) | X | X |
| **DMenu** | X | X | X | X | O (A/T/K/C/G) |
| **Align** | O | X | X | X | O (1-6, H/V) |
| **Text** | O | O (스타일) | O | O | X |
| **Keyframe** | X | X | O (4슬롯) | X | X |
| **Control** | O | X | O (6슬롯) | X | O (↑↓ Enter) |
| **Comp** | X | X | X | X | X |

### 모드/상태

| 모듈 | 다중 모드 | 실시간 미리보기 | AE 연동 |
|------|:--------:|:--------------:|:-------:|
| **Grid** | O (Comp/Selection, Mask) | O (호버 하이라이트) | O (앵커 설정) |
| **DMenu** | X | X | X (라우터) |
| **Align** | O (Align/Distribute, Sel/Comp) | X | O (레이어 정렬) |
| **Text** | X | O (값 변경 즉시 적용) | O (텍스트 속성) |
| **Keyframe** | X | O (커브 에디터) | O (키프레임 이징) |
| **Control** | O (Search/Effects List) | X | O (이펙트 추가/삭제) |
| **Comp** | O (레이어 타입별 액션) | X | O (레이어 액션) |

---

## 결론

**전체 상태: 양호**

- 모든 7개 모듈이 정상 작동
- 최근 버그 수정 완료 (DMenu, TextUI, SnapPlugin)
- 스타일 복사/붙여넣기 신규 기능 추가 완료
- GitHub Actions 빌드 성공

---

## 참고

- **ESC 닫기**: `WM_KEYDOWN`에서 `VK_ESCAPE` 처리
- **외부 클릭 닫기**: `WM_ACTIVATE(WA_INACTIVE)` 또는 `WM_KILLFOCUS` 처리
- **핀 버튼**: 활성화 시 외부 클릭 닫기 비활성화 (`g_keepPanelOpen`)
