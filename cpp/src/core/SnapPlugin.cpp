/*****************************************************************************
 * SnapPlugin.cpp
 *
 * Main AEGP plugin implementation for Anchor Snap
 * Uses Native Win32 UI for floating anchor grid (fast, custom styling)
 *****************************************************************************/

#include "SnapPlugin.h"
#include "KeyboardMonitor.h"
#include "GridUI.h"
#include "ControlUI.h"
#include "KeyframeUI.h"
#include "AlignUI.h"
#include "TextUI.h"
#include "CompUI.h"
#include "DMenuUI.h"
#include "CEPBridge.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#ifdef MSWindows
#include <windows.h>
#include <imm.h>
#pragma comment(lib, "imm32.lib")
#endif

#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
#include "AE_Macros.h"

// Global state
static SnapPluginGlobals g_globals = {0, NULL, false, false, false};
static int g_mouseStartX = 0;
static int g_mouseStartY = 0;
static std::chrono::steady_clock::time_point g_keyPressTime;
static bool g_waitingForHold = false;
static const int HOLD_DELAY_MS = 400; // 0.4 seconds
static const int DOUBLE_TAP_MS = 250; // Max time between double-tap
static std::chrono::steady_clock::time_point g_lastYRelease;
static bool g_toggleClickMode = false; // Toggle mode vs hold mode

// Control module state
static bool g_controlVisible = false;
static bool g_eKeyWasHeld = false;

// Keyframe module state (toggle mode via D→K or Right Shift+K)
static bool g_keyframeVisible = false;
static bool g_rshiftKWasHeld = false;  // For Right Shift+K trigger

// D Menu state (D key shows menu, then A/T/K opens panels)
static bool g_dMenuVisible = false;
static bool g_dKeyWasHeld = false;

// Align module state
static bool g_alignVisible = false;

// Text editing detection via UpdateMenuHook
// If UpdateMenuHook is called recently, user is NOT in text editing mode
static auto g_lastMenuHookTime = std::chrono::steady_clock::now();
static const int MENU_HOOK_THRESHOLD_MS = 50;  // 50ms 이내에 호출됐으면 편집 모드 아님

// Panel activation window - allows key input for 1 second after UpdateMenuHook
// Fixes: switching from other window, then immediately pressing key
static auto g_panelActivationTime = std::chrono::steady_clock::now();
static const int PANEL_ACTIVATION_WINDOW_MS = 1000;  // UpdateMenuHook 후 1초간 키 입력 허용

// Text module state
static bool g_textVisible = false;

// Layer module state (D → C)
static bool g_layerVisible = false;

// Text editing mode detection (cached, updated periodically)
static bool g_isTextLayerSelected = false;
static auto g_lastTextLayerCheck = std::chrono::steady_clock::now();

// Forward declaration for ExecuteScript (defined later)
A_Err ExecuteScript(const char *script, char *resultBuf = nullptr, size_t bufSize = 0);

/*****************************************************************************
 * LogToFile
 * Write debug messages to a log file in TEMP folder
 * Usage: LogToFile("message %d", value);
 *****************************************************************************/
#ifdef MSWindows
static FILE* g_logFile = nullptr;
static bool g_logInitialized = false;

void LogToFile(const char* format, ...) {
    if (!g_logInitialized) {
        g_logInitialized = true;
        char path[MAX_PATH];
        if (GetTempPathA(MAX_PATH, path)) {
            strcat_s(path, MAX_PATH, "AnchorSnap.log");
            fopen_s(&g_logFile, path, "a");
            if (g_logFile) {
                fprintf(g_logFile, "\n=== AnchorSnap Log Started ===\n");
                fflush(g_logFile);
            }
        }
    }
    if (g_logFile) {
        va_list args;
        va_start(args, format);
        vfprintf(g_logFile, format, args);
        va_end(args);
        fprintf(g_logFile, "\n");
        fflush(g_logFile);
    }
    // Also output to DebugView
    char buffer[1024];
    va_list args2;
    va_start(args2, format);
    vsnprintf(buffer, sizeof(buffer), format, args2);
    va_end(args2);
    OutputDebugStringA(buffer);
}
#else
void LogToFile(const char* format, ...) { (void)format; }
#endif

#ifdef MSWindows
/*****************************************************************************
 * IsEffectControlsFocused
 * Check if Effect Controls panel is the active/focused panel in AE
 * Uses ExtendScript to detect active panel type
 *****************************************************************************/
bool IsEffectControlsFocused() {
  // Use ExtendScript to check activeViewer type
  // Effect Controls is VIEWER_EFFECT_CONTROLS
  char result[64] = {0};
  ExecuteScript("(function(){"
                "try{"
                "var v=app.activeViewer;"
                "if(v&&v.type==ViewerType.VIEWER_EFFECT_CONTROLS)return '1';"
                "}catch(e){}"
                "return '0';"
                "})();",
                result, sizeof(result));
  return result[0] == '1';
}

/*****************************************************************************
 * IsTextToolActive
 * Check if text tool (horizontal or vertical) is currently active
 * When text tool is active, user is likely editing text and D menu should not appear
 *****************************************************************************/
bool IsTextToolActive() {
  char result[32] = {0};
  ExecuteScript(
      "(function(){"
      "try{"
      "var t=app.project.toolType;"
      "if(t===ToolType.Tool_TextH||t===ToolType.Tool_TextV)return '1';"
      "}catch(e){}"
      "return '0';"
      "})();",
      result, sizeof(result));
  return result[0] == '1';
}

/*****************************************************************************
 * OpenEffectControls
 * Opens Effect Controls panel for selected layer (locked to that layer)
 * Uses app.executeCommand(2163) or findMenuCommandId
 *****************************************************************************/
void OpenEffectControls() {
  // Try to open Effect Controls using known command ID
  // 2163 = Effect Controls (from aenhancers.com)
  ExecuteScript(
      "(function(){"
      "try{"
      "var c=app.project.activeItem;"
      "if(!c||!(c instanceof CompItem))return;"
      "if(c.selectedLayers.length==0)return;"
      // Open Effect Controls panel
      "app.executeCommand(2163);"
      "}catch(e){}"
      "})();",
      NULL, 0);
}

/*****************************************************************************
 * FindEffectControlsWindow
 * Find Effect Controls window position using Win32 API
 * Returns window rect or {0,0,0,0} if not found
 *****************************************************************************/
RECT FindEffectControlsWindow() {
  RECT result = {0, 0, 0, 0};

  // Find AE main window first
  HWND aeWnd = NULL;

  // Callback to find Effect Controls window
  struct FindData {
    HWND found;
  } findData = {NULL};

  // Look for windows with "Effect Controls" in title
  EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
    auto* data = (FindData*)lParam;
    wchar_t title[256];
    GetWindowTextW(hwnd, title, sizeof(title)/sizeof(wchar_t));

    // Check if this is an Effect Controls window
    if (wcsstr(title, L"Effect Controls") != NULL) {
      // Verify it's visible
      if (IsWindowVisible(hwnd)) {
        data->found = hwnd;
        return FALSE; // Stop enumeration
      }
    }
    return TRUE; // Continue
  }, (LPARAM)&findData);

  if (findData.found) {
    GetWindowRect(findData.found, &result);
  }

  return result;
}

/*****************************************************************************
 * GetAllEffectsList
 * Get all available effects from AE (localized names)
 * Returns: "displayName|matchName|category;..."
 *****************************************************************************/
static bool g_effectsLoaded = false;
void GetAllEffectsList(wchar_t* outBuffer, size_t bufSize) {
  outBuffer[0] = L'\0';

  // Use heap allocation to avoid stack overflow
  char* resultBuf = new char[65536];
  if (!resultBuf) return;
  memset(resultBuf, 0, 65536);

  // Inline script to get all effects (doesn't depend on CEP panel)
  ExecuteScript(
      "(function(){"
      "try{"
      "var r=[];"
      "for(var i=0;i<app.effects.length;i++){"
      "var e=app.effects[i];"
      "if(e.category==='')continue;"
      "r.push(e.displayName+'|'+e.matchName+'|'+e.category);"
      "}"
      "return r.join(';');"
      "}catch(ex){return '';}"
      "})();",
      resultBuf, 65536);

  if (resultBuf[0] != '\0' && strncmp(resultBuf, "Error", 5) != 0) {
    MultiByteToWideChar(CP_UTF8, 0, resultBuf, -1, outBuffer, (int)bufSize);
  }
  delete[] resultBuf;
}

/*****************************************************************************
 * GetLayerEffectsList
 * Get list of effects on selected layer for Mode 2
 * Returns: "name1|matchName1|0;name2|matchName2|1;..."
 *****************************************************************************/
void GetLayerEffectsList(wchar_t* outBuffer, size_t bufSize) {
  outBuffer[0] = L'\0';

  char resultBuf[4096] = {0};
  ExecuteScript(
      "(function(){"
      "var c=app.project.activeItem;"
      "if(!c||!(c instanceof CompItem))return '';"
      "if(c.selectedLayers.length==0)return '';"
      "var L=c.selectedLayers[0];"
      "var fx=L.Effects;"
      "if(!fx||fx.numProperties==0)return '';"
      "var arr=[];"
      "for(var i=1;i<=fx.numProperties;i++){"
      "var e=fx.property(i);"
      "arr.push(e.name+'|'+e.matchName+'|'+(i-1));"
      "}"
      "return arr.join(';');"
      "})();",
      resultBuf, sizeof(resultBuf));

  // Convert to wide string
  if (resultBuf[0] != '\0') {
    MultiByteToWideChar(CP_UTF8, 0, resultBuf, -1, outBuffer, (int)bufSize);
  }
}
#else
// macOS stub - TODO: implement
bool IsEffectControlsFocused() { return false; }
bool IsTextToolActive() { return false; }
static bool g_effectsLoaded = false;
void GetAllEffectsList(wchar_t* outBuffer, size_t bufSize) { outBuffer[0] = L'\0'; }
void GetLayerEffectsList(wchar_t* outBuffer, size_t bufSize) { outBuffer[0] = L'\0'; }
void ApplyTextPropertyValue(const char* propName, float value) {}
void ApplyTextColorValue(bool stroke, float r, float g, float b) {}
void ApplyTextJustificationValue(int just) {}
void GetFontsList(wchar_t* outBuffer, size_t bufSize) { outBuffer[0] = L'\0'; }
void ApplyTextFont(const char* postScriptName) {}
#endif

/*****************************************************************************
 * ApplyTextPropertyValue
 * Apply a single text property to the selected text layer
 *****************************************************************************/
#ifdef MSWindows
void ApplyTextPropertyValue(const char* propName, float value) {
  char script[512];
  snprintf(script, sizeof(script),
    "(function(){"
    "try{"
    "var c=app.project.activeItem;"
    "if(!c||!(c instanceof CompItem))return;"
    "var sel=c.selectedLayers;"
    "for(var i=0;i<sel.length;i++){"
    "if(!(sel[i] instanceof TextLayer))continue;"
    "var txt=sel[i].text.sourceText;"
    "var doc=txt.value;"
    "doc.%s=%f;"
    "txt.setValue(doc);"
    "}"
    "}catch(e){}"
    "})();",
    propName, value);
  ExecuteScript(script);
}

/*****************************************************************************
 * ApplyTextColorValue
 * Apply fill or stroke color to the selected text layer
 *****************************************************************************/
void ApplyTextColorValue(bool stroke, float r, float g, float b) {
  char script[512];
  snprintf(script, sizeof(script),
    "(function(){"
    "try{"
    "var c=app.project.activeItem;"
    "if(!c||!(c instanceof CompItem))return;"
    "var sel=c.selectedLayers;"
    "for(var i=0;i<sel.length;i++){"
    "if(!(sel[i] instanceof TextLayer))continue;"
    "var txt=sel[i].text.sourceText;"
    "var doc=txt.value;"
    "doc.%sColor=[%f,%f,%f];"
    "txt.setValue(doc);"
    "}"
    "}catch(e){}"
    "})();",
    stroke ? "stroke" : "fill", r, g, b);
  ExecuteScript(script);
}

/*****************************************************************************
 * ApplyTextJustificationValue
 * Apply paragraph justification to the selected text layer
 *****************************************************************************/
void ApplyTextJustificationValue(int just) {
  const char* justNames[] = {
    "ParagraphJustification.LEFT_JUSTIFY",
    "ParagraphJustification.CENTER_JUSTIFY",
    "ParagraphJustification.RIGHT_JUSTIFY",
    "ParagraphJustification.FULL_JUSTIFY_LASTLINE_LEFT",
    "ParagraphJustification.FULL_JUSTIFY_LASTLINE_CENTER",
    "ParagraphJustification.FULL_JUSTIFY_LASTLINE_RIGHT",
    "ParagraphJustification.FULL_JUSTIFY_LASTLINE_FULL"
  };
  if (just < 0 || just > 6) just = 0;

  char script[512];
  snprintf(script, sizeof(script),
    "(function(){"
    "try{"
    "var c=app.project.activeItem;"
    "if(!c||!(c instanceof CompItem))return;"
    "var sel=c.selectedLayers;"
    "for(var i=0;i<sel.length;i++){"
    "if(!(sel[i] instanceof TextLayer))continue;"
    "var txt=sel[i].text.sourceText;"
    "var doc=txt.value;"
    "doc.justification=%s;"
    "txt.setValue(doc);"
    "}"
    "}catch(e){}"
    "})();",
    justNames[just]);
  ExecuteScript(script);
}

