# Script-Only Template

가장 간단한 After Effects 스크립트 템플릿입니다.

## 파일 구조

```
script-only/
├── MyScript.jsx      # 메인 스크립트 (수정해서 사용)
└── README.md         # 이 파일
```

## 사용법

### 1. 복사
```bash
cp MyScript.jsx ~/Desktop/MyNewScript.jsx
```

### 2. 스크립트 수정
`MyScript.jsx`를 열어서 로직 수정

### 3. 실행
- **임시 실행**: File > Scripts > Run Script File...
- **메뉴 등록**: Scripts 폴더에 복사

## Scripts 폴더 위치

| OS | 경로 |
|----|------|
| Windows | `C:\Program Files\Adobe\Adobe After Effects [버전]\Support Files\Scripts\` |
| macOS | `/Applications/Adobe After Effects [버전]/Scripts/` |

## 팁

- `ScriptUI Panels` 폴더에 넣으면 Window 메뉴에서 접근 가능
- `.jsxbin`으로 컴파일하면 코드 보호 가능 (ExtendScript Toolkit 사용)
