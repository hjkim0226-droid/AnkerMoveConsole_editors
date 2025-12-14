/*****************************************************************************
 * AnchorRadialMenu.cpp
 *
 * Main AEGP plugin implementation for Anchor Grid
 * Uses Native Win32 UI for floating anchor grid (fast, custom styling)
 *****************************************************************************/

#include "AnchorRadialMenu.h"
#include "KeyboardMonitor.h"
#include "NativeUI.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
#include "AE_Macros.h"

// Global state
static AnchorRadialMenuGlobals g_globals = {0, NULL, false, false, false};
static int g_mouseStartX = 0;
static int g_mouseStartY = 0;
static std::chrono::steady_clock::time_point g_keyPressTime;
static bool g_waitingForHold = false;
static const int HOLD_DELAY_MS = 400; // 0.4 seconds
static const int DOUBLE_TAP_MS = 250; // Max time between double-tap
static std::chrono::steady_clock::time_point g_lastYRelease;
static bool g_toggleClickMode = false; // Toggle mode vs hold mode

// Settings loaded from CEP
static int g_loadedGridWidth = 3;
static int g_loadedGridHeight = 3;
static int g_loadedGridScale = 2; // 0-4, default 2 (0%)
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
  // Windows: %APPDATA%\Adobe\CEP\extensions\com.anchor.grid\settings.json
  char path[512];
  const char* appdata = getenv("APPDATA");
  if (!appdata) return;
  snprintf(path, sizeof(path), "%s\\Adobe\\CEP\\extensions\\com.anchor.grid\\settings.json", appdata);
#else
  // macOS: ~/Library/Application Support/Adobe/CEP/extensions/com.anchor.grid/settings.json
  char path[512];
  const char* home = getenv("HOME");
  if (!home) return;
  snprintf(path, sizeof(path), "%s/Library/Application Support/Adobe/CEP/extensions/com.anchor.grid/settings.json", home);
