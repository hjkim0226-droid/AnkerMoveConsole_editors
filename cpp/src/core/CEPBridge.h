/*****************************************************************************
 * CEPBridge.h
 *
 * Bridge for communication between C++ AEGP plugin and CEP panel
 * Uses file-based IPC for simplicity and reliability
 *****************************************************************************/

#pragma once

#include <string>

namespace CEPBridge {

/**
 * Initialize the CEP communication bridge
 * Creates necessary IPC files/directories
 */
void Initialize();

/**
 * Send a command to the CEP panel
 * @param command The command string to send
 */
void SendCommand(const char *command);

/**
 * Get the current grid selection from CEP panel
 * @param gridX Output: X position in grid (-1 if outside)
 * @param gridY Output: Y position in grid (-1 if outside)
 * @return true if successfully read selection
 */
bool GetCurrentGridSelection(int *gridX, int *gridY);

/**
 * Check if the CEP panel is currently active/visible
 */
bool IsPanelActive();

// IPC file paths
// These files are used for inter-process communication between AEGP and CEP
// Location: ~/Library/Application Support/AnchorRadialMenu/
std::string GetIPCDirectory();
std::string GetCommandFilePath();
std::string GetStateFilePath();

} // namespace CEPBridge
