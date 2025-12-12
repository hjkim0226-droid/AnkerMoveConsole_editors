/**
 * My Script Template
 * After Effects ScriptUI 스크립트 템플릿
 * 
 * 사용법:
 * 1. 이 파일을 복사하여 원하는 이름으로 저장
 * 2. File > Scripts > Run Script File... 에서 실행
 * 3. 또는 Scripts 폴더에 넣어서 메뉴에서 접근
 * 
 * Scripts 폴더 위치:
 * - Windows: C:\Program Files\Adobe\Adobe After Effects [버전]\Support Files\Scripts\
 * - macOS: /Applications/Adobe After Effects [버전]/Scripts/
 */

(function (thisObj) {
    // ========================================
    // 설정
    // ========================================
    var SCRIPT_NAME = "My Script";
    var SCRIPT_VERSION = "1.0.0";

    // ========================================
    // UI 생성
    // ========================================
    function createUI(thisObj) {
        var panel = (thisObj instanceof Panel)
            ? thisObj
            : new Window("palette", SCRIPT_NAME + " v" + SCRIPT_VERSION, undefined, { resizeable: true });

        // 메인 그룹
        var mainGroup = panel.add("group");
        mainGroup.orientation = "column";
        mainGroup.alignment = ["fill", "top"];

        // 버튼
        var runButton = mainGroup.add("button", undefined, "Run Script");
        runButton.alignment = ["fill", "center"];

        // 정보 텍스트
        var infoText = mainGroup.add("statictext", undefined, "Select a layer and click Run");
        infoText.alignment = ["center", "center"];

        // ========================================
        // 이벤트 핸들러
        // ========================================
        runButton.onClick = function () {
            try {
                app.beginUndoGroup(SCRIPT_NAME);

                // 여기에 스크립트 로직 작성
                var comp = app.project.activeItem;
                if (!(comp instanceof CompItem)) {
                    alert("Please select a composition.");
                    return;
                }

                var selectedLayers = comp.selectedLayers;
                if (selectedLayers.length === 0) {
                    alert("Please select at least one layer.");
                    return;
                }

                // 예시: 선택된 레이어들의 이름 출력
                var names = [];
                for (var i = 0; i < selectedLayers.length; i++) {
                    names.push(selectedLayers[i].name);
                }
                alert("Selected layers:\n" + names.join("\n"));

                app.endUndoGroup();
            } catch (e) {
                alert("Error: " + e.toString());
            }
        };

        // ========================================
        // 패널 표시
        // ========================================
        if (panel instanceof Window) {
            panel.center();
            panel.show();
        } else {
            panel.layout.layout(true);
        }

        return panel;
    }

    // UI 생성 실행
    createUI(thisObj);

})(this);
