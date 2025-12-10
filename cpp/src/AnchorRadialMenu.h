/*****************************************************************************
 * AnchorRadialMenu.h
 *
 * Main AEGP plugin header for Anchor Radial Menu
 * Handles plugin initialization and IdleHook registration
 *****************************************************************************/

#pragma once

#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"

// Plugin identification
#define PLUGIN_NAME "Anchor Radial Menu"
#define PLUGIN_VERSION "1.0.0"

#ifdef MSWindows
#define DllExport __declspec(dllexport)
#else
#define DllExport __attribute__((visibility("default")))
#endif

// Global plugin state
struct AnchorRadialMenuGlobals {
  AEGP_PluginID plugin_id;
  SPBasicSuite *pica_basicP;
  bool menu_visible;
  bool key_was_held;
  bool click_mode_active; // Option+Y toggle mode
};

// Plugin entry point
extern "C" {
DllExport A_Err EntryPointFunc(struct SPBasicSuite *pica_basicP,
                               A_long major_versionL, A_long minor_versionL,
                               AEGP_PluginID aegp_plugin_id,
                               AEGP_GlobalRefcon *global_refconP);
}

// Hook callbacks
A_Err IdleHook(AEGP_GlobalRefcon plugin_refconP, AEGP_IdleRefcon refconP,
               A_long *max_sleepPL);

// Utility functions
void ShowRadialMenu();
void HideRadialMenu();
void ToggleClickMode();
void ApplyAnchorSelection(int gridX, int gridY);
