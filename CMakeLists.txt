# Language3D — portable cross-platform build.
#
# Host (Linux / Windows / macOS SDL2 game + test suite):
#   cmake -S . -B build -DLANGUAGE3D_PLATFORM=host
#   cmake --build build
#   ctest --test-dir build --output-on-failure
#
# RP2040 firmware (requires the Raspberry Pi Pico SDK):
#   cmake -S . -B build-rp2040 -DLANGUAGE3D_PLATFORM=rp2040 \
#       -DPICO_SDK_PATH=$HOME/pico-sdk -DPICO_BOARD=pico
#   cmake --build build-rp2040
#
# Or simply use the matching preset from CMakePresets.json.

cmake_minimum_required(VERSION 3.20)

# Single source of truth for the project version: the VERSION file.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" LANGUAGE3D_VERSION_RAW)
string(STRIP "${LANGUAGE3D_VERSION_RAW}" LANGUAGE3D_VERSION)

set(LANGUAGE3D_PLATFORM "host" CACHE STRING "Target platform: host or rp2040")
set_property(CACHE LANGUAGE3D_PLATFORM PROPERTY STRINGS host rp2040)

if(LANGUAGE3D_PLATFORM STREQUAL "rp2040")
  # pico_sdk_import.cmake must be included PRIOR to project() so the SDK can
  # select the ARM cross toolchain before compiler detection. The SDK picks
  # the toolchain itself, so never force CMAKE_C/CXX_COMPILER for this build.
  if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
  endif()
  if(NOT PICO_SDK_PATH)
    message(FATAL_ERROR
      "PICO_SDK_PATH is not set. Pass -DPICO_SDK_PATH=<path> or export it, e.g. "
      "export PICO_SDK_PATH=$HOME/pico-sdk")
  endif()
  if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico CACHE STRING "Pico board")
  endif()
  include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
  project(language3d VERSION ${LANGUAGE3D_VERSION} LANGUAGES C CXX ASM)
  pico_sdk_init()
else()
  project(language3d VERSION ${LANGUAGE3D_VERSION} LANGUAGES C CXX)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LANGUAGE3D_BUILD_TESTS "Build the host automated test suite" ON)

# Portable warning set (MSVC uses /W3; GCC/Clang use -Wall -Wextra -Wpedantic).
if(MSVC)
  set(LANGUAGE3D_WARNING_FLAGS /W3)
else()
  set(LANGUAGE3D_WARNING_FLAGS -Wall -Wextra -Wpedantic)
endif()

if(LANGUAGE3D_PLATFORM STREQUAL "rp2040")

  # ---------------------------------------------------------------- RP2040 --
  # Production RP2040/ST7789 240x240 game firmware. The low-level display
  # driver (platform/rp2040) is intentionally kept as-is; this branch only
  # wires it into the portable build.

  find_package(Python3 COMPONENTS Interpreter REQUIRED)

  set(GENERATED_ASSET_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated_assets.cpp)
  add_custom_command(
    OUTPUT ${GENERATED_ASSET_SRC}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_assets.py
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/map.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures.raw
            ${GENERATED_ASSET_SRC}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/map.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures.raw
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_assets.py
    VERBATIM
  )
  add_custom_target(language3d_assets DEPENDS ${GENERATED_ASSET_SRC})

  add_executable(language3d_rp2040
    platform/rp2040/src/main.cpp
    platform/rp2040/src/display.cpp
    platform/rp2040/src/platform_rp2040.cpp
    engine/game.cpp
    engine/math.cpp
    engine/renderer.cpp
    engine/assets.cpp
    engine/api.cpp
    engine/api.cpp
    engine/world3.cpp
    engine/world3.cpp
    engine/entity.cpp
    engine/entity.cpp
    engine/npc.cpp
    engine/npc.cpp
    engine/schedule.cpp
    engine/npc_dispatch.cpp
    engine/interact.cpp
    engine/dialogue.cpp
    engine/item.cpp
    engine/player.cpp
    engine/quest.cpp
    engine/dialogue_quest.cpp
    engine/language.cpp
    ${GENERATED_ASSET_SRC}
  )
  add_dependencies(language3d_rp2040 language3d_assets)

  target_include_directories(language3d_rp2040 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/api
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/rp2040/include
    ${CMAKE_CURRENT_BINARY_DIR}
  )

  target_compile_definitions(language3d_rp2040 PRIVATE L3D_RP2040_PROFILE=1)
  target_compile_options(language3d_rp2040 PRIVATE
    -Wall -Wextra -Wpedantic
    -ffunction-sections -fdata-sections
  )
  target_link_options(language3d_rp2040 PRIVATE -Wl,--gc-sections)
  target_link_libraries(language3d_rp2040 PRIVATE pico_stdlib hardware_spi)

  pico_enable_stdio_usb(language3d_rp2040 1)
  pico_enable_stdio_uart(language3d_rp2040 0)
  pico_add_extra_outputs(language3d_rp2040)

