# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
include(FetchContent)

# Official release archives, pinned by content. Local downloads are optional.
set(imgui_url "https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.92.5-docking")
set(sdl_url "https://codeload.github.com/libsdl-org/SDL/tar.gz/refs/tags/release-3.2.28")
if(EXISTS "${PROJECT_SOURCE_DIR}/.cache/downloads/imgui-1.92.5-docking.tar.gz")
  set(imgui_url "${PROJECT_SOURCE_DIR}/.cache/downloads/imgui-1.92.5-docking.tar.gz")
endif()
if(EXISTS "${PROJECT_SOURCE_DIR}/.cache/downloads/SDL-3.2.28.tar.gz")
  set(sdl_url "${PROJECT_SOURCE_DIR}/.cache/downloads/SDL-3.2.28.tar.gz")
endif()
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3 URL "${sdl_url}"
  URL_HASH SHA256=5c908f07a93087037d3319bc5d820ba8df14ac98bdbdf24af9084ef460ce01b1
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(imgui URL "${imgui_url}"
  URL_HASH SHA256=c816c20e8c75f3e15ae867350e79925502d1a6a85938bb1a73b8927e5f31f9cb
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(SDL3 imgui)

