/*****************************************************************************
 * DMenuUI.h
 *
 * D-key quick menu for accessing sub-modules
 * Shows when D key is pressed, receives A/T/etc keys to open panels
 *
 * Features:
 * - Takes focus to intercept key input (prevents AE shortcuts)
 * - Shows available options: [A] Align, [T] Text, [K] Keyframe
 * - Closes on ESC, outside click, or after action
 *****************************************************************************/

#ifndef DMENUUI_H
#define DMENUUI_H

namespace DMenuUI {

// Menu action result
enum MenuAction {
    ACTION_NONE = 0,
    ACTION_CANCELLED,
    ACTION_ALIGN,
    ACTION_TEXT,
    ACTION_KEYFRAME,
    ACTION_COMP,
    ACTION_SETTINGS
};

// Initialize the D Menu UI system
void Initialize();

// Cleanup
void Shutdown();

// Show the menu at specified position
void ShowMenu(int x, int y);

// Hide the menu
void HideMenu();

// Check if menu is visible
bool IsVisible();

// Get the action after menu closes
MenuAction GetAction();

// Update (call from idle hook)
void Update();

} // namespace DMenuUI

#endif // DMENUUI_H
