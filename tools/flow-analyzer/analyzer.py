#!/usr/bin/env python3
"""
Multi-language Code Flow Analyzer
알고리즘 구조를 역할/기능 설명으로 시각화
노드 클릭 시 파일:라인 복사
"""

import os
import re
import json
import argparse
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Dict, Optional

# 함수명 → 역할 설명 매핑
FUNCTION_LABELS = {
    'EntryPointFunc': '플러그인 시작점',
    'IdleHook': '메인 루프 (키 입력 감지)',
    'IsTextInputFocused': '텍스트 입력 중인지 확인',
    'ShowAnchorGrid': '앵커 그리드 표시',
    'HideAndApplyAnchor': '그리드 닫고 앵커 적용',
    'ExecuteScript': 'ExtendScript 실행',
    'IsKeyHeld': '키 눌림 확인',
}

@dataclass
class FunctionInfo:
    name: str
    label: str
    file: str
    line: int
    language: str

@dataclass
class CodeAnalysis:
    functions: Dict[str, FunctionInfo] = field(default_factory=dict)


class FlowAnalyzer:
    def __init__(self, project_root: str):
        self.project_root = Path(project_root)
        self.analysis = CodeAnalysis()
        self.cpp_func_pattern = re.compile(
            r'^(?:static\s+)?(?:inline\s+)?(?:[\w:]+\s+)+(\w+)\s*\([^)]*\)\s*(?:const\s*)?\{',
            re.MULTILINE
        )

    def analyze(self):
        cpp_dir = self.project_root / 'cpp' / 'src'
        if cpp_dir.exists():
            for cpp_file in cpp_dir.rglob('*.cpp'):
                self._analyze_cpp(cpp_file)
        return self.analysis

    def _analyze_cpp(self, filepath: Path):
        try:
            content = filepath.read_text(encoding='utf-8', errors='ignore')
            lines = content.split('\n')
        except Exception:
            return

        for i, line in enumerate(lines, 1):
            match = self.cpp_func_pattern.search(line)
            if match:
                name = match.group(1)
                label = FUNCTION_LABELS.get(name, name)
                self.analysis.functions[f"cpp:{name}"] = FunctionInfo(
                    name=name, label=label, file=str(filepath), line=i, language='cpp'
                )


