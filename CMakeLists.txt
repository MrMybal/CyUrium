# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(CyUrium VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# Preserve useful source locations in Release diagnostics without embedding the
# build machine's source directory in __FILE__ strings (including SDL's).
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}" cyurium_source_prefix)
  foreach(language IN ITEMS C CXX)
    add_compile_options("$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:${language}>>:/d1trimfile:${cyurium_source_prefix}\\>")
  endforeach()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
  foreach(language IN ITEMS C CXX)
    add_compile_options("$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:${language}>>:-ffile-prefix-map=${PROJECT_SOURCE_DIR}=.>")
  endforeach()
endif()
if(MSVC)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(CMAKE_C_COMPILER_LAUNCHER "${Python3_EXECUTABLE};${PROJECT_SOURCE_DIR}/tools/msvc_launcher.py")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${Python3_EXECUTABLE};${PROJECT_SOURCE_DIR}/tools/msvc_launcher.py")
  set(CMAKE_CL_SHOWINCLUDES_PREFIX "Note: including file:")
endif()
option(CYURIUM_BUILD_APP "Build the native desktop interface" ON)
include(CTest)

add_library(cyurium_core src/browser_model.cpp src/browser_workspace.cpp src/tab_drag.cpp src/session_store.cpp src/session_protection.cpp)
if(WIN32)
  target_link_libraries(cyurium_core PUBLIC crypt32)
  add_library(cyurium_bridge src/bridge_channel.cpp)
  target_include_directories(cyurium_bridge PUBLIC src third_party/json)
  target_link_libraries(cyurium_bridge PUBLIC ws2_32 bcrypt)
endif()
target_include_directories(cyurium_core PUBLIC src third_party/json)
if(MSVC)
  target_compile_options(cyurium_core PUBLIC /utf-8 /W4)
else()
  target_compile_options(cyurium_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  add_executable(cyurium_core_tests tests/browser_model_tests.cpp)
  target_link_libraries(cyurium_core_tests PRIVATE cyurium_core)
  add_test(NAME browser_state COMMAND cyurium_core_tests)
  add_executable(cyurium_workspace_tests tests/browser_workspace_tests.cpp)
  target_link_libraries(cyurium_workspace_tests PRIVATE cyurium_core)
  add_test(NAME windows_and_tab_drag COMMAND cyurium_workspace_tests)
  add_executable(cyurium_session_tests tests/session_store_tests.cpp)
  target_link_libraries(cyurium_session_tests PRIVATE cyurium_core)
  add_test(NAME session_recovery COMMAND cyurium_session_tests)
  if(WIN32)
    add_executable(cyurium_update_fixture tests/update_fixture.cpp)
    add_executable(cyurium_bridge_tests tests/bridge_channel_tests.cpp)
    target_link_libraries(cyurium_bridge_tests PRIVATE cyurium_bridge)
    add_test(NAME privileged_bridge_security COMMAND cyurium_bridge_tests)
  endif()
endif()

if(CYURIUM_BUILD_APP)
  include(cmake/Dependencies.cmake)
  add_library(cyurium_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp)
  target_include_directories(cyurium_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(cyurium_imgui PUBLIC SDL3::SDL3)

  add_library(cyurium_cyui STATIC third_party/cyui/src/Theme.cpp third_party/cyui/src/Widgets.cpp)
  target_include_directories(cyurium_cyui PUBLIC third_party/cyui/include)
  target_link_libraries(cyurium_cyui PUBLIC cyurium_imgui)
  if(MSVC)
    target_compile_options(cyurium_cyui PRIVATE /utf-8)
  endif()
  add_executable(CyUrium MACOSX_BUNDLE src/main.cpp src/browser_ui.cpp src/browser_service_ui.cpp src/browser_assistant_ui.cpp src/browser_theme.cpp src/web_engine.cpp src/browser_sdk.cpp src/updater.cpp)
  target_include_directories(CyUrium PRIVATE third_party/json)
  target_compile_definitions(CyUrium PRIVATE CYURIUM_VERSION="${PROJECT_VERSION}")
  target_link_libraries(CyUrium PRIVATE cyurium_core cyurium_cyui)
  if(BUILD_TESTING)
    add_executable(cyurium_ui_tests tests/browser_ui_tests.cpp src/browser_ui.cpp src/browser_service_ui.cpp src/browser_assistant_ui.cpp src/browser_theme.cpp)
    target_compile_definitions(cyurium_ui_tests PRIVATE CYURIUM_VERSION="${PROJECT_VERSION}")
    target_link_libraries(cyurium_ui_tests PRIVATE cyurium_core cyurium_cyui)
    add_test(NAME interface_interactions COMMAND cyurium_ui_tests)
  endif()
  if(WIN32)
    target_link_libraries(CyUrium PRIVATE cyurium_bridge)
    target_link_libraries(CyUrium PRIVATE psapi ws2_32 bcrypt)
    configure_file(src/CyUrium.rc.in "${CMAKE_CURRENT_BINARY_DIR}/CyUrium.rc" @ONLY)
    target_sources(CyUrium PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/CyUrium.rc")
  endif()
  set_target_properties(CyUrium PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.cyurium.desktop"
    MACOSX_BUNDLE_BUNDLE_NAME "CyUrium"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
  add_custom_command(TARGET CyUrium POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CyUrium>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf" "$<TARGET_FILE_DIR:CyUrium>/assets/Roboto-Medium.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${PROJECT_SOURCE_DIR}/assets/Roboto-LICENSE.txt" "$<TARGET_FILE_DIR:CyUrium>/assets/Roboto-LICENSE.txt")
  configure_file(assets/update-channel.json "${CMAKE_CURRENT_BINARY_DIR}/assets/update-channel.json" COPYONLY)
  add_custom_command(TARGET CyUrium POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/assets/branding" "$<TARGET_FILE_DIR:CyUrium>/assets/branding")
  add_custom_command(TARGET CyUrium POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CyUrium>/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${imgui_SOURCE_DIR}/LICENSE.txt" "$<TARGET_FILE_DIR:CyUrium>/licenses/Dear-ImGui.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${sdl3_SOURCE_DIR}/LICENSE.txt" "$<TARGET_FILE_DIR:CyUrium>/licenses/SDL3.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/third_party/cyui/LICENSE" "$<TARGET_FILE_DIR:CyUrium>/licenses/CyUI.txt")
  add_custom_command(TARGET CyUrium POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/third_party/json/LICENSE.MIT" "$<TARGET_FILE_DIR:CyUrium>/licenses/nlohmann-json.txt")
  foreach(notice IN ITEMS LICENSE NOTICE THIRD_PARTY_NOTICES.md)
    add_custom_command(TARGET CyUrium POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/${notice}" "$<TARGET_FILE_DIR:CyUrium>/${notice}")
  endforeach()
endif()

