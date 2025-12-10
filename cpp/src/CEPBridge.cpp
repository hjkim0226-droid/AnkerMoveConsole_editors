/*****************************************************************************
 * CEPBridge.cpp
 *
 * Bridge implementation for AEGP-CEP communication
 * Uses file-based IPC - writes commands to a file that CEP panel watches
 *****************************************************************************/

#include "CEPBridge.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifdef MAC_ENV
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>
#else
#include <direct.h> // for _mkdir
#endif

namespace CEPBridge {

// IPC directory path (cached after first call)
static std::string s_ipc_directory;
static bool s_initialized = false;

std::string GetIPCDirectory() {
  if (!s_ipc_directory.empty()) {
    return s_ipc_directory;
  }

#ifdef MAC_ENV
  // Use ~/Library/Application Support/AnchorRadialMenu/
  const char *home = getenv("HOME");
  if (home) {
    s_ipc_directory =
        std::string(home) + "/Library/Application Support/AnchorRadialMenu/";
  } else {
    s_ipc_directory = "/tmp/AnchorRadialMenu/";
  }
#else
  // Windows: Use %APPDATA%\AnchorRadialMenu\
    const char* appdata = getenv("APPDATA");
  if (appdata) {
    s_ipc_directory = std::string(appdata) + "\\AnchorRadialMenu\\";
  } else {
    s_ipc_directory = "C:\\Temp\\AnchorRadialMenu\\";
  }
#endif

  return s_ipc_directory;
}

std::string GetCommandFilePath() { return GetIPCDirectory() + "command.txt"; }

std::string GetStateFilePath() { return GetIPCDirectory() + "state.txt"; }

void Initialize() {
  if (s_initialized)
    return;

  std::string dir = GetIPCDirectory();

  // Create directory if it doesn't exist
#ifdef MAC_ENV
  mkdir(dir.c_str(), 0755);
#else
  _mkdir(dir.c_str());
#endif

  // Clear any existing command file
  std::ofstream ofs(GetCommandFilePath(), std::ios::trunc);
  ofs.close();

  s_initialized = true;
}

void SendCommand(const char *command) {
  if (!s_initialized) {
    Initialize();
  }

  // Write command to file - CEP panel will watch this file
  std::ofstream ofs(GetCommandFilePath(), std::ios::trunc);
  if (ofs.is_open()) {
    ofs << command << std::endl;
    ofs.close();
  }
}

bool GetCurrentGridSelection(int *gridX, int *gridY) {
  if (!gridX || !gridY)
    return false;

  *gridX = -1;
  *gridY = -1;

  // Read state file written by CEP panel
  std::ifstream ifs(GetStateFilePath());
  if (!ifs.is_open()) {
    return false;
  }

  std::string line;
  if (std::getline(ifs, line)) {
    // Expected format: "gridX,gridY" or "outside"
    if (line == "outside") {
      return true; // Valid response, just outside grid
    }

    std::istringstream iss(line);
    char comma;
    if (iss >> *gridX >> comma >> *gridY) {
      return true;
    }
  }

  return false;
}

bool IsPanelActive() {
  // Check if state file exists and was recently modified
  struct stat statbuf;
  if (stat(GetStateFilePath().c_str(), &statbuf) == 0) {
    // Consider panel active if file was modified in last 5 seconds
    time_t now = time(NULL);
    return (now - statbuf.st_mtime) < 5;
  }
  return false;
}

} // namespace CEPBridge