def generate_mermaid() -> str:
    lines = ["flowchart TD"]

    # 입력 (둥근 모양)
    lines.append('    subgraph INPUT["⌨️ 입력"]')
    lines.append('        I1(["Y키 누름"])')
    lines.append('        I2(["Y키 더블탭"])')
    lines.append('        I3(["Shift+E"])')
    lines.append('        I4(["D키"])')
    lines.append('    end')

    # C++ (둥근 모양, 조건은 다이아몬드)
    lines.append('    subgraph CPP["🔵 C++ - 키보드 감지 & UI"]')
    lines.append('        C1(["메인 루프\\n키 입력 감지"])')
    lines.append('        C2{"텍스트 입력 중?"}')
    lines.append('        C3(["0.4초 대기"])')
    lines.append('        C4{"더블탭?\\n250ms 내"}')
    lines.append('        C5(["앵커 그리드 표시"])')
    lines.append('        C6(["토글 모드 ON"])')
    lines.append('        C7(["이펙트 패널"])')
    lines.append('        C8{"D 메뉴\\nA/T/K 선택"}')
    lines.append('        C9(["앵커 적용"])')
    lines.append('    end')

    # ExtendScript (둥근 모양)
    lines.append('    subgraph ES["🟢 ExtendScript - AE 작업"]')
    lines.append('        E1(["앵커 포인트 설정"])')
    lines.append('        E2(["이펙트 추가"])')
    lines.append('        E3(["이펙트 삭제"])')
    lines.append('        E4(["키프레임 설정"])')
    lines.append('        E5(["레이어 정렬"])')
    lines.append('        E6(["텍스트 속성 변경"])')
    lines.append('    end')

    # CEP (둥근 모양)
    lines.append('    subgraph CEP["🟡 CEP - 패널 & 설정"]')
    lines.append('        P1(["설정 로드"])')
    lines.append('        P2(["설정 저장"])')
    lines.append('        P3(["정렬 패널"])')
    lines.append('        P4(["텍스트 패널"])')
    lines.append('        P5(["키프레임 패널"])')
    lines.append('    end')

    # 출력 (둥근 모양)
    lines.append('    subgraph OUTPUT["🟠 결과"]')
    lines.append('        O1(["앵커 이동됨"])')
    lines.append('        O2(["이펙트 적용됨"])')
    lines.append('        O3(["키프레임 생성됨"])')
    lines.append('        O4(["레이어 정렬됨"])')
    lines.append('        O5(["텍스트 변경됨"])')
    lines.append('    end')

    # 연결 (화살표에 전달 데이터 표시 - 한글)
    lines.append('')
    lines.append('    I1 -->|"Y키 신호"| C1')
    lines.append('    I2 -->|"더블탭 신호"| C1')
    lines.append('    I3 -->|"Shift+E 신호"| C7')
    lines.append('    I4 -->|"D키 신호"| C8')
    lines.append('    C1 -->|"현재 포커스 창"| C2')
    lines.append('    C2 -->|"아니오"| C3')
    lines.append('    C2 -->|"예"| X1["무시"]')
    lines.append('    C3 -->|"0.4초 후"| C4')
    lines.append('    C4 -->|"홀드 중"| C5')
    lines.append('    C4 -->|"더블탭 감지"| C6')
    lines.append('    C6 -->|"고정 모드"| C5')
    lines.append('    C5 -->|"선택한 셀 위치"| C9')
    lines.append('    C9 -->|"스크립트 코드"| E1')
    lines.append('    E1 -->|"x, y 좌표"| O1')
    lines.append('    C7 -->|"이펙트 이름"| E2')
    lines.append('    E2 -->|"이펙트 객체"| O2')
    lines.append('    C7 -->|"이펙트 번호"| E3')
    lines.append('    E3 -->|"삭제 완료"| O2')
    # D 메뉴 분기 → CEP 패널
    lines.append('    C8 -->|"A 선택"| P3')
    lines.append('    C8 -->|"T 선택"| P4')
    lines.append('    C8 -->|"K 선택"| P5')
    # CEP 패널 → ExtendScript
    lines.append('    P3 -->|"정렬 방향"| E5')
    lines.append('    P4 -->|"텍스트 설정"| E6')
    lines.append('    P5 -->|"키프레임 설정값"| E4')
    # ExtendScript → 결과
    lines.append('    E4 -->|"시간, 값"| O3')
    lines.append('    E5 -->|"위치 값"| O4')
    lines.append('    E6 -->|"속성 값"| O5')
    lines.append('    C5 -.->|"그리드 설정"| P1')
    lines.append('    P2 -.->|"저장된 설정"| C5')

    # 스타일
    lines.append('')
    lines.append('    style INPUT fill:#0d47a1,color:#fff,stroke:#1565c0,stroke-width:2px')
    lines.append('    style CPP fill:#0a3d62,color:#fff,stroke:#1565c0,stroke-width:2px')
    lines.append('    style ES fill:#1b5e20,color:#fff,stroke:#2e7d32,stroke-width:2px')
    lines.append('    style CEP fill:#b8860b,color:#fff,stroke:#f9a825,stroke-width:2px')
    lines.append('    style OUTPUT fill:#bf360c,color:#fff,stroke:#e65100,stroke-width:2px')
    lines.append('    style X1 fill:#666,color:#fff')

    return '\n'.join(lines)


