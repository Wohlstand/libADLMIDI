
if(NOT ADLMIDI_DOS AND NOT MIDIPLAY_WAVE_ONLY)
    if(NOT WIN32)
        libADLMIDI_find_SDL2()
        set(USE_SDL2_AUDIO 1)
    else()
    endif()
endif()

include(utf8main.cmake)

set(ADLMIDI_PLAY_SRC
    adlmidiplay.cpp
    wave_writer.c
    wave_writer.h
    audio.h
    ${UTF8MAIN_SRCS}
)

if(USE_SDL2_AUDIO)
    list(APPEND ADLMIDI_PLAY_SRC
        audio_sdl.c
    )
endif()

if(WIN32 AND NOT USE_SDL2_AUDIO)
    list(APPEND ADLMIDI_PLAY_SRC
        audio_winmm.c
    )
endif()

option(DEBUG_SONG_SWITCHING  "Enable song switching by left and right keys (Linux only)" OFF)
option(ENABLE_TERMINAL_HOTKEYS  "Enable use of ncurses library to handle keyboard actions" OFF)
option(TERMINAL_USE_NCURSES  "Enable use of ncurses library to handle keyboard actions" OFF)

add_executable(adlmidiplay ${ADLMIDI_PLAY_SRC})

if(USE_SDL2_AUDIO)
    target_link_libraries(adlmidiplay PRIVATE ADLMIDI_SDL2)
    target_compile_definitions(adlmidiplay PRIVATE ADLMIDI_USE_SDL2)
elseif(WIN32)
    target_link_libraries(adlmidiplay PRIVATE winmm)
endif()

set_nopie(adlmidiplay)

if(DEBUG_SONG_SWITCHING)
    target_compile_definitions(adlmidiplay PRIVATE DEBUG_SONG_SWITCHING)
endif()

if(MIDIPLAY_WAVE_ONLY)
    target_compile_definitions(adlmidiplay PUBLIC OUTPUT_WAVE_ONLY)
    message("Demo tool will only output WAVE file, no playing support.")
endif()

if(ENABLE_TERMINAL_HOTKEYS)
    target_compile_definitions(adlmidiplay PUBLIC ENABLE_TERMINAL_HOTKEYS)
endif()

if(TERMINAL_USE_NCURSES)
    target_compile_definitions(adlmidiplay PUBLIC TERMINAL_USE_NCURSES)
    target_link_libraries(adlmidiplay PRIVATE ncurses)
endif()

if(ADLMIDI_DOS)
    target_compile_definitions(adlmidiplay PUBLIC HARDWARE_OPL3)
    message("Turn on hardware OPL3 support on demo tool")
endif()

target_link_libraries(adlmidiplay PRIVATE ADLMIDI)

if(WIN32)
    if(NOT MSVC)
        target_link_libraries(adlmidiplay PRIVATE pthread)
    endif()
    set_property(TARGET adlmidiplay PROPERTY WIN32_EXECUTABLE OFF)
elseif(NOT ADLMIDI_DOS)
    if(NOT OPENBSD_LOCALBASE AND NOT HAIKU)
        target_link_libraries(adlmidiplay PRIVATE dl)
    endif()
    target_link_libraries(adlmidiplay PRIVATE pthread m)
endif()

if(ADLMIDI_DOS)
    set_target_properties(adlmidiplay PROPERTIES OUTPUT_NAME adlmidi)
endif()

if(TARGET ADLMIDI_shared)
    set_target_properties(adlmidiplay PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
endif()

install(TARGETS adlmidiplay
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

