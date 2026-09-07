cmake_minimum_required(VERSION 3.20)
project(language3d_pc_windows LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(GENERATED_ASSET_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated_assets.cpp)
add_custom_command(
    OUTPUT ${GENERATED_ASSET_SRC}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/embed_assets.py
            ${CMAKE_CURRENT_SOURCE_DIR}/../assets/map.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/../assets/textures.raw
            ${GENERATED_ASSET_SRC}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/../assets/map.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/../assets/textures.raw
        ${CMAKE_CURRENT_SOURCE_DIR}/../tools/embed_assets.py
    VERBATIM)
find_package(SDL2 CONFIG QUIET)
if(TARGET SDL2::SDL2)
    set(SDL2_TARGET SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    set(SDL2_TARGET SDL2::SDL2-static)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    set(SDL2_TARGET PkgConfig::SDL2)
endif()
add_executable(language3d_mvp
    ../engine/game.cpp ../engine/math.cpp ../engine/renderer.cpp ../engine/assets.cpp ../engine/api.cpp
    ../platform/sdl2/main.cpp ${GENERATED_ASSET_SRC})
target_include_directories(language3d_mvp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(language3d_mvp PRIVATE L3D_PC_PROFILE=1)
target_compile_options(language3d_mvp PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(language3d_mvp PRIVATE ${SDL2_TARGET})
if(TARGET SDL2::SDL2main)
    target_link_libraries(language3d_mvp PRIVATE SDL2::SDL2main)
endif()
if(MINGW)
    target_link_options(language3d_mvp PRIVATE -mconsole)
endif()
set_target_properties(language3d_mvp PROPERTIES OUTPUT_NAME "language3d_mvp")
