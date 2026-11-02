cmake_minimum_required(VERSION 3.20)
project(assaultcube C CXX)

set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/../../deps)
set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../../source)

if(NOT EXISTS ${DEPS}/SDL2/CMakeLists.txt)
    message(FATAL_ERROR "Third-party sources missing: run android/fetch-deps.sh first")
endif()

# SDL2 itself is a shared library, loaded by SDLActivity
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(${DEPS}/SDL2 SDL2)
# SDL2_image looks for SDL2::SDL2-static when built static; point it at the shared SDL2
add_library(SDL2::SDL2-static ALIAS SDL2)

# everything else is linked statically into libmain.so
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(SDL2IMAGE_VENDORED OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNG_SAVE OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_JPG_SAVE OFF CACHE BOOL "" FORCE)
foreach(fmt AVIF JXL TIF WEBP SVG XCF XPM XV LBM PCX PNM QOI GIF)
    set(SDL2IMAGE_${fmt} OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory(${DEPS}/SDL2_image SDL2_image)

# gl4es: runs the engine's desktop OpenGL code on top of OpenGL ES 2.
# The context comes from SDL, so no EGL/X11 and no dynamic loader inside gl4es.
set(NOX11 ON CACHE BOOL "" FORCE)
set(NOEGL ON CACHE BOOL "" FORCE)
set(NO_LOADER ON CACHE BOOL "" FORCE)
set(NO_INIT_CONSTRUCTOR ON CACHE BOOL "" FORCE)
set(STATICLIB ON CACHE BOOL "" FORCE)
set(USE_ANDROID_LOG ON CACHE BOOL "" FORCE)
set(DEFAULT_ES 2 CACHE STRING "" FORCE)
add_subdirectory(${DEPS}/gl4es gl4es)
# gl4es puts its library in its own source tree by default, which all ABIs would share
set_target_properties(GL PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gl4es/lib)
# only gl4esinit.h is needed from gl4es' headers; the game uses its own GL headers
file(COPY ${DEPS}/gl4es/include/gl4esinit.h DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/gl4es-include)

# OpenAL Soft (sound output through OpenSL ES)
set(LIBTYPE STATIC CACHE STRING "" FORCE)
set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
set(ALSOFT_NO_CONFIG_UTIL ON CACHE BOOL "" FORCE)
set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_UTILS OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_OPENSL ON CACHE BOOL "" FORCE)
set(ALSOFT_REQUIRE_OPENSL ON CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_OBOE OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_WAVE OFF CACHE BOOL "" FORCE)
add_subdirectory(${DEPS}/openal-soft openal-soft)

# libogg and libvorbis (music and sounds are Ogg Vorbis); built from their sources directly
add_library(ogg STATIC ${DEPS}/ogg/src/bitwise.c ${DEPS}/ogg/src/framing.c)
target_include_directories(ogg PUBLIC ${DEPS}/ogg/include ${CMAKE_CURRENT_SOURCE_DIR}/ogg-include)

set(VORBIS_SOURCES analysis.c bitrate.c block.c codebook.c envelope.c floor0.c floor1.c info.c
    lookup.c lpc.c lsp.c mapping0.c mdct.c psy.c registry.c res0.c sharedbook.c smallft.c
    synthesis.c window.c vorbisfile.c)
list(TRANSFORM VORBIS_SOURCES PREPEND ${DEPS}/vorbis/lib/)
add_library(vorbis STATIC ${VORBIS_SOURCES})
target_include_directories(vorbis PUBLIC ${DEPS}/vorbis/include PRIVATE ${DEPS}/vorbis/lib)
target_compile_definitions(vorbis PRIVATE HAVE_ALLOCA_H=1)
target_compile_options(vorbis PRIVATE -Wno-everything)
target_link_libraries(vorbis PUBLIC ogg m)

# enet
file(GLOB ENET_SOURCES ${SRC}/enet/*.c)
list(FILTER ENET_SOURCES EXCLUDE REGEX ".*/win32\\.c$")
add_library(enet STATIC ${ENET_SOURCES})
target_include_directories(enet PUBLIC ${SRC}/enet/include)
target_compile_definitions(enet PRIVATE HAS_POLL=1 HAS_FCNTL=1 HAS_INET_PTON=1 HAS_INET_NTOP=1
    HAS_MSGHDR_FLAGS=1 HAS_SOCKLEN_T=1 HAS_GETADDRINFO=1 HAS_GETNAMEINFO=1)

# the game, built as libmain.so (SDLActivity calls its SDL_main)
set(GAME_SOURCES
    crypto.cpp audiomanager.cpp autodownload.cpp client.cpp clientgame.cpp clients2c.cpp
    command.cpp console.cpp docs.cpp editing.cpp entities.cpp gamepad.cpp http.cpp log.cpp
    main.cpp menus.cpp oggstream.cpp openal.cpp packetqueue.cpp physics.cpp protocol.cpp
    rendercubes.cpp rendergl.cpp renderhud.cpp rendermodel.cpp renderparticles.cpp rendertext.cpp
    rndmap.cpp scoreboard.cpp serverms.cpp server.cpp serverbrowser.cpp shadow.cpp
    soundlocation.cpp soundscheduler.cpp stream.cpp texture.cpp tools.cpp water.cpp weapon.cpp
    wizard.cpp world.cpp worldio.cpp worldlight.cpp worldocull.cpp worldrender.cpp zip.cpp
    bot/bot.cpp bot/botmanager.cpp bot/bot_ai.cpp bot/bot_util.cpp bot/bot_waypoint.cpp
    bot/ac_bot.cpp bot/ac_bot_ai.cpp)
list(TRANSFORM GAME_SOURCES PREPEND ${SRC}/src/)

add_library(main SHARED ${GAME_SOURCES})
target_include_directories(main PRIVATE ${SRC}/src ${SRC}/src/bot ${CMAKE_CURRENT_BINARY_DIR}/gl4es-include)
# the bundled headers in source/include (GL/glext.h) come last, like the desktop Makefile's -idirafter,
# so they don't shadow the SDL2/OpenAL/Ogg headers built here
target_compile_options(main PRIVATE -idirafter ${SRC}/include)
target_compile_definitions(main PRIVATE USE_GLES)
# -fsigned-char matters: ARM defaults to unsigned char, the engine assumes signed
target_compile_options(main PRIVATE -O2 -ffast-math -fsigned-char -fno-strict-aliasing
    -Wno-unused-variable -Wno-unused-function -Wno-deprecated-declarations -Wno-format-security
    -Wno-c++11-narrowing -Wno-writable-strings)
target_link_libraries(main PRIVATE SDL2::SDL2 SDL2_image::SDL2_image-static GL OpenAL vorbis ogg
    enet z log android m dl)
