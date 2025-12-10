#!/bin/bash

# Configuration
PLUGIN_NAME="AnchorRadialMenu.plugin"
CEP_ID="com.anchor.grid"

# Paths
BUILD_DIR="$(pwd)/cpp/build/plugin/$PLUGIN_NAME"
CEP_SRC="$(pwd)/cep"

# Adobe Paths (macOS - User)
PLUGIN_DEST="$HOME/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
CEP_DEST="$HOME/Library/Application Support/Adobe/CEP/extensions/$CEP_ID"

echo "============================================="
echo "Installing Anchor Grid Plugin..."
echo "============================================="

# 1. Install C++ Plugin
if [ -d "$BUILD_DIR" ]; then
    echo "Installing C++ Plugin to: $PLUGIN_DEST"
    mkdir -p "$PLUGIN_DEST"
    rm -rf "$PLUGIN_DEST/$PLUGIN_NAME"
    cp -r "$BUILD_DIR" "$PLUGIN_DEST/"
else
    echo "ERROR: Plugin build not found at $BUILD_DIR"
    exit 1
fi

# 2. Install CEP Panel
if [ -d "$CEP_SRC" ]; then
    echo "Installing CEP Panel to: $CEP_DEST"
    mkdir -p "$CEP_DEST"
    rm -rf "$CEP_DEST"
    cp -r "$CEP_SRC" "$CEP_DEST"
else
    echo "ERROR: CEP source not found at $CEP_SRC"
    exit 1
fi

# 3. Enable Debug Mode
echo "Enabling CEP Debug Mode..."
defaults write com.adobe.CSXS.10 PlayerDebugMode 1
defaults write com.adobe.CSXS.11 PlayerDebugMode 1
defaults write com.adobe.CSXS.12 PlayerDebugMode 1
defaults write com.adobe.CSXS.13 PlayerDebugMode 1
defaults write com.adobe.CSXS.14 PlayerDebugMode 1
defaults write com.adobe.CSXS.15 PlayerDebugMode 1
defaults write com.adobe.CSXS.16 PlayerDebugMode 1

echo "============================================="
echo "Installation Complete!"
echo "Please restart After Effects."
echo "============================================="
