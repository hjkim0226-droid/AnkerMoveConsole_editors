# 구현 계획: Native AE API 리팩토링

**상태**: 🔄 진행 중
**시작일**: 2025-12-26
**최종 업데이트**: 2025-12-26
**예상 완료일**: 2025-12-28

---

**⚠️ 중요 지침**: 각 단계 완료 후:
1. ✅ 완료된 태스크 체크박스 체크
2. 🧪 모든 품질 게이트 검증 명령어 실행
3. ⚠️ 모든 품질 게이트 항목 통과 확인
4. 📅 상단의 "최종 업데이트" 날짜 업데이트
5. 📝 노트 섹션에 학습 내용 문서화
6. ➡️ 그 후에만 다음 단계로 진행

⛔ **품질 게이트를 건너뛰거나 실패한 체크로 진행하지 마세요**

---

## 📋 개요

### 기능 설명
ae-anchor-radial-menu 플러그인의 커스텀 함수들을 분석하여, Windows API 직접 호출을 AE SDK 네이티브 API로 대체하거나 최적화하는 리팩토링 작업.

### 현재 상태 분석 요약

| 기능 영역 | 현재 방식 | AE SDK 대체 가능성 | 우선순위 |
|---------|---------|-------------------|---------|
| 키 입력 감지 | GetAsyncKeyState (Win32) | UpdateMenuHook으로 보완됨 | 중 |
| 마우스 위치 | GetCursorPos (Win32) | AEGP API 없음 | 낮음 |
| AE 포그라운드 | GetForegroundWindow + AEGP_GetMainHWND | ✅ 이미 최적화됨 | 완료 |
| Effect 조작 | ExtendScript | AEGP EffectSuite (복잡) | 낮음 |
| 텍스트 속성 | ExtendScript | ExtendScript만 가능 | 유지 |
| 텍스트 편집 감지 | UpdateMenuHook + 타이머 | ✅ AEGP 네이티브 사용 중 | 완료 |
| 윈도우 생성 | Win32 API | AEGP API 없음 | 유지 |
| 미사용 함수 | 4개 Windows API 호출 | 제거 가능 | 높음 |

### 성공 기준
- [ ] 미사용 Windows API 호출 함수 제거
- [ ] UpdateMenuHook 기반 키 입력 감지 강화
- [ ] macOS 포팅 준비 (플랫폼 추상화)
- [ ] 코드 복잡도 감소 및 유지보수성 향상
- [ ] 기존 기능 100% 유지 (회귀 없음)

### 사용자 영향
- 성능: Windows API 호출 감소로 약간의 성능 향상
- 안정성: AE 내부 상태와 더 밀접하게 연동
- 유지보수: 코드베이스 정리로 향후 개발 용이

---

## 🏗️ 아키텍처 결정

| 결정 | 근거 | 트레이드오프 |
|------|------|-------------|
| ExtendScript 유지 | AEGP Suite보다 간단하고 충분히 빠름 | AEGP Suite 전환 시 미세한 성능 향상 포기 |
| Win32 윈도우 유지 | 네이티브 성능, AEGP 대체 API 없음 | CEP HTML 통일성 포기 |
| UpdateMenuHook 중심 | AE 내부 상태와 동기화, 텍스트 편집 자동 감지 | GetAsyncKeyState 즉시성 일부 포기 |
| 파일 기반 IPC 유지 | 현재 안정적으로 작동, 대체 복잡 | 소켓/파이프 성능 향상 포기 |

---

## 📦 의존성

### 시작 전 필요 사항
- [ ] 현재 빌드가 정상 작동 확인
- [ ] GitHub Actions 빌드 성공 확인
- [ ] 모든 모듈 기능 테스트 완료

### 외부 의존성
- AE SDK 25.6 (현재 사용 중)
- Windows SDK (Win32 API)
- GDI+ (UI 렌더링)

---

## 🧪 테스트 전략

### 테스팅 접근 방식
**수동 테스트 중심**: AEGP 플러그인은 자동화 테스트가 어려우므로 수동 테스트 수행

### 테스트 시나리오
| 테스트 | 검증 내용 |
|-------|----------|
| Y키 홀드 | Grid 모듈 정상 표시 |
| Shift+E | Control 모듈 정상 표시 |
| D키 | DMenu 표시 및 서브메뉴 작동 |
| 텍스트 편집 중 | 키 입력 훅킹 방지 |
| 다른 앱 활성화 | 키 입력 훅킹 방지 |
| ESC | 모든 모듈 닫기 |

---

## 🚀 구현 단계

### 1단계: 미사용 함수 제거 및 코드 정리
**목표**: 사용되지 않는 Windows API 호출 함수 제거로 코드 복잡도 감소
**예상 시간**: 1시간
**상태**: ⏳ 대기

