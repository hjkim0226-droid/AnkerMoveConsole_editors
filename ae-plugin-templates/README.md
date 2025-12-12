# After Effects Plugin Templates

After Effects 플러그인 개발을 위한 3가지 템플릿 모음입니다.

## 📁 템플릿 종류

| 폴더 | 유형 | 설명 | 복잡도 |
|------|------|------|--------|
| `script-only/` | ScriptUI | JSX 스크립트만 (가장 간단) | ⭐ |
| `cep-panel/` | CEP 패널 | HTML/JS + JSX (중간) | ⭐⭐ |
| `native-plugin/` | C++ 플러그인 | C++ + CEP (고급) | ⭐⭐⭐ |

## 🚀 빠른 시작

### 1. Script-Only (가장 간단)
```bash
cp -r script-only/ ../my-new-script/
# my-new-script.jsx 수정 후 AE에서 File > Scripts > Run
```

### 2. CEP Panel (중간)
```bash
cp -r cep-panel/ ../my-new-panel/
# manifest.xml에서 Extension ID 변경
# AE에서 Window > Extensions에서 접근
```

### 3. Native Plugin (고급)
```bash
cp -r native-plugin/ ../my-new-plugin/
# CMakeLists.txt와 PiPL 수정
# Visual Studio 또는 CMake로 빌드
```

## 📋 공통 참고 문서

- `.guides/windows-sdk-version.md` - Windows SDK 버전 요구사항
- `.guides/gdi-include-order.md` - GDI+ Include 순서 이슈
- `.guides/cep-setup.md` - CEP 개발 환경 설정

## ⚠️ 주의사항

1. **SDK 버전**: Windows SDK 10.0.22621.0 사용 권장
2. **CEP 서명**: 개발 중에는 PlayerDebugMode 레지스트리 필요
3. **macOS**: .plugin 번들은 별도 서명 필요
