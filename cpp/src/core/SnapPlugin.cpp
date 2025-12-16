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
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#ifdef MSWindows
#include <windows.h>
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
static bool g_semicolonWasHeld = false;

/*****************************************************************************
 * IsTextInputFocused
 * Check if focus is on a text input field (Edit, RichEdit, etc.)
 * Returns true if user is typing in a text field
 *****************************************************************************/
#ifdef MSWindows
bool IsTextInputFocused() {
  HWND focused = GetFocus();
  if (!focused) {
    // GetFocus only works for current thread, try GetForegroundWindow +
    // GetGUIThreadInfo
    HWND fg = GetForegroundWindow();
    if (fg) {
      DWORD threadId = GetWindowThreadProcessId(fg, NULL);
      GUITHREADINFO gti = {0};
      gti.cbSize = sizeof(GUITHREADINFO);
      if (GetGUIThreadInfo(threadId, &gti)) {
        focused = gti.hwndFocus;
      }
    }
  }

  if (!focused)
    return false;

  char className[64] = {0};
  GetClassNameA(focused, className, sizeof(className));

  // Common text input class names
  // "Edit" - standard Win32 edit control
  // "RichEdit" - rich text edit variants
  // "Scintilla" - code editors
  // AE internal text fields may use these
  if (_strnicmp(className, "Edit", 4) == 0 ||
      _strnicmp(className, "RichEdit", 8) == 0 ||
      _strnicmp(className, "RICHEDIT", 8) == 0 ||
      _strnicmp(className, "Scintilla", 9) == 0) {
    return true;
  }

  // Check window style for ES_MULTILINE or similar edit styles
  LONG style = GetWindowLong(focused, GWL_STYLE);
  if (style & ES_MULTILINE || style & ES_AUTOHSCROLL) {
    // Likely an edit control even if class name differs
    // But be careful - many controls have these styles
  }

  return false;
}

/*****************************************************************************
 * IsAfterEffectsForeground
 * Check if After Effects is the foreground (active) window
 * Returns true only when AE is in focus
 *****************************************************************************/
bool IsAfterEffectsForeground() {
  HWND fg = GetForegroundWindow();
  if (!fg)
    return false;

  // Get window title
  char title[256] = {0};
  GetWindowTextA(fg, title, sizeof(title));

  // Check if window title contains "After Effects"
  // AE main window title format: "Adobe After Effects 2024 - project.aep"
  if (strstr(title, "After Effects") != NULL) {
    return true;
  }

  // Also check window class for AE
  char className[64] = {0};
  GetClassNameA(fg, className, sizeof(className));

  // AE uses class names starting with "AE_"
  if (_strnicmp(className, "AE_", 3) == 0) {
    return true;
  }

  return false;
}
#else
// macOS stub - TODO: implement NSTextField focus check
bool IsTextInputFocused() { return false; }
bool IsAfterEffectsForeground() { return true; } // Always true for now on macOS
#endif

// Settings loaded from CEP
static int g_loadedGridWidth = 3;
static int g_loadedGridHeight = 3;
static int g_loadedGridScale = 2;    // 0-9, default 2 (0%)
static int g_loadedGridOpacity = 75; // 0-100%
static int g_loadedCellOpacity = 50; // 0-100%

// Define MissingSuiteError for AEGP_SuiteHandler
void AEGP_SuiteHandler::MissingSuiteError() const {
  throw std::runtime_error("Missing AEGP Suite");
}

/*****************************************************************************
 * ExecuteScript
 * Execute ExtendScript using AEGP_ExecuteScript, returns result string
 *****************************************************************************/
A_Err ExecuteScript(const char *script, char *resultBuf = NULL,
                    size_t bufSize = 0) {
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

  // customAnchors array - parse [{x:50,y:50},{x:50,y:50},{x:50,y:50}]
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
        int xVal = atoi(xp);

        // Find "y":
        const char *yp = strstr(xp, "\"y\":");
        if (!yp)
          break;
        yp += 4;
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
      "ap.setValue([nx,ny]);"
      "if(pos.length==3)pp.setValue([pos[0]+rdx,pos[1]+rdy,pos[2]]);"
      "else pp.setValue([pos[0]+rdx,pos[1]+rdy]);"
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
      "ap.setValue([nx,ny]);"
      "if(pos.length==3)pp.setValue([pos[0]+rdx,pos[1]+rdy,pos[2]]);"
      "else pp.setValue([pos[0]+rdx,pos[1]+rdy]);"
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
    // Notify CEP of change via ExtendScript
    char script[128];
    snprintf(script, sizeof(script), "notifyModeChange(%s, %s)",
             settings.useCompMode ? "true" : "false",
             settings.useMaskRecognition ? "true" : "false");
    ExecuteScript(script);
    // Don't return - fall through to hide grid
  } else if (hoverOpt == NativeUI::OPT_MASK_MODE) {
    settings.useMaskRecognition = !settings.useMaskRecognition;
    SaveSettingsToFile(); // Persist to file
    // Notify CEP of change via ExtendScript
    char script[128];
    snprintf(script, sizeof(script), "notifyModeChange(%s, %s)",
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
                    "var menuId=app.findMenuCommandId('Anchor Grid');"
                    "if(menuId>0)app.executeCommand(menuId);"
                    "}catch(e){"
                    "alert('Please open Window > Extensions > Anchor Grid');"
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
 * IdleHook
 * Called periodically by After Effects - we use this to check keyboard state
 *****************************************************************************/
A_Err IdleHook(AEGP_GlobalRefcon plugin_refconP, AEGP_IdleRefcon refconP,
               A_long *max_sleepPL) {
  A_Err err = A_Err_NONE;

  bool y_key_held = KeyboardMonitor::IsKeyHeld(KeyboardMonitor::KEY_Y);
  bool alt_held = KeyboardMonitor::IsAltHeld();
  auto now = std::chrono::steady_clock::now();

  // Y key just pressed
  // Skip if: user is typing in text field OR After Effects is not in
  // foreground
  if (y_key_held && !g_globals.key_was_held && !alt_held &&
      !IsTextInputFocused() && IsAfterEffectsForeground()) {
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
  // CONTROL MODULE: Semicolon key (;) for effect search
  // =========================================================================
  bool semicolon_held = KeyboardMonitor::IsKeyHeld(KeyboardMonitor::KEY_SEMICOLON);

  // Semicolon just pressed - show Control panel
  if (semicolon_held && !g_semicolonWasHeld && !IsTextInputFocused() &&
      IsAfterEffectsForeground() && !g_globals.menu_visible) {
    ControlUI::ShowPanel();
    g_controlVisible = true;
  }

  // Check if Control panel closed and effect was selected
  if (g_controlVisible && !ControlUI::IsVisible()) {
    g_controlVisible = false;
    ControlUI::ControlResult result = ControlUI::GetResult();

    if (result.effectSelected) {
      // Apply selected effect to layer via ExtendScript
      // TODO: Call ExtendScript to apply effect
      // For now, just log the selection
    }
  }

  g_semicolonWasHeld = semicolon_held;
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

    *global_refconP = (AEGP_GlobalRefcon)&g_globals;

    ERR(suites.RegisterSuite5()->AEGP_RegisterIdleHook(
        aegp_plugin_id, IdleHook, (AEGP_IdleRefcon)&g_globals));

  } catch (...) {
    err = A_Err_GENERIC;
  }

  return err;
}
