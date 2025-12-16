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
