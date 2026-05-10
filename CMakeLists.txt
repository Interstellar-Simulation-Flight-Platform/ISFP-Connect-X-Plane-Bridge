# ISFP Connect X-Plane Plugin
# CMake build configuration for X-Plane 11/12 plugin

cmake_minimum_required(VERSION 3.15)
project(ISFPConnectBridge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# X-Plane SDK path
set(XPLANE_SDK_ROOT "${CMAKE_SOURCE_DIR}/XPSDK430/SDK")

# SDK include directories
include_directories(
    ${XPLANE_SDK_ROOT}/CHeaders/XPLM
    ${XPLANE_SDK_ROOT}/CHeaders/Widgets
    ${CMAKE_SOURCE_DIR}/include
)

# SDK library directories
link_directories(${XPLANE_SDK_ROOT}/Libraries/Win)

# Source files
set(SOURCES
    src/main.cpp
    src/networkmanager.cpp
    src/simulatordatamanager.cpp
)

# Create shared library (X-Plane plugin)
add_library(ISFPConnectBridge SHARED ${SOURCES})

# Link libraries
target_link_libraries(ISFPConnectBridge
    XPLM_64
    ws2_32  # Windows socket library
)

# Remove "lib" prefix from output name
set_target_properties(ISFPConnectBridge PROPERTIES PREFIX "")

# Set output name with .xpl extension (X-Plane plugin format)
set_target_properties(ISFPConnectBridge PROPERTIES OUTPUT_NAME "ISFPConnectBridge")
set_target_properties(ISFPConnectBridge PROPERTIES SUFFIX ".xpl")

# Compiler flags for Windows
if(WIN32)
    target_compile_definitions(ISFPConnectBridge PRIVATE
        -DIBM=1
        -DWIN32
        -D_WINDOWS
        -DXPLM200
        -DXPLM210
        -DXPLM300
        -DXPLM301
    )
    
    # Disable warnings that are common in X-Plane SDK
    target_compile_options(ISFPConnectBridge PRIVATE
        /W3
        /wd4996  # Disable deprecation warnings
        /wd4267  # Disable size_t conversion warnings
    )
endif()

# Install target
install(TARGETS ISFPConnectBridge
    RUNTIME DESTINATION "${CMAKE_SOURCE_DIR}/../Resources/plugins/ISFPConnectBridge/win_x64"
)

# Print configuration message
message(STATUS "ISFP Connect Plugin Configuration:")
message(STATUS "  X-Plane SDK: ${XPLANE_SDK_ROOT}")
message(STATUS "  Output: ${CMAKE_BINARY_DIR}/ISFPConnectBridge.dll")
message(STATUS "  Install: Resources/plugins/ISFPConnectBridge/win_x64/")