/*****************************************************************************
 * GetFontsList
 * Get list of all available fonts from AE
 * Returns: "familyName|styleName|postScriptName;..."
 *****************************************************************************/
void GetFontsList(wchar_t* outBuffer, size_t bufSize) {
  outBuffer[0] = L'\0';

  // Use heap allocation to avoid stack overflow
  char* resultBuf = new char[131072];  // 128KB for fonts
  if (!resultBuf) return;
  memset(resultBuf, 0, 131072);

  ExecuteScript(
      "(function(){"
      "try{"
      "var fonts=app.fonts.allFonts;"
      "var r=[];"
      "for(var i=0;i<fonts.length;i++){"
      "var fam=fonts[i];"
      "for(var j=0;j<fam.length;j++){"
      "var f=fam[j];"
      "if(f.isSubstitute)continue;"
      "r.push(f.familyName+'|'+f.styleName+'|'+f.postScriptName);"
      "}"
      "}"
      "return r.join(';');"
      "}catch(e){return '';}"
      "})();",
      resultBuf, 131072);

  if (resultBuf[0] != '\0' && strncmp(resultBuf, "Error", 5) != 0) {
    MultiByteToWideChar(CP_UTF8, 0, resultBuf, -1, outBuffer, (int)bufSize);
  }
  delete[] resultBuf;
}

/*****************************************************************************
 * ApplyTextFont
 * Apply a font to the selected text layer by PostScript name
 *****************************************************************************/
void ApplyTextFont(const char* postScriptName) {
  char script[1024];
  snprintf(script, sizeof(script),
    "(function(){"
    "try{"
    "var c=app.project.activeItem;"
    "if(!c||!(c instanceof CompItem))return;"
    "var sel=c.selectedLayers;"
    "for(var i=0;i<sel.length;i++){"
    "if(!(sel[i] instanceof TextLayer))continue;"
    "var txt=sel[i].text.sourceText;"
    "var doc=txt.value;"
    "doc.font='%s';"
    "txt.setValue(doc);"
    "}"
    "}catch(e){}"
    "})();",
    postScriptName);
  ExecuteScript(script);
}
#endif

// Settings loaded from CEP
static int g_loadedGridWidth = 3;
static int g_loadedGridHeight = 3;
static int g_loadedGridScale = 2;    // 0-9, default 2 (0%)
static int g_loadedGridOpacity = 75; // 0-100%
static int g_loadedCellOpacity = 50; // 0-100%

// Module scales (0-9: -20% to +70%, default 2 = 0%)
static int g_moduleScaleControl = 2;   // Effect Search (Shift+E)
static int g_moduleScaleKeyframe = 2;  // Keyframe (K)
static int g_moduleScaleAlign = 2;     // Align (A)
static int g_moduleScaleText = 2;      // Text (T)
static int g_moduleScaleDmenu = 2;     // D-Menu (D)
static int g_moduleScaleComp = 2;      // Comp (C)

// Scale factor lookup: index 0-9 → -20% to +70%
static const float g_scaleFactors[10] = {
    0.80f, 0.90f, 1.00f, 1.10f, 1.20f, 1.30f, 1.40f, 1.50f, 1.60f, 1.70f
};

// Module scale factor getters (returns 0.8 ~ 1.7)
float GetModuleScaleFactor(const char* moduleName) {
    int scaleIdx = 2; // default 0%
    if (strcmp(moduleName, "control") == 0) scaleIdx = g_moduleScaleControl;
    else if (strcmp(moduleName, "keyframe") == 0) scaleIdx = g_moduleScaleKeyframe;
    else if (strcmp(moduleName, "align") == 0) scaleIdx = g_moduleScaleAlign;
    else if (strcmp(moduleName, "text") == 0) scaleIdx = g_moduleScaleText;
    else if (strcmp(moduleName, "dmenu") == 0) scaleIdx = g_moduleScaleDmenu;
    else if (strcmp(moduleName, "comp") == 0) scaleIdx = g_moduleScaleComp;

    if (scaleIdx < 0) scaleIdx = 0;
    if (scaleIdx > 9) scaleIdx = 9;
    return g_scaleFactors[scaleIdx];
}

// Define MissingSuiteError for AEGP_SuiteHandler
void AEGP_SuiteHandler::MissingSuiteError() const {
  throw std::runtime_error("Missing AEGP Suite");
}

/*****************************************************************************
 * ExecuteScript
 * Execute ExtendScript using AEGP_ExecuteScript, returns result string
 *****************************************************************************/
A_Err ExecuteScript(const char *script, char *resultBuf,
                    size_t bufSize) {
  A_Err err = A_Err_NONE;

  try {
    AEGP_SuiteHandler suites(g_globals.pica_basicP);

    AEGP_MemHandle resultH = NULL;
    AEGP_MemHandle errorH = NULL;

    err = suites.UtilitySuite6()->AEGP_ExecuteScript(
        g_globals.plugin_id, script, TRUE, &resultH, &errorH);

    // Get result if requested
    if (resultBuf && bufSize > 0 && resultH) {
      A_char *resultStr = NULL;
      suites.MemorySuite1()->AEGP_LockMemHandle(resultH, (void **)&resultStr);
      if (resultStr) {
        strncpy(resultBuf, resultStr, bufSize - 1);
        resultBuf[bufSize - 1] = '\0';
      }
      suites.MemorySuite1()->AEGP_UnlockMemHandle(resultH);
    }

    if (resultH) {
      suites.MemorySuite1()->AEGP_FreeMemHandle(resultH);
    }
    if (errorH) {
      suites.MemorySuite1()->AEGP_FreeMemHandle(errorH);
    }

  } catch (...) {
    err = A_Err_GENERIC;
  }

  return err;
}

/*****************************************************************************
 * HasSelectedLayers
 * Check if there are selected layers and active panel is Viewer/Timeline
 *****************************************************************************/
bool HasSelectedLayers() {
  char result[64] = {0};
  ExecuteScript("(function(){"
                "var c=app.project.activeItem;"
                "if(!c||!(c instanceof CompItem))return 0;"
                "if(c.selectedLayers.length==0)return 0;"
                "var v=app.activeViewer;"
                "if(!v)return 0;"
                "var t=v.type;"
                "if(t!=ViewerType.VIEWER_COMPOSITION&&t!=ViewerType.VIEWER_"
                "LAYER)return 0;"
                "return 1;"
                "})();",
                result, sizeof(result));
  return atoi(result) > 0;
}

/*****************************************************************************
 * HasSelectedTextLayer
 * Check if any selected layer is a text layer
 *****************************************************************************/
bool HasSelectedTextLayer() {
  char result[64] = {0};
  ExecuteScript("(function(){"
                "var c=app.project.activeItem;"
                "if(!c||!(c instanceof CompItem))return 0;"
                "var sel=c.selectedLayers;"
                "if(sel.length==0)return 0;"
                "for(var i=0;i<sel.length;i++){"
                "if(sel[i] instanceof TextLayer)return 1;"
                "}"
                "return 0;"
                "})();",
                result, sizeof(result));
  return atoi(result) > 0;
}

/*****************************************************************************
 * LoadSettingsFromFile
 * Read settings from CEP's settings file (cross-platform)
 *****************************************************************************/
void LoadSettingsFromFile() {
#ifdef MSWindows
  // Windows: %APPDATA%\Adobe\CEP\extensions\com.anchor.snap\settings.json
  char path[512];
  const char *appdata = getenv("APPDATA");
  if (!appdata)
    return;
  snprintf(path, sizeof(path),
           "%s\\Adobe\\CEP\\extensions\\com.anchor.snap\\settings.json",
           appdata);
#else
  // macOS: ~/Library/Application
  // Support/Adobe/CEP/extensions/com.anchor.snap/settings.json
  char path[512];
  const char *home = getenv("HOME");
  if (!home)
    return;
  snprintf(path, sizeof(path),
           "%s/Library/Application "
           "Support/Adobe/CEP/extensions/com.anchor.snap/settings.json",
           home);
#endif

  FILE *f = fopen(path, "r");
  if (!f)
    return;

  char buffer[2048];
  size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
  buffer[len] = '\0';
  fclose(f);

  // Parse simple JSON
  NativeUI::GridSettings &settings = NativeUI::GetSettings();

  const char *p;
  // useCompMode
  if ((p = strstr(buffer, "\"useCompMode\":")) != NULL) {
    p += 14; // skip past key
    settings.useCompMode =
        (strstr(p, "true") != NULL &&
         (strstr(p, "true") < strstr(p, ",") || strstr(p, ",") == NULL));
  }
  // useMaskRecognition
  if ((p = strstr(buffer, "\"useMaskRecognition\":")) != NULL) {
    p += 20; // "useMaskRecognition": = 20 chars
    settings.useMaskRecognition =
        (strstr(p, "true") != NULL &&
         (strstr(p, "true") < strstr(p, ",") || strstr(p, ",") == NULL));
  }
  // settingsPanelOpen
  if ((p = strstr(buffer, "\"settingsPanelOpen\":")) != NULL) {
    p += 20; // "settingsPanelOpen": = 20 chars
    settings.settingsPanelOpen =
        (strstr(p, "true") != NULL &&
         (strstr(p, "true") < strstr(p, ",") || strstr(p, ",") == NULL));
  }
  // gridWidth
  if ((p = strstr(buffer, "\"gridWidth\":")) != NULL) {
    p += 12;
    int val = atoi(p);
    if (val >= 3 && val <= 7) {
      g_loadedGridWidth = val;
    }
  }
  // gridHeight
  if ((p = strstr(buffer, "\"gridHeight\":")) != NULL) {
    p += 13;
    int val = atoi(p);
    if (val >= 3 && val <= 7) {
      g_loadedGridHeight = val;
    }
  }
  // gridScale (0-9)
  if ((p = strstr(buffer, "\"gridScale\":")) != NULL) {
    p += 12;
    int val = atoi(p);
    if (val >= 0 && val <= 9) {
      g_loadedGridScale = val;
    }
  }
  // gridOpacity
  if ((p = strstr(buffer, "\"gridOpacity\":")) != NULL) {
    p += 14;
    int val = atoi(p);
    if (val >= 0 && val <= 100) {
      g_loadedGridOpacity = val;
    }
  }
  // cellOpacity
  if ((p = strstr(buffer, "\"cellOpacity\":")) != NULL) {
    p += 14;
    int val = atoi(p);
    if (val >= 0 && val <= 100) {
      g_loadedCellOpacity = val;
    }
  }

  // customAnchors array - parse [{"x":50,"y":50},{"x":50,"y":50},{"x":50,"y":50}]
  if ((p = strstr(buffer, "\"customAnchors\":")) != NULL) {
    // Find the array start
    p = strchr(p, '[');
    if (p) {
      NativeUI::GridSettings &settings = NativeUI::GetSettings();
      // Parse each anchor object
      for (int i = 0; i < 3; i++) {
        // Find "x":
        const char *xp = strstr(p, "\"x\":");
        if (!xp)
          break;
        xp += 4;
        // Skip whitespace
        while (*xp == ' ' || *xp == '\t')
          xp++;
        int xVal = atoi(xp);

        // Find "y":
        const char *yp = strstr(xp, "\"y\":");
        if (!yp)
          break;
        yp += 4;
        // Skip whitespace
        while (*yp == ' ' || *yp == '\t')
          yp++;
        int yVal = atoi(yp);

        // Convert from 0-100 percent to 0.0-1.0 ratio
        float rx = xVal / 100.0f;
        float ry = yVal / 100.0f;

        switch (i) {
        case 0:
          settings.customAnchor1X = rx;
          settings.customAnchor1Y = ry;
          break;
        case 1:
          settings.customAnchor2X = rx;
          settings.customAnchor2Y = ry;
          break;
        case 2:
          settings.customAnchor3X = rx;
          settings.customAnchor3Y = ry;
          break;
        }

        // Move to next object
        p = strchr(yp, '}');
        if (!p)
          break;
      }
    }
  }

  // clipboardAnchor - parse {"x":0.5,"y":0.5} (ratio 0-1)
  if ((p = strstr(buffer, "\"clipboardAnchor\":")) != NULL) {
    // Check if it's not null
    const char *nullCheck = strstr(p, "null");
    const char *objStart = strchr(p, '{');
    if (objStart && (!nullCheck || objStart < nullCheck)) {
      const char *xp = strstr(objStart, "\"x\":");
      if (xp) {
        xp += 4;
        while (*xp == ' ' || *xp == '\t')
          xp++;
        float rx = (float)atof(xp);

        const char *yp = strstr(xp, "\"y\":");
        if (yp) {
          yp += 4;
          while (*yp == ' ' || *yp == '\t')
            yp++;
          float ry = (float)atof(yp);

          // Store in NativeUI clipboard
          NativeUI::SetClipboardAnchor(rx, ry);
        }
      }
    }
  }

  // moduleScales - parse {"control":2,"keyframe":2,"align":2,"text":2,"dmenu":2,"comp":2}
  if ((p = strstr(buffer, "\"moduleScales\":")) != NULL) {
    const char *objStart = strchr(p, '{');
    if (objStart) {
      // control
      const char *kp = strstr(objStart, "\"control\":");
      if (kp) {
        kp += 10;
        int val = atoi(kp);
        if (val >= 0 && val <= 9) g_moduleScaleControl = val;
      }
      // keyframe
      kp = strstr(objStart, "\"keyframe\":");
      if (kp) {
        kp += 11;
        int val = atoi(kp);
        if (val >= 0 && val <= 9) g_moduleScaleKeyframe = val;
      }
      // align
      kp = strstr(objStart, "\"align\":");
      if (kp) {
        kp += 8;
        int val = atoi(kp);
        if (val >= 0 && val <= 9) g_moduleScaleAlign = val;
      }
      // text
      kp = strstr(objStart, "\"text\":");
      if (kp) {
        kp += 7;
        int val = atoi(kp);
        if (val >= 0 && val <= 9) g_moduleScaleText = val;
      }
      // dmenu
      kp = strstr(objStart, "\"dmenu\":");
      if (kp) {
        kp += 8;
        int val = atoi(kp);
        if (val >= 0 && val <= 9) g_moduleScaleDmenu = val;
      }
      // comp
      kp = strstr(objStart, "\"comp\":");
      if (kp) {
        kp += 7;
        int val = atoi(kp);
        if (val >= 0 && val <= 9) g_moduleScaleComp = val;
      }
    }
  }
}

