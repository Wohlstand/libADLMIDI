
include(../common/utf8main.cmake)

set(ADLMIDI_PLAY_SRC
    mididump.cpp
    ${UTF8MAIN_SRCS}
    ../../src/midiseq/file_reader.hpp
    ../../src/midiseq/midi_sequencer.h
    ../../src/midiseq/midi_sequencer.hpp
    ../../src/midiseq/midi_sequencer_impl.hpp
    ../../src/midiseq/impl/common.hpp
    ../../src/midiseq/impl/cvt_xmi2mid.hpp
    ../../src/midiseq/impl/databank_impl.hpp
    ../../src/midiseq/impl/debug_songdump.hpp
    ../../src/midiseq/impl/durated_note_impl.hpp
    ../../src/midiseq/impl/err_string_impl.hpp
    ../../src/midiseq/impl/io_impl.hpp
    ../../src/midiseq/impl/load_music_impl.hpp
    ../../src/midiseq/impl/loop_impl.hpp
    ../../src/midiseq/impl/mididata_impl.hpp
    ../../src/midiseq/impl/miditrack_impl.hpp
    ../../src/midiseq/impl/platform_impl.hpp
    ../../src/midiseq/impl/process_impl.hpp
    ../../src/midiseq/impl/read_cmf_impl.hpp
    ../../src/midiseq/impl/read_gmf_impl.hpp
    ../../src/midiseq/impl/read_hmi_impl.hpp
    ../../src/midiseq/impl/read_imf_impl.hpp
    ../../src/midiseq/impl/read_klm_impl.hpp
    ../../src/midiseq/impl/read_mus_impl.hpp
    ../../src/midiseq/impl/read_rsxx_impl.hpp
    ../../src/midiseq/impl/read_smf_impl.hpp
    ../../src/midiseq/impl/read_xmi_impl.hpp
    ../../src/midiseq/impl/tempo_fraction.hpp
)

add_executable(mididump ${ADLMIDI_PLAY_SRC})

set_nopie(mididump)

target_compile_definitions(mididump PRIVATE BWMIDI_ENABLE_DEBUG_SONG_DUMP BWMIDI_ENABLE_OPL_MUSIC_SUPPORT)
target_include_directories(mididump PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/midiseq/")
target_link_libraries(mididump PRIVATE ADLMIDI)

install(TARGETS mididump
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