#endif

  FILE *f = fopen(path, "r");
  if (!f) return;
  
  char buffer[2048];
  size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
  buffer[len] = '\0';
  fclose(f);
  
  // Parse simple JSON
  NativeUI::GridSettings& settings = NativeUI::GetSettings();
  
  const char* p;
  // useCompMode
  if ((p = strstr(buffer, "\"useCompMode\":")) != NULL) {
    p += 14; // skip past key
    settings.useCompMode = (strstr(p, "true") != NULL && (strstr(p, "true") < strstr(p, ",") || strstr(p, ",") == NULL));
  }
  // useMaskRecognition
  if ((p = strstr(buffer, "\"useMaskRecognition\":")) != NULL) {
    p += 21;
    settings.useMaskRecognition = (strstr(p, "true") != NULL && (strstr(p, "true") < strstr(p, ",") || strstr(p, ",") == NULL));
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
  // gridScale (0-4)
  if ((p = strstr(buffer, "\"gridScale\":")) != NULL) {
    p += 12;
    int val = atoi(p);
    if (val >= 0 && val <= 4) {
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
}

/*****************************************************************************
 * ShowAnchorGrid
 * Show the native anchor grid at specified position
 *****************************************************************************/
void ShowAnchorGrid(int mouseX, int mouseY) {
  NativeUI::GridConfig config;
  config.gridWidth = g_loadedGridWidth;
  config.gridHeight = g_loadedGridHeight;
  
  // Scale: 0=-40%, 1=-20%, 2=0%, 3=+20%, 4=+40%
  int baseSize = 40;
  float scaleFactors[] = {0.6f, 0.8f, 1.0f, 1.2f, 1.4f};
  config.cellSize = (int)(baseSize * scaleFactors[g_loadedGridScale]);
  
  config.spacing = 1;
  config.margin = 2;

  // Apply opacity settings
  NativeUI::GridSettings& settings = NativeUI::GetSettings();
  settings.gridOpacity = g_loadedGridOpacity;
  settings.cellOpacity = g_loadedCellOpacity;

  NativeUI::ShowGrid(mouseX, mouseY, config);
}

/*****************************************************************************
 * ApplyAnchorToLayers
 * Apply anchor point to selected layers based on grid position
 *****************************************************************************/
void ApplyAnchorToLayers(int gridX, int gridY) {
  int gridW = g_loadedGridWidth;
  int gridH = g_loadedGridHeight;

  char script[5000];
  snprintf(
      script, sizeof(script),
      "(function(){"
      "var gx=%d,gy=%d,gridW=%d,gridH=%d;"
      "var c=app.project.activeItem;"
      "if(!c||!(c instanceof CompItem))return;"
      "if(c.selectedLayers.length==0)return;"
      "app.beginUndoGroup('Set Anchor');"
      "for(var i=0;i<c.selectedLayers.length;i++){"
      "var L=c.selectedLayers[i];"
      // Function to get mask bounds
      "function getMaskBounds(layer){"
      "var masks=layer.property('ADBE Mask Parade');"
      "if(!masks||masks.numProperties==0)return null;"
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
      "if(minX==Infinity)return null;"
      "return{left:minX,top:minY,width:maxX-minX,height:maxY-minY};"
      "}"
      // Check for masks first, then fall back to sourceRectAtTime
      "var b=getMaskBounds(L);"
      "if(!b)b=L.sourceRectAtTime(c.time,false);"
      "if(!b||b.width<=0||b.height<=0)continue;"
      "var px=gx/(gridW-1),py=gy/(gridH-1);"
      "var nx=b.left+b.width*px,ny=b.top+b.height*py;"
      "var ap=L.property('ADBE Transform Group').property('ADBE Anchor Point');"
      "var pp=L.property('ADBE Transform Group').property('ADBE Position');"
      "if(!ap||!pp)continue;"
      "var oa=ap.value,pos=pp.value;"
      "var dx=nx-oa[0],dy=ny-oa[1];"
      "var sc=L.property('ADBE Transform Group').property('ADBE Scale').value;"
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
      gridX, gridY, gridW, gridH);

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
      "var ap=L.property('ADBE Transform Group').property('ADBE Anchor Point');"
      "var pp=L.property('ADBE Transform Group').property('ADBE Position');"
      "if(!ap||!pp)continue;"
      "var oa=ap.value,pos=pp.value;"
      "var dx=nx-oa[0],dy=ny-oa[1];"
      "var sc=L.property('ADBE Transform Group').property('ADBE Scale').value;"
      "var sx=sc[0]/100,sy=sc[1]/100;"
      "var rot=L.property('ADBE Transform Group').property('ADBE Rotate Z').value*Math.PI/180;"
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

  NativeUI::GridResult result = NativeUI::HideGrid(mouseX, mouseY);

  // Handle extended menu options (side panel icons)
  if (result.extendedOption != NativeUI::OPT_NONE) {
    NativeUI::GridSettings &settings = NativeUI::GetSettings();
    switch (result.extendedOption) {
    // Left panel: Custom anchors
    case NativeUI::OPT_CUSTOM_1:
      // Apply custom anchor preset 1
      // Apply custom anchor preset 1
      ApplyCustomAnchor(settings.customAnchor1X, settings.customAnchor1Y);
      break;
    case NativeUI::OPT_CUSTOM_2:
      // Apply custom anchor preset 2
      ApplyCustomAnchor(settings.customAnchor2X, settings.customAnchor2Y);
      break;
    case NativeUI::OPT_CUSTOM_3:
      // Apply custom anchor preset 3
      ApplyCustomAnchor(settings.customAnchor3X, settings.customAnchor3Y);
      break;
    // Right panel: Mode controls
    case NativeUI::OPT_COMP_MODE:
      settings.useCompMode = !settings.useCompMode;
      break;
    case NativeUI::OPT_MASK_MODE:
      settings.useMaskRecognition = !settings.useMaskRecognition;
      break;
    case NativeUI::OPT_SETTINGS:
      // Open CEP panel via Window menu
      ExecuteScript(
        "(function(){"
        "try{"
        "var menuId=app.findMenuCommandId('Anchor Grid');"
        "if(menuId>0)app.executeCommand(menuId);"
        "}catch(e){"
        "alert('Please open Window > Extensions > Anchor Grid');"
        "}"
        "})();"
      );
      break;
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
  if (y_key_held && !g_globals.key_was_held && !alt_held) {
    if (HasSelectedLayers()) {
      // Check for double-tap (Y~Y)
      auto timeSinceLastRelease = std::chrono::duration_cast<std::chrono::milliseconds>(
          now - g_lastYRelease).count();
      
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

  g_globals.key_was_held = y_key_held;
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

    // Initialize native UI
    NativeUI::Initialize();

    *global_refconP = (AEGP_GlobalRefcon)&g_globals;

    ERR(suites.RegisterSuite5()->AEGP_RegisterIdleHook(
        aegp_plugin_id, IdleHook, (AEGP_IdleRefcon)&g_globals));

  } catch (...) {
    err = A_Err_GENERIC;
  }

  return err;
}