else()

  # ------------------------------------------------------------------ Host --
  # Desktop SDL2 build (Linux / Windows / macOS). Shares the exact same
  # platform-independent engine sources as the RP2040 firmware.
  if(MSVC)
    # Static MSVC runtime (/MT): the Windows game links the C/C++ runtime
    # into the binary, so the release runs on clean Windows installs with
    # no VC Redistributable required. SDL2 itself is consumed as a static
    # library (vcpkg x64-windows-static triplet), therefore no DLL in the
    # dependency chain needs the dynamic CRT either. Requires CMP0091 NEW,
    # which is the default with this cmake_minimum_required.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
  set(LANGUAGE3D_ENGINE_SOURCES
    engine/game.cpp
    engine/math.cpp
    engine/renderer.cpp
    engine/assets.cpp
    engine/api.cpp
    engine/world3.cpp
    engine/entity.cpp
    engine/npc.cpp
    engine/schedule.cpp
    engine/npc_dispatch.cpp
    engine/interact.cpp
    engine/dialogue.cpp
    engine/item.cpp
    engine/player.cpp
    engine/quest.cpp
    engine/dialogue_quest.cpp
    engine/language.cpp
  )

  find_package(Python3 COMPONENTS Interpreter REQUIRED)

  # Build-time generated L3DP asset pack (assets/map.txt + textures.raw).
  set(GENERATED_ASSET_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated_assets.cpp)
  add_custom_command(
    OUTPUT ${GENERATED_ASSET_SRC}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_assets.py
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/map.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures.raw
            ${GENERATED_ASSET_SRC}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/map.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures.raw
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_assets.py
    VERBATIM
  )
  add_custom_target(language3d_assets DEPENDS ${GENERATED_ASSET_SRC})

  # Build-time dialogue content (content/dialogues/*.dlg -> C++ tables).
  # Validation failures break the build on every platform by construction.
  file(GLOB LANGUAGE3D_DIALOGUE_SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/content/dialogues/*.dlg)
  set(GENERATED_DIALOGUE_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated_dialogue.cpp)
  add_custom_command(
    OUTPUT ${GENERATED_DIALOGUE_SRC}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_dialogue.py
            ${CMAKE_CURRENT_SOURCE_DIR}/content/dialogues
            ${GENERATED_DIALOGUE_SRC}
    DEPENDS
        ${LANGUAGE3D_DIALOGUE_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_dialogue.py
    VERBATIM
  )
  add_custom_target(language3d_dialogues DEPENDS ${GENERATED_DIALOGUE_SRC})

  # Build-time item content (content/items/*.item -> C++ tables).
  file(GLOB LANGUAGE3D_ITEM_SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/content/items/*.item)
  set(GENERATED_ITEM_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated_items.cpp)
  add_custom_command(
    OUTPUT ${GENERATED_ITEM_SRC}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_items.py
            ${CMAKE_CURRENT_SOURCE_DIR}/content/items
            ${GENERATED_ITEM_SRC}
    DEPENDS
        ${LANGUAGE3D_ITEM_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_items.py
    VERBATIM
  )
  add_custom_target(language3d_items DEPENDS ${GENERATED_ITEM_SRC})

  # Build-time quest content (content/quests/*.quest -> C++ tables).
  file(GLOB LANGUAGE3D_QUEST_SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/content/quests/*.quest)
  set(GENERATED_QUEST_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated_quests.cpp)
  add_custom_command(
    OUTPUT ${GENERATED_QUEST_SRC}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_quests.py
            ${CMAKE_CURRENT_SOURCE_DIR}/content/quests
            ${GENERATED_QUEST_SRC}
    DEPENDS
        ${LANGUAGE3D_QUEST_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_quests.py
    VERBATIM
  )
  add_custom_target(language3d_quests DEPENDS ${GENERATED_QUEST_SRC})

  # Build-time vocabulary content (content/vocabulary/*.vocab -> C++ tables).
  file(GLOB LANGUAGE3D_VOCAB_SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/content/vocabulary/*.vocab)
  set(GENERATED_VOCAB_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated_vocab.cpp)
  add_custom_command(
    OUTPUT ${GENERATED_VOCAB_SRC}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_vocab.py
            ${CMAKE_CURRENT_SOURCE_DIR}/content/vocabulary
            ${GENERATED_VOCAB_SRC}
    DEPENDS
        ${LANGUAGE3D_VOCAB_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_vocab.py
    VERBATIM
  )
  add_custom_target(language3d_vocab DEPENDS ${GENERATED_VOCAB_SRC})

  add_library(language3d_core STATIC ${LANGUAGE3D_ENGINE_SOURCES})
  target_include_directories(language3d_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
  )
  # NOTE: no L3D_*_PROFILE define here. The display profile is a
  # per-binary property (exactly like the legacy Makefile, which compiles
  # tests without any profile define and the game with L3D_PC_PROFILE):
  # every test binary links this default-profile core, while the game
  # target below recompiles the same engine sources with L3D_PC_PROFILE.
  # Mixing profiles inside one binary would violate one-definition rules
  # for the Config/MemoryBudget traits, so the game intentionally does
  # NOT link language3d_core.
  target_compile_options(language3d_core PRIVATE ${LANGUAGE3D_WARNING_FLAGS})

  # SDL2 discovery: vcpkg CONFIG packages (Windows CI) first, then pkg-config
  # (Linux CI / developer machines).
  find_package(SDL2 CONFIG QUIET)
  if(TARGET SDL2::SDL2)
    set(LANGUAGE3D_SDL2_TARGET SDL2::SDL2)
  elseif(TARGET SDL2::SDL2-static)
    set(LANGUAGE3D_SDL2_TARGET SDL2::SDL2-static)
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    set(LANGUAGE3D_SDL2_TARGET PkgConfig::SDL2)
  endif()

  add_executable(language3d
    platform/sdl2/main.cpp
    platform/sdl2/platform_sdl2.cpp
    ${LANGUAGE3D_ENGINE_SOURCES}
    ${GENERATED_ASSET_SRC}
  )
  add_dependencies(language3d language3d_assets)
  target_include_directories(language3d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/api
    ${CMAKE_CURRENT_BINARY_DIR}
  )
  target_compile_definitions(language3d PRIVATE L3D_PC_PROFILE=1)
  target_compile_options(language3d PRIVATE ${LANGUAGE3D_WARNING_FLAGS})
  # SDL2main provides the Windows WinMain shim and forwards to SDL_main
  # (which SDL.h maps from main()). It is a no-op on pkg-config platforms
  # where the SDL2main target does not exist.
  target_link_libraries(language3d PRIVATE ${LANGUAGE3D_SDL2_TARGET})
  if(TARGET SDL2::SDL2main)
    target_link_libraries(language3d PRIVATE SDL2::SDL2main)
  endif()
  if(MINGW)
    target_link_options(language3d PRIVATE -mconsole)
  endif()
  if(WIN32)
    set_target_properties(language3d PROPERTIES OUTPUT_NAME "language3d")
  endif()

  # Install layout reused by the CI packaging jobs.
  install(TARGETS language3d RUNTIME DESTINATION .)
  install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/map.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures.raw
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/demo.l3dp
    DESTINATION assets)
  install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    DESTINATION .)

  if(LANGUAGE3D_BUILD_TESTS)
    include(CTest)
    enable_testing()

    function(language3d_add_test name source)
      add_executable(${name} tests/${source} ${ARGN})
      target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/engine
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_BINARY_DIR}
      )
      target_compile_options(${name} PRIVATE ${LANGUAGE3D_WARNING_FLAGS})
      target_link_libraries(${name} PRIVATE language3d_core)
      add_test(NAME ${name} COMMAND ${name})
    endfunction()

    language3d_add_test(core_test core_test.cpp)
    language3d_add_test(renderer_test renderer_test.cpp)
    language3d_add_test(asset_test asset_test.cpp)
    language3d_add_test(generated_assets_test generated_assets_test.cpp ${GENERATED_ASSET_SRC})
    add_dependencies(generated_assets_test language3d_assets)
    language3d_add_test(input_test input_test.cpp)
    language3d_add_test(v10_test v10_test.cpp)
    language3d_add_test(v11_test v11_test.cpp)
    language3d_add_test(v13_test v13_test.cpp)
    language3d_add_test(v14_test v14_test.cpp)
    language3d_add_test(v15_test v15_test.cpp)
    language3d_add_test(v16_test v16_test.cpp)
    language3d_add_test(v17_test v17_test.cpp)
    language3d_add_test(v18_test v18_test.cpp)
    language3d_add_test(v21_visual_test v21_visual_test.cpp)
    language3d_add_test(v23_input_test v23_input_test.cpp)
    language3d_add_test(v24_camera_alignment_test v24_camera_alignment_test.cpp)
    language3d_add_test(world_test world_test.cpp)
    language3d_add_test(world3_test world3_test.cpp)
    language3d_add_test(entity_test entity_test.cpp)
    language3d_add_test(npc_test npc_test.cpp)
    language3d_add_test(schedule_test schedule_test.cpp)
    language3d_add_test(npc_dispatch_test npc_dispatch_test.cpp)
    language3d_add_test(interact_test interact_test.cpp)
    language3d_add_test(dialogue_test dialogue_test.cpp ${GENERATED_DIALOGUE_SRC})
    add_dependencies(dialogue_test language3d_dialogues)
    language3d_add_test(item_test item_test.cpp ${GENERATED_ITEM_SRC})
    add_dependencies(item_test language3d_items)
    language3d_add_test(player_test player_test.cpp)
    language3d_add_test(quest_test quest_test.cpp ${GENERATED_QUEST_SRC})
    add_dependencies(quest_test language3d_quests)
    language3d_add_test(dialogue_quest_test dialogue_quest_test.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/engine/dialogue_quest.cpp)
    language3d_add_test(playable_quest_test playable_quest_test.cpp
      ${GENERATED_DIALOGUE_SRC} ${GENERATED_QUEST_SRC} ${GENERATED_ITEM_SRC})
    add_dependencies(playable_quest_test
      language3d_dialogues language3d_quests language3d_items)
    language3d_add_test(language_test language_test.cpp ${GENERATED_VOCAB_SRC})
    add_dependencies(language_test language3d_vocab)
    language3d_add_test(language_scenario_test language_scenario_test.cpp
      ${GENERATED_DIALOGUE_SRC} ${GENERATED_QUEST_SRC} ${GENERATED_ITEM_SRC}
      ${GENERATED_VOCAB_SRC})
    add_dependencies(language_scenario_test
      language3d_dialogues language3d_quests language3d_items
      language3d_vocab)
    language3d_add_test(platform_test platform_test.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/platform/null/platform_null.cpp)
    target_include_directories(platform_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/platform/api
      ${CMAKE_CURRENT_SOURCE_DIR}/platform/null
    )

    # Map connectivity guard (gameplay start must reach the NPC target).
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME map-path-test
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_map_path.py
              ${CMAKE_CURRENT_SOURCE_DIR}/assets/map.txt 4 4 18 18)

    # NOTE: tests/display_test.cpp is intentionally not built. It targets the
    # retired RP2350 display/transport headers (rp2350_display.h,
    # rp2350_transport.h) which no longer exist in the tree, and it was
    # already excluded from `make test-all`. The RP2040 display path is
    # covered by firmware compilation plus on-hardware testing instead.
  endif()

endif()