/*****************************************************************************
 * SaveClipboardAnchorToFile
 * Save clipboard anchor to settings.json for CEP panel access
 *****************************************************************************/
void SaveClipboardAnchorToFile(float rx, float ry) {
#ifdef MSWindows
  char path[512];
  const char *appdata = getenv("APPDATA");
  if (!appdata)
    return;
  snprintf(path, sizeof(path),
           "%s\\Adobe\\CEP\\extensions\\com.anchor.snap\\settings.json",
           appdata);
#else
  char path[512];
  const char *home = getenv("HOME");
  if (!home)
    return;
  snprintf(path, sizeof(path),
           "%s/Library/Application "
           "Support/Adobe/CEP/extensions/com.anchor.snap/settings.json",
           home);
#endif

  // Read existing file
  char buffer[4096] = {0};
  FILE *f = fopen(path, "r");
  if (f) {
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[len] = '\0';
    fclose(f);
  }

  // Check if clipboardAnchor already exists
  char *p = strstr(buffer, "\"clipboardAnchor\":");
  if (p) {
    // Update existing value
    char *valStart = strchr(p, '{');
    char *valEnd = valStart ? strchr(valStart, '}') : NULL;
    if (valStart && valEnd) {
      char newBuffer[4096];
      strncpy(newBuffer, buffer, valStart + 1 - buffer);
      newBuffer[valStart + 1 - buffer] = '\0';
      char newVal[64];
      snprintf(newVal, sizeof(newVal), "\"x\":%.4f,\"y\":%.4f", rx, ry);
      strcat(newBuffer, newVal);
      strcat(newBuffer, valEnd);
      f = fopen(path, "w");
      if (f) {
        fputs(newBuffer, f);
        fclose(f);
      }
    }
  } else {
    // Add clipboardAnchor before the last }
    char *lastBrace = strrchr(buffer, '}');
    if (lastBrace) {
      char newBuffer[4096];
      strncpy(newBuffer, buffer, lastBrace - buffer);
      newBuffer[lastBrace - buffer] = '\0';
      char newVal[128];
      snprintf(newVal, sizeof(newVal),
               ",\"clipboardAnchor\":{\"x\":%.4f,\"y\":%.4f}}", rx, ry);
      strcat(newBuffer, newVal);
      f = fopen(path, "w");
      if (f) {
        fputs(newBuffer, f);
        fclose(f);
      }
    }
  }
}

/*****************************************************************************
 * SaveSettingsToFile
 * Write mode settings to CEP's settings file (only useCompMode and
 *useMaskRecognition)
 *****************************************************************************/
void SaveSettingsToFile() {
#ifdef MSWindows
  char path[512];
  const char *appdata = getenv("APPDATA");
  if (!appdata)
    return;
  snprintf(path, sizeof(path),
           "%s\\Adobe\\CEP\\extensions\\com.anchor.snap\\settings.json",
           appdata);
#else
  char path[512];
  const char *home = getenv("HOME");
  if (!home)
    return;
  snprintf(path, sizeof(path),
           "%s/Library/Application "
           "Support/Adobe/CEP/extensions/com.anchor.snap/settings.json",
           home);
#endif

  // Read existing file
  char buffer[2048] = {0};
  FILE *f = fopen(path, "r");
  if (f) {
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[len] = '\0';
    fclose(f);
  }

  NativeUI::GridSettings &settings = NativeUI::GetSettings();

  // Update useCompMode and useMaskRecognition in the JSON
  char newBuffer[2048];
  char *p;

  // Simple replacement: find and replace the values
  strcpy(newBuffer, buffer);

  // Update useCompMode
  if ((p = strstr(newBuffer, "\"useCompMode\":")) != NULL) {
    char *valStart = p + 14;
    char *valEnd = strchr(valStart, ',');
    if (!valEnd)
      valEnd = strchr(valStart, '}');
    if (valEnd) {
      char temp[2048];
      strncpy(temp, newBuffer, valStart - newBuffer);
      temp[valStart - newBuffer] = '\0';
      strcat(temp, settings.useCompMode ? "true" : "false");
      strcat(temp, valEnd);
      strcpy(newBuffer, temp);
    }
  }

  // Update useMaskRecognition
  if ((p = strstr(newBuffer, "\"useMaskRecognition\":")) != NULL) {
    char *valStart = p + 21;
    char *valEnd = strchr(valStart, ',');
    if (!valEnd)
      valEnd = strchr(valStart, '}');
    if (valEnd) {
      char temp[2048];
      strncpy(temp, newBuffer, valStart - newBuffer);
      temp[valStart - newBuffer] = '\0';
      strcat(temp, settings.useMaskRecognition ? "true" : "false");
      strcat(temp, valEnd);
      strcpy(newBuffer, temp);
    }
  }

  // Write back
  f = fopen(path, "w");
  if (f) {
    fputs(newBuffer, f);
    fclose(f);
  }
}

/*****************************************************************************
 * GetPresetFilePath
 * Get the file path for a preset slot
 *****************************************************************************/
void GetPresetFilePath(int slotIndex, char* outPath, size_t pathSize) {
#ifdef MSWindows
  const char *appdata = getenv("APPDATA");
  if (!appdata) {
    outPath[0] = '\0';
    return;
  }
  snprintf(outPath, pathSize,
           "%s\\Adobe\\CEP\\extensions\\com.anchor.snap\\preset_%d.json",
           appdata, slotIndex);
#else
  const char *home = getenv("HOME");
  if (!home) {
    outPath[0] = '\0';
    return;
  }
  snprintf(outPath, pathSize,
           "%s/Library/Application Support/Adobe/CEP/extensions/com.anchor.snap/preset_%d.json",
           home, slotIndex);
#endif
}

/*****************************************************************************
 * SavePresetToSlot
 * Save effect preset data to a file
 *****************************************************************************/
void SavePresetToSlot(int slotIndex, const char* presetJson) {
  if (slotIndex < 0 || slotIndex > 2) return;
  if (!presetJson || presetJson[0] == '\0') return;

  char path[512];
  GetPresetFilePath(slotIndex, path, sizeof(path));
  if (path[0] == '\0') return;

  FILE *f = fopen(path, "w");
  if (f) {
    fputs(presetJson, f);
    fclose(f);
  }
}

/*****************************************************************************
 * LoadPresetFromSlot
 * Load effect preset data from a file
 *****************************************************************************/
bool LoadPresetFromSlot(int slotIndex, char* outBuffer, size_t bufSize) {
  if (slotIndex < 0 || slotIndex > 2) return false;
  if (!outBuffer || bufSize == 0) return false;

  outBuffer[0] = '\0';

  char path[512];
  GetPresetFilePath(slotIndex, path, sizeof(path));
  if (path[0] == '\0') return false;

  FILE *f = fopen(path, "r");
  if (!f) return false;

  size_t len = fread(outBuffer, 1, bufSize - 1, f);
  outBuffer[len] = '\0';
  fclose(f);

  return len > 0;
}

/*****************************************************************************
 * PresetSlotExists
 * Check if a preset file exists for a slot
 *****************************************************************************/
bool PresetSlotExists(int slotIndex) {
  if (slotIndex < 0 || slotIndex > 2) return false;

  char path[512];
  GetPresetFilePath(slotIndex, path, sizeof(path));
  if (path[0] == '\0') return false;

  FILE *f = fopen(path, "r");
  if (f) {
    fclose(f);
    return true;
  }
  return false;
}

/*****************************************************************************
 * EscapeJsonForScript
 * Escape JSON string for use in ExtendScript
 *****************************************************************************/
void EscapeJsonForScript(const char* input, char* output, size_t outputSize) {
  size_t j = 0;
  for (size_t i = 0; input[i] != '\0' && j < outputSize - 2; i++) {
    if (input[i] == '\\') {
      output[j++] = '\\';
      output[j++] = '\\';
    } else if (input[i] == '\'') {
      output[j++] = '\\';
      output[j++] = '\'';
    } else if (input[i] == '\n') {
      output[j++] = '\\';
      output[j++] = 'n';
    } else if (input[i] == '\r') {
      output[j++] = '\\';
      output[j++] = 'r';
    } else {
      output[j++] = input[i];
    }
  }
  output[j] = '\0';
}

/*****************************************************************************
 * ShowAnchorGrid
 * Show the native anchor grid at specified position
 *****************************************************************************/
void ShowAnchorGrid(int mouseX, int mouseY) {
  NativeUI::GridConfig config;
  config.gridWidth = g_loadedGridWidth;
  config.gridHeight = g_loadedGridHeight;

  // Scale: 0=-20%, 1=-10%, 2=0%, 3=+10%, ... 9=+70%
  int baseSize = 40;
  float scaleFactors[] = {0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f};
  config.cellSize = (int)(baseSize * scaleFactors[g_loadedGridScale]);

  config.spacing = 1;
  config.margin = 2;

  // Apply opacity settings
  NativeUI::GridSettings &settings = NativeUI::GetSettings();
  settings.gridOpacity = g_loadedGridOpacity;
  settings.cellOpacity = g_loadedCellOpacity;

  NativeUI::ShowGrid(mouseX, mouseY, config);
}

/*****************************************************************************
 * ApplyAnchorToLayers
 * Apply anchor point to selected layers based on grid position
 * Respects composition mode and mask recognition settings
 *****************************************************************************/
void ApplyAnchorToLayers(int gridX, int gridY) {
  int gridW = g_loadedGridWidth;
  int gridH = g_loadedGridHeight;

  // Get current mode settings
  NativeUI::GridSettings &settings = NativeUI::GetSettings();
  bool useCompMode = settings.useCompMode;
  bool useMaskMode = settings.useMaskRecognition;

  char script[6000];
  snprintf(
      script, sizeof(script),
      "(function(){"
      "var gx=%d,gy=%d,gridW=%d,gridH=%d;"
      "var useCompMode=%s,useMaskMode=%s;"
      "var c=app.project.activeItem;"
      "if(!c||!(c instanceof CompItem))return;"
      "if(c.selectedLayers.length==0)return;"
      "app.beginUndoGroup('Set Anchor');"
      "for(var i=0;i<c.selectedLayers.length;i++){"
      "var L=c.selectedLayers[i];"
      "var b=null;"
      // Composition mode: use comp bounds
      "if(useCompMode){"
      "b={left:0,top:0,width:c.width,height:c.height};"
      "}else{"
      // Selection mode: try mask first if enabled, then sourceRect
      "if(useMaskMode){"
      "var masks=L.property('ADBE Mask Parade');"
      "if(masks&&masks.numProperties>0){"
      "var minX=Infinity,minY=Infinity,maxX=-Infinity,maxY=-Infinity;"
      "for(var m=1;m<=masks.numProperties;m++){"
      "var mask=masks.property(m);"
      "var path=mask.property('ADBE Mask Shape').valueAtTime(c.time,false);"
      "if(!path||!path.vertices)continue;"
      "var verts=path.vertices;"
      "for(var v=0;v<verts.length;v++){"
      "var pt=verts[v];"
      "if(pt[0]<minX)minX=pt[0];"
      "if(pt[0]>maxX)maxX=pt[0];"
      "if(pt[1]<minY)minY=pt[1];"
      "if(pt[1]>maxY)maxY=pt[1];"
      "}}"
      "if(minX!=Infinity){"
      "b={left:minX,top:minY,width:maxX-minX,height:maxY-minY};"
      "}}}"
      // Fall back to sourceRectAtTime
      "if(!b)b=L.sourceRectAtTime(c.time,false);"
      "}"
      "if(!b||b.width<=0||b.height<=0)continue;"
      "var px=gx/(gridW-1),py=gy/(gridH-1);"
      // Calculate anchor in appropriate coordinate space
      "var nx,ny;"
      "if(useCompMode){"
      // Composition mode: convert comp coord to layer local
      // Get layer transform properties
      "var pos=L.position.value;"
      "var anc=L.anchorPoint.value;"
      "var sc=L.scale.value;"
      "var rot=L.rotation.value*Math.PI/180;"
      // Target position in comp space
      "var compX=c.width*px,compY=c.height*py;"
      // Translate: comp coord relative to layer position
      "var dx=compX-pos[0],dy=compY-pos[1];"
      // Reverse rotation
      "var cos_r=Math.cos(-rot),sin_r=Math.sin(-rot);"
      "var rx=dx*cos_r-dy*sin_r,ry=dx*sin_r+dy*cos_r;"
      // Reverse scale
      "var sx=100/sc[0],sy=100/sc[1];"
      "rx=rx*sx;ry=ry*sy;"
      // Add anchor offset to get local coord
      "nx=rx+anc[0];ny=ry+anc[1];"
      "}else{"
      // Selection mode: already in layer local coordinates
      "nx=b.left+b.width*px;ny=b.top+b.height*py;"
      "}"
      "var ap=L.property('ADBE Transform Group').property('ADBE Anchor "
      "Point');"
      "var pp=L.property('ADBE Transform Group').property('ADBE Position');"
      "if(!ap||!pp)continue;"
      "var oa=ap.value,pos=pp.value;"
      "var dx=nx-oa[0],dy=ny-oa[1];"
      "var sc=L.property('ADBE Transform Group').property('ADBE "
      "Scale').value;"
      "var sx=sc[0]/100,sy=sc[1]/100;"
      "var rot=L.property('ADBE Transform Group').property('ADBE Rotate "
      "Z').value*Math.PI/180;"
      "var rdx=(dx*Math.cos(rot)-dy*Math.sin(rot))*sx;"
      "var rdy=(dx*Math.sin(rot)+dy*Math.cos(rot))*sy;"
      "var newAp=[nx,ny];"
      "var newPos=pos.length==3?[pos[0]+rdx,pos[1]+rdy,pos[2]]:[pos[0]+rdx,pos[1]+rdy];"
      "if(ap.numKeys>0)ap.setValueAtTime(c.time,newAp);else ap.setValue(newAp);"
      "if(pp.numKeys>0)pp.setValueAtTime(c.time,newPos);else pp.setValue(newPos);"
      "}"
      "app.endUndoGroup();"
      "})();",
      gridX, gridY, gridW, gridH, useCompMode ? "true" : "false",
      useMaskMode ? "true" : "false");

  ExecuteScript(script);
}

