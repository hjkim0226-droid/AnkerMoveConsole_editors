# CEP Panel Template

HTML/CSS/JS 기반의 After Effects 패널 템플릿입니다.

## 파일 구조

```
cep-panel/
├── CSXS/
│   └── manifest.xml      # ⚠️ Extension ID 변경 필요
├── css/
│   └── style.css         # 스타일 (Adobe 다크 테마)
├── js/
│   ├── CSInterface.js    # Adobe 라이브러리 (수정 금지)
│   └── main.js           # UI 로직
├── jsx/
│   └── main.jsx          # ExtendScript (AE 제어)
├── index.html            # 메인 HTML
└── README.md
```

## 설치 방법

### 1. 폴더 복사
```bash
cp -r cep-panel/ ~/path/to/extensions/com.yourname.mypanel/
```

### 2. manifest.xml 수정
`CSXS/manifest.xml`에서 반드시 변경:
- `ExtensionBundleId="com.yourname.mypanel"`
- `Extension Id="com.yourname.mypanel.main"`
- `<Menu>My Panel</Menu>` → 원하는 메뉴 이름

### 3. 설치 경로

| OS | 경로 |
|----|------|
| Windows | `C:\Program Files (x86)\Common Files\Adobe\CEP\extensions\` |
| macOS | `/Library/Application Support/Adobe/CEP/extensions/` |

### 4. PlayerDebugMode 설정 (개발용)

**Windows Registry:**
```
HKEY_CURRENT_USER\Software\Adobe\CSXS.11
→ String: PlayerDebugMode = 1
```

**macOS Terminal:**
```bash
defaults write com.adobe.CSXS.11 PlayerDebugMode 1
```

## 개발 팁

### JavaScript ↔ ExtendScript 통신

```javascript
// main.js에서 JSX 호출
csInterface.evalScript('myFunction("arg1", "arg2")', function(result) {
    console.log(result);
});
```

```javascript
// main.jsx에서 결과 반환
function myFunction(arg1, arg2) {
    return "Result: " + arg1 + ", " + arg2;
}
```

### 디버깅
1. Chrome에서 `http://localhost:포트번호` 접속 (manifest.xml에서 설정)
2. 또는 Creative Cloud 앱의 개발자 도구 사용
