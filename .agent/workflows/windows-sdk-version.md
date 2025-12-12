---
description: Windows SDK version requirement for After Effects plugin development
---

# Windows SDK Version Requirement

## Required Configuration

- **Target SDK Version:** `10.0.22621.0`
- **IDE / Toolset:** Visual Studio 2022 (v143)
- **Platform Toolset:** v143

## Rationale

1. **Stability**: SDK 10.0.22621.0 is the most stable version for After Effects plugin development
2. **Compatibility**: Best balance between Windows 11 features and Windows 10 backward compatibility
3. **VS2022 Default**: Default recommendation for Visual Studio 2022
4. **GDI+ Compatibility**: Newer SDKs (e.g., 26100) have GDI+ header bugs

## Setup Instructions

### CMakeLists.txt
```cmake
if(WIN32)
    set(CMAKE_SYSTEM_VERSION 10.0.22621.0)
endif()
```

### GitHub Actions Workflow
```yaml
# Install SDK 22621 via PowerShell
- name: Install Windows SDK 22621
  shell: pwsh
  run: |
    $url = "https://go.microsoft.com/fwlink/?linkid=2196241"
    $installer = "$env:TEMP\winsdksetup.exe"
    Invoke-WebRequest -Uri $url -OutFile $installer
    Start-Process -FilePath $installer -ArgumentList "/features OptionId.DesktopCPPx64 /quiet /norestart" -Wait

# Setup MSVC with SDK
- name: Setup MSVC
  uses: ilammy/msvc-dev-cmd@v1
  with:
    arch: x64
    sdk: '10.0.22621.0'
```

### Visual Studio Project (.vcxproj)
```xml
<WindowsTargetPlatformVersion>10.0.22621.0</WindowsTargetPlatformVersion>
```

## Warning

> [!CAUTION]
> Do NOT use SDK 10.0.26100.0 - it has GDI+ header bugs (missing META_*, EMR_*, CALLBACK definitions)
