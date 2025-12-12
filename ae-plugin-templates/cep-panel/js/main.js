/**
 * My Panel - Main JavaScript
 */

(function () {
    'use strict';

    // CSInterface 인스턴스
    var csInterface = new CSInterface();

    // DOM 요소
    var btnRun = document.getElementById('btn-run');
    var btnSettings = document.getElementById('btn-settings');
    var statusEl = document.getElementById('status');

    // 상태 업데이트
    function updateStatus(message) {
        statusEl.textContent = message;
    }

    // ExtendScript 호출
    function callScript(functionName, args, callback) {
        var script = functionName + '(' + (args ? JSON.stringify(args) : '') + ')';
        csInterface.evalScript(script, function (result) {
            if (callback) callback(result);
        });
    }

    // 이벤트 핸들러
    btnRun.addEventListener('click', function () {
        updateStatus('Running...');
        callScript('myScriptFunction', null, function (result) {
            updateStatus('Result: ' + result);
        });
    });

    btnSettings.addEventListener('click', function () {
        updateStatus('Settings clicked');
        // 설정 다이얼로그 열기 등
    });

    // 초기화
    updateStatus('Panel loaded');

})();
