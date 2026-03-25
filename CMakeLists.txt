cmake_minimum_required(VERSION 3.20)
project(LanAudioBridge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── PortAudio ──────────────────────────────────────────────────────────────────
set(PORTAUDIO_ROOT "" CACHE PATH "Path to PortAudio root (contains include/ and lib/)")
if(NOT PORTAUDIO_ROOT)
    message(FATAL_ERROR "Set PORTAUDIO_ROOT. Example: cmake -DPORTAUDIO_ROOT=C:/libs/portaudio ..")
endif()

find_path(PA_INCLUDE_DIR portaudio.h
    HINTS "${PORTAUDIO_ROOT}/include"
          "${PORTAUDIO_ROOT}/../include"
    REQUIRED)
find_library(PA_LIB portaudio
    HINTS "${PORTAUDIO_ROOT}/lib"
          "${PORTAUDIO_ROOT}/Release"
          "${PORTAUDIO_ROOT}/build"
          "${PORTAUDIO_ROOT}/build/Release"
    REQUIRED)

# ── ImGui ──────────────────────────────────────────────────────────────────────
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

# ── App sources ────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/ui/gui.cpp
    src/network/udp_receiver.cpp
    src/audio/wasapi_capture.cpp
    src/audio/portaudio_output.cpp
    src/mixer/mixer.cpp
)

add_executable(lan_audio_bridge ${SOURCES} ${IMGUI_SOURCES})

target_include_directories(lan_audio_bridge PRIVATE
    ${PA_INCLUDE_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    src/
)

target_link_libraries(lan_audio_bridge PRIVATE
    ${PA_LIB}
    Ws2_32
    Ole32
    Mmdevapi
    d3d11
    dxgi
)

target_compile_definitions(lan_audio_bridge PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
)

if(MSVC)
    target_compile_options(lan_audio_bridge PRIVATE /W3 /utf-8)
else()
    target_compile_options(lan_audio_bridge PRIVATE -Wall -Wextra)
endif()

# Find portaudio.dll and copy it to the output directory post-build
get_filename_component(PA_LIB_DIR "${PA_LIB}" DIRECTORY)
find_file(PA_DLL "portaudio.dll" HINTS "${PA_LIB_DIR}" "${PORTAUDIO_ROOT}/Release" "${PORTAUDIO_ROOT}/build/Release")
if(PA_DLL)
    add_custom_command(TARGET lan_audio_bridge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PA_DLL}"
        "$<TARGET_FILE_DIR:lan_audio_bridge>"
    )
endif()
