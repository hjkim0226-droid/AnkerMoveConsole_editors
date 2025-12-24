cmake_minimum_required(VERSION 3.20)
project(CommandLogger)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDK 경로 설정
set(AE_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/include/AE_SDK")

# 소스 파일
set(SOURCES
    CommandLogger.cpp
)

# Windows 전용
if(WIN32)
    # PiPL 리소스
    set(RESOURCES
        CommandLogger.rc
    )

    # 플러그인 빌드
    add_library(${PROJECT_NAME} SHARED ${SOURCES} ${RESOURCES})

    # SDK 포함 경로
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${AE_SDK_PATH}
        ${AE_SDK_PATH}/SP
        ${AE_SDK_PATH}/Util
    )

    # SDK 유틸리티 소스
    target_sources(${PROJECT_NAME} PRIVATE
        ${AE_SDK_PATH}/Util/AEGP_SuiteHandler.cpp
        ${AE_SDK_PATH}/Util/MissingSuiteError.cpp
    )

    # 링크 라이브러리
    target_link_libraries(${PROJECT_NAME} PRIVATE
        version.lib
    )

    # 출력 설정
    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".aex"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output"
    )

    # 컴파일 옵션
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _WINDOWS
        WIN32
        _CRT_SECURE_NO_WARNINGS
    )
endif()