#### 태스크

**🔴 RED: 현재 상태 확인**
- [ ] **태스크 1.1**: `FindEffectControlsWindow()` 호출 검색
  - 파일: `cpp/src/core/SnapPlugin.cpp` 줄 197-230
  - 예상: 호출하는 코드 없음 확인
  - Windows API: `EnumWindows`, `GetWindowTextW`, `IsWindowVisible`, `GetWindowRect`

- [ ] **태스크 1.2**: `CommandHook()` 기능 분석
  - 파일: `cpp/src/core/SnapPlugin.cpp` 줄 1406-1427
  - 예상: 디버그 로깅만 수행, 실제 기능 없음
  - 훅 등록: 줄 2871 `AEGP_RegisterCommandHook`

**🟢 GREEN: 함수 제거**
- [ ] **태스크 1.3**: `FindEffectControlsWindow()` 함수 삭제
  - 파일: `cpp/src/core/SnapPlugin.cpp`
  - 목표: 4개 Windows API 호출 제거
  - 상세: 줄 197-230 삭제

- [ ] **태스크 1.4**: `CommandHook()` 함수 및 등록 제거
  - 파일: `cpp/src/core/SnapPlugin.cpp`
  - 삭제 대상:
    - 함수 정의: 줄 1406-1427
    - 훅 등록: 줄 2871 `AEGP_RegisterCommandHook` 호출
  - 목표: 불필요한 훅 제거

**🔵 REFACTOR: 관련 코드 정리**
- [ ] **태스크 1.5**: Effect Controls 감지 방식 문서화
  - 파일: `cpp/src/core/SnapPlugin.cpp`
  - 목표: `IsEffectControlsFocused()` (줄 137-150)만 사용함을 주석으로 명시
  - 상세: ExtendScript 방식이 더 신뢰적임을 문서화

#### 품질 게이트 ✋

**⚠️ 정지: 모든 체크가 통과할 때까지 2단계로 진행하지 마세요**

**빌드 및 테스트**:
- [ ] GitHub Actions 빌드 성공
- [ ] 모든 모듈 정상 작동 (Y, Shift+E, D 키)
- [ ] Effect Controls 감지 정상 (Shift+E Mode 2)

**코드 품질**:
- [ ] 컴파일 경고 없음
- [ ] 삭제한 함수 참조 없음

**수동 테스트 체크리스트**:
- [ ] Y키 홀드 → Grid 표시
- [ ] Shift+E → Control 모듈 표시
- [ ] Effect Controls 포커스 시 → 이펙트 목록 표시
- [ ] D키 → DMenu 표시

---

### 2단계: 키 입력 감지 로직 최적화
**목표**: UpdateMenuHook 기반 감지 강화, GetAsyncKeyState 의존도 감소
**예상 시간**: 2시간
**상태**: ⏳ 대기

#### 태스크

**🔴 RED: 현재 구현 분석**
- [ ] **태스크 2.1**: 키 입력 감지 흐름 문서화
  - 파일: `cpp/src/core/SnapPlugin.cpp`, `cpp/src/core/KeyboardMonitor.cpp`
  - 현재 흐름:
    1. `IdleHook` 주기적 호출
    2. `GetAsyncKeyState(KEY_*)` 로 키 상태 확인
    3. `IsKeyInputAllowed()` 로 허용 여부 판단
    4. `IsMenuHookRecent() || IsInPanelActivationWindow()` 조건 확인

- [ ] **태스크 2.2**: UpdateMenuHook 호출 패턴 분석
  - 파일: `cpp/src/core/SnapPlugin.cpp` 줄 1434-1447
  - 분석: 어떤 키 입력 시 호출되는지 확인
  - 문서화: 텍스트 편집 중 미호출 패턴

**🟢 GREEN: 감지 로직 개선**
- [ ] **태스크 2.3**: `PANEL_ACTIVATION_WINDOW_MS` 값 조정
  - 파일: `cpp/src/core/SnapPlugin.cpp` 줄 68
  - 현재: 300ms (이미 1000→300 변경됨)
  - 테스트: 200ms, 400ms 비교 테스트

- [ ] **태스크 2.4**: 키 입력 감지 플로우 주석 추가
  - 파일: `cpp/src/core/SnapPlugin.cpp`
  - 목표: 새 개발자가 이해할 수 있도록 상세 주석
  - 위치: `IdleHook`, `UpdateMenuHook`, `IsKeyInputAllowed`

**🔵 REFACTOR: 코드 구조화**
- [ ] **태스크 2.5**: 키 입력 상태 관리 함수 분리
  - 파일: `cpp/src/core/SnapPlugin.cpp`
  - 목표: 관련 함수들을 그룹화
  - 체크리스트:
    - [ ] `IsMenuHookRecent()`
    - [ ] `IsInPanelActivationWindow()`
    - [ ] `IsKeyInputAllowed()`
    - [ ] `IsAEForeground()`
    - [ ] 주석 블록으로 그룹화

