/**
 * CommandLogger AEGP
 *
 * AE의 모든 Command ID를 로깅하여 텍스트 편집 관련 명령을 찾기 위한 디버깅 플러그인
 *
 * 사용법:
 * 1. 빌드 후 MediaCore 폴더에 설치
 * 2. AE 실행
 * 3. 텍스트 레이어 더블클릭 (편집 모드 진입)
 * 4. ESC 또는 다른 곳 클릭 (편집 모드 종료)
 * 5. 로그 파일 확인: %TEMP%\AE_CommandLog.txt
 */

#include "AE_GeneralPlug.h"
#include "AE_Macros.h"
#include "AEGP_SuiteHandler.h"
#include "entry.h"

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <ctime>
#include <string>
#include <iomanip>

// 전역 변수
static AEGP_PluginID g_pluginId = 0;
static SPBasicSuite* g_pBasicSuite = nullptr;
static std::string g_logFilePath;

// 로그 파일 경로 설정
void InitLogFile() {
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    g_logFilePath = std::string(tempPath) + "AE_CommandLog.txt";
}

// 타임스탬프 가져오기
std::string GetTimestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

// 로그 기록
void LogCommand(AEGP_Command command, const char* extra = nullptr) {
    std::ofstream logFile(g_logFilePath, std::ios::app);
    if (logFile.is_open()) {
        logFile << "[" << GetTimestamp() << "] Command ID: " << command;
        if (extra) {
            logFile << " - " << extra;
        }
        logFile << std::endl;
        logFile.close();
    }
}

// 로그 파일에 메시지 기록
void LogMessage(const char* message) {
    std::ofstream logFile(g_logFilePath, std::ios::app);
    if (logFile.is_open()) {
        logFile << "[" << GetTimestamp() << "] " << message << std::endl;
        logFile.close();
    }
}

// Command Hook 콜백
static A_Err CommandHook(
    AEGP_GlobalRefcon plugin_refconP,
    AEGP_CommandRefcon refconP,
    AEGP_Command command,
    AEGP_HookPriority hook_priority,
    A_Boolean already_handledB,
    A_Boolean* handledPB)
{
    // 텍스트 관련 주요 Command ID 범위 (2800-3100)
    // 자주 호출되는 명령은 필터링 (너무 많은 로그 방지)

    // 모든 명령 로깅 (디버깅용)
    LogCommand(command);

    // 특정 범위의 명령만 상세 로깅
    if (command >= 2800 && command <= 3100) {
        char extra[64];
        sprintf(extra, "[TEXT RANGE] priority=%d", hook_priority);
        LogCommand(command, extra);
    }

    // AE가 계속 처리하도록
    *handledPB = FALSE;
    return A_Err_NONE;
}

// UpdateMenu Hook 콜백 - 활성 창 타입 확인
static A_Err UpdateMenuHook(
    AEGP_GlobalRefcon plugin_refconP,
    AEGP_UpdateMenuRefcon refconP,
    AEGP_WindowType active_window)
{
    // 창 타입 변경 시 로깅
    static AEGP_WindowType lastWindow = AEGP_WindType_NONE;

    if (active_window != lastWindow) {
        char msg[128];
        sprintf(msg, "Window Type Changed: %d -> %d", lastWindow, active_window);
        LogMessage(msg);
        lastWindow = active_window;
    }

    return A_Err_NONE;
}

// Idle Hook 콜백 - 활성 레이어 체크
static A_Err IdleHook(
    AEGP_GlobalRefcon plugin_refconP,
    AEGP_IdleRefcon refconP,
    A_long* max_sleepPL)
{
    A_Err err = A_Err_NONE;

    // 너무 자주 호출되지 않도록
    static int counter = 0;
    counter++;

    // 약 1초마다 체크 (max_sleep이 100ms일 때)
    if (counter % 10 == 0) {
        AEGP_SuiteHandler suites(g_pBasicSuite);

        AEGP_LayerH activeLayer = NULL;
        err = suites.LayerSuite9()->AEGP_GetActiveLayer(&activeLayer);

        if (!err && activeLayer) {
            AEGP_ObjectType type;
            err = suites.LayerSuite9()->AEGP_GetLayerObjectType(activeLayer, &type);

            if (!err && type == AEGP_ObjectType_TEXT) {
                static bool wasTextLayer = false;
                if (!wasTextLayer) {
                    LogMessage("=== TEXT LAYER ACTIVE ===");
                    wasTextLayer = true;
                }
            } else {
                static bool wasTextLayer = true;
                if (wasTextLayer) {
                    LogMessage("=== TEXT LAYER INACTIVE ===");
                    wasTextLayer = false;
                }
            }
        }
    }

    *max_sleepPL = 100; // 100ms
    return err;
}

// Entry Point
extern "C" DllExport A_Err EntryPointFunc(
    struct SPBasicSuite* pica_basicP,
    A_long major_versionL,
    A_long minor_versionL,
    AEGP_PluginID aegp_plugin_id,
    AEGP_GlobalRefcon* global_refconP)
{
    A_Err err = A_Err_NONE;

    g_pluginId = aegp_plugin_id;
    g_pBasicSuite = pica_basicP;

    // 로그 파일 초기화
    InitLogFile();
    LogMessage("========================================");
    LogMessage("CommandLogger AEGP Started");
    LogMessage("========================================");

    char pathMsg[512];
    sprintf(pathMsg, "Log file: %s", g_logFilePath.c_str());
    LogMessage(pathMsg);

    AEGP_SuiteHandler suites(pica_basicP);

    // Command Hook 등록 - 모든 명령 받기
    err = suites.RegisterSuite5()->AEGP_RegisterCommandHook(
        aegp_plugin_id,
        AEGP_HP_BeforeAE,
        AEGP_Command_ALL,
        CommandHook,
        nullptr);

    if (!err) {
        LogMessage("Command Hook registered (AEGP_Command_ALL)");
    }

    // UpdateMenu Hook 등록
    err = suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(
        aegp_plugin_id,
        UpdateMenuHook,
        nullptr);

    if (!err) {
        LogMessage("UpdateMenu Hook registered");
    }

    // Idle Hook 등록
    err = suites.RegisterSuite5()->AEGP_RegisterIdleHook(
        aegp_plugin_id,
        IdleHook,
        nullptr);

    if (!err) {
        LogMessage("Idle Hook registered");
    }

    LogMessage("Ready! Perform text editing actions and check the log.");

    return err;
}
