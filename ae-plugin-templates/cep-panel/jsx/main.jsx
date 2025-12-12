/**
 * My Panel - ExtendScript (JSX)
 * After Effects에서 실행되는 스크립트
 */

// CEP에서 호출되는 메인 함수
function myScriptFunction(args) {
    try {
        var comp = app.project.activeItem;
        if (!(comp instanceof CompItem)) {
            return "Error: No composition selected";
        }

        var selectedLayers = comp.selectedLayers;
        if (selectedLayers.length === 0) {
            return "Error: No layers selected";
        }

        // 여기에 스크립트 로직 작성
        var layerNames = [];
        for (var i = 0; i < selectedLayers.length; i++) {
            layerNames.push(selectedLayers[i].name);
        }

        return "Selected: " + layerNames.join(", ");

    } catch (e) {
        return "Error: " + e.toString();
    }
}

// 앵커 포인트 설정 예시
function setAnchorPoint(x, y) {
    try {
        app.beginUndoGroup("Set Anchor Point");

        var comp = app.project.activeItem;
        var selectedLayers = comp.selectedLayers;

        for (var i = 0; i < selectedLayers.length; i++) {
            var layer = selectedLayers[i];
            if (layer.property("Anchor Point")) {
                var width = layer.width;
                var height = layer.height;
                layer.property("Anchor Point").setValue([width * x, height * y]);
            }
        }

        app.endUndoGroup();
        return "Success";

    } catch (e) {
        return "Error: " + e.toString();
    }
}
