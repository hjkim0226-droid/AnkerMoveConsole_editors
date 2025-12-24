/**
 * Script Runner Panel for After Effects
 * 스크립트 입력 후 실행하는 간단한 패널 (ES3 호환)
 *
 * 설치: After Effects Scripts/ScriptUI Panels 폴더에 복사
 *       → Window 메뉴에서 "ScriptRunner.jsx" 선택
 */

#targetengine "scriptrunner"

// 전역 상태 (scheduleTask에서 접근 가능하도록)
$.global._sr = {
    isWatching: false,
    taskId: null,
    scriptInput: null,
    resultOutput: null,
    watchBtn: null
};

// 폴링 함수 (전역)
$.global._srPoll = function() {
    var sr = $.global._sr;
    if (!sr.isWatching) return;

    try {
        var code = sr.scriptInput.text;
        var result = eval(code);
        sr.resultOutput.text = String(result);
    } catch (e) {
        sr.resultOutput.text = "Error: " + e.message;
    }

    if (sr.isWatching) {
        sr.taskId = app.scheduleTask("$.global._srPoll()", 500, false);
    }
};

(function(thisObj) {
    var panel;
    if (thisObj instanceof Panel) {
        panel = thisObj;
    } else {
        panel = new Window("palette", "Script Runner", undefined, {resizeable: true});
    }

    panel.orientation = "column";
    panel.alignChildren = ["fill", "fill"];

    // 스크립트 입력 영역 (고정 높이)
    panel.add("statictext", undefined, "Script:");
    var scriptInput = panel.add("edittext", undefined, "", {multiline: true, scrolling: true});
    scriptInput.alignment = ["fill", "top"];
    scriptInput.preferredSize = [300, 120];
    $.global._sr.scriptInput = scriptInput;

    // 기본 예제 코드
    scriptInput.text = "var v = app.activeViewer;\nif (v == null) {\n  'null';\n} else {\n  'active: ' + v.active + ', type: ' + v.type;\n}";

    // 버튼 그룹 (고정)
    var btnGroup = panel.add("group");
    btnGroup.alignment = ["center", "top"];
    var runBtn = btnGroup.add("button", undefined, "Run");
    var watchBtn = btnGroup.add("button", undefined, "Watch");
    var clearBtn = btnGroup.add("button", undefined, "Clear");
    $.global._sr.watchBtn = watchBtn;

    // 결과 출력 영역 (창 크기에 따라 늘어남)
    var resultLabel = panel.add("statictext", undefined, "Result:");
    resultLabel.alignment = ["left", "top"];
    var resultOutput = panel.add("edittext", undefined, "", {multiline: true, scrolling: true, readonly: true});
    resultOutput.alignment = ["fill", "fill"];
    resultOutput.minimumSize = [200, 100];
    $.global._sr.resultOutput = resultOutput;

    // Run 버튼
    runBtn.onClick = function() {
        try {
            var code = scriptInput.text;
            var result = eval(code);
            resultOutput.text = String(result);
        } catch (e) {
            resultOutput.text = "Error: " + e.message;
        }
    };

    // Clear 버튼
    clearBtn.onClick = function() {
        resultOutput.text = "";
    };

    // Watch 버튼
    watchBtn.onClick = function() {
        var sr = $.global._sr;

        if (sr.isWatching) {
            sr.isWatching = false;
            watchBtn.text = "Watch";
            if (sr.taskId != null) {
                app.cancelTask(sr.taskId);
                sr.taskId = null;
            }
        } else {
            sr.isWatching = true;
            watchBtn.text = "Stop";
            $.global._srPoll();
        }
    };

    // 리사이즈
    panel.onResizing = panel.onResize = function() {
        this.layout.resize();
    };

    if (panel instanceof Window) {
        panel.center();
        panel.show();
    } else {
        panel.layout.layout(true);
    }

    return panel;
})(this);
