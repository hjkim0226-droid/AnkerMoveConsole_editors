/*****************************************************************************
 * AnchorRadialMenu.cpp
 *
 * Main AEGP plugin implementation for Anchor Grid
 * Uses ScriptUI via AEGP_ExecuteScript for floating anchor grid
 *****************************************************************************/

#include "AnchorRadialMenu.h"
#include "KeyboardMonitor.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
#include "AE_Macros.h"

// Global state
static AnchorRadialMenuGlobals g_globals = {0, NULL, false, false, false};

// ScriptUI Anchor Grid - Using C string concatenation for compatibility
static const char *ANCHOR_GRID_SCRIPT =
    "(function(){"
    "var gridSize=3,cellSize=50;"
    "var w=new Window('palette','Anchor Grid',undefined,{borderless:true});"
    "w.orientation='column';"
    "w.margins=10;"
    "for(var y=0;y<gridSize;y++){"
    "var row=w.add('group');"
    "row.orientation='row';"
    "row.spacing=4;"
    "for(var x=0;x<gridSize;x++){"
    "var b=row.add('button',undefined,'');"
    "b.preferredSize=[cellSize,cellSize];"
    "b.gx=x;b.gy=y;"
    "b.onClick=function(){applyAnchor(this.gx,this.gy);w.close();};"
    "}}"
    "function applyAnchor(gx,gy){"
    "var c=app.project.activeItem;"
    "if(!c||!(c instanceof CompItem))return;"
    "if(c.selectedLayers.length==0)return;"
    "app.beginUndoGroup('Set Anchor');"
    "for(var i=0;i<c.selectedLayers.length;i++){"
    "var L=c.selectedLayers[i];"
    "var b=L.sourceRectAtTime(c.time,false);"
    "var px=gx/(gridSize-1),py=gy/(gridSize-1);"
    "var nx=b.left+b.width*px,ny=b.top+b.height*py;"
    "var ap=L.property('ADBE Transform Group').property('ADBE Anchor Point');"
    "var pp=L.property('ADBE Transform Group').property('ADBE Position');"
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
    "}"
    "w.center();w.show();"
    "})();";

// Define MissingSuiteError for AEGP_SuiteHandler
void AEGP_SuiteHandler::MissingSuiteError() const {
  throw std::runtime_error("Missing AEGP Suite");
}

/*****************************************************************************
 * ExecuteScript
 * Execute ExtendScript using AEGP_ExecuteScript
 *****************************************************************************/
A_Err ExecuteScript(const char *script) {
  A_Err err = A_Err_NONE;

  try {
    AEGP_SuiteHandler suites(g_globals.pica_basicP);

    AEGP_MemHandle resultH = NULL;
    AEGP_MemHandle errorH = NULL;

    err =
        suites.UtilitySuite6()->AEGP_ExecuteScript(g_globals.plugin_id, script,
                                                   TRUE, // platform_encoding
                                                   &resultH, &errorH);

    // Free memory handles if allocated
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
 * IdleHook
 * Called periodically by After Effects - we use this to check keyboard state
 *****************************************************************************/
A_Err IdleHook(AEGP_GlobalRefcon plugin_refconP, AEGP_IdleRefcon refconP,
               A_long *max_sleepPL) {
  A_Err err = A_Err_NONE;

  // Check keyboard state
  bool y_key_held = KeyboardMonitor::IsKeyHeld(KeyboardMonitor::KEY_Y);
  bool alt_held = KeyboardMonitor::IsAltHeld();

  // Y key pressed (not held before) - show anchor grid
  if (y_key_held && !g_globals.key_was_held && !alt_held) {
    // Execute ScriptUI to show anchor grid
    ExecuteScript(ANCHOR_GRID_SCRIPT);
    g_globals.menu_visible = true;
  }

  g_globals.key_was_held = y_key_held;

  // Check every 50ms for responsive keyboard detection
  *max_sleepPL = 50;

  return err;
}

/*****************************************************************************
 * EntryPointFunc
 * Main plugin entry point - called when After Effects loads the plugin
 *****************************************************************************/
extern "C" DllExport A_Err EntryPointFunc(struct SPBasicSuite *pica_basicP,
                                          A_long major_versionL,
                                          A_long minor_versionL,
                                          AEGP_PluginID aegp_plugin_id,
                                          AEGP_GlobalRefcon *global_refconP) {
  A_Err err = A_Err_NONE;

  try {
    AEGP_SuiteHandler suites(pica_basicP);

    // Store global references
    g_globals.plugin_id = aegp_plugin_id;
    g_globals.pica_basicP = pica_basicP;
    g_globals.menu_visible = false;
    g_globals.key_was_held = false;

    *global_refconP = (AEGP_GlobalRefcon)&g_globals;

    // Register IdleHook for keyboard monitoring
    ERR(suites.RegisterSuite5()->AEGP_RegisterIdleHook(
        aegp_plugin_id, IdleHook, (AEGP_IdleRefcon)&g_globals));

  } catch (const std::exception &e) {
    err = A_Err_GENERIC;
  } catch (...) {
    err = A_Err_GENERIC;
  }

  return err;
}
