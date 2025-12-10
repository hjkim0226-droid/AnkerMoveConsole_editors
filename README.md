# Anchor Radial Menu - After Effects Plugin

애프터 이펙트에서 레이어 선택 후 Y 키를 누르고 있으면 라디얼 메뉴 형태의 앵커 포인트 그리드가 나타나는 플러그인입니다.

## 프로젝트 구조

```
ae-anchor-radial-menu/
├── cpp/              # C++ AEGP Plugin (키보드 감지)
│   ├── CMakeLists.txt
│   ├── Info.plist
│   └── src/
│       ├── AnchorRadialMenu.cpp/h   # 메인 엔트리포인트
│       ├── KeyboardMonitor.cpp/h    # macOS 키보드 감지
│       └── CEPBridge.cpp/h          # CEP 통신
└── cep/              # CEP Panel (라디얼 UI) - TODO
```

## 빌드 요구사항

### 1. After Effects C++ SDK 다운로드

1. [Adobe Developer Console](https://developer.adobe.com/after-effects/) 접속 (로그인 필요)
2. "SDK" 섹션에서 "After Effects SDK" 다운로드
3. 압축 해제 후 아래와 같이 복사:

```bash
# SDK 압축 해제 후 (예: AfterEffectsCC25.0SDK 폴더)
# Headers 폴더 내용을 include/AE_SDK로 복사

cp -r "AfterEffectsCC25.0SDK/Examples/Headers/"* cpp/include/AE_SDK/
cp -r "AfterEffectsCC25.0SDK/Examples/Util/"* cpp/include/AE_SDK/
```

**SDK 설치 후 디렉토리 구조:**
```
cpp/include/AE_SDK/
├── AE_Effect.h
├── AE_EffectCB.h
├── AE_GeneralPlug.h
├── AEGP_SuiteHandler.h
├── ... (기타 헤더 파일들)
└── Util/
    └── AEGP_SuiteHandler.cpp
```

### 2. 개발 환경

- **macOS**: Xcode 12.2+
- **CMake**: `brew install cmake`
- **Command Line Tools**: `xcode-select --install`

## 빌드 방법

```bash
cd cpp

# Xcode 프로젝트 생성
cmake -G "Xcode" -B build

# 빌드
cmake --build build --config Release

# 또는 Xcode에서 직접 열기
open build/AnchorRadialMenu.xcodeproj
```

## 플러그인 설치

빌드된 `.plugin` 번들을 다음 위치에 복사:

```bash
# macOS
/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/

# 또는 사용자별
~/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/
```

## 현재 상태

- [x] C++ AEGP 프로젝트 구조
- [x] CMake 빌드 설정
- [x] 키보드 모니터링 (macOS)
- [x] CEP 통신 브릿지
- [ ] After Effects SDK 헤더 추가 필요
- [ ] CEP 패널 UI (Phase 2)
- [ ] 앵커 포인트 적용 로직 (Phase 3)

## 라이선스

MIT
