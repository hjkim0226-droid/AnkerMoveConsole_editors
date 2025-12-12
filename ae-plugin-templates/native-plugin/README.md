# Native Plugin Template (C++ + CEP)

After Effects C++ 네이티브 플러그인 + CEP 패널 템플릿입니다.

## 파일 구조

```
native-plugin/
├── 📁 cpp/                          # C++ 소스
│   ├── CMakeLists.txt               # CMake 빌드 설정
│   ├── 📁 include/AE_SDK/           # After Effects SDK
│   ├── 📁 resources/                # PiPL 리소스
│   └── 📁 src/                      # 소스 코드
├── 📁 cep/                          # CEP 패널
│   ├── CSXS/manifest.xml
│   ├── 📁 js/, jsx/, css/
│   └── index.html
├── 📁 .github/workflows/            # GitHub Actions CI/CD
├── BUILD_WINDOWS.md                 # 빌드 가이드
├── install_windows.bat              # Windows 설치 스크립트
└── install_mac.sh                   # macOS 설치 스크립트
```

## 새 플러그인 만들기

### 1. 폴더 복사
```bash
cp -r native-plugin/ ~/projects/my-new-plugin/
cd ~/projects/my-new-plugin
rm -rf .git && git init
```

### 2. 이름 변경이 필요한 파일들

| 파일 | 변경 내용 |
|------|-----------|
| `cpp/CMakeLists.txt` | `project(MyNewPlugin ...)` |
| `cpp/resources/*.r` | 플러그인 이름, ID |
| `cep/CSXS/manifest.xml` | Extension ID |
| `.github/workflows/*.yml` | Artifact 이름 |

### 3. 빌드

**Visual Studio:**
1. `파일 > 열기 > 폴더` → `cpp/` 선택
2. CMake 자동 인식
3. `빌드 > 모두 빌드`

**Command Line:**
```powershell
cd cpp
mkdir build && cd build
cmake .. -DCMAKE_SYSTEM_VERSION=10.0.22621.0
cmake --build . --config Release
```

## 중요 참고사항

### Windows SDK 버전
⚠️ **SDK 10.0.22621.0 사용 권장** (26100에는 GDI+ 헤더 버그 있음)

### GDI+ Include 순서
```cpp
// 반드시 이 순서!
#include <windows.h>   // 1. 먼저
#include <objidl.h>    // 2. 중간
#include <gdiplus.h>   // 3. 마지막
```

상세 내용은 `../.guides/` 폴더 참조