def generate_html(mermaid_code: str, project_root: str) -> str:
    # 노드 ID → 파일:라인 매핑
    node_locations = {
        'I1': 'input:Y키',
        'I2': 'input:Y키 더블탭',
        'I3': 'input:Shift+E',
        'I4': 'input:D키',
        'C1': f'{project_root}/cpp/src/core/SnapPlugin.cpp:1200',
        'C2': f'{project_root}/cpp/src/core/SnapPlugin.cpp:65',
        'C3': f'{project_root}/cpp/src/core/SnapPlugin.cpp:1210',
        'C4': f'{project_root}/cpp/src/core/SnapPlugin.cpp:1217',
        'C5': f'{project_root}/cpp/src/core/SnapPlugin.cpp:1100',
        'C6': f'{project_root}/cpp/src/core/SnapPlugin.cpp:1225',
        'C7': f'{project_root}/cpp/src/modules/control/ControlUI.cpp:1',
        'C8': f'{project_root}/cpp/src/modules/dmenu/DMenuUI.cpp:1',
        'C9': f'{project_root}/cpp/src/core/SnapPlugin.cpp:1050',
        'E1': 'ExtendScript:layer.anchorPoint',
        'E2': 'ExtendScript:addProperty',
        'E3': 'ExtendScript:remove',
        'E4': 'ExtendScript:setValueAtTime',
        'E5': 'ExtendScript:layer.position',
        'E6': 'ExtendScript:textDocument',
        'P1': f'{project_root}/cep/js/core/settings.js:1',
        'P2': f'{project_root}/cep/js/core/settings.js:50',
        'P3': f'{project_root}/cep/js/modules/align.js:1',
        'P4': f'{project_root}/cep/js/modules/text.js:1',
        'P5': f'{project_root}/cep/js/modules/keyframe.js:1',
        'O1': 'output:앵커 이동',
        'O2': 'output:이펙트 적용',
        'O3': 'output:키프레임 생성',
        'O4': 'output:레이어 정렬',
        'O5': 'output:텍스트 변경',
    }
    node_json = json.dumps(node_locations, ensure_ascii=False)

    html = f'''<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="UTF-8">
    <title>Code Flow - AE Anchor Radial Menu</title>
    <script src="https://cdn.jsdelivr.net/npm/mermaid/dist/mermaid.min.js"></script>
    <style>
        * {{ margin: 0; padding: 0; box-sizing: border-box; }}
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            color: #e0e0e0;
            min-height: 100vh;
            padding: 20px;
        }}
        h1 {{ color: #4fc3f7; text-align: center; margin-bottom: 5px; }}
        .subtitle {{ text-align: center; color: #888; margin-bottom: 15px; font-size: 14px; }}
        .instructions {{
            text-align: center;
            background: rgba(74, 158, 255, 0.15);
            border: 1px solid rgba(74, 158, 255, 0.3);
            border-radius: 8px;
            padding: 10px 20px;
            margin: 0 auto 20px auto;
            max-width: 500px;
            font-size: 13px;
        }}
        .legend {{
            display: flex;
            justify-content: center;
            gap: 20px;
            margin-bottom: 20px;
            flex-wrap: wrap;
        }}
        .legend-item {{ display: flex; align-items: center; gap: 6px; font-size: 12px; }}
        .legend-color {{ width: 14px; height: 14px; border-radius: 3px; }}
        .flowchart {{
            background: rgba(255,255,255,0.05);
            border-radius: 12px;
            padding: 20px;
            max-width: 1200px;
            margin: 0 auto;
            overflow: auto;
        }}
        .mermaid {{ background: transparent; cursor: default; }}
        .mermaid .node {{ cursor: pointer !important; }}
        .mermaid .node:hover {{ filter: brightness(1.2); }}
        .filters {{
            display: flex;
            justify-content: center;
            gap: 20px;
            margin-bottom: 20px;
            flex-wrap: wrap;
        }}
        .filter-item {{
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 8px 16px;
            background: rgba(255,255,255,0.1);
            border-radius: 8px;
            cursor: pointer;
            transition: background 0.2s;
        }}
        .filter-item:hover {{ background: rgba(255,255,255,0.15); }}
        .filter-item input {{ cursor: pointer; width: 16px; height: 16px; }}
        .filter-item label {{ cursor: pointer; font-size: 14px; }}
        .node-hidden {{ opacity: 0.15 !important; }}
        .edge-hidden {{ opacity: 0.1 !important; }}
        .toast {{
            position: fixed;
            bottom: 20px;
            left: 50%;
            transform: translateX(-50%) translateY(100px);
            background: #4caf50;
            color: white;
            padding: 12px 24px;
            border-radius: 8px;
            font-size: 14px;
            font-family: monospace;
            transition: transform 0.3s;
            z-index: 1000;
            box-shadow: 0 4px 20px rgba(0,0,0,0.3);
        }}
        .toast.show {{ transform: translateX(-50%) translateY(0); }}
    </style>
</head>
<body>
    <h1>🔍 알고리즘 흐름도</h1>
    <p class="subtitle">AE Anchor Radial Menu - 노드를 클릭하면 파일:라인이 복사됩니다</p>
    <div class="instructions">
        💡 <strong>노드 클릭</strong> → 클립보드에 복사 → 터미널에 붙여넣기
    </div>
    <div class="legend">
        <div class="legend-item"><div class="legend-color" style="background:#0d47a1"></div><span>입력</span></div>
        <div class="legend-item"><div class="legend-color" style="background:#0a3d62"></div><span>C++</span></div>
        <div class="legend-item"><div class="legend-color" style="background:#1b5e20"></div><span>ExtendScript</span></div>
        <div class="legend-item"><div class="legend-color" style="background:#b8860b"></div><span>CEP</span></div>
        <div class="legend-item"><div class="legend-color" style="background:#bf360c"></div><span>결과</span></div>
    </div>
    <div class="filters">
        <div class="filter-item">
            <input type="checkbox" id="filter-y" checked onchange="updateFilter()">
            <label for="filter-y">Y키 (앵커)</label>
        </div>
        <div class="filter-item">
            <input type="checkbox" id="filter-yy" checked onchange="updateFilter()">
            <label for="filter-yy">Y~Y 더블탭</label>
        </div>
        <div class="filter-item">
            <input type="checkbox" id="filter-e" checked onchange="updateFilter()">
            <label for="filter-e">Shift+E (이펙트)</label>
        </div>
        <div class="filter-item">
            <input type="checkbox" id="filter-d" checked onchange="updateFilter()">
            <label for="filter-d">D키 (메뉴)</label>
        </div>
    </div>
    <div class="flowchart">
        <div class="mermaid">
{mermaid_code}
        </div>
    </div>
    <div class="toast" id="toast"></div>
    <script>
        const nodeLocations = {node_json};

        function showToast(msg) {{
            const toast = document.getElementById('toast');
            toast.textContent = msg;
            toast.classList.add('show');
            setTimeout(function() {{ toast.classList.remove('show'); }}, 2500);
        }}

        function copyToClipboard(text) {{
            navigator.clipboard.writeText(text).then(function() {{
                showToast('📋 복사됨: ' + text);
            }});
        }}

        mermaid.initialize({{
            startOnLoad: true,
            theme: 'dark',
            flowchart: {{ useMaxWidth: true, curve: 'basis', nodeSpacing: 50, rankSpacing: 80 }},
            securityLevel: 'loose'
        }});

        // 각 입력별 관련 노드 정의
        const flowNodes = {{
            'y': ['I1', 'C1', 'C2', 'C3', 'C4', 'C5', 'C9', 'E1', 'O1', 'P1', 'P2', 'X1'],
            'yy': ['I2', 'C1', 'C2', 'C3', 'C4', 'C5', 'C6', 'C9', 'E1', 'O1', 'P1', 'P2'],
            'e': ['I3', 'C7', 'E2', 'E3', 'O2'],
            'd': ['I4', 'C8', 'P3', 'P4', 'P5', 'E4', 'E5', 'E6', 'O3', 'O4', 'O5']
        }};

        function updateFilter() {{
            const yChecked = document.getElementById('filter-y').checked;
            const yyChecked = document.getElementById('filter-yy').checked;
            const eChecked = document.getElementById('filter-e').checked;
            const dChecked = document.getElementById('filter-d').checked;

            // 활성화된 노드 수집
            const activeNodes = new Set();
            if (yChecked) flowNodes['y'].forEach(n => activeNodes.add(n));
            if (yyChecked) flowNodes['yy'].forEach(n => activeNodes.add(n));
            if (eChecked) flowNodes['e'].forEach(n => activeNodes.add(n));
            if (dChecked) flowNodes['d'].forEach(n => activeNodes.add(n));

            // 노드 표시/숨김
            document.querySelectorAll('.node').forEach(function(node) {{
                const nodeId = node.id.replace('flowchart-', '').split('-')[0];
                if (activeNodes.size === 0 || activeNodes.has(nodeId)) {{
                    node.classList.remove('node-hidden');
                }} else {{
                    node.classList.add('node-hidden');
                }}
            }});

            // 엣지(화살표) 표시/숨김
            document.querySelectorAll('.edgePath, .edgeLabel').forEach(function(edge) {{
                const edgeId = edge.id || '';
                let shouldShow = false;
                activeNodes.forEach(function(n) {{
                    if (edgeId.includes(n)) shouldShow = true;
                }});
                if (activeNodes.size === 0 || shouldShow) {{
                    edge.classList.remove('edge-hidden');
                }} else {{
                    edge.classList.add('edge-hidden');
                }}
            }});
        }}

        // 노드 클릭 이벤트 연결
        setTimeout(function() {{
            document.querySelectorAll('.node').forEach(function(node) {{
                const nodeId = node.id.replace('flowchart-', '').split('-')[0];
                if (nodeLocations[nodeId]) {{
                    node.style.cursor = 'pointer';
                    node.addEventListener('click', function() {{
                        copyToClipboard(nodeLocations[nodeId]);
                    }});
                }}
            }});
        }}, 500);
    </script>
</body>
</html>'''
    return html


def main():
    parser = argparse.ArgumentParser(description='Code Flow Analyzer')
    parser.add_argument('project_root', nargs='?', default='.', help='Project root')
    parser.add_argument('--output', '-o', default='docs/flow-analysis.html', help='Output file')
    args = parser.parse_args()

    project_root = os.path.abspath(args.project_root)
    print(f"🔍 분석 중: {project_root}")

    analyzer = FlowAnalyzer(project_root)
    analysis = analyzer.analyze()
    print(f"✅ 발견: C++ {len(analysis.functions)}개")

    mermaid_code = generate_mermaid()
    html = generate_html(mermaid_code, project_root)

    output_path = os.path.join(project_root, args.output)
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(html)

    print(f"📄 생성: {output_path}")
    return output_path


if __name__ == '__main__':
    main()
