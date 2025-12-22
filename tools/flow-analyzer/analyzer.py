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

    # 스타일 (더 어두운 배경)
    lines.append('')
    lines.append('    style INPUT fill:#082a5a,color:#fff,stroke:#1565c0,stroke-width:2px')
    lines.append('    style CPP fill:#041c2c,color:#fff,stroke:#1565c0,stroke-width:2px')
    lines.append('    style ES fill:#0a2010,color:#fff,stroke:#2e7d32,stroke-width:2px')
    lines.append('    style CEP fill:#4a3505,color:#fff,stroke:#f9a825,stroke-width:2px')
    lines.append('    style OUTPUT fill:#4a1505,color:#fff,stroke:#e65100,stroke-width:2px')
    lines.append('    style X1 fill:#333,color:#fff')

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
            background: linear-gradient(135deg, #121220 0%, #101828 100%);
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
        .edge-hidden path {{ opacity: 0.1 !important; }}
        .edgePath.edge-hidden {{ opacity: 0.1 !important; }}
        .edgePath.edge-hidden path {{ stroke-opacity: 0.1 !important; }}
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

        // 각 입력별 고유 노드 정의 (공유되지 않는 핵심 노드만)
        const flowNodes = {{
            'y': ['I1', 'X1'],  // Y키: 입력, 무시(홀드 실패)
            'yy': ['I2', 'C6'],  // Y~Y 더블탭: 입력, 토글 모드
            'e': ['I3', 'C7', 'E2', 'E3', 'O2'],  // Shift+E: 이펙트
            'd': ['I4', 'C8', 'P3', 'P4', 'P5', 'E4', 'E5', 'E6', 'O3', 'O4', 'O5']  // D키: 메뉴
        }};

        // 공유 노드 (Y키/YY 공통 경로)
        const sharedNodes = {{
            'y_yy': ['C1', 'C2', 'C3', 'C4', 'C5', 'C9', 'E1', 'O1', 'P1', 'P2']
        }};

        // 엣지 연결 정보 (Mermaid 코드 순서와 동일)
        const edgeConnections = [
            ['I1', 'C1'], ['I2', 'C1'], ['I3', 'C7'], ['I4', 'C8'],
            ['C1', 'C2'], ['C2', 'C3'], ['C2', 'X1'], ['C3', 'C4'],
            ['C4', 'C5'], ['C4', 'C6'], ['C6', 'C5'], ['C5', 'C9'],
            ['C9', 'E1'], ['E1', 'O1'],
            ['C7', 'E2'], ['E2', 'O2'], ['C7', 'E3'], ['E3', 'O2'],
            ['C8', 'P3'], ['C8', 'P4'], ['C8', 'P5'],
            ['P3', 'E5'], ['P4', 'E6'], ['P5', 'E4'],
            ['E4', 'O3'], ['E5', 'O4'], ['E6', 'O5'],
            ['C5', 'P1'], ['P2', 'C5']
        ];

        // 입력별 강조 색상
        const highlightColors = {{
            'y': '#4fc3f7',   // 밝은 파랑
            'yy': '#81d4fa',  // 더 밝은 파랑
            'e': '#81c784',   // 초록
            'd': '#ffb74d'    // 주황
        }};

        function updateFilter() {{
            const yChecked = document.getElementById('filter-y').checked;
            const yyChecked = document.getElementById('filter-yy').checked;
            const eChecked = document.getElementById('filter-e').checked;
            const dChecked = document.getElementById('filter-d').checked;

            // 활성화된 노드 수집 + 노드별 색상 매핑
            const activeNodes = new Set();
            const nodeColors = {{}};

            // 고유 노드 추가
            if (yChecked) flowNodes['y'].forEach(n => {{ activeNodes.add(n); nodeColors[n] = highlightColors['y']; }});
            if (yyChecked) flowNodes['yy'].forEach(n => {{ activeNodes.add(n); nodeColors[n] = highlightColors['yy']; }});
            if (eChecked) flowNodes['e'].forEach(n => {{ activeNodes.add(n); nodeColors[n] = highlightColors['e']; }});
            if (dChecked) flowNodes['d'].forEach(n => {{ activeNodes.add(n); nodeColors[n] = highlightColors['d']; }});

            // 공유 노드 추가 (Y나 YY 중 하나라도 체크되면)
            if (yChecked || yyChecked) {{
                sharedNodes['y_yy'].forEach(n => {{
                    activeNodes.add(n);
                    // 색상은 먼저 체크된 것 기준
                    if (!nodeColors[n]) nodeColors[n] = yChecked ? highlightColors['y'] : highlightColors['yy'];
                }});
            }}

            // 활성 필터 개수 확인
            const activeCount = [yChecked, yyChecked, eChecked, dChecked].filter(Boolean).length;
            const shouldHighlight = activeCount >= 1;  // 1개 이상 체크 시 색상 표시
            const allChecked = activeCount === 4;  // 전체 선택 시 필터링 스킵
            const noneChecked = activeCount === 0;  // 아무것도 선택 안 됨 → 전체 표시

            // 노드 표시/숨김 + 강조
            document.querySelectorAll('.node').forEach(function(node) {{
                const nodeId = node.id.replace('flowchart-', '').split('-')[0];
                const rect = node.querySelector('rect, polygon, circle, ellipse, path');

                if (allChecked || noneChecked || activeNodes.has(nodeId)) {{
                    node.classList.remove('node-hidden');
                    // 단일 필터 선택 시 테두리 강조
                    if (shouldHighlight && nodeColors[nodeId] && rect) {{
                        rect.style.stroke = nodeColors[nodeId];
                        rect.style.strokeWidth = '3px';
                    }} else if (rect) {{
                        rect.style.stroke = '';
                        rect.style.strokeWidth = '';
                    }}
                }} else {{
                    node.classList.add('node-hidden');
                    if (rect) {{
                        rect.style.stroke = '';
                        rect.style.strokeWidth = '';
                    }}
                }}
            }});

            // 엣지(화살표) 필터링 - SVG 구조 직접 분석
            const svg = document.querySelector('.mermaid svg');
            if (!svg) {{ console.log('SVG not found'); return; }}

            // 방법 1: edgePath 또는 edgePaths 클래스
            let edgeGs = svg.querySelectorAll('.edgePath, .edgePaths, [class*="edge-"]');

            // 방법 2: 없으면 edge로 시작하는 id를 가진 g 요소
            if (edgeGs.length === 0) {{
                edgeGs = svg.querySelectorAll('g[id^="edge"], g[id*="-to-"]');
            }}

            // 방법 3: 여전히 없으면, path들 중 stroke 스타일이 있고 fill이 none인 것 (화살표 라인)
            if (edgeGs.length === 0) {{
                const allPaths = svg.querySelectorAll('path');
                const linePaths = [];
                allPaths.forEach(p => {{
                    const fill = p.getAttribute('fill') || getComputedStyle(p).fill;
                    const stroke = p.getAttribute('stroke') || getComputedStyle(p).stroke;
                    // fill이 none이고 stroke가 있으면 화살표 라인
                    if ((fill === 'none' || fill === 'transparent') && stroke && stroke !== 'none') {{
                        linePaths.push(p);
                    }}
                }});
                edgeGs = linePaths;
                console.log('방법3: stroke 있는 path 수:', linePaths.length);
            }}

            console.log('찾은 엣지 요소 수:', edgeGs.length, '연결 정의 수:', edgeConnections.length);

            // 엣지 요소 필터링
            edgeGs.forEach(function(edge, idx) {{
                if (idx >= edgeConnections.length) return;

                const conn = edgeConnections[idx];
                const srcNode = conn[0];
                const tgtNode = conn[1];
                const srcActive = activeNodes.has(srcNode);
                const tgtActive = activeNodes.has(tgtNode);
                const edgeColor = nodeColors[srcNode] || nodeColors[tgtNode];

                // edge가 g 요소면 내부 path들을, 직접 path면 그것 자체를
                const paths = edge.tagName === 'g' ? edge.querySelectorAll('path') : [edge];

                // OR 로직: 양쪽 중 하나라도 활성이면 표시
                if (allChecked || noneChecked || srcActive || tgtActive) {{
                    paths.forEach(p => {{
                        p.style.opacity = '1';
                        if (shouldHighlight && edgeColor) {{
                            p.style.stroke = edgeColor;
                            p.style.strokeWidth = '2.5px';
                        }} else {{
                            p.style.stroke = '';
                            p.style.strokeWidth = '';
                        }}
                    }});
                    if (edge.tagName === 'g') edge.style.opacity = '1';
                }} else {{
                    paths.forEach(p => p.style.opacity = '0.1');
                    if (edge.tagName === 'g') edge.style.opacity = '0.1';
                }}
            }});

            // 엣지 라벨 - 일단 전부 표시 (필터링 비활성화)
            // 복잡한 매칭 대신 단순하게 처리
            const edgeLabels = document.querySelectorAll('.edgeLabel');
            console.log('엣지 라벨 수:', edgeLabels.length);
            if (edgeLabels.length > 0) {{
                console.log('라벨 ID 예시:', Array.from(edgeLabels).slice(0, 3).map(l => l.id));
            }}
            // 라벨은 필터링하지 않음 - 항상 표시
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

            // 초기 필터 적용 및 SVG 구조 디버깅
            const svg = document.querySelector('.mermaid svg');
            if (svg) {{
                const allG = svg.querySelectorAll('g');
                const edgeGs = [];
                allG.forEach(g => {{
                    const cls = g.getAttribute('class') || '';
                    if (cls.includes('edge') || cls.includes('link')) edgeGs.push(cls);
                }});
                console.log('Edge 관련 g 요소 클래스들:', edgeGs);

                const pathsWithMarker = svg.querySelectorAll('path[marker-end]');
                console.log('marker-end 있는 path 수:', pathsWithMarker.length);
            }}
            updateFilter();
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