#### 품질 게이트 ✋

**빌드 및 테스트**:
- [ ] GitHub Actions 빌드 성공
- [ ] 텍스트 편집 중 키 훅킹 방지 확인
- [ ] 다른 앱에서 키 훅킹 방지 확인

**수동 테스트 체크리스트**:
- [ ] 텍스트 레이어 선택 → 텍스트 편집 시작 → D키 → DMenu 안 열림
- [ ] 다른 앱 활성화 → D키 → DMenu 안 열림
- [ ] AE 패널 클릭 → 300ms 이내 D키 → DMenu 열림

---

### 3단계: AE SDK API 참조 주석 추가
**목표**: 각 함수에 대응하는 AEGP API 또는 대체 불가 이유 문서화
**예상 시간**: 1시간
**상태**: ⏳ 대기

#### 태스크

**🔴 RED: API 매핑 표 작성**
- [ ] **태스크 3.1**: Windows API → AEGP API 매핑 표 작성
  - 문서: `docs/API_MAPPING.md` 생성
  - 내용:
    | Windows API | 사용 위치 | AEGP 대체 | 비고 |
    |------------|----------|----------|-----|
    | GetAsyncKeyState | KeyboardMonitor.cpp:56 | 없음 | 필수 |
    | GetCursorPos | KeyboardMonitor.cpp:67 | 없음 | 필수 |
    | GetForegroundWindow | SnapPlugin.cpp:1468 | AEGP_GetMainHWND 보조 | 개선됨 |
    | CreateWindowExW | 모든 모듈 UI | 없음 | 필수 |

**🟢 GREEN: 소스 코드 주석 추가**
- [ ] **태스크 3.2**: KeyboardMonitor.cpp에 API 주석 추가
  - 파일: `cpp/src/core/KeyboardMonitor.cpp`
  - 목표: 각 함수에 AEGP 대체 가능성 주석
  - 예시:
    ```cpp
    // AEGP 대체: 불가능
    // 이유: AEGP에 키보드 상태 조회 API 없음
    // 참고: UpdateMenuHook으로 키 입력 이벤트는 감지 가능
    bool IsKeyHeld(KeyCode keyCode) { ... }
    ```

- [ ] **태스크 3.3**: SnapPlugin.cpp 핵심 함수에 API 주석 추가
  - 파일: `cpp/src/core/SnapPlugin.cpp`
  - 대상 함수:
    - `IsAEForeground()`: AEGP_GetMainHWND 사용 중
    - `IsEffectControlsFocused()`: ExtendScript 사용 중
    - `IsTextToolActive()`: ExtendScript 사용 중
    - `ExecuteScript()`: AEGP_UtilitySuite 사용 중

**🔵 REFACTOR: 문서 정리**
- [ ] **태스크 3.4**: CLAUDE.md 업데이트
  - 파일: `CLAUDE.md`
  - 목표: API 매핑 섹션 추가
  - 내용: AEGP API 사용 현황, Windows API 필수 사용 목록

#### 품질 게이트 ✋

**문서화**:
- [ ] `docs/API_MAPPING.md` 생성됨
- [ ] 핵심 함수에 AEGP 관련 주석 추가됨
- [ ] CLAUDE.md 업데이트됨

**코드 품질**:
- [ ] 주석이 정확하고 이해하기 쉬움
- [ ] 빌드에 영향 없음

---

### 4단계: macOS 포팅 준비 (선택적)
**목표**: 플랫폼 분기 코드 정리 및 macOS 구현 준비
**예상 시간**: 2시간
**상태**: ⏳ 대기

#### 태스크

**🔴 RED: 현재 플랫폼 분기 분석**
- [ ] **태스크 4.1**: macOS 분기 코드 확인
  - 파일: `cpp/src/core/KeyboardMonitor.cpp`
  - 현재: `#ifdef MSWindows` / `#else` 분기
  - macOS: `CGEventSourceKeyState` 사용

- [ ] **태스크 4.2**: macOS 미구현 함수 목록
  - `IsAEForeground()`: macOS 구현 필요
  - 모든 UI 모듈: macOS Cocoa 구현 필요

**🟢 GREEN: 플랫폼 추상화**
- [ ] **태스크 4.3**: 플랫폼 추상화 헤더 생성
  - 파일: `cpp/include/PlatformUtils.h`
  - 목표: 공통 인터페이스 정의
  - 내용:
    ```cpp
    namespace PlatformUtils {
      bool IsKeyHeld(int keyCode);
      void GetMousePosition(int* x, int* y);
      bool IsAppForeground(void* mainWindow);
    }
    ```