/*****************************************************************************
 * ApplyCustomAnchor
 * Apply a custom anchor point at specified ratio (0-1)
 *****************************************************************************/
void ApplyCustomAnchor(float ratioX, float ratioY) {
  char script[3000];
  snprintf(
      script, sizeof(script),
      "(function(){"
      "var rx=%.4f,ry=%.4f;"
      "var c=app.project.activeItem;"
      "if(!c||!(c instanceof CompItem))return;"
      "if(c.selectedLayers.length==0)return;"
      "app.beginUndoGroup('Set Custom Anchor');"
      "for(var i=0;i<c.selectedLayers.length;i++){"
      "var L=c.selectedLayers[i];"
      "var b=L.sourceRectAtTime(c.time,false);"
      "if(!b||b.width<=0||b.height<=0)continue;"
      "var nx=b.left+b.width*rx,ny=b.top+b.height*ry;"
      "var ap=L.property('ADBE Transform Group').property('ADBE Anchor "
      "Point');"
      "var pp=L.property('ADBE Transform Group').property('ADBE Position');"
      "if(!ap||!pp)continue;"
      "var oa=ap.value,pos=pp.value;"
      "var dx=nx-oa[0],dy=ny-oa[1];"
      "var sc=L.property('ADBE Transform Group').property('ADBE "
      "Scale').value;"
      "var sx=sc[0]/100,sy=sc[1]/100;"
      "var rot=L.property('ADBE Transform Group').property('ADBE Rotate "
      "Z').value*Math.PI/180;"
      "var rdx=(dx*Math.cos(rot)-dy*Math.sin(rot))*sx;"
      "var rdy=(dx*Math.sin(rot)+dy*Math.cos(rot))*sy;"
      "var newAp=[nx,ny];"
      "var newPos=pos.length==3?[pos[0]+rdx,pos[1]+rdy,pos[2]]:[pos[0]+rdx,pos[1]+rdy];"
      "if(ap.numKeys>0)ap.setValueAtTime(c.time,newAp);else ap.setValue(newAp);"
      "if(pp.numKeys>0)pp.setValueAtTime(c.time,newPos);else pp.setValue(newPos);"
      "}"
      "app.endUndoGroup();"
      "})();",
      ratioX, ratioY);

  ExecuteScript(script);
}

/*****************************************************************************
 * HideAndApplyAnchor
 * Close the grid and apply anchor or handle extended menu option
 *****************************************************************************/
void HideAndApplyAnchor() {
  int mouseX = 0, mouseY = 0;
  KeyboardMonitor::GetMousePosition(&mouseX, &mouseY);

  // First check if hovering on a mode toggle - don't hide, just toggle and
  // repaint
  NativeUI::GridSettings &settings = NativeUI::GetSettings();
  int hoverX = -1, hoverY = -1;
  NativeUI::GetHoverCell(&hoverX, &hoverY);

  // Get current hover option from the grid using getter function
  NativeUI::ExtendedOption hoverOpt = NativeUI::GetHoverExtOption();

  // Handle mode toggles - toggle setting, then notify CEP
  if (hoverOpt == NativeUI::OPT_COMP_MODE) {
    settings.useCompMode = !settings.useCompMode;
    SaveSettingsToFile(); // Persist to file
    // Notify CEP of change via inline ExtendScript (fails gracefully if CEP not open)
    char script[512];
    snprintf(script, sizeof(script),
             "(function(){"
             "try{"
             "var lib=new ExternalObject('lib:PlugPlugExternalObject');"
             "if(lib){"
             "var e=new CSXSEvent();"
             "e.type='anchorGridModeChanged';"
             "e.data=JSON.stringify({useCompMode:%s,useMaskRecognition:%s});"
             "e.dispatch();"
             "}"
             "}catch(ex){}"
             "})();",
             settings.useCompMode ? "true" : "false",
             settings.useMaskRecognition ? "true" : "false");
    ExecuteScript(script);
    // Don't return - fall through to hide grid
  } else if (hoverOpt == NativeUI::OPT_MASK_MODE) {
    settings.useMaskRecognition = !settings.useMaskRecognition;
    SaveSettingsToFile(); // Persist to file
    // Notify CEP of change via inline ExtendScript (fails gracefully if CEP not open)
    char script[512];
    snprintf(script, sizeof(script),
             "(function(){"
             "try{"
             "var lib=new ExternalObject('lib:PlugPlugExternalObject');"
             "if(lib){"
             "var e=new CSXSEvent();"
             "e.type='anchorGridModeChanged';"
             "e.data=JSON.stringify({useCompMode:%s,useMaskRecognition:%s});"
             "e.dispatch();"
             "}"
             "}catch(ex){}"
             "})();",
             settings.useCompMode ? "true" : "false",
             settings.useMaskRecognition ? "true" : "false");
    ExecuteScript(script);
    // Don't return - fall through to hide grid
  }

  // For all other options, hide the grid first
  NativeUI::GridResult result = NativeUI::HideGrid(mouseX, mouseY);

  // Handle extended menu options (side panel icons)
  if (result.extendedOption != NativeUI::OPT_NONE) {
    switch (result.extendedOption) {
    // Left panel: Custom anchors
    case NativeUI::OPT_CUSTOM_1:
      ApplyCustomAnchor(settings.customAnchor1X, settings.customAnchor1Y);
      break;
    case NativeUI::OPT_CUSTOM_2:
      ApplyCustomAnchor(settings.customAnchor2X, settings.customAnchor2Y);
      break;
    case NativeUI::OPT_CUSTOM_3:
      ApplyCustomAnchor(settings.customAnchor3X, settings.customAnchor3Y);
      break;
    case NativeUI::OPT_SETTINGS:
      // Open CEP panel via Window menu
      ExecuteScript("(function(){"
                    "try{"
                    "var menuId=app.findMenuCommandId('Anchor Snap');"
                    "if(menuId>0)app.executeCommand(menuId);"
                    "}catch(e){"
                    "alert('Please open Window > Extensions > Anchor Snap');"
                    "}"
                    "})();");
      break;
    // Copy/Paste anchor
    case NativeUI::OPT_COPY_ANCHOR: {
      // Get current anchor ratio from selected layer
      char resultBuf[256] = {0};
      ExecuteScript("(function(){"
                    "var c=app.project.activeItem;"
                    "if(!c||!(c instanceof CompItem))return 'null';"
                    "if(c.selectedLayers.length==0)return 'null';"
                    "var L=c.selectedLayers[0];"
                    "var b=L.sourceRectAtTime(c.time,false);"
                    "if(!b||b.width<=0||b.height<=0)return 'null';"
                    "var ap=L.property('ADBE Transform Group').property('ADBE "
                    "Anchor Point').value;"
                    "var rx=(ap[0]-b.left)/b.width;"
                    "var ry=(ap[1]-b.top)/b.height;"
                    "return rx.toFixed(4)+','+ry.toFixed(4);"
                    "})();",
                    resultBuf, sizeof(resultBuf));
      // Parse result
      if (resultBuf[0] != 'n' && resultBuf[0] != '\0') {
        float rx = 0.5f, ry = 0.5f;
        if (sscanf(resultBuf, "%f,%f", &rx, &ry) == 2) {
          // Store using NativeUI clipboard functions
          NativeUI::SetClipboardAnchor(rx, ry);
          // Also save to settings.json for CEP panel access
          SaveClipboardAnchorToFile(rx, ry);
        }
      }
      break;
    }
    case NativeUI::OPT_PASTE_ANCHOR: {
      // Apply copied anchor ratio
      if (NativeUI::HasClipboardAnchor()) {
        float rx, ry;
        NativeUI::GetClipboardAnchor(&rx, &ry);
        ApplyCustomAnchor(rx, ry);
      }
      break;
    }
    default:
      break;
    }
    return;
  }

  // Normal grid selection
  if (!result.cancelled && result.gridX >= 0 && result.gridY >= 0) {
    ApplyAnchorToLayers(result.gridX, result.gridY);
  }
}

/*****************************************************************************
 * CommandHook
 * Monitors AE commands - logs all command IDs for debugging
 *****************************************************************************/
static A_Err CommandHook(
    AEGP_GlobalRefcon plugin_refconP,
    AEGP_CommandRefcon refconP,
    AEGP_Command command,
    AEGP_HookPriority hook_priority,
    A_Boolean already_handledB,
    A_Boolean *handledPB) {

  // Log to AE debug console (visible with -debug flag or in log file)
  if (g_globals.pica_basicP) {
    AEGP_SuiteHandler suites(g_globals.pica_basicP);
    char info[64];
    snprintf(info, sizeof(info), "cmd=%d", command);
    suites.UtilitySuite6()->AEGP_WriteToDebugLog("AnchorSnap", "CommandHook", info);
  }

  *handledPB = FALSE;
  return A_Err_NONE;
}

/*****************************************************************************
 * UpdateMenuHook
 * Called when menus need updating - used to detect NOT in text editing mode
 * If this hook is called, user is NOT typing in a text field
 *****************************************************************************/
static A_Err UpdateMenuHook(
    AEGP_GlobalRefcon plugin_refconP,
    AEGP_UpdateMenuRefcon refconP,
    AEGP_WindowType active_window) {

  auto now = std::chrono::steady_clock::now();
  g_lastMenuHookTime = now;

  // Also update panel activation time for 1-second key input window
  // This covers: panel clicks, key inputs, returning from other apps
  g_panelActivationTime = now;

  return A_Err_NONE;
}

// Helper: Check if UpdateMenuHook was called recently (= NOT in text editing)
static bool IsMenuHookRecent() {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastMenuHookTime).count();
  return elapsed < MENU_HOOK_THRESHOLD_MS;
}

// Helper: Check if within panel activation window (1 second after valid panel state)
static bool IsInPanelActivationWindow() {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_panelActivationTime).count();
  return elapsed < PANEL_ACTIVATION_WINDOW_MS;
}

// Combined check: either UpdateMenuHook recent OR within panel activation window
static bool IsKeyInputAllowed() {
  return IsMenuHookRecent() || IsInPanelActivationWindow();
}

/*****************************************************************************
 * IdleHook
 * Called periodically by After Effects - we use this to check keyboard state
 *****************************************************************************/
