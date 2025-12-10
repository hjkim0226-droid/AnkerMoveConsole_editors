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

// ScriptUI JSX code embedded as string
static const char *ANCHOR_GRID_SCRIPT = R"JSX(
(function() {
    var config = { gridSize: 3, cellSize: 50 };
    
    function showAnchorGrid() {
        var winSize = config.gridSize * config.cellSize + 20;
        var win = new Window('palette', 'Anchor Grid', undefined, { borderless: true });
        
        // Center on screen (ScriptUI can't get mouse position directly)
        win.center();
        
        win.graphics.backgroundColor = win.graphics.newBrush(
            win.graphics.BrushType.SOLID_COLOR, [0.14, 0.14, 0.14]
        );
        
        var gridPanel = win.add('group');
        gridPanel.orientation = 'column';
        gridPanel.spacing = 4;
        gridPanel.margins = 10;
        
        for (var y = 0; y < config.gridSize; y++) {
            var row = gridPanel.add('group');
            row.orientation = 'row';
            row.spacing = 4;
            
            for (var x = 0; x < config.gridSize; x++) {
                var btn = row.add('button', undefined, '');
                btn.size = [config.cellSize, config.cellSize];
                btn.gridX = x;
                btn.gridY = y;
                
                btn.onClick = function() {
                    applyAnchor(this.gridX, this.gridY, config.gridSize);
                    win.close();
                };
            }
        }
        
        win.addEventListener('keydown', function(e) {
            if (e.keyName === 'Escape') win.close();
        });
        
        win.show();
    }
    
    function applyAnchor(gridX, gridY, gridSize) {
        var comp = app.project.activeItem;
        if (!comp || !(comp instanceof CompItem)) return;
        if (comp.selectedLayers.length === 0) return;
        
        app.beginUndoGroup('Set Anchor Point');
        
        for (var i = 0; i < comp.selectedLayers.length; i++) {
            var layer = comp.selectedLayers[i];
            var bounds = layer.sourceRectAtTime(comp.time, false);
            
            var percentX = gridX / (gridSize - 1);
            var percentY = gridY / (gridSize - 1);
            
            var newAnchorX = bounds.left + (bounds.width * percentX);
            var newAnchorY = bounds.top + (bounds.height * percentY);
            
            var anchorProp = layer.property('ADBE Transform Group').property('ADBE Anchor Point');
            var positionProp = layer.property('ADBE Transform Group').property('ADBE Position');
            
            var oldAnchor = anchorProp.value;
            var position = positionProp.value;
            
            var deltaX = newAnchorX - oldAnchor[0];
            var deltaY = newAnchorY - oldAnchor[1];
            
            var scaleProp = layer.property('ADBE Transform Group').property('ADBE Scale');
            var scale = scaleProp.value;
            var scaleX = scale[0] / 100;
            var scaleY = scale[1] / 100;
            
            var rotationProp = layer.property('ADBE Transform Group').property('ADBE Rotate Z');
            var rotation = rotationProp.value * Math.PI / 180;
            
            var rotatedDeltaX = (deltaX * Math.cos(rotation) - deltaY * Math.sin(rotation)) * scaleX;
            var rotatedDeltaY = (deltaX * Math.sin(rotation) + deltaY * Math.cos(rotation)) * scaleY;
            
            anchorProp.setValue([newAnchorX, newAnchorY]);
            
            if (position.length === 3) {
                positionProp.setValue([position[0] + rotatedDeltaX, position[1] + rotatedDeltaY, position[2]]);
            } else {
                positionProp.setValue([position[0] + rotatedDeltaX, position[1] + rotatedDeltaY]);
            }
        }
        
        app.endUndoGroup();
    }
    
    showAnchorGrid();
})();
)JSX";

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
