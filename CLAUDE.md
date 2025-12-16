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
- **E key**: Effect Search popup (Control module)

## Install Paths (Windows)
- Plugin: `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\AnchorSnap.aex`
- CEP: `C:\Program Files (x86)\Common Files\Adobe\CEP\extensions\com.anchor.snap\`

## Language
- 한국어로 대화

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

### Control 모듈 (E key) - 구현 예정 기능

- [ ] E key 0.4s hold 트리거 (Y key와 동일한 방식)
- [ ] Mode 1 (Search): Viewer/Timeline 포커스 → Search bar + Effect Controls
- [ ] Mode 2 (Effects List): Effect Controls 포커스 → Presets + 레이어 이펙트 목록
- [ ] Search bar가 Effect Controls 윈도우 위치 추적
- [ ] 닫기 버튼 [x] 및 ESC 키
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

## Commit Convention

```
feat: 새로운 기능 추가
fix: 버그 수정
docs: 문서 변경
refactor: 코드 리팩토링
ci: CI/CD 설정 변경
```

## Workflow

1. 코드 수정
2. `git add . && git commit -m "..."` → 자동 빌드 트리거
3. `gh run list --limit 1` → 빌드 상태 확인
4. GitHub Actions에서 Artifact 다운로드
5. `install_windows.bat` 실행 (관리자 권한)
6. After Effects 재시작