A_Err IdleHook(AEGP_GlobalRefcon plugin_refconP, AEGP_IdleRefcon refconP,
               A_long *max_sleepPL) {
  A_Err err = A_Err_NONE;

  // Preload effects list on first idle (so Shift+E is fast)
  if (!g_effectsLoaded) {
    // Use heap allocation to avoid stack overflow (65536 * 2 = 128KB is too large for stack)
    wchar_t* allEffects = new wchar_t[65536];
    if (allEffects) {
      GetAllEffectsList(allEffects, 65536);
      if (allEffects[0] != L'\0') {
        ControlUI::SetAvailableEffects(allEffects);
        g_effectsLoaded = true;
      }
      delete[] allEffects;
    }
  }

  bool y_key_held = KeyboardMonitor::IsKeyHeld(KeyboardMonitor::KEY_Y);
  bool alt_held = KeyboardMonitor::IsAltHeld();
  auto now = std::chrono::steady_clock::now();

  // Y key just pressed
  // Skip if: not in valid key input state (text editing mode, AE not foreground, or panel not active)
  if (y_key_held && !g_globals.key_was_held && !alt_held && IsKeyInputAllowed()) {
    if (HasSelectedLayers()) {
      // Check for double-tap (Y~Y)
      auto timeSinceLastRelease =
          std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                g_lastYRelease)
              .count();

      if (timeSinceLastRelease < DOUBLE_TAP_MS) {
        // Double-tap detected - toggle click mode
        g_toggleClickMode = !g_toggleClickMode;
        if (g_toggleClickMode) {
          // Show grid in toggle mode (stays visible)
          KeyboardMonitor::GetMousePosition(&g_mouseStartX, &g_mouseStartY);
          LoadSettingsFromFile(); // Sync settings
          ShowAnchorGrid(g_mouseStartX, g_mouseStartY);
          g_globals.menu_visible = true;
        } else {
          // Hide grid
          HideAndApplyAnchor();
          g_globals.menu_visible = false;
        }
        g_waitingForHold = false;
      } else {
        // Normal press - start waiting for hold
        g_keyPressTime = now;
        g_waitingForHold = true;
        KeyboardMonitor::GetMousePosition(&g_mouseStartX, &g_mouseStartY);
      }
    }
  }
  // Y key still held - check if hold duration reached
  else if (y_key_held && g_waitingForHold && !g_globals.menu_visible) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - g_keyPressTime)
                       .count();
    if (elapsed >= HOLD_DELAY_MS) {
      // Update mouse position to current
      KeyboardMonitor::GetMousePosition(&g_mouseStartX, &g_mouseStartY);
      LoadSettingsFromFile(); // Sync settings from CEP
      ShowAnchorGrid(g_mouseStartX, g_mouseStartY);
      g_globals.menu_visible = true;
      g_waitingForHold = false;
    }
  }
  // Y key still held and grid visible - update hover
  else if (y_key_held && g_globals.menu_visible) {
    int mouseX = 0, mouseY = 0;
    KeyboardMonitor::GetMousePosition(&mouseX, &mouseY);
    NativeUI::UpdateHover(mouseX, mouseY);
  }
  // Y key just released
  else if (!y_key_held && g_globals.key_was_held) {
    g_lastYRelease = now;

    // In toggle mode, don't hide on release
    if (!g_toggleClickMode && g_globals.menu_visible) {
      HideAndApplyAnchor();
      g_globals.menu_visible = false;
    }
    g_waitingForHold = false;
  }

  // In toggle mode, handle mouse click to apply
  if (g_toggleClickMode && g_globals.menu_visible) {
    if (KeyboardMonitor::IsMouseButtonPressed()) {
      HideAndApplyAnchor();
      g_globals.menu_visible = false;
      g_toggleClickMode = false;
    }
  }

  // Safety: If grid visible, Y not held, and not toggle mode - force close
  // This catches edge cases where key release was missed
  if (g_globals.menu_visible && !y_key_held && !g_toggleClickMode) {
    HideAndApplyAnchor();
    g_globals.menu_visible = false;
  }

  g_globals.key_was_held = y_key_held;

  // =========================================================================
  // CONTROL MODULE: Shift+E for layer effects panel
  // Shows layer effects list when a layer is selected
  // =========================================================================
  bool shift_held = KeyboardMonitor::IsShiftHeld();
  bool e_key_held = KeyboardMonitor::IsKeyHeld(KeyboardMonitor::KEY_E);
  bool shift_e_pressed = shift_held && e_key_held;

  // Shift+E just pressed - toggle panel
  // Skip if: not in valid key input state
  if (shift_e_pressed && !g_eKeyWasHeld &&
      IsKeyInputAllowed() && !g_globals.menu_visible) {

    if (g_controlVisible) {
      // Already open - close it (toggle off)
      ControlUI::HidePanel();
      g_controlVisible = false;
    } else {
      // Not open - show panel
      // Only show if a layer is selected
      if (!HasSelectedLayers()) {
        g_eKeyWasHeld = shift_e_pressed;
        return err;
      }

      // Show layer effects panel (Mode 2)
      // (effects list is preloaded in IdleHook)
      ControlUI::SetMode(ControlUI::MODE_EFFECTS);
      wchar_t effectsList[4096];
      GetLayerEffectsList(effectsList, sizeof(effectsList) / sizeof(wchar_t));
      ControlUI::SetLayerEffects(effectsList);
      ControlUI::ShowPanel();

      g_controlVisible = true;
    }
  }

  // =========================================================================
  // BACKUP: Mode 1 (Search & add effects) - 나중에 퀵모드로 이동 예정
  // =========================================================================
  // if (IsEffectControlsFocused()) {
  //   // ... Mode 2 code
  // } else {
  //   // Mode 1: Open new locked Effect Controls viewer
  //   ControlUI::SetMode(ControlUI::MODE_SEARCH);
  //   ExecuteScript("app.executeCommand(2163); app.executeCommand(3700);");
  //   ControlUI::ShowPanel();
  // }
  // =========================================================================

  // Check if Control panel closed and process result
  if (g_controlVisible && !ControlUI::IsVisible()) {
    g_controlVisible = false;
    ControlUI::ControlResult result = ControlUI::GetResult();

    if (result.effectSelected) {
      if (result.selectedEffect.isLayerEffect) {
        // Mode 2: Handle layer effect action
        if (result.action == ControlUI::ACTION_DELETE) {
          // Delete effect from layer
          char script[512];
          snprintf(script, sizeof(script),
                   "(function(){"
                   "var c=app.project.activeItem;"
                   "if(!c||!(c instanceof CompItem))return;"
                   "if(c.selectedLayers.length==0)return;"
                   "var L=c.selectedLayers[0];"
                   "var fx=L.Effects;"
                   "if(fx&&fx.numProperties>%d){"
                   "app.beginUndoGroup('Delete Effect');"
                   "fx.property(%d).remove();"
                   "app.endUndoGroup();"
                   "}"
                   "})();",
                   result.effectIndex, result.effectIndex + 1);
          ExecuteScript(script);
        } else if (result.action == ControlUI::ACTION_EXPAND) {
          // Expand this effect in timeline (collapse others) - inline script
          // Note: ExtendScript can't directly control Effect Controls twirl state
          // But we can show/hide properties in timeline which helps visibility
          char script[2048];
          snprintf(script, sizeof(script),
                   "(function(){"
                   "try{"
                   "var c=app.project.activeItem;"
                   "if(!c||!(c instanceof CompItem))return;"
                   "if(c.selectedLayers.length===0)return;"
                   "var layer=c.selectedLayers[0];"
                   "var fx=layer.property('ADBE Effect Parade');"
                   "if(!fx||fx.numProperties===0)return;"
                   "var idx=%d+1;"  // Convert 0-based to 1-based
                   "if(idx<1||idx>fx.numProperties)return;"
                   // Deselect all effects and their properties
                   "for(var i=1;i<=fx.numProperties;i++){"
                   "var e=fx.property(i);"
                   "e.selected=false;"
                   // Collapse: set all properties to not selected
                   "for(var j=1;j<=e.numProperties;j++){"
                   "try{e.property(j).selected=false;}catch(ex){}"
                   "}"
                   "}"
                   // Select target effect and its properties (expands in timeline)
                   "var target=fx.property(idx);"
                   "target.selected=true;"
                   // Select first few properties to show them expanded
                   "for(var k=1;k<=Math.min(target.numProperties,5);k++){"
                   "try{target.property(k).selected=true;}catch(ex){}"
                   "}"
                   "}catch(e){}"
                   "})();",
                   result.effectIndex);
          ExecuteScript(script);
        }
      } else if (result.action == ControlUI::ACTION_NEW_EC_WINDOW) {
        // Open new locked Effect Controls window - inline script
        // Command 3734 opens Effect Controls without toggle behavior
        ExecuteScript(
            "(function(){"
            "try{"
            "var c=app.project.activeItem;"
            "if(!c||!(c instanceof CompItem))return;"
            "if(c.selectedLayers.length===0)return;"
            "app.executeCommand(3734);"
            "}catch(e){}"
            "})();");
      } else if (result.action == ControlUI::ACTION_SAVE_PRESET) {
        // Save current effects to preset slot - inline script
        // Use heap allocation to avoid stack overflow
        char* presetData = new char[65536];
        if (presetData) {
          memset(presetData, 0, 65536);
          ExecuteScript(
              "(function(){"
              "try{"
              "var c=app.project.activeItem;"
              "if(!c||!(c instanceof CompItem))return 'Error';"
              "if(c.selectedLayers.length===0)return 'Error';"
              "var layer=c.selectedLayers[0];"
              "var fx=layer.property('ADBE Effect Parade');"
              "if(!fx||fx.numProperties===0)return 'Error';"
              "var allPresets={effects:[]};"
              "for(var ei=0;ei<fx.numProperties;ei++){"
              "var effect=fx.property(ei+1);"
              "var preset={matchName:effect.matchName,name:effect.name,properties:[],expressions:[]};"
              "for(var pi=1;pi<=effect.numProperties;pi++){"
              "try{"
              "var prop=effect.property(pi);"
              "if(prop.propertyValueType===PropertyValueType.NO_VALUE)continue;"
              "var pd={index:pi,name:prop.name,matchName:prop.matchName};"
              "if(prop.numKeys===0){pd.value=prop.value;}else{pd.value=prop.valueAtTime(c.time,false);pd.hasKeyframes=true;}"
              "if(prop.expression&&prop.expression.length>0){"
              "preset.expressions.push({index:pi,expression:prop.expression,enabled:prop.expressionEnabled});"
              "}"
              "preset.properties.push(pd);"
              "}catch(pe){}"
              "}"
              "allPresets.effects.push(preset);"
              "}"
              "return JSON.stringify(allPresets);"
              "}catch(e){return 'Error';}"
              "})();",
              presetData, 65536);

          if (presetData[0] != '\0' && strncmp(presetData, "null", 4) != 0 &&
              strncmp(presetData, "Error", 5) != 0) {
            SavePresetToSlot(result.presetSlotIndex, presetData);
            // Mark slot as filled for UI
            ControlUI::SetPresetSlotFilled(result.presetSlotIndex, true);
            // Show confirmation
            char confirmScript[256];
            snprintf(confirmScript, sizeof(confirmScript),
                     "alert('Preset saved to slot %d')",
                     result.presetSlotIndex + 1);
            ExecuteScript(confirmScript);
          }
          delete[] presetData;
        }
      } else if (result.action == ControlUI::ACTION_APPLY_PRESET) {
        // Apply preset from quick slot - inline script
        // Use heap allocation to avoid stack overflow
        char* presetJson = new char[65536];
        if (presetJson) {
          memset(presetJson, 0, 65536);
          if (LoadPresetFromSlot(result.presetSlotIndex, presetJson, 65536)) {
            // Escape the JSON for use in script
            char* escapedJson = new char[131072];
            if (escapedJson) {
              memset(escapedJson, 0, 131072);
              EscapeJsonForScript(presetJson, escapedJson, 131072);

              // Apply the preset with inline script
              char* script = new char[150000];
              if (script) {
                snprintf(script, 150000,
                   "(function(){"
                   "try{"
                   "var allPresets=JSON.parse('%s');"
                   "if(!allPresets||!allPresets.effects||allPresets.effects.length===0)return;"
                   "var c=app.project.activeItem;"
                   "if(!c||!(c instanceof CompItem))return;"
                   "if(c.selectedLayers.length===0)return;"
                   "app.beginUndoGroup('Apply Multi-Effect Preset');"
                   "for(var li=0;li<c.selectedLayers.length;li++){"
                   "var layer=c.selectedLayers[li];"
                   "var fx=layer.property('ADBE Effect Parade');"
                   "for(var ei=0;ei<allPresets.effects.length;ei++){"
                   "var preset=allPresets.effects[ei];"
                   "if(!fx.canAddProperty(preset.matchName))continue;"
                   "var newFx=fx.addProperty(preset.matchName);"
                   "for(var pi=0;pi<preset.properties.length;pi++){"
                   "try{"
                   "var pd=preset.properties[pi];"
                   "var prop=newFx.property(pd.index);"
                   "if(prop&&prop.propertyValueType!==PropertyValueType.NO_VALUE&&!pd.hasKeyframes){"
                   "prop.setValue(pd.value);"
                   "}"
                   "}catch(pe){}"
                   "}"
                   "for(var xi=0;xi<preset.expressions.length;xi++){"
                   "try{"
                   "var xd=preset.expressions[xi];"
                   "var xp=newFx.property(xd.index);"
                   "if(xp){xp.expression=xd.expression;xp.expressionEnabled=xd.enabled;}"
                   "}catch(xe){}"
                   "}"
                   "}"
                   "}"
                   "app.endUndoGroup();"
                   "}catch(e){}"
                   "})();",
                   escapedJson);
                ExecuteScript(script);
                delete[] script;
              }
              delete[] escapedJson;
            }
          } else {
            // Slot is empty
            char emptyMsg[256];
            snprintf(emptyMsg, sizeof(emptyMsg),
                     "alert('Preset slot %d is empty')",
                     result.presetSlotIndex + 1);
            ExecuteScript(emptyMsg);
          }
          delete[] presetJson;
        }
      } else {
        // Mode 1: Add new effect to layer
        char matchNameA[256];
        WideCharToMultiByte(CP_ACP, 0, result.selectedEffect.matchName, -1,
                            matchNameA, sizeof(matchNameA), NULL, NULL);

        char script[1024];
        snprintf(script, sizeof(script),
                 "(function(){"
                 "var c=app.project.activeItem;"
                 "if(!c||!(c instanceof CompItem))return;"
                 "if(c.selectedLayers.length==0)return;"
                 "app.beginUndoGroup('Add Effect');"
                 "for(var i=0;i<c.selectedLayers.length;i++){"
                 "try{c.selectedLayers[i].Effects.addProperty('%s');}catch(e){}"
                 "}"
                 "app.endUndoGroup();"
                 "})();",
                 matchNameA);
        ExecuteScript(script);
      }
    }
  }

  g_eKeyWasHeld = shift_e_pressed;

  // =========================================================================
  // KEYFRAME MODULE: Right Shift + K for direct keyframe panel access
  // =========================================================================
  bool rshift_held = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
  bool k_key_held = KeyboardMonitor::IsKeyHeld(KeyboardMonitor::KEY_K);
  bool rshift_k_pressed = rshift_held && k_key_held;

  // Right Shift+K just pressed - toggle panel
  if (rshift_k_pressed && !g_rshiftKWasHeld &&
      IsKeyInputAllowed() && !g_globals.menu_visible && !g_dMenuVisible) {

    if (g_keyframeVisible) {
      // Already open - close it (toggle off)
      KeyframeUI::HidePanel();
      g_keyframeVisible = false;
    } else {
      // Not open - show keyframe panel with current selection info
      int mouseX = 0, mouseY = 0;
      KeyboardMonitor::GetMousePosition(&mouseX, &mouseY);

      // Get keyframe info before showing panel (same script as D→K)
      const char* getInfoScript =
        "(function(){"
        "try{"
        "var c=app.project.activeItem;"
        "if(!c||!(c instanceof CompItem))return '';"
        "var props=c.selectedProperties;"
        "if(!props||props.length===0)return '';"
        "var prop=props[0];"
        "if(!prop.selectedKeys||prop.selectedKeys.length<2)return '';"
        "var keys=prop.selectedKeys;"
        "var k1=keys[0],k2=keys[1];"
        "var t1=prop.keyTime(k1),t2=prop.keyTime(k2);"
        "var v1=prop.keyValue(k1),v2=prop.keyValue(k2);"
        "var val1=v1,val2=v2;"
        "if(v1 instanceof Array){"
        "var sum1=0,sum2=0;"
        "for(var i=0;i<v1.length;i++){sum1+=v1[i]*v1[i];sum2+=v2[i]*v2[i];}"
        "val1=Math.sqrt(sum1);val2=Math.sqrt(sum2);"
        "}"
        "var outEase=prop.keyOutTemporalEase(k1);"
        "var inEase=prop.keyInTemporalEase(k2);"
        "var outSpd=outEase[0].speed,outInf=outEase[0].influence;"
        "var inSpd=inEase[0].speed,inInf=inEase[0].influence;"
        "var dur=t2-t1;"
        "var valChange=val2-val1;"
        "var avgSpd=Math.abs(dur)>0.0001?Math.abs(valChange/dur):0;"
        "return '{\"propName\":\"'+prop.name.replace(/\"/g,'\\\\\"')+'\",'+"
        "'\"propMatchName\":\"'+prop.matchName.replace(/\"/g,'\\\\\"')+'\",'+"
        "'\"keyIndex1\":'+k1+',\"keyIndex2\":'+k2+','+"
        "'\"time1\":'+t1+',\"time2\":'+t2+','+"
        "'\"value1\":'+val1+',\"value2\":'+val2+','+"
        "'\"outSpeed\":'+outSpd+',\"outInfluence\":'+outInf+','+"
        "'\"inSpeed\":'+inSpd+',\"inInfluence\":'+inInf+','+"
        "'\"avgSpeed\":'+avgSpd+'}';"
        "}catch(e){return '';}"
        "})();";

      char resultBuf[2048] = {0};
      ExecuteScript(getInfoScript, resultBuf, sizeof(resultBuf));

      // Set keyframe info if we got valid data
      if (resultBuf[0] == '{') {
        wchar_t wResult[2048];
        MultiByteToWideChar(CP_UTF8, 0, resultBuf, -1, wResult, 2048);
        KeyframeUI::SetKeyframeInfo(wResult);
      }

      KeyframeUI::ShowPanel(mouseX, mouseY);
      g_keyframeVisible = true;
    }
  }

  g_rshiftKWasHeld = rshift_k_pressed;

  // =========================================================================
  // KEYFRAME MODULE: Toggle mode via D→K (DMenuUI)
  // Panel stays open until closed by outside click, ESC, or D→K toggle
  // =========================================================================

  // Check if Keyframe panel closed and process result
  if (g_keyframeVisible && !KeyframeUI::IsVisible()) {
    KeyframeUI::KeyframeResult result = KeyframeUI::GetResult();
    g_keyframeVisible = false;

    if (result.applied) {
      // Validate and sanitize values before script generation
      // NaN/inf would break ExtendScript parsing (produces "nan", "inf" strings)
      float outSpd = result.outSpeed;
      float outInf = result.outInfluence;
      float inSpd = result.inSpeed;
      float inInf = result.inInfluence;

      // Check for NaN (x != x is true for NaN) and infinity
      // Using inline check: finite numbers satisfy (x - x == 0)
      if (outSpd != outSpd || (outSpd - outSpd) != 0.0f) outSpd = 1.0f;
      if (outInf != outInf || (outInf - outInf) != 0.0f) outInf = 33.33f;
      if (inSpd != inSpd || (inSpd - inSpd) != 0.0f) inSpd = 1.0f;
      if (inInf != inInf || (inInf - inInf) != 0.0f) inInf = 33.33f;

      // Clamp to valid ranges (also catches any remaining edge cases)
      outSpd = (outSpd < 0.0f) ? 0.0f : ((outSpd > 10000.0f) ? 10000.0f : outSpd);
      outInf = (outInf < 0.1f) ? 0.1f : ((outInf > 100.0f) ? 100.0f : outInf);
      inSpd = (inSpd < 0.0f) ? 0.0f : ((inSpd > 10000.0f) ? 10000.0f : inSpd);
      inInf = (inInf < 0.1f) ? 0.1f : ((inInf > 100.0f) ? 100.0f : inInf);

      // Apply keyframe easing using inline ExtendScript
      char script[4096];
      snprintf(script, sizeof(script),
               "(function(){"
               "try{"
               "var c=app.project.activeItem;"
               "if(!c||!(c instanceof CompItem))return;"
               "var props=c.selectedProperties;"
               "if(!props||props.length===0)return;"
               "var outSpd=%.2f,outInf=%.2f,inSpd=%.2f,inInf=%.2f;"
               "app.beginUndoGroup('Apply Keyframe Easing');"
               "for(var i=0;i<props.length;i++){"
               "var prop=props[i];"
               "if(!prop.selectedKeys||prop.selectedKeys.length<2)continue;"
               "var keys=prop.selectedKeys;"
               "var numDims=1;"
               "if(prop.propertyValueType===PropertyValueType.TwoD)numDims=2;"
               "else if(prop.propertyValueType===PropertyValueType.ThreeD)numDims=3;"
               "var outArr=[],inArr=[];"
               "for(var d=0;d<numDims;d++){"
               "outArr.push(new KeyframeEase(outSpd,outInf));"
               "inArr.push(new KeyframeEase(inSpd,inInf));"
               "}"
               "for(var j=0;j<keys.length;j++){"
               "var k=keys[j];"
               "if(j<keys.length-1){"
               "try{prop.setTemporalEaseAtKey(k,prop.keyInTemporalEase(k),outArr);}catch(e){}"
               "}"
               "if(j>0){"
               "try{prop.setTemporalEaseAtKey(k,inArr,prop.keyOutTemporalEase(k));}catch(e){}"
               "}"
               "}"
               "}"
               "app.endUndoGroup();"
               "}catch(e){}"
               "})();",
               outSpd, outInf, inSpd, inInf);
      ExecuteScript(script);
    }
  }

  // Update hover while keyframe panel is visible
  if (g_keyframeVisible && KeyframeUI::IsVisible()) {
    int mouseX = 0, mouseY = 0;
    KeyboardMonitor::GetMousePosition(&mouseX, &mouseY);
    KeyframeUI::UpdateHover(mouseX, mouseY);

    // Check if Load button was pressed (reload keyframe info)
    KeyframeUI::KeyframeResult currentResult = KeyframeUI::GetResult();
    if (currentResult.loadRequested) {
      // Re-fetch keyframe info from current selection
      const char* getInfoScript =
        "(function(){"
        "try{"
        "var c=app.project.activeItem;"
        "if(!c||!(c instanceof CompItem))return '';"
        "var props=c.selectedProperties;"
        "if(!props||props.length===0)return '';"
        "var prop=props[0];"
        "if(!prop.selectedKeys||prop.selectedKeys.length<2)return '';"
        "var keys=prop.selectedKeys;"
        "var k1=keys[0],k2=keys[1];"
        "var t1=prop.keyTime(k1),t2=prop.keyTime(k2);"
        "var v1=prop.keyValue(k1),v2=prop.keyValue(k2);"
        "var val1=v1,val2=v2;"
        "if(v1 instanceof Array){"
        "var sum1=0,sum2=0;"
        "for(var i=0;i<v1.length;i++){sum1+=v1[i]*v1[i];sum2+=v2[i]*v2[i];}"
        "val1=Math.sqrt(sum1);val2=Math.sqrt(sum2);"
        "}"
        "var outEase=prop.keyOutTemporalEase(k1);"
        "var inEase=prop.keyInTemporalEase(k2);"
        "var outSpd=outEase[0].speed,outInf=outEase[0].influence;"
        "var inSpd=inEase[0].speed,inInf=inEase[0].influence;"
        "var dur=t2-t1;"
        "var valChange=val2-val1;"
        "var avgSpd=Math.abs(dur)>0.0001?Math.abs(valChange/dur):0;"
        "return '{\"propName\":\"'+prop.name.replace(/\"/g,'\\\\\"')+'\",'+"
        "'\"propMatchName\":\"'+prop.matchName.replace(/\"/g,'\\\\\"')+'\",'+"
        "'\"keyIndex1\":'+k1+',\"keyIndex2\":'+k2+','+"
        "'\"time1\":'+t1+',\"time2\":'+t2+','+"
        "'\"value1\":'+val1+',\"value2\":'+val2+','+"
        "'\"outSpeed\":'+outSpd+',\"outInfluence\":'+outInf+','+"
        "'\"inSpeed\":'+inSpd+',\"inInfluence\":'+inInf+','+"
        "'\"avgSpeed\":'+avgSpd+'}';"
        "}catch(e){return '';}"
        "})();";

      char resultBuf[2048] = {0};
      ExecuteScript(getInfoScript, resultBuf, sizeof(resultBuf));

      if (resultBuf[0] == '{') {
        wchar_t wResult[2048];
        MultiByteToWideChar(CP_UTF8, 0, resultBuf, -1, wResult, 2048);
        KeyframeUI::SetKeyframeInfo(wResult);
      }
    }
  }

  // =========================================================================
  // D MENU MODULE: D key shows menu, user selects A/T/K
  // Menu popup takes focus, so keys go to menu not AE
  // =========================================================================
  bool d_key_held = KeyboardMonitor::IsKeyHeld(KeyboardMonitor::KEY_D);

  // D key just pressed - show D menu
  // Skip if: modifier keys held, or not in valid key input state
  bool ctrl_held = KeyboardMonitor::IsCtrlHeld();
  if (d_key_held && !g_dKeyWasHeld && !alt_held && !shift_held && !ctrl_held &&
      IsKeyInputAllowed() &&
      !g_globals.menu_visible && !g_controlVisible && !g_keyframeVisible &&
      !g_alignVisible && !g_textVisible && !g_dMenuVisible) {
    int mouseX = 0, mouseY = 0;
    KeyboardMonitor::GetMousePosition(&mouseX, &mouseY);
    DMenuUI::ShowMenu(mouseX, mouseY);
    g_dMenuVisible = true;
  }

  // Check if D menu closed and handle action
  if (g_dMenuVisible && !DMenuUI::IsVisible()) {
    g_dMenuVisible = false;
    DMenuUI::MenuAction action = DMenuUI::GetAction();

    int mouseX = 0, mouseY = 0;
    KeyboardMonitor::GetMousePosition(&mouseX, &mouseY);

    switch (action) {
    case DMenuUI::ACTION_ALIGN:
      AlignUI::ShowPanel(mouseX, mouseY);
      g_alignVisible = true;
      break;

    case DMenuUI::ACTION_TEXT: {
      // Get text layer info if a text layer is selected (panel opens regardless)
      const char* getTextInfoScript =
        "(function(){"
        "try{"
        "var c=app.project.activeItem;"
        "if(!c||!(c instanceof CompItem))return '';"
        "var sel=c.selectedLayers;"
        "var textLayer=null;"
        "for(var i=0;i<sel.length;i++){"
        "if(sel[i] instanceof TextLayer){textLayer=sel[i];break;}"
        "}"
        "if(!textLayer)return '';"
        "var txt=textLayer.text.sourceText.value;"
        "var font=txt.font||'Arial';"
        "var fontStyle=txt.fontStyle||'Regular';"
        "var fontSize=txt.fontSize||72;"
        "var tracking=txt.tracking||0;"
        "var leading=txt.leading||0;"
        "var strokeWidth=txt.strokeWidth||0;"
        "var fill=txt.fillColor||[1,1,1];"
        "var stroke=txt.strokeColor||[0,0,0];"
        "var applyFill=txt.applyFill!==false;"
        "var applyStroke=txt.applyStroke||false;"
        "var just=txt.justification||ParagraphJustification.LEFT_JUSTIFY;"
        "var justNum=0;"
        "if(just==ParagraphJustification.LEFT_JUSTIFY)justNum=0;"
        "else if(just==ParagraphJustification.CENTER_JUSTIFY)justNum=1;"
        "else if(just==ParagraphJustification.RIGHT_JUSTIFY)justNum=2;"
        "else if(just==ParagraphJustification.FULL_JUSTIFY_LASTLINE_LEFT)justNum=3;"
        "else if(just==ParagraphJustification.FULL_JUSTIFY_LASTLINE_CENTER)justNum=4;"
        "else if(just==ParagraphJustification.FULL_JUSTIFY_LASTLINE_RIGHT)justNum=5;"
        "else if(just==ParagraphJustification.FULL_JUSTIFY_LASTLINE_FULL)justNum=6;"
        "return '{'+"
        "'\"font\":\"'+font.replace(/\"/g,'\\\\\"')+'\",'+"
        "'\"fontStyle\":\"'+fontStyle.replace(/\"/g,'\\\\\"')+'\",'+"
        "'\"fontSize\":'+fontSize+','+"
        "'\"tracking\":'+tracking+','+"
        "'\"leading\":'+leading+','+"
        "'\"strokeWidth\":'+strokeWidth+','+"
        "'\"fillColor\":['+fill[0]+','+fill[1]+','+fill[2]+'],'+"
        "'\"strokeColor\":['+stroke[0]+','+stroke[1]+','+stroke[2]+'],'+"
        "'\"applyFill\":'+applyFill+','+"
        "'\"applyStroke\":'+applyStroke+','+"
        "'\"justify\":'+justNum+','+"
        "'\"layerName\":\"'+textLayer.name.replace(/\"/g,'\\\\\"')+'\"'+"
        "'}';"
        "}catch(e){return '';}"
        "})();";

      char resultBuf[2048] = {0};
      ExecuteScript(getTextInfoScript, resultBuf, sizeof(resultBuf));

      if (resultBuf[0] == '{') {
        wchar_t wResult[2048];
        MultiByteToWideChar(CP_UTF8, 0, resultBuf, -1, wResult, 2048);
        TextUI::SetTextInfo(wResult);
      }

      // Always open the panel (even without text layer selected)
      TextUI::ShowPanel(mouseX, mouseY);
      g_textVisible = true;
      break;
    }

    case DMenuUI::ACTION_KEYFRAME:
      // Toggle keyframe panel (not hold mode)
      if (g_keyframeVisible) {
        KeyframeUI::HidePanel();
        g_keyframeVisible = false;
      } else {
        // Get keyframe info before showing panel
        const char* getInfoScript =
          "(function(){"
          "try{"
          "var c=app.project.activeItem;"
          "if(!c||!(c instanceof CompItem))return '';"
          "var props=c.selectedProperties;"
          "if(!props||props.length===0)return '';"
          "var prop=props[0];"
          "if(!prop.selectedKeys||prop.selectedKeys.length<2)return '';"
          "var keys=prop.selectedKeys;"
          "var k1=keys[0],k2=keys[1];"
          "var t1=prop.keyTime(k1),t2=prop.keyTime(k2);"
          "var v1=prop.keyValue(k1),v2=prop.keyValue(k2);"
          "var val1=v1,val2=v2;"
          "if(v1 instanceof Array){"
          "var sum1=0,sum2=0;"
          "for(var i=0;i<v1.length;i++){sum1+=v1[i]*v1[i];sum2+=v2[i]*v2[i];}"
          "val1=Math.sqrt(sum1);val2=Math.sqrt(sum2);"
          "}"
          "var outEase=prop.keyOutTemporalEase(k1);"
          "var inEase=prop.keyInTemporalEase(k2);"
          "var outSpd=outEase[0].speed,outInf=outEase[0].influence;"
          "var inSpd=inEase[0].speed,inInf=inEase[0].influence;"
          "var dur=t2-t1;"
          "var valChange=val2-val1;"
          "var avgSpd=Math.abs(dur)>0.0001?Math.abs(valChange/dur):0;"
          "return '{\"propName\":\"'+prop.name.replace(/\"/g,'\\\\\"')+'\",'+"
          "'\"propMatchName\":\"'+prop.matchName.replace(/\"/g,'\\\\\"')+'\",'+"
          "'\"keyIndex1\":'+k1+',\"keyIndex2\":'+k2+','+"
          "'\"time1\":'+t1+',\"time2\":'+t2+','+"
          "'\"value1\":'+val1+',\"value2\":'+val2+','+"
          "'\"outSpeed\":'+outSpd+',\"outInfluence\":'+outInf+','+"
          "'\"inSpeed\":'+inSpd+',\"inInfluence\":'+inInf+','+"
          "'\"avgSpeed\":'+avgSpd+'}';"
          "}catch(e){return '';}"
          "})();";

        char resultBuf[2048] = {0};
        ExecuteScript(getInfoScript, resultBuf, sizeof(resultBuf));

        // Set keyframe info if we got valid data
        if (resultBuf[0] == '{') {
          wchar_t wResult[2048];
          MultiByteToWideChar(CP_UTF8, 0, resultBuf, -1, wResult, 2048);
          KeyframeUI::SetKeyframeInfo(wResult);
        }

        KeyframeUI::ShowPanel(mouseX, mouseY);
        g_keyframeVisible = true;
      }
      break;

    case DMenuUI::ACTION_COMP:  // ACTION_COMP triggers Layer module
      // Show layer editor panel
      if (g_layerVisible) {
        CompUI::HidePanel();
        g_layerVisible = false;
      } else {
        // Get selected layer info via ExtendScript
        // LayerType enum values: 0=NONE, 1=TEXT, 2=SHAPE, 3=SOLID, 4=NULL, 5=FOOTAGE, 6=CAMERA, 7=LIGHT, 8=ADJUSTMENT, 9=PRECOMP
        char layerResult[2048] = {0};
        ExecuteScript(
          "(function(){"
          "var c=app.project.activeItem;"
          "if(!c||!(c instanceof CompItem))return '';"
          "var layers=c.selectedLayers;"
          "if(layers.length===0)return '';"
          "var layer=layers[0];"
          "var type=0;"  // Default: NONE
          "if(layer instanceof TextLayer)type=1;"
          "else if(layer instanceof ShapeLayer)type=2;"
          "else if(layer instanceof CameraLayer)type=6;"
          "else if(layer instanceof LightLayer)type=7;"
          "else if(layer instanceof AVLayer){"
          "  if(layer.nullLayer)type=4;"
          "  else if(layer.adjustmentLayer)type=8;"
          "  else if(layer.source instanceof CompItem)type=9;"
          "  else if(layer.source&&layer.source.mainSource instanceof SolidSource)type=3;"
          "  else type=5;"  // Footage
          "}"
          "var hasParent=layer.parent!==null;"
          "var parentIdx=hasParent?layer.parent.index:0;"
          "var isSeq=false,hasTimeRemap=false;"
          "if(type===5&&layer.source&&layer.source.mainSource){"
          "  isSeq=!layer.source.mainSource.isStill;"
          "  hasTimeRemap=layer.timeRemapEnabled;"
          "}"
          "var solidColor=0;"
          "if(type===3&&layer.source&&layer.source.mainSource instanceof SolidSource){"
          "  var col=layer.source.mainSource.color;"
          "  solidColor=Math.round(col[0]*255)*65536+Math.round(col[1]*255)*256+Math.round(col[2]*255);"
          "}"
          "return JSON.stringify({"
          "  name:layer.name,"
          "  type:type,"
          "  index:layer.index,"
          "  hasParent:hasParent,"
          "  parentIndex:parentIdx,"
          "  isSelected:true,"
          "  solidColor:solidColor,"
          "  isSequence:isSeq,"
          "  hasTimeRemap:hasTimeRemap"
          "});"
          "})()",
          layerResult, sizeof(layerResult));

        // Convert to wide string and set layer info
        if (layerResult[0] != '\0') {
          wchar_t wResult[2048];
          MultiByteToWideChar(CP_UTF8, 0, layerResult, -1, wResult, 2048);
          CompUI::SetLayerInfo(wResult);
        } else {
          CompUI::SetLayerInfo(L"");  // No layer selected
        }

        CompUI::ShowPanel(mouseX, mouseY);
        g_layerVisible = true;
      }
      break;

    case DMenuUI::ACTION_SETTINGS: {
      // Try to find and focus the CEP Settings panel
      // Window title should be "Anchor Snap" or contain it
      HWND settingsWnd = NULL;

      // Callback to find window with matching title
      struct EnumData {
        HWND found;
      } enumData = { NULL };

      EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* data = reinterpret_cast<EnumData*>(lParam);
        wchar_t title[256];
        if (GetWindowTextW(hwnd, title, 256)) {
          // Check if title contains "Anchor Snap"
          if (wcsstr(title, L"Anchor Snap") != NULL) {
            data->found = hwnd;
            return FALSE;  // Stop enumeration
          }
        }
        return TRUE;  // Continue enumeration
      }, reinterpret_cast<LPARAM>(&enumData));

      if (enumData.found) {
        // Restore if minimized
        if (IsIconic(enumData.found)) {
          ShowWindow(enumData.found, SW_RESTORE);
        }
        SetForegroundWindow(enumData.found);
        SetFocus(enumData.found);
      }
      // If not found, panel might not be open - user needs to open from Window > Extensions
      break;
    }

    default:
      // ACTION_NONE or ACTION_CANCELLED - do nothing
      break;
    }
  }

  // Check if Align panel closed and process result
  if (g_alignVisible && !AlignUI::IsVisible()) {
    g_alignVisible = false;
    AlignUI::AlignResult result = AlignUI::GetResult();

    if (result.applied) {
      // Execute alignment/distribution via ExtendScript
      // The actual script will be built based on the result
      if (result.funcMode == AlignUI::FUNC_ALIGN) {
        // Build align script based on direction and reference mode
        char script[4096];
        const char* refMode = (result.refMode == AlignUI::REF_COMPOSITION) ? "true" : "false";
        int dir = static_cast<int>(result.alignDir);

        snprintf(script, sizeof(script),
          "(function(){"
          "try{"
          "var useComp=%s;"
          "var dir=%d;" // 0=left,1=centerH,2=right,3=top,4=middleV,5=bottom
          "var c=app.project.activeItem;"
          "if(!c||!(c instanceof CompItem))return;"
          "var layers=c.selectedLayers;"
          "if(layers.length===0)return;"
          "app.beginUndoGroup('Align Layers');"

          // Get bounds for all layers
          "function getBounds(L){"
          "var r=L.sourceRectAtTime(c.time,false);"
          "var pos=L.position.value,anc=L.anchorPoint.value,sc=L.scale.value;"
          "var w=r.width*sc[0]/100,h=r.height*sc[1]/100;"
          "var left=pos[0]-(anc[0]-r.left)*sc[0]/100;"
          "var top=pos[1]-(anc[1]-r.top)*sc[1]/100;"
          "return{left:left,top:top,right:left+w,bottom:top+h,cx:left+w/2,cy:top+h/2,w:w,h:h};"
          "}"

          "var allBounds=[];"
          "for(var i=0;i<layers.length;i++)allBounds.push(getBounds(layers[i]));"

          // Calculate target position based on mode
          "var target;"
          "if(useComp){"
          "target={left:0,top:0,right:c.width,bottom:c.height,cx:c.width/2,cy:c.height/2};"
          "}else{"
          "var minL=Infinity,minT=Infinity,maxR=-Infinity,maxB=-Infinity;"
          "for(var i=0;i<allBounds.length;i++){"
          "var b=allBounds[i];"
          "if(b.left<minL)minL=b.left;if(b.top<minT)minT=b.top;"
          "if(b.right>maxR)maxR=b.right;if(b.bottom>maxB)maxB=b.bottom;"
          "}"
          "target={left:minL,top:minT,right:maxR,bottom:maxB,cx:(minL+maxR)/2,cy:(minT+maxB)/2};"
          "}"

          // Apply alignment
          "for(var i=0;i<layers.length;i++){"
          "var L=layers[i],b=allBounds[i];"
          "var pos=L.position.value;"
          "var dx=0,dy=0;"
          "if(dir===0)dx=target.left-b.left;"       // Left
          "else if(dir===1)dx=target.cx-b.cx;"      // Center H
          "else if(dir===2)dx=target.right-b.right;" // Right
          "else if(dir===3)dy=target.top-b.top;"    // Top
          "else if(dir===4)dy=target.cy-b.cy;"      // Middle V
          "else if(dir===5)dy=target.bottom-b.bottom;" // Bottom
          "var pp=L.property('ADBE Transform Group').property('ADBE Position');"
          "var newPos=[pos[0]+dx,pos[1]+dy];"
          "if(pp.numKeys>0)pp.setValueAtTime(c.time,newPos);else pp.setValue(newPos);"
          "}"
          "app.endUndoGroup();"
          "}catch(e){alert('Align error: '+e.toString());}"
          "})();",
          refMode, dir);
        ExecuteScript(script);
      } else {
        // Distribute
        char script[4096];
        const char* refMode = (result.refMode == AlignUI::REF_COMPOSITION) ? "true" : "false";
        bool isHorizontal = (result.distDir == AlignUI::DIST_HORIZONTAL);

        snprintf(script, sizeof(script),
          "(function(){"
          "try{"
          "var useComp=%s;"
          "var isH=%s;"
          "var c=app.project.activeItem;"
          "if(!c||!(c instanceof CompItem))return;"
          "var layers=c.selectedLayers;"
          "if(layers.length<3)return;" // Need at least 3 layers
          "app.beginUndoGroup('Distribute Layers');"

          "function getBounds(L){"
          "var r=L.sourceRectAtTime(c.time,false);"
          "var pos=L.position.value,anc=L.anchorPoint.value,sc=L.scale.value;"
          "var w=r.width*sc[0]/100,h=r.height*sc[1]/100;"
          "var left=pos[0]-(anc[0]-r.left)*sc[0]/100;"
          "var top=pos[1]-(anc[1]-r.top)*sc[1]/100;"
          "return{left:left,top:top,right:left+w,bottom:top+h,cx:left+w/2,cy:top+h/2,w:w,h:h,layer:L};"
          "}"

          // Collect and sort layers by position
          "var data=[];"
          "for(var i=0;i<layers.length;i++){"
          "var b=getBounds(layers[i]);b.layer=layers[i];data.push(b);"
          "}"
          "data.sort(function(a,b){return isH?(a.cx-b.cx):(a.cy-b.cy);});"

          // Calculate spacing
          "var first,last,total;"
          "if(useComp){"
          "first=isH?0:0;last=isH?c.width:c.height;"
          "}else{"
          "first=isH?data[0].cx:data[0].cy;"
          "last=isH?data[data.length-1].cx:data[data.length-1].cy;"
          "}"
          "var spacing=(last-first)/(data.length-1);"

          // Apply distribution (skip first and last)
          "for(var i=1;i<data.length-1;i++){"
          "var targetPos=first+spacing*i;"
          "var delta=isH?(targetPos-data[i].cx):(targetPos-data[i].cy);"
          "var pos=data[i].layer.position.value;"
          "var pp=data[i].layer.property('ADBE Transform Group').property('ADBE Position');"
          "var newPos=isH?[pos[0]+delta,pos[1]]:[pos[0],pos[1]+delta];"
          "if(pp.numKeys>0)pp.setValueAtTime(c.time,newPos);else pp.setValue(newPos);"
          "}"
          "app.endUndoGroup();"
          "}catch(e){alert('Distribute error: '+e.toString());}"
          "})();",
          refMode, isHorizontal ? "true" : "false");
        ExecuteScript(script);
      }
    }
  }

  // Update hover while align panel is visible
  if (g_alignVisible && AlignUI::IsVisible()) {
    int mouseX = 0, mouseY = 0;
    KeyboardMonitor::GetMousePosition(&mouseX, &mouseY);
    AlignUI::UpdateHover(mouseX, mouseY);
  }

  g_dKeyWasHeld = d_key_held;

  // Check if Text panel closed and process result
  if (g_textVisible && !TextUI::IsVisible()) {
    g_textVisible = false;
    TextUI::TextResult result = TextUI::GetResult();

    if (result.applied) {
      // Text changes are applied in real-time via TextUI callbacks
      // No additional processing needed here
    }
  }

  // Update hover while text panel is visible
  if (g_textVisible && TextUI::IsVisible()) {
    int mouseX = 0, mouseY = 0;
    KeyboardMonitor::GetMousePosition(&mouseX, &mouseY);
    TextUI::UpdateHover(mouseX, mouseY);
  }

  // Check if Layer panel closed and process result
  if (g_layerVisible && !CompUI::IsVisible()) {
    g_layerVisible = false;
    CompUI::CompResult result = CompUI::GetResult();

    if (result.applied) {
      char script[4096];
      char resultBuf[512];

      switch (result.action) {
      // Text layer actions
      case CompUI::ACTION_TEXT_ANIMATOR_TYPEWRITER:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer||!(layer instanceof TextLayer))return;"
          "app.beginUndoGroup('Add Typewriter');"
          "var tp=layer.Text.property('ADBE Text Animators');"
          "var anim=tp.addProperty('ADBE Text Animator');"
          "anim.name='Typewriter';"
          "var sel=anim.property('ADBE Text Selectors').addProperty('ADBE Text Selector');"
          "var range=sel.property('ADBE Text Percent Start');"
          "range.setValueAtTime(0,100);range.setValueAtTime(c.duration*0.8,0);"
          "var props=anim.property('ADBE Text Animator Properties');"
          "props.addProperty('ADBE Text Opacity');props.property('ADBE Text Opacity').setValue(0);"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      case CompUI::ACTION_TEXT_ANIMATOR_FADE:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer||!(layer instanceof TextLayer))return;"
          "app.beginUndoGroup('Add Fade In');"
          "var tp=layer.Text.property('ADBE Text Animators');"
          "var anim=tp.addProperty('ADBE Text Animator');"
          "anim.name='Fade In';"
          "var sel=anim.property('ADBE Text Selectors').addProperty('ADBE Text Selector');"
          "var range=sel.property('ADBE Text Percent Start');"
          "range.setValueAtTime(0,100);range.setValueAtTime(c.duration*0.5,0);"
          "var props=anim.property('ADBE Text Animator Properties');"
          "props.addProperty('ADBE Text Opacity');props.property('ADBE Text Opacity').setValue(0);"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      case CompUI::ACTION_TEXT_ANIMATOR_SCALE:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer||!(layer instanceof TextLayer))return;"
          "app.beginUndoGroup('Add Scale');"
          "var tp=layer.Text.property('ADBE Text Animators');"
          "var anim=tp.addProperty('ADBE Text Animator');"
          "anim.name='Scale In';"
          "var sel=anim.property('ADBE Text Selectors').addProperty('ADBE Text Selector');"
          "var range=sel.property('ADBE Text Percent Start');"
          "range.setValueAtTime(0,100);range.setValueAtTime(c.duration*0.5,0);"
          "var props=anim.property('ADBE Text Animator Properties');"
          "props.addProperty('ADBE Text Scale');props.property('ADBE Text Scale').setValue([0,0]);"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      case CompUI::ACTION_TEXT_ANIMATOR_BLUR:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer||!(layer instanceof TextLayer))return;"
          "app.beginUndoGroup('Add Blur');"
          "var tp=layer.Text.property('ADBE Text Animators');"
          "var anim=tp.addProperty('ADBE Text Animator');"
          "anim.name='Blur In';"
          "var sel=anim.property('ADBE Text Selectors').addProperty('ADBE Text Selector');"
          "var range=sel.property('ADBE Text Percent Start');"
          "range.setValueAtTime(0,100);range.setValueAtTime(c.duration*0.5,0);"
          "var props=anim.property('ADBE Text Animator Properties');"
          "props.addProperty('ADBE Text Blur');props.property('ADBE Text Blur').setValue(20);"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      case CompUI::ACTION_TEXT_ANIMATOR_TRACKING:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer||!(layer instanceof TextLayer))return;"
          "app.beginUndoGroup('Add Tracking');"
          "var tp=layer.Text.property('ADBE Text Animators');"
          "var anim=tp.addProperty('ADBE Text Animator');"
          "anim.name='Tracking';"
          "var sel=anim.property('ADBE Text Selectors').addProperty('ADBE Text Selector');"
          "var range=sel.property('ADBE Text Percent Start');"
          "range.setValueAtTime(0,100);range.setValueAtTime(c.duration*0.5,0);"
          "var props=anim.property('ADBE Text Animator Properties');"
          "props.addProperty('ADBE Text Tracking Amount');props.property('ADBE Text Tracking Amount').setValue(50);"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      // Shape layer actions
      case CompUI::ACTION_SHAPE_TRIM_PATH:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer||!(layer instanceof ShapeLayer))return;"
          "app.beginUndoGroup('Add Trim Paths');"
          "var contents=layer.property('ADBE Root Vectors Group');"
          "var trim=contents.addProperty('ADBE Vector Filter - Trim');"
          "trim.property('ADBE Vector Trim End').setValueAtTime(0,0);"
          "trim.property('ADBE Vector Trim End').setValueAtTime(c.duration*0.5,100);"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      case CompUI::ACTION_SHAPE_REPEATER:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer||!(layer instanceof ShapeLayer))return;"
          "app.beginUndoGroup('Add Repeater');"
          "var contents=layer.property('ADBE Root Vectors Group');"
          "var rep=contents.addProperty('ADBE Vector Filter - Repeater');"
          "rep.property('ADBE Vector Repeater Copies').setValue(5);"
          "rep.property('ADBE Vector Repeater Transform').property('ADBE Vector Repeater Position').setValue([100,0]);"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      case CompUI::ACTION_SHAPE_WIGGLE_PATH:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer||!(layer instanceof ShapeLayer))return;"
          "app.beginUndoGroup('Add Wiggle Paths');"
          "var contents=layer.property('ADBE Root Vectors Group');"
          "var wig=contents.addProperty('ADBE Vector Filter - Wiggler');"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      case CompUI::ACTION_SHAPE_WIGGLE_TRANSFORM:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer||!(layer instanceof ShapeLayer))return;"
          "app.beginUndoGroup('Add Wiggle Transform');"
          "var contents=layer.property('ADBE Root Vectors Group');"
          "var wig=contents.addProperty('ADBE Vector Filter - Wig-Zag');"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      // Solid layer actions
      case CompUI::ACTION_SOLID_CHANGE_COLOR:
        // TODO: Open color picker
        break;

      case CompUI::ACTION_SOLID_FIT_TO_COMP:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer)return;"
          "if(!(layer.source instanceof SolidSource))return;"
          "app.beginUndoGroup('Fit Solid to Comp');"
          "layer.source.mainSource.width=c.width;"
          "layer.source.mainSource.height=c.height;"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      // Footage layer actions
      case CompUI::ACTION_FOOTAGE_LOOP_CYCLE:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer)return;"
          "app.beginUndoGroup('Add Loop Cycle');"
          "layer.timeRemapEnabled=true;"
          "layer.timeRemap.expression='loopOut(\"cycle\")';"
          "layer.outPoint=c.duration;"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      case CompUI::ACTION_FOOTAGE_LOOP_PINGPONG:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer)return;"
          "app.beginUndoGroup('Add Loop Ping Pong');"
          "layer.timeRemapEnabled=true;"
          "layer.timeRemap.expression='loopOut(\"pingpong\")';"
          "layer.outPoint=c.duration;"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      case CompUI::ACTION_FOOTAGE_LAST_FRAME_HOLD:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer)return;"
          "app.beginUndoGroup('Last Frame Hold');"
          "layer.timeRemapEnabled=true;"
          "var dur=layer.source.duration;"
          "layer.timeRemap.setValueAtTime(dur,dur-c.frameDuration);"
          "layer.outPoint=c.duration;"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      // Common actions
      case CompUI::ACTION_RESET_TRANSFORM:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer)return;"
          "app.beginUndoGroup('Reset Transform');"
          "var hasParent=layer.parent!==null;"
          "var oldParent=layer.parent;"
          "if(hasParent)layer.parent=null;"
          "layer.position.setValue([c.width/2,c.height/2]);"
          "layer.scale.setValue([100,100]);"
          "layer.rotation.setValue(0);"
          "if(hasParent)layer.parent=oldParent;"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      case CompUI::ACTION_RESET_POSITION:
        snprintf(script, sizeof(script),
          "(function(){"
          "var c=app.project.activeItem;if(!c)return;"
          "var layer=c.selectedLayers[0];if(!layer)return;"
          "app.beginUndoGroup('Reset Position');"
          "var hasParent=layer.parent!==null;"
          "var oldParent=layer.parent;"
          "if(hasParent)layer.parent=null;"
          "layer.position.setValue([c.width/2,c.height/2,0]);"
          "if(hasParent)layer.parent=oldParent;"
          "app.endUndoGroup();"
          "})()");
        ExecuteScript(script, resultBuf, sizeof(resultBuf));
        break;

      default:
        break;
      }
    }
  }

  // Update hover while layer panel is visible
  if (g_layerVisible && CompUI::IsVisible()) {
    int mouseX = 0, mouseY = 0;
    KeyboardMonitor::GetMousePosition(&mouseX, &mouseY);
    CompUI::UpdateHover(mouseX, mouseY);
  }

  *max_sleepPL = 33; // ~30fps for hover updates

  return err;
}

