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
