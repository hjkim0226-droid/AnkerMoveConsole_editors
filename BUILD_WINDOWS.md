# Anchor Grid Plugin - Windows Build Guide

이 가이드는 Windows 환경에서 플러그인을 빌드하고 설치하는 방법을 설명합니다.

## 1. 요구 사항

- **Visual Studio 2022** (C++ 데스크톱 개발 워크로드 설치 필요)
- **CMake** (Visual Studio Installer에서 선택하거나 별도 설치)
- **Windows SDK 10.0.22621.0** (필수 - 아래 참조)
- **After Effects** 설치됨

### Windows SDK 버전 요구사항

> ⚠️ **중요**: SDK 10.0.22621.0을 사용해야 합니다.

| SDK 버전 | 상태 |
|----------|------|
| 10.0.22621.0 | ✅ 권장 (안정적, GDI+ 호환) |
| 10.0.26100.0 | ❌ 사용 금지 (GDI+ 헤더 버그) |

**이유:**
1. SDK 22621은 VS2022의 기본 권장 버전
2. Windows 11 기능과 Windows 10 호환성의 최적 균형
3. SDK 26100은 GDI+ 헤더 버그가 있음 (META_*, EMR_* 정의 누락)

## 2. 프로젝트 준비

이 프로젝트 폴더 전체를 Windows PC로 복사합니다.
`SDK` 관련 파일들이 `cpp/include/AE_SDK`에 이미 포함되어 있으므로 별도 SDK 다운로드는 필요 없습니다.

## 3. 빌드 방법

### 방법 A: Visual Studio 열기 (권장)

1. Visual Studio 2022 실행
2. `파일 > 열기 > 폴더` 선택 후 `ae-anchor-radial-menu` 폴더 열기
3. Visual Studio가 자동으로 `CMakeLists.txt`를 인식하고 구성합니다.
4. 상단 메뉴에서 `빌드 > 모두 빌드` 선택

### 방법 B: Command Line

PowerShell에서 프로젝트 폴더로 이동 후:

```powershell
cd cpp
mkdir build
cd build

# SDK 버전을 10.0.22621.0으로 강제 지정하여 구성
cmake .. -DCMAKE_SYSTEM_VERSION=10.0.22621.0
cmake --build . --config Release
```

## 4. 설치

### C++ 플러그인 (.aex)
빌드된 `AnchorRadialMenu.aex` 파일을 다음 경로로 복사:
`C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\`

### CEP 패널
`cep` 폴더 전체를 다음 경로로 복사:
`C:\Program Files (x86)\Common Files\Adobe\CEP\extensions\com.anchor.grid\`
(폴더가 없으면 생성)

> ⚠️ **주의**: 설치 후 After Effects가 켜져 있다면 **반드시 종료하고 다시 실행**해야 적용됩니다.

### 레지스트리 설정 (Debug Mode)
CEP 패널(UI)이 서명되지 않았으므로 개발자 모드를 켜야 합니다.

1. `Win + R` -> `regedit` 실행
2. 다음 경로로 이동 (사용 중인 AE 버전에 맞는 곳, 없으면 다 하셔도 됩니다):
   - `HKEY_CURRENT_USER\Software\Adobe\CSXS.11` (AE 2024 등)
   - `HKEY_CURRENT_USER\Software\Adobe\CSXS.10`
   - `HKEY_CURRENT_USER\Software\Adobe\CSXS.12`
3. 우클릭 -> 새로 만들기 -> 문자열 값(String Value)
4. 이름: `PlayerDebugMode`, 값: `1`

## 5. 실행 확인

After Effects 실행 후 `Window -> Extensions -> Anchor Grid` 메뉴가 보이면 성공!
레이어 선택 후 `Y` 키를 눌러보세요.