/*****************************************************************************
 * EntryPointFunc
 * Main plugin entry point
 *****************************************************************************/
extern "C" DllExport A_Err EntryPointFunc(struct SPBasicSuite *pica_basicP,
                                          A_long major_versionL,
                                          A_long minor_versionL,
                                          AEGP_PluginID aegp_plugin_id,
                                          AEGP_GlobalRefcon *global_refconP) {
  A_Err err = A_Err_NONE;

  try {
    AEGP_SuiteHandler suites(pica_basicP);

    g_globals.plugin_id = aegp_plugin_id;
    g_globals.pica_basicP = pica_basicP;
    g_globals.menu_visible = false;
    g_globals.key_was_held = false;

    // Initialize native UI modules
    NativeUI::Initialize();
    ControlUI::Initialize();
    KeyframeUI::Initialize();
    AlignUI::Initialize();
    TextUI::Initialize();
    CompUI::Initialize();
    DMenuUI::Initialize();

    *global_refconP = (AEGP_GlobalRefcon)&g_globals;

    ERR(suites.RegisterSuite5()->AEGP_RegisterIdleHook(
        aegp_plugin_id, IdleHook, (AEGP_IdleRefcon)&g_globals));

    // Register Command Hook to detect text editing mode (2136=enter, 2004=exit)
    ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(
        aegp_plugin_id,
        AEGP_HP_BeforeAE,
        AEGP_Command_ALL,
        CommandHook,
        nullptr));

    // Register UpdateMenu Hook to detect window type changes
    ERR(suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(
        aegp_plugin_id,
        UpdateMenuHook,
        nullptr));

  } catch (...) {
    err = A_Err_GENERIC;
  }

  return err;
}
