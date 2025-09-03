cmake_minimum_required(VERSION 3.15)
project(vis_avs_standalone LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable clang-tidy if available
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-warnings-as-errors=*")
else()
    message(STATUS "clang-tidy not found; skipping semantic lint (install with: brew install llvm)")
endif()

# clang-format (formatting target)
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE PROJECT_CLANG_FORMAT_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/modern/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/modern/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/standalone/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/standalone/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/avs/vis_avs/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/avs/vis_avs/*.cpp
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${PROJECT_CLANG_FORMAT_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Applying clang-format to project sources")
else()
    message(STATUS "clang-format not found; 'format' target will be unavailable (install with: brew install clang-format or brew install llvm)")
endif()

option(AVS_STANDALONE "Build standalone (no Winamp)" ON)

option(AVS_WITH_EEL "Include legacy EEL scripting engine" OFF)
option(AVS_USE_SDL2 "Use SDL2 for window/display in avs_runner" ON)
option(AVS_USE_IMGUI "Enable Dear ImGui UI for editor" ON)

if(WIN32)
    file(GLOB AVS_SOURCES
        avs/vis_avs/*.cpp
        avs/vis_avs/laser/*.cpp
    )
    # Filter out EEL integration source when scripting disabled
    if(NOT AVS_WITH_EEL)
        list(FILTER AVS_SOURCES EXCLUDE REGEX ".*/avs_eelif.cpp$")
    endif()
else()
    # Minimal portable nucleus for non-Windows platforms (plan 1)
    set(AVS_SOURCES
        avs/vis_avs/portable_minimal.cpp
    )
    add_compile_definitions(NO_MMX=1)
endif()

if(AVS_WITH_EEL)
    file(GLOB EEL_SRC avs/vis_avs/evallib/*.c avs/ns-eel/*.c)
    list(APPEND AVS_SOURCES ${EEL_SRC})
    add_compile_definitions(AVS_WITH_EEL=1)
else()
    add_compile_definitions(AVS_WITH_EEL=0)
endif()

add_library(avs_core ${AVS_SOURCES})
target_include_directories(avs_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/avs/vis_avs
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
)
if(AVS_WITH_EEL)
    target_include_directories(avs_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/avs/vis_avs/evallib
        ${CMAKE_CURRENT_SOURCE_DIR}/avs/ns-eel)
endif()

if(APPLE)
    target_compile_definitions(avs_core PRIVATE AVS_APPLE=1)
    # Apple Silicon (ARM) - disable MMX assembly blocks
    target_compile_definitions(avs_core PRIVATE NO_MMX=1)
endif()

add_executable(avs_standalone
    standalone/avs_standalone.cpp)
target_link_libraries(avs_standalone PRIVATE avs_core)

add_executable(avs_runner
    modern/avs_runner.cpp
    modern/fft_analyzer.cpp
    modern/effect_oscstar.cpp
    modern/effect_radial.cpp
    modern/preset_io.cpp)
target_link_libraries(avs_runner PRIVATE avs_core)
target_compile_definitions(avs_runner PRIVATE NO_MMX=1)

if(AVS_USE_SDL2 AND NOT WIN32)
    find_package(SDL2 2.0 QUIET)
    if(SDL2_FOUND)
        target_link_libraries(avs_runner PRIVATE SDL2::SDL2)
        target_compile_definitions(avs_runner PRIVATE AVS_SDL2=1)
        if(AVS_USE_IMGUI)
            include(FetchContent)
            FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.90.9 GIT_SHALLOW TRUE)
            FetchContent_MakeAvailable(imgui)
            # Core sources
            set(IMGUI_CORE_SRCS
                ${imgui_SOURCE_DIR}/imgui.cpp
                ${imgui_SOURCE_DIR}/imgui_draw.cpp
                ${imgui_SOURCE_DIR}/imgui_tables.cpp
                ${imgui_SOURCE_DIR}/imgui_widgets.cpp
                ${imgui_SOURCE_DIR}/imgui_demo.cpp)
            add_library(imgui_core STATIC ${IMGUI_CORE_SRCS})
            target_include_directories(imgui_core PUBLIC ${imgui_SOURCE_DIR})
            # Backend sources (SDL2 + SDLRenderer2)
            set(IMGUI_BACKEND_SRCS
                ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
                ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp)
            add_library(imgui_backend STATIC ${IMGUI_BACKEND_SRCS})
            target_link_libraries(imgui_backend PUBLIC imgui_core SDL2::SDL2)
            target_include_directories(imgui_backend PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
            target_link_libraries(avs_runner PRIVATE imgui_core imgui_backend)
            target_compile_definitions(avs_runner PRIVATE AVS_IMGUI=1)
        endif()
        if(APPLE)
            find_library(ACCELERATE_LIB Accelerate)
            if(ACCELERATE_LIB)
                target_link_libraries(avs_runner PRIVATE ${ACCELERATE_LIB})
                target_compile_definitions(avs_runner PRIVATE AVS_ACCELERATE=1)
            endif()
        endif()
        message(STATUS "SDL2 found: enabling windowed runner")
    else()
        target_compile_definitions(avs_runner PRIVATE AVS_SDL2=0)
        message(WARNING "SDL2 not found; avs_runner will run headless. Install SDL2 (e.g. brew install sdl2) and reconfigure.")
    endif()
else()
    target_compile_definitions(avs_runner PRIVATE AVS_SDL2=0)
endif()

if(APPLE)
    find_library(COCOA_LIB Cocoa)
    if(COCOA_LIB)
        target_link_libraries(avs_standalone PRIVATE ${COCOA_LIB})
    endif()
endif()

message(STATUS "Standalone build configured. This is an early portability scaffold.")
