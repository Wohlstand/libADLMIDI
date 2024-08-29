set(CMAKE_CXX_STANDARD 11)

find_package(OpenMP QUIET)
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

if(NOT WIN32 AND NOT ADLMIDI_DOS)
    option(ADLMIDI2_HAS_PUZZLE_GAME     "Enable the built-in puzzle game" ON)
endif()
if(NOT ADLMIDI_DOS)
    option(ADLMIDI2_VIDEO_OUT_SUPPORT   "Allow video recording with using of FFMPEG" ON)
endif()

try_compile(HAS_TERMIO ${CMAKE_BINARY_DIR}/compile_tests "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-termio.c")
try_compile(HAS_TERMIOS ${CMAKE_BINARY_DIR}/compile_tests "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-termios.c")

include(../midiplay/utf8main.cmake)

set(adlmidi2_src)
list(APPEND adlmidi2_src
    midiplay.cc
    input.cc
    input.hpp
    6x9.hpp
    8x16.hpp
    9x15.hpp
    ${UTF8MAIN_SRCS}
)

if(ADLMIDI2_HAS_PUZZLE_GAME)
    list(APPEND adlmidi2_src puzzlegame.cc puzzlegame.hpp)
endif()

add_executable(adlmidi2 ${adlmidi2_src})
target_link_libraries(adlmidi2 PRIVATE ADLMIDI)

if(NOT WIN32 AND NOT ADLMIDI_DOS)
    libADLMIDI_find_SDL2()
    target_link_libraries(adlmidi2 PRIVATE ADLMIDI_SDL2)
endif()

set_nopie(adlmidi2)

if(HAS_TERMIO)
    target_compile_definitions(adlmidi2 PRIVATE -DHAS_TERMIO)
endif()

if(HAS_TERMIOS)
    target_compile_definitions(adlmidi2 PRIVATE -DHAS_TERMIOS)
endif()

if(ADLMIDI2_HAS_PUZZLE_GAME)
    target_compile_definitions(adlmidi2 PUBLIC SUPPORT_PUZZLE_GAME)
endif()

if(ADLMIDI2_VIDEO_OUT_SUPPORT)
    target_compile_definitions(adlmidi2 PUBLIC SUPPORT_VIDEO_OUTPUT)
endif()

if(ADLMIDI_DOS)
    target_compile_definitions(adlmidi2 PUBLIC HARDWARE_OPL3)
    message("Turn on hardware OPL3 support on ADLMIDI2 tool")
endif()

if(WIN32)
    target_link_libraries(adlmidi2 PRIVATE winmm)
    set_property(TARGET adlmidi2 PROPERTY WIN32_EXECUTABLE TRUE)
elseif(NOT ADLMIDI_DOS)
    if(NOT OPENBSD_LOCALBASE AND NOT HAIKU)
        target_link_libraries(adlmidi2 PRIVATE dl)
    endif()
    target_link_libraries(adlmidi2 PRIVATE pthread m)
endif()

if(MSVC)
    message("Turned on C++11 on MSVC")
    target_compile_options(adlmidi2 PUBLIC "/std:c++latest")
endif()

if(TARGET ADLMIDI_shared AND NOT MSVC)
    set_target_properties(adlmidi2 PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
endif()

install(TARGETS adlmidi2
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