- [ ] **태스크 4.4**: 플랫폼별 구현 분리
  - 파일: `cpp/src/platform/PlatformUtils_Win.cpp`
  - 파일: `cpp/src/platform/PlatformUtils_Mac.cpp` (스텁)

**🔵 REFACTOR: 빌드 시스템 업데이트**
- [ ] **태스크 4.5**: CMakeLists.txt 플랫폼 분기 추가
  - 파일: `cpp/CMakeLists.txt`
  - 목표: Windows/macOS 소스 파일 분리

#### 품질 게이트 ✋

**빌드 및 테스트**:
- [ ] Windows 빌드 성공
- [ ] 기존 기능 모두 작동

**코드 품질**:
- [ ] 플랫폼 추상화 인터페이스 명확
- [ ] Windows 구현 기존과 동일하게 작동

---

## ⚠️ 위험 평가

| 위험 | 확률 | 영향 | 완화 전략 |
|------|------|------|----------|
| 함수 제거 시 숨겨진 의존성 | 낮음 | 높음 | 전체 코드베이스 grep 검색 후 제거 |
| 키 입력 타이밍 변경으로 UX 저하 | 중간 | 중간 | A/B 테스트로 최적 타이밍 결정 |
| 주석 추가 시 빌드 오류 | 낮음 | 낮음 | 점진적 추가, 각 변경 후 빌드 확인 |
| macOS 코드 분리 시 Windows 빌드 실패 | 중간 | 높음 | Windows 빌드 우선, macOS는 스텁 |

---

## 🔄 롤백 전략

### 1단계 실패 시
**되돌리기 단계**:
- `git checkout cpp/src/core/SnapPlugin.cpp`
- 함수 복원 확인

### 2단계 실패 시
**되돌리기 단계**:
- 1단계 완료 상태로 복원
- `PANEL_ACTIVATION_WINDOW_MS` 원복 (300ms 유지 또는 조정)

### 3단계 실패 시
**되돌리기 단계**:
- 주석만 제거, 코드 로직 변경 없음

### 4단계 실패 시
**되돌리기 단계**:
- 새 파일 삭제
- CMakeLists.txt 원복

---

## 📊 진행 상황 추적

### 완료 상태
- **1단계**: ⏳ 0%
- **2단계**: ⏳ 0%
- **3단계**: ⏳ 0%
- **4단계**: ⏳ 0%

**전체 진행률**: 0% 완료

### 시간 추적
| 단계 | 예상 | 실제 | 차이 |
|------|------|------|------|
| 1단계 | 1시간 | - | - |
| 2단계 | 2시간 | - | - |
| 3단계 | 1시간 | - | - |
| 4단계 | 2시간 | - | - |
| **총계** | 6시간 | - | - |

---

## 📝 노트 및 학습

### 구현 노트
- [구현 중 발견한 인사이트 추가]

### 발생한 블로커
- 없음

### 향후 계획을 위한 개선점
- [다음에는 무엇을 다르게 할 것인가?]

---

## 📚 참조

### AE SDK 문서
- [After Effects SDK Guide](https://ae-plugins.docsforadobe.dev/)
- [AEGP Suites 참조](https://ae-plugins.docsforadobe.dev/aegps/aegp-suites.html)

### 프로젝트 내 관련 파일
- `cpp/include/AE_SDK/AE_GeneralPlug.h`: AEGP_UtilitySuite 정의
- `cpp/include/AE_SDK/Util/AEGP_SuiteHandler.h`: Suite 접근 헬퍼

### 관련 코드 위치
| 함수 | 파일 | 줄 번호 |
|-----|------|--------|
| IsAEForeground | SnapPlugin.cpp | 1449-1496 |
| IsMenuHookRecent | SnapPlugin.cpp | 1498-1503 |
| IsInPanelActivationWindow | SnapPlugin.cpp | 1505-1510 |
| IsKeyInputAllowed | SnapPlugin.cpp | 1512-1520 |
| UpdateMenuHook | SnapPlugin.cpp | 1434-1447 |
| IsKeyHeld | KeyboardMonitor.cpp | 56-60 |
| GetMousePosition | KeyboardMonitor.cpp | 67-79 |
| ExecuteScript | SnapPlugin.cpp | 502-569 |

---

## ✅ 최종 체크리스트

**계획을 완료로 표시하기 전에**:
- [ ] 품질 게이트를 통과한 모든 단계 완료
- [ ] 전체 통합 테스트 수행됨
- [ ] CLAUDE.md 업데이트됨
- [ ] 모든 모듈 기능 정상 작동 확인
- [ ] GitHub Actions 빌드 성공

---

**계획 상태**: 🔄 진행 중
**다음 조치**: 1단계 - 미사용 함수 제거 시작
**블로커**: 없음
