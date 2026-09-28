cmake_minimum_required(VERSION 3.16)

option(ICYTOWER_VITA "Build the PS Vita VPK" OFF)
if(ICYTOWER_VITA AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain")
endif()

project(icytower C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
add_executable(icytower ${SOURCES})
target_include_directories(icytower PRIVATE src third_party third_party/stb)
# -ffp-contract=off: no fused multiply-add, the engine must round like the
# original so replays stay in sync.
target_compile_options(icytower PRIVATE -Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -Wno-format-truncation
  -O2 -ffp-contract=off)

if(VITA)
  include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
  set(VITA_APP_NAME "Icy Tower")
  set(VITA_TITLEID "ICYT00131")
  set(VITA_VERSION "01.00")

  target_link_options(icytower PRIVATE -Wl,-T,${CMAKE_SOURCE_DIR}/sce_gap.ld)
  target_link_libraries(icytower
    vita2d SDL2 vitaGL vitashark SceShaccCg_stub_weak mathneon SceShaccCgExt taihen_stub
    SceKernelDmacMgr_stub SceIme_stub SceGxm_stub SceDisplay_stub SceCtrl_stub
    SceAppMgr_stub SceAppUtil_stub SceAudio_stub SceAudioIn_stub SceSysmodule_stub
    SceIofilemgr_stub SceCommonDialog_stub SceTouch_stub SceHid_stub SceMotion_stub
    ScePower_stub SceProcessmgr_stub stdc++ pthread m)

  vita_create_self(eboot.bin icytower UNSAFE)
  vita_create_vpk(icytower.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
    FILE ${CMAKE_SOURCE_DIR}/sce_sys/icon0.png sce_sys/icon0.png
    FILE ${CMAKE_SOURCE_DIR}/sce_sys/pic0.png sce_sys/pic0.png
    FILE ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
    FILE ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
    FILE ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml)
else()
  find_package(SDL2 REQUIRED)
  target_link_libraries(icytower SDL2::SDL2 m)
  add_executable(replay_test tests/replay_test.c src/replay.c src/core.c)
  target_include_directories(replay_test PRIVATE src)
  target_compile_options(replay_test PRIVATE -O2 -ffp-contract=off)
endif()
