---
name: flowchart
description: 프로젝트 흐름도를 생성합니다
arguments:
  - name: type
    description: "흐름도 타입: architecture, data-flow, module, init, full"
    required: false
    default: "full"
  - name: output
    description: "출력 파일 경로 (기본: docs/flowchart.html)"
    required: false
---

# Flowchart Generator

프로젝트 코드베이스를 분석하여 Mermaid 흐름도를 생성합니다.

## 사용법

```
/flowchart                    # 전체 흐름도 생성
/flowchart architecture       # 아키텍처 다이어그램
/flowchart data-flow          # 데이터 흐름도
/flowchart module             # 모듈 관계도
/flowchart init               # 초기화 흐름도
```

## 작업 순서

1. **코드베이스 분석**
   - Glob으로 주요 파일 탐색 (*.cpp, *.h, *.ts, *.js 등)
   - Grep으로 함수/클래스 관계 파악
   - Read로 핵심 로직 이해

2. **흐름도 타입별 생성**
   - `architecture`: 전체 레이어 구조 (Core → Modules → External)
   - `data-flow`: 입력 → 처리 → 출력 흐름
   - `module`: 모듈간 의존성 관계
   - `init`: 초기화/시작 시퀀스
   - `full`: 위 모든 것 포함

3. **Mermaid 코드 생성**
   - flowchart TD/LR 문법 사용
   - subgraph로 레이어 구분
   - style로 색상 적용 (입력=파랑, 출력=주황)

4. **HTML 파일 출력**
   - 다크 테마 스타일 적용
   - Mermaid.js CDN 포함
   - 범례 및 파일 위치 주석

## 출력 예시

```html
<!DOCTYPE html>
<html lang="ko">
<head>
    <script src="https://cdn.jsdelivr.net/npm/mermaid/dist/mermaid.min.js"></script>
    <style>/* 다크 테마 */</style>
</head>
<body>
    <h1>프로젝트명 - 흐름도</h1>
    <div class="mermaid">
    flowchart TD
        A[입력] --> B[처리] --> C[출력]
    </div>
</body>
</html>
```

## 분석 대상 파일 패턴

- **C/C++**: `*.cpp`, `*.h`, `*.hpp`
- **JavaScript/TypeScript**: `*.js`, `*.ts`, `*.tsx`
- **Python**: `*.py`
- **설정**: `*.json`, `*.yaml`, `*.toml`

코드베이스를 분석하고 `{{ type }}` 타입의 흐름도를 생성하세요.
출력 경로: `{{ output | default: "docs/flowchart.html" }}`
