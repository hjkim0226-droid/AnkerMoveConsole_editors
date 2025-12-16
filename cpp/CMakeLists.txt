cmake_minimum_required(VERSION 3.20)
project(AnchorSnap VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# After Effects SDK Configuration
# ============================================================================
# Set the path to After Effects SDK
# Download from: https://adobe.io/after-effects
set(AE_SDK_PATH "${CMAKE_SOURCE_DIR}/include/AE_SDK" CACHE PATH "Path to After Effects SDK")

if(NOT EXISTS "${AE_SDK_PATH}")
    message(WARNING "
    =========================================================================
    After Effects SDK not found at: ${AE_SDK_PATH}
    
    Please download the SDK from: https://adobe.io/after-effects
    and extract it to: ${CMAKE_SOURCE_DIR}/include/AE_SDK
    
    Expected structure:
    ${CMAKE_SOURCE_DIR}/include/AE_SDK/
        ├── AE_Effect.h
        ├── AE_EffectCB.h
        ├── AEGP_SuiteHandler.h
        └── ...
    =========================================================================
    ")
endif()

# ============================================================================
# macOS Bundle Configuration
# ============================================================================
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build for both Intel and Apple Silicon")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
endif()

# ============================================================================
# Source Files - Modular Structure
# ============================================================================
set(PLUGIN_SOURCES
    # Core modules
    src/core/SnapPlugin.cpp
    src/core/KeyboardMonitor.cpp
    src/core/CEPBridge.cpp
    # Grid module
    src/modules/grid/GridUI.cpp
    # Control module
    src/modules/control/ControlUI.cpp
    # SDK
    ${AE_SDK_PATH}/Util/AEGP_SuiteHandler.cpp
)

set(PLUGIN_HEADERS
    # Core modules
    src/core/SnapPlugin.h
    src/core/KeyboardMonitor.h
    src/core/CEPBridge.h
    src/core/GdiPlusIncludes.h
    # Grid module
    src/modules/grid/GridUI.h
    # Control module
    src/modules/control/ControlUI.h
)

# ============================================================================
# Plugin Target
# ============================================================================
add_library(${PROJECT_NAME} MODULE ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/modules/grid
    ${CMAKE_SOURCE_DIR}/src/modules/control
    ${AE_SDK_PATH}
    ${AE_SDK_PATH}/SP
    ${AE_SDK_PATH}/Util
)

# ============================================================================
# macOS Framework Linking
# ============================================================================
if(APPLE)
    find_library(CARBON_FRAMEWORK Carbon REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${CARBON_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${COREGRAPHICS_FRAMEWORK}
    )
    
    # Bundle configuration for .plugin
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "plugin"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugin"
    )
endif()

# ============================================================================
# Windows Configuration
# ============================================================================
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        MSWindows
        _WINDOWS
        WIN32
    )
    
    # Add resource file (PiPL) and Module Definition file (.def)
    target_sources(${PROJECT_NAME} PRIVATE
        resources/win/AnchorSnap.rc
        resources/win/AnchorSnap.def
    )
    
    # AE Plugins need to be linked with specific flags on Windows
    # They are essentially DLLs renamed to .aex
    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".aex"
        PREFIX ""
    )
endif()

# ============================================================================
# Compiler Flags
# ============================================================================
target_compile_definitions(${PROJECT_NAME} PRIVATE
    # macOS specific
    $<$<PLATFORM_ID:Darwin>:MAC_ENV>
)

if(APPLE)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang>:-Wall -Wextra>
    )
endif()
