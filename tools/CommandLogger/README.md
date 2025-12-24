# CommandLogger AEGP

AE의 모든 Command ID를 로깅하여 **텍스트 편집 관련 명령**을 찾기 위한 디버깅 플러그인

## 빌드 방법

### 요구사항
- Visual Studio 2022
- CMake 3.20+
- After Effects SDK (이미 프로젝트에 포함됨)

### 빌드
```cmd
cd tools\CommandLogger
build.bat
```

### 수동 빌드
```cmd
cd tools\CommandLogger

# PiPL 생성
..\..\cpp\resources\PiPLtool.exe CommandLogger_PiPL.r CommandLogger_PiPL_temp.rc

# CMake 설정
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ..

# 빌드
cmake --build . --config Release
```

## 설치

빌드된 `CommandLogger.aex`를 다음 위치에 복사:
```
C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\
```

## 사용법

1. AE 실행
2. 컴포지션 생성 및 텍스트 레이어 추가
3. **텍스트 레이어 더블클릭** (편집 모드 진입)
4. 텍스트 입력
5. **ESC** 또는 **Ctrl+Enter** 또는 다른 곳 클릭 (편집 모드 종료)
6. 로그 파일 확인

## 로그 파일 위치

```
%TEMP%\AE_CommandLog.txt
```

예: `C:\Users\{username}\AppData\Local\Temp\AE_CommandLog.txt`

## 로그 예시

```
[15:30:45] ========================================
[15:30:45] CommandLogger AEGP Started
[15:30:45] ========================================
[15:30:45] Command Hook registered (AEGP_Command_ALL)
[15:30:45] UpdateMenu Hook registered
[15:30:45] Idle Hook registered
[15:30:45] Ready! Perform text editing actions and check the log.
[15:31:02] Command ID: 2836
[15:31:02] Command ID: 2836 - [TEXT RANGE] priority=1
[15:31:02] === TEXT LAYER ACTIVE ===
[15:31:05] Window Type Changed: 2 -> 5
...
```

## 찾아야 할 것

| 동작 | 찾을 Command ID |
|------|-----------------|
| 텍스트 도구 전환 | 2836 (예상) |
| 텍스트 편집 모드 진입 | ? |
| 텍스트 편집 모드 종료 | ? |
| ESC 키 | ? |

## 주의사항

- 모든 Command가 로깅되므로 로그 파일이 빠르게 커질 수 있음
- 테스트 후 플러그인 제거 권장
- 디버깅 목적으로만 사용
