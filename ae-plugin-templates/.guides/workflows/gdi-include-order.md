---
description: GDI+ Include Order Issue Analysis and Solution
---

# GDI+ Include Order Issue Analysis

## 문제 요약

Windows SDK에서 GDI+를 사용할 때 `META_*`, `EMR_*`, `CALLBACK` 등의 식별자가 정의되지 않는 빌드 오류 발생.

## 근본 원인

### 1. Include 순서 문제

GDI+ 헤더(`gdiplus.h`)가 `windows.h`보다 먼저 include되면, GDI+ 내부에서 필요한 타입들이 정의되지 않음.

```cpp
// ❌ 잘못된 순서
#include <gdiplus.h>   // META_*, EMR_* 등이 없어서 에러!
#include <objidl.h>
#include <windows.h>

// ✅ 올바른 순서
#include <windows.h>   // 1. META_*, EMR_*, CALLBACK 등 제공
#include <objidl.h>    // 2. IStream 제공
#include <gdiplus.h>   // 3. 위 타입들 사용
```

### 2. WIN32_LEAN_AND_MEAN 매크로

이 매크로가 정의되면 `windows.h`가 GDI 관련 헤더(`wingdi.h`)를 제외함.

```cpp
// ❌ 잘못된 사용
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gdiplus.h>  // GDI 타입들이 없어서 에러!

// ✅ 올바른 사용 - WIN32_LEAN_AND_MEAN 제거
#include <windows.h>
#include <gdiplus.h>
```

### 3. SDK 버전 이슈

| SDK 버전 | 상태 |
|----------|------|
| 10.0.22621.0 | ✅ 권장 (Include 순서만 맞추면 작동) |
| 10.0.26100.0 | ⚠️ 동일한 이슈 발생 |

## 해결 방법

### NativeUI.cpp 상단

```cpp
#ifdef MSWindows

// IMPORTANT: GDI+ Include Order
// 1. windows.h FIRST (provides META_*, EMR_*, CALLBACK, etc.)
// 2. objidl.h (provides IStream for GDI+)
// 3. gdiplus.h LAST (uses all types from above)
// DO NOT define WIN32_LEAN_AND_MEAN - it excludes GDI headers
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#endif
```

## 파일 편집 도구 문제

### 발생한 현상

`replace_file_content` 도구 사용 시:
- 도구가 "성공"이라고 응답
- 실제로는 주석만 변경되고 include 라인은 변경되지 않음

### 원인 추정

1. **여러 줄 매칭 실패**: 줄바꿈 문자(`\n` vs `\r\n`) 불일치
2. **부분 매칭**: 일부 라인만 교체됨
3. **캐싱 문제**: 도구 내부 동기화 실패

### 해결 방법

복잡한 여러 줄 교체는 Python 스크립트 사용:

```python
python3 -c "
import re
with open('file.cpp', 'r') as f:
    content = f.read()

old = '''#include <gdiplus.h>
#include <objidl.h>
#include <windows.h>'''

new = '''#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>'''

content = content.replace(old, new)

with open('file.cpp', 'w') as f:
    f.write(content)
"
```

### 교훈

1. **도구 응답을 맹신하지 말 것** - 변경 후 `cat` 또는 `view_file`로 확인
2. **복잡한 여러 줄 교체는 Python 스크립트가 더 안정적**
3. **Git diff로 실제 변경사항 검증 필요**
