cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(VST3_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/vst3sdk" CACHE PATH "Path to Steinberg vst3sdk checkout")
option(NDX_LEGACY_INITMODULE "Define InitModule/DeinitModule (only for VST3 SDK older than 3.7)" OFF)

if(NOT EXISTS "${VST3_SDK_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "VST3 SDK not found at ${VST3_SDK_ROOT}. Run 02_get_sdk.bat first.")
endif()

project(NDX VERSION 1.0.0 LANGUAGES C CXX)

# ---- SDK options: keep the build small, no VSTGUI, no admin-only symlink into the VST3 folder
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_PLUGINS_SAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_HOSTING_SAMPLES ON CACHE BOOL "" FORCE)      # needed for the 'validator' target
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "" FORCE)  # (newer option name)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)            # validation is done by 05_validate.bat

set(vst3sdk_SOURCE_DIR "${VST3_SDK_ROOT}")
add_subdirectory(${VST3_SDK_ROOT} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

set(NDX_SOURCES
    src/ndx_params.h
    src/ndx_dsp.h
    src/ndx_version.h
    src/ndx_ids.h
    src/ndx_processor.h
    src/ndx_processor.cpp
    src/ndx_controller.h
    src/ndx_controller.cpp
    src/ndx_entry.cpp)

if(WIN32)
    list(APPEND NDX_SOURCES resource/ndx.rc)
endif()

smtg_add_vst3plugin(NDX ${NDX_SOURCES})

target_include_directories(NDX PRIVATE src)
target_link_libraries(NDX PRIVATE sdk)
target_compile_features(NDX PRIVATE cxx_std_17)

if(NDX_LEGACY_INITMODULE)
    target_compile_definitions(NDX PRIVATE NDX_LEGACY_INITMODULE=1)
endif()
if(MSVC)
    # Do NOT use /fp:fast - the engine relies on isfinite() guards.
    target_compile_definitions(NDX PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
