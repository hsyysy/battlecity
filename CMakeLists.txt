cmake_minimum_required(VERSION 3.16)
project(battlecity VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3>=3.2.0)
pkg_check_modules(GME REQUIRED IMPORTED_TARGET libgme>=0.6.3)
option(ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes)
    if(ENABLE_SANITIZERS)
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# The ROM and all data exported from it are intentionally not in the
# repository.  BATTLECITY_ROM may point anywhere; when it is set the build
# invokes tools/export_all.py once to create the generated headers.
set(BATTLECITY_ROM "$ENV{BATTLECITY_ROM}" CACHE FILEPATH
    "Path to a user-supplied BattleCity.nes")
if(NOT BATTLECITY_ROM AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/BattleCity.nes")
    set(BATTLECITY_ROM "${CMAKE_CURRENT_SOURCE_DIR}/BattleCity.nes" CACHE FILEPATH
        "Path to a user-supplied BattleCity.nes" FORCE)
endif()
set(BATTLECITY_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated" CACHE PATH
    "Directory for headers generated from BATTLECITY_ROM")
set(BATTLECITY_FCEUX "fceux" CACHE FILEPATH
    "FCEUX executable used to capture nametables for the exported headers")
option(BATTLECITY_AUTO_EXPORT "Run tools/export_all.py during the build when BATTLECITY_ROM is set" ON)

set(GENERATED_HEADERS
    "${BATTLECITY_GENERATED_DIR}/stages.h"
    "${BATTLECITY_GENERATED_DIR}/stage1.h"
    "${BATTLECITY_GENERATED_DIR}/result_screen.h"
    "${BATTLECITY_GENERATED_DIR}/gameover_screen.h")

if(BATTLECITY_AUTO_EXPORT AND BATTLECITY_ROM AND EXISTS "${BATTLECITY_ROM}")
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_custom_command(OUTPUT ${GENERATED_HEADERS}
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/export_all.py"
                    --rom "${BATTLECITY_ROM}"
                    --out "${BATTLECITY_GENERATED_DIR}"
                    --fceux "${BATTLECITY_FCEUX}"
            DEPENDS "${BATTLECITY_ROM}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/export_all.py"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/export_stages.py"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/export_reference.py"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/export_result.py"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/export_gameover.py"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/capture_reference.lua"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/capture_result.lua"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/capture_gameover.lua"
            COMMENT "Exporting Battle City headers from ${BATTLECITY_ROM}"
            VERBATIM)
        add_custom_target(battlecity_assets DEPENDS ${GENERATED_HEADERS})
    endif()
endif()
if(NOT TARGET battlecity_assets)
    foreach(header ${GENERATED_HEADERS})
        if(NOT EXISTS "${header}")
            message(FATAL_ERROR
                "Missing ${header}.\n"
                "The repository does not ship the ROM or exported assets.  Place your "
                "legally obtained BattleCity.nes in the source tree, or pass "
                "-DBATTLECITY_ROM=/path/to/BattleCity.nes, then run:\n"
                "  python3 tools/export_all.py --rom /path/to/BattleCity.nes --out build/generated")
        endif()
    endforeach()
endif()

add_library(battlecity_core src/game.c src/rom.c src/render.c)
add_library(battlecity_sound src/sound.c)
target_include_directories(battlecity_sound PUBLIC src)
target_link_libraries(battlecity_sound PUBLIC PkgConfig::GME)
target_include_directories(battlecity_core PUBLIC src "${BATTLECITY_GENERATED_DIR}")
if(TARGET battlecity_assets)
    add_dependencies(battlecity_core battlecity_assets)
endif()
if(UNIX)
    target_link_libraries(battlecity_core PUBLIC m)
endif()
add_executable(battlecity src/main.c)
target_link_libraries(battlecity PRIVATE battlecity_core battlecity_sound PkgConfig::SDL3)
if(BATTLECITY_ROM)
    set(DEFAULT_ROM_PATH "${BATTLECITY_ROM}")
else()
    set(DEFAULT_ROM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/BattleCity.nes")
endif()
target_compile_definitions(battlecity PRIVATE DEFAULT_ROM_PATH="${DEFAULT_ROM_PATH}")

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter)
    add_executable(test_core tests/test_core.c)
    target_link_libraries(test_core PRIVATE battlecity_core)
    target_compile_definitions(test_core PRIVATE TEST_ROM_PATH="${DEFAULT_ROM_PATH}")
    add_executable(test_sound tests/test_sound.c)
    target_link_libraries(test_sound PRIVATE battlecity_core battlecity_sound PkgConfig::SDL3)
    target_compile_definitions(test_sound PRIVATE TEST_ROM_PATH="${DEFAULT_ROM_PATH}")
    if(Python3_Interpreter_FOUND)
        add_executable(render_reference tests/render_reference.c)
        target_link_libraries(render_reference PRIVATE battlecity_core)
        target_compile_definitions(render_reference PRIVATE TEST_ROM_PATH="${DEFAULT_ROM_PATH}")
    endif()
    if(EXISTS "${DEFAULT_ROM_PATH}")
        add_test(NAME core COMMAND test_core)
        add_test(NAME sound COMMAND test_sound "${CMAKE_CURRENT_BINARY_DIR}/sound-preview.wav")
        add_test(NAME sdl3_smoke COMMAND battlecity --rom "${DEFAULT_ROM_PATH}" --headless --demo --frames 1200
                 --screenshot "${CMAKE_CURRENT_BINARY_DIR}/smoke.bmp"
                 --audio-out "${CMAKE_CURRENT_BINARY_DIR}/smoke.wav")
        if(Python3_Interpreter_FOUND)
            set(BATTLECITY_REFERENCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/reference" CACHE PATH
                "Directory containing regenerated FCEUX reference PNGs")
            option(BATTLECITY_REFERENCE_TESTS "Enable pixel comparisons against regenerated references" OFF)
            if(BATTLECITY_REFERENCE_TESTS AND EXISTS "${BATTLECITY_REFERENCE_DIR}/title.png")
                add_test(NAME reference_pixels COMMAND ${Python3_EXECUTABLE}
                         "${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_reference.py"
                         --renderer $<TARGET_FILE:render_reference>
                         --reference "${BATTLECITY_REFERENCE_DIR}"
                         --output "${CMAKE_CURRENT_BINARY_DIR}/reference-validation")
            endif()
        endif()
    endif()
endif()
