
if(ADLMIDI_DOS)
    set(MIDIPLAY_PREFER_SDL OFF)
else()
    option(MIDIPLAY_PREFER_SDL  "Prefer use of SDL2 or SDL3 audio instead of WinMM/ALSA/PulseAudio" OFF)
endif()

if(UNIX AND NOT APPLE AND NOT MIDIPLAY_PREFER_SDL)
    set(USE_ALSA_AUDIO 1)
endif()

if(MIDIPLAY_PREFER_SDL OR (NOT ADLMIDI_DOS AND NOT WIN32 AND NOT MIDIPLAY_WAVE_ONLY AND NOT USE_ALSA_AUDIO))
    libADLMIDI_find_SDL3()
    if(TARGET ADLMIDI_SDL3)
        set(USE_SDL3_AUDIO 1)
    else()
        libADLMIDI_find_SDL2()
        set(USE_SDL2_AUDIO 1)
    endif()
endif()

include(../common/utf8main.cmake)

set(ADLMIDI_PLAY_SRC
    adlmidiplay.cpp
    dev_setup.cpp
    dev_setup.h
    misc.c
    misc.h
    time_counter.cpp
    time_counter.h
    audio/audio.h
    playback/playback_common.cpp
    playback/playback.h
    ${UTF8MAIN_SRCS}
)

if(ADLMIDI_DOS)
    list(APPEND ADLMIDI_PLAY_SRC
        playback/playback_hw_dos.cpp
        playback/dos_tman.h
        playback/dos_tman_c.h
        playback/dos_tman.cpp
    )
else()
    list(APPEND ADLMIDI_PLAY_SRC
        playback/playback_wave.cpp
        playback/wave_writer.c
        playback/wave_writer.h
    )

    if(NOT MIDIPLAY_WAVE_ONLY)
        list(APPEND ADLMIDI_PLAY_SRC playback/playback_audio.cpp)

        if(USE_HW_SERIAL)
            list(APPEND ADLMIDI_PLAY_SRC playback/playback_hw_serial.cpp)
        endif()
    endif()
endif()

if(USE_SDL2_AUDIO OR USE_SDL3_AUDIO)
    list(APPEND ADLMIDI_PLAY_SRC
        audio/audio_sdl.c
    )
endif()

if(USE_ALSA_AUDIO)
    list(APPEND ADLMIDI_PLAY_SRC
        audio/audio_alsa.c
    )
endif()

if(WIN32)
    list(APPEND ADLMIDI_PLAY_SRC
        audio/audio_winmm.c
    )
endif()

option(DEBUG_SONG_SWITCHING  "Enable song switching by left and right keys (Linux only)" OFF)
option(ENABLE_TERMINAL_HOTKEYS  "Enable use of ncurses library to handle keyboard actions" OFF)
option(TERMINAL_USE_NCURSES  "Enable use of ncurses library to handle keyboard actions" OFF)

add_executable(adlmidiplay ${ADLMIDI_PLAY_SRC})

if(USE_SDL3_AUDIO)
    target_link_libraries(adlmidiplay PRIVATE ADLMIDI_SDL3)
    target_compile_definitions(adlmidiplay PRIVATE ADLMIDI_USE_SDL3 MIDIPLAY_USE_SDL3)
elseif(USE_SDL2_AUDIO)
    target_link_libraries(adlmidiplay PRIVATE ADLMIDI_SDL2)
    target_compile_definitions(adlmidiplay PRIVATE ADLMIDI_USE_SDL2 MIDIPLAY_USE_SDL2)
elseif(USE_ALSA_AUDIO)
    target_link_libraries(adlmidiplay PRIVATE asound)
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
    if(MSVC)
        target_compile_definitions(adlmidiplay PRIVATE -DWIN32_CONSOLE)
    endif()
elseif(NOT ADLMIDI_DOS)
    if(NOT OPENBSD_LOCALBASE AND NOT HAIKU)
        target_link_libraries(adlmidiplay PRIVATE dl)
    endif()
    target_link_libraries(adlmidiplay PRIVATE pthread m)
endif()

if(ADLMIDI_DOS)
    set_target_properties(adlmidiplay PROPERTIES OUTPUT_NAME adlmidi SUFFIX ".exe")
endif()

if(TARGET ADLMIDI_shared)
    set_target_properties(adlmidiplay PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
endif()

install(TARGETS adlmidiplay
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
