/*****************************************************************************
 * AnchorRadialMenu.cpp
 *
 * Main AEGP plugin implementation for Anchor Radial Menu
 * Entry point and IdleHook for keyboard event monitoring
 *****************************************************************************/

#include "AnchorRadialMenu.h"
#include "CEPBridge.h"
#include "KeyboardMonitor.h"
#include <cstdarg>
#include <cstdio>
#include <stdexcept>

#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
#include "AE_Macros.h"

// Global state
static AnchorRadialMenuGlobals g_globals = {0, NULL, false, false, false};

// Define MissingSuiteError for AEGP_SuiteHandler
void AEGP_SuiteHandler::MissingSuiteError() const {
  throw std::runtime_error("Missing AEGP Suite");
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

  // Option+Y: Toggle click mode
  if (alt_held && y_key_held && !g_globals.key_was_held) {
    // Toggle click mode on/off
    if (g_globals.click_mode_active) {
      HideRadialMenu();
      g_globals.click_mode_active = false;
      g_globals.menu_visible = false;
    } else {
      ToggleClickMode();
      g_globals.click_mode_active = true;
      g_globals.menu_visible = true;
    }
  }
  // Y only (no Alt): Hold mode
  else if (!alt_held && y_key_held && !g_globals.key_was_held &&
           !g_globals.click_mode_active) {
    // Key just pressed - show radial menu in hold mode
    ShowRadialMenu();
    g_globals.menu_visible = true;
  } else if (!y_key_held && g_globals.key_was_held &&
             !g_globals.click_mode_active) {
    // Key just released in hold mode - apply selection and hide menu
    if (g_globals.menu_visible) {
      // Get current mouse position and calculate grid selection
      int gridX = 0, gridY = 0;
      CEPBridge::GetCurrentGridSelection(&gridX, &gridY);

      if (gridX >= 0 && gridY >= 0) {
        ApplyAnchorSelection(gridX, gridY);
      }

      HideRadialMenu();
      g_globals.menu_visible = false;
    }
  }

  g_globals.key_was_held = y_key_held;

  // Check every 50ms for responsive keyboard detection
  *max_sleepPL = 50;

  return err;
}

/*****************************************************************************
 * ToggleClickMode
 * Communicate with CEP panel to toggle click mode
 *****************************************************************************/
void ToggleClickMode() { CEPBridge::SendCommand("toggleClickMode"); }

/*****************************************************************************
 * ShowRadialMenu
 * Communicate with CEP panel to show the radial overlay
 *****************************************************************************/
void ShowRadialMenu() { CEPBridge::SendCommand("showRadialMenu"); }

/*****************************************************************************
 * HideRadialMenu
 * Communicate with CEP panel to hide the radial overlay
 *****************************************************************************/
void HideRadialMenu() { CEPBridge::SendCommand("hideRadialMenu"); }

/*****************************************************************************
 * ApplyAnchorSelection
 * Apply the selected anchor point to the current layer via ExtendScript
 *****************************************************************************/
void ApplyAnchorSelection(int gridX, int gridY) {
  char command[256];
  snprintf(command, sizeof(command), "applyAnchor:%d,%d", gridX, gridY);
  CEPBridge::SendCommand(command);
}

/*****************************************************************************
 * EntryPointFunc
 * Main plugin entry point - called when After Effects loads the plugin
 *****************************************************************************/
// Debug Logging Helper
void LogDebug(const char *fmt, ...) {
#ifdef MSWindows
  char path[1024];
  if (const char *home = getenv("USERPROFILE")) {
    snprintf(path, sizeof(path), "%s\\Desktop\\AnchorDebug.txt", home);
  } else {
    return;
  }

  FILE *fp = fopen(path, "a");
  if (fp) {
    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    fprintf(fp, "\n");
    fclose(fp);
  }
#endif
}

extern "C" DllExport A_Err EntryPointFunc(struct SPBasicSuite *pica_basicP,
                                          A_long major_versionL,
                                          A_long minor_versionL,
                                          AEGP_PluginID aegp_plugin_id,
                                          AEGP_GlobalRefcon *global_refconP) {
  A_Err err = A_Err_NONE;

  LogDebug("AnchorRadialMenu: EntryPointFunc Called! Plugin ID: %d",
           aegp_plugin_id);

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

    if (!err) {
      LogDebug("AnchorRadialMenu: IdleHook Registered Successfully");
    } else {
      LogDebug("AnchorRadialMenu: Failed to register IdleHook. Err: %d", err);
    }

    // Initialize CEP communication bridge
    CEPBridge::Initialize();
    LogDebug("AnchorRadialMenu: CEPBridge Initialized");

  } catch (const std::exception &e) {
    LogDebug("CRITICAL ERROR: Exception in EntryPointFunc: %s", e.what());
    err = A_Err_GENERIC;
  } catch (...) {
    LogDebug("CRITICAL ERROR: Unknown exception in EntryPointFunc");
    err = A_Err_GENERIC;
  }

  return err;
}
