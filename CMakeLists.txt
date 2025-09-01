cmake_minimum_required (VERSION 3.2...3.10)
project (libADLMIDI VERSION 1.6.1 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# Prefer C90 standard
set(CMAKE_C_STANDARD 90)
# Prefer C++98 standard
set(CMAKE_CXX_STANDARD 98)

if(MSVC)
    check_cxx_compiler_flag("/std:c++14" COMPILER_SUPPORTS_CXX14)
    if(COMPILER_SUPPORTS_CXX14)
        set(FLAG_CPP14 "/std:c++14")
    endif()
    if(MSVC_VERSION LESS 1910)
        unset(COMPILER_SUPPORTS_CXX14) # MSVC older than 2017 fails to build YMFM
    endif()
else()
    check_cxx_compiler_flag("-std=c++14" COMPILER_SUPPORTS_CXX14)
    if(COMPILER_SUPPORTS_CXX14)
        set(FLAG_CPP14 "-std=c++14")
    endif()
endif()

function(CheckCPP14Build OUTVAR)
    set(CMAKE_CXX_STANDARD 14)
    try_compile(${OUTVAR}
        ${CMAKE_CURRENT_BINARY_DIR}/cmake-checks
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/checks/cpp14.cpp
        CMAKE_FLAGS -DCMAKE_CXX_FLAGS=${FLAG_CPP14}
    )
endfunction()

if(COMPILER_SUPPORTS_CXX14)
    CheckCPP14Build(COMPILER_BUILDS_CXX14)
    if(COMPILER_BUILDS_CXX14)
        message("== Your C++ compiler supports C++14, YMFM emulator will be ENABLED")
    else()
        message("== Your C++ compiler can't build C++14, YMFM emulator will be DISABLED")
        set(COMPILER_SUPPORTS_CXX14 FALSE)
    endif()
else()
    message("== Your C++ compiler does NOT supports C++14, YMFM emulator will be DISABLED")
endif()

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

if(DJGPP OR MSDOS)
    set(ADLMIDI_DOS 1)
    add_definitions(-DADLMIDI_HW_OPL)
endif()

if(NINTENDO_3DS)
    set(ADLMIDI_3DS 1)
    add_definitions(-DDOSBOX_NO_MUTEX)
endif()

if(NINTENDO_DS)
    set(ADLMIDI_DS 1)
    add_definitions(-DDOSBOX_NO_MUTEX)
endif()

if(NINTENDO_WII)
    set(ADLMIDI_WII 1)
    add_definitions(-DUSE_LIBOGC_MUTEX)
endif()

if(NINTENDO_WIIU)
    set(ADLMIDI_WIIU 1)
    add_definitions(-DUSE_WUT_MUTEX)
endif()

#===========================================================================================
# Strip garbage
if(APPLE)
    set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -dead_strip")
elseif(NOT MSVC AND NOT MSDOS AND NOT OPENBSD_LOCALBASE)
    string(REGEX REPLACE "-O3" ""
            CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REGEX REPLACE "-O3" ""
            CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -fdata-sections -ffunction-sections")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -fdata-sections -ffunction-sections")
    string(REGEX REPLACE "-O3" ""
            CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    string(REGEX REPLACE "-O3" ""
            CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2 -fdata-sections -ffunction-sections")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -fdata-sections -ffunction-sections")
    if(ANDROID)
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -funwind-tables")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -funwind-tables")
    endif()
    if(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s -Wl,--gc-sections -Wl,-s")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -Wl,--gc-sections -Wl,-s")
        set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -Wl,--gc-sections -Wl,-s")
    endif()
endif()

if(NOT MSVC AND NOT MSDOS)
# Global optimization flags
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")
# Turn on all warnings
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
# Deny undefined symbols
    if(NOT APPLE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-undefined" )
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined" )
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-undefined" )
    endif()
# Supress the std::vector::insert() GCC change warning
    if(VITA)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVITA -DVITA=1 -fcompare-debug-second")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVITA -DVITA=1 -fcompare-debug-second")
    endif()
    if(NINTENDO_3DS OR NINTENDO_WII OR NINTENDO_WIIU OR NINTENDO_SWITCH)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcompare-debug-second")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcompare-debug-second")
    endif()
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    if(NINTENDO_3DS OR NINTENDO_WII OR NINTENDO_WIIU OR NINTENDO_SWITCH)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu90")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c89")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98")
endif()

if(DEFINED CMAKE_BUILD_TYPE)
    string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
endif()
if(CMAKE_BUILD_TYPE_LOWER EQUAL "release")
    add_definitions(-DNDEBUG)
ENDIF()

if(CMAKE_BUILD_TYPE_LOWER STREQUAL "debug" AND CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-D_GLIBCXX_DEBUG=1 -D_GLIBCXX_ASSERTIONS=1)
endif()

# Disable bogus MSVC warnings
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if(OPENBSD_LOCALBASE)
    message("OpenBSD Detected!")
    include_directories(${OPENBSD_LOCALBASE}/include)
endif()

if(NOT WIN32 AND CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    check_c_compiler_flag("-no-pie" HAS_NO_PIE)
endif()

function(set_nopie _target)
    if(NOT WIN32 AND HAS_NO_PIE AND CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
        set_property(TARGET ${_target} APPEND_STRING PROPERTY LINK_FLAGS " -no-pie")
    endif()
endfunction()

function(set_legacy_standard destTarget)
    if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
        # Turn on warnings and legacy C/C++ standards to support more compilers
        target_compile_options(${destTarget} PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-Wall -pedantic -std=gnu90 -Wno-long-long>
            $<$<COMPILE_LANGUAGE:CXX>:-Wall -pedantic -std=gnu++98>
        )
    endif()
endfunction()

function(set_visibility_hidden destTarget)
    if(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$" AND NOT DJGPP)
        if(CMAKE_VERSION VERSION_GREATER 3.2)
            target_compile_options(${destTarget} PRIVATE
              $<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden> -fvisibility=hidden)
        else()
            target_compile_options(${destTarget} PRIVATE -fvisibility=hidden)
        endif()
        target_compile_definitions(${destTarget} PRIVATE LIBADLMIDI_VISIBILITY=)
    endif()
endfunction()
#===========================================================================================

if(VITA OR NINTENDO_3DS OR NINTENDO_WII OR NINTENDO_WIIU)
    set(DEFAULT_HEAVY_EMULATORS OFF)
else()
    set(DEFAULT_HEAVY_EMULATORS ON)
endif()

if(NOT EMSCRIPTEN
   AND NOT VITA
   AND NOT NINTENDO_3DS
   AND NOT NINTENDO_WII
   AND NOT NINTENDO_WIIU
   AND NOT ADLMIDI_DOS)

    set(libADLMIDI_STATIC_ENABLED_BY_DEFAULT ON)
    set(libADLMIDI_SHARED_ENABLED_BY_DEFAULT OFF)

    # When defined, respect CMake's BUILD_SHARED_LIBS setting
    if (BUILD_SHARED_LIBS)
        set(libADLMIDI_SHARED_ENABLED_BY_DEFAULT ON)
        set(libADLMIDI_STATIC_ENABLED_BY_DEFAULT OFF)
    endif()

    option(libADLMIDI_STATIC   "Build static library of libADLMIDI" ${libADLMIDI_STATIC_ENABLED_BY_DEFAULT})
    option(libADLMIDI_SHARED   "Build shared library of libADLMIDI" ${libADLMIDI_SHARED_ENABLED_BY_DEFAULT})
else()
    set(libADLMIDI_STATIC ON)
    set(libADLMIDI_SHARED OFF)
endif()

option(WITH_MIDI_SEQUENCER  "Build with embedded MIDI sequencer. Disable this if you want use library in real-time MIDI drivers or plugins." ON)
option(WITH_EMBEDDED_BANKS  "Use embedded banks" ON)
option(WITH_HQ_RESAMPLER    "Build with support for high quality resampling" OFF)
option(WITH_MUS_SUPPORT     "Build with support for DMX MUS files" ON)
option(WITH_XMI_SUPPORT     "Build with support for AIL XMI files" ON)
option(USE_DOSBOX_EMULATOR  "Use DosBox 0.74 OPL3 emulator (semi-accurate, suggested for slow or mobile platforms)" ON)
option(USE_NUKED_EMULATOR   "Use Nuked OPL3 emulator (most accurate, powerful)" ${DEFAULT_HEAVY_EMULATORS})
option(USE_OPAL_EMULATOR    "Use Opal emulator (inaccurate)" ${DEFAULT_HEAVY_EMULATORS})
option(USE_JAVA_EMULATOR    "Use JavaOPL emulator" ${DEFAULT_HEAVY_EMULATORS})
option(USE_ESFMU_EMULATOR   "Use ESFMu emulator (Based on Nuked OPL3, powerful)" ${DEFAULT_HEAVY_EMULATORS})
option(USE_MAME_EMULATOR    "Use MAME OPL2 emulator" ${DEFAULT_HEAVY_EMULATORS})
if(COMPILER_SUPPORTS_CXX14)
    option(USE_YMFM_EMULATOR    "Use YMFM emulator (requires C++14 support)" ON)
endif()
option(USE_NUKED_OPL2_LLE_EMULATOR  "Use Nuked OPL2-LLE emulator [!EXTRA HEAVY!]" OFF)
option(USE_NUKED_OPL3_LLE_EMULATOR  "Use Nuked OPL3-LLE emulator [!EXTRA HEAVY!]" OFF)
option(USE_HW_SERIAL        "Use the hardware OPL3 chip via Serial on modern systems" OFF)

option(WITH_GENADLDATA      "Build and run full rebuild of embedded banks cache" OFF)
option(WITH_GENADLDATA_COMMENTS "Enable comments in a generated embedded instruments cache file" OFF)
set(GENADLDATA_CUSTOM_BANKLIST "" CACHE PATH "Use the custom INI file to build the database")

option(WITH_MIDIPLAY        "Build also demo MIDI player" OFF)
option(MIDIPLAY_WAVE_ONLY   "Build Demo MIDI player without support of real time playing. It will output into WAV only." OFF)
option(WITH_ADLMIDI2        "Build also classic ADLMIDI player [EXPERIMENTAL]" OFF)
option(WITH_VLC_PLUGIN      "Build also a plugin for VLC Media Player" OFF)
option(VLC_PLUGIN_NOINSTALL "Don't install VLC plugin into VLC directory" OFF)
option(WITH_OLD_UTILS       "Build also old utilities" OFF)
option(WITH_MUS2MID         "Build a MUS to MIDI converter" OFF)
option(WITH_XMI2MID         "Build a XMI to MIDI converter" OFF)
option(EXAMPLE_SDL2_AUDIO   "Build also a simple SDL2 demo MIDI player" OFF)

if(WIN32)
    option(WITH_WINMMDRV    "Build a WinMM MIDI driver" OFF)
endif()

option(ENABLE_ADDRESS_SANITIZER "Enable the Address Sanitizer GCC feature" OFF)

if(NOT WIN32
   AND NOT VITA
   AND NOT NINTENDO_3DS
   AND NOT NINTENDO_WII
   AND NOT NINTENDO_WIIU
   AND NOT NINTENDO_SWITCH
   AND NOT EMSCRIPTEN
   AND NOT ADLMIDI_DOS)
    option(ENABLE_FPIC "Build static libraries as position independent code" ON)
else()
    set(ENABLE_FPIC OFF)
endif()

if(WITH_EMBEDDED_BANKS)
    if(NOT WITH_GENADLDATA OR "${GENADLDATA_CUSTOM_BANKLIST}" STREQUAL "")
        set(ADLDATA_DATABASE "${libADLMIDI_SOURCE_DIR}/src/inst_db.cpp")
    else()
        make_directory("${CMAKE_BINARY_DIR}/instdb")
        set(ADLDATA_DATABASE "${CMAKE_BINARY_DIR}/instdb/inst_db.cpp")
    endif()
endif()

function(handle_options targetLib)
    if(WITH_MIDI_SEQUENCER)
        target_sources(${targetLib} PRIVATE ${libADLMIDI_SOURCE_DIR}/src/adlmidi_sequencer.cpp)
        target_compile_definitions(${targetLib} PUBLIC ENABLE_END_SILENCE_SKIPPING)
    endif()

    if(NOT WITH_MUS_SUPPORT OR NOT WITH_MIDI_SEQUENCER)
        target_compile_definitions(${targetLib} PUBLIC BWMIDI_DISABLE_MUS_SUPPORT)
    endif()

    if(NOT WITH_XMI_SUPPORT OR NOT WITH_MIDI_SEQUENCER)
        target_compile_definitions(${targetLib} PUBLIC BWMIDI_DISABLE_XMI_SUPPORT)
    endif()

    if(ADLMIDI_DOS)
        set(HAS_EMULATOR TRUE)
        target_sources(${targetLib} PRIVATE
            ${libADLMIDI_SOURCE_DIR}/src/chips/dos_hw_opl.cpp
            ${libADLMIDI_SOURCE_DIR}/src/chips/dos_hw_opl.h
        )
        target_compile_definitions(${targetLib} PRIVATE ENABLE_HW_OPL_DOS)
        target_compile_definitions(${targetLib} PUBLIC ADLMIDI_ENABLE_HW_DOS)
    else()
        if(USE_DOSBOX_EMULATOR)
            set(HAS_EMULATOR TRUE)
            target_sources(${targetLib} PRIVATE
                ${libADLMIDI_SOURCE_DIR}/src/chips/dosbox_opl3.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/dosbox/dbopl.cpp
            )
        else()
            target_compile_definitions(${targetLib} PUBLIC ADLMIDI_DISABLE_DOSBOX_EMULATOR)
        endif()

        if(USE_NUKED_EMULATOR)
            set(HAS_EMULATOR TRUE)
            target_sources(${targetLib} PRIVATE
                ${libADLMIDI_SOURCE_DIR}/src/chips/nuked_opl3.cpp       # v 1.8
                ${libADLMIDI_SOURCE_DIR}/src/chips/nuked_opl3.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/nuked/nukedopl3.c
                ${libADLMIDI_SOURCE_DIR}/src/chips/nuked/nukedopl3.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/nuked_opl3_v174.cpp  # v 1.7.4
                ${libADLMIDI_SOURCE_DIR}/src/chips/nuked_opl3_v174.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/nuked/nukedopl3_174.c
                ${libADLMIDI_SOURCE_DIR}/src/chips/nuked/nukedopl3_174.h
            )
        else()
            target_compile_definitions(${targetLib} PUBLIC ADLMIDI_DISABLE_NUKED_EMULATOR)
        endif()

        if(USE_OPAL_EMULATOR)
            set(HAS_EMULATOR TRUE)
            target_sources(${targetLib} PRIVATE
                ${libADLMIDI_SOURCE_DIR}/src/chips/opal_opl3.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/opal_opl3.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/opal/opal.c
                ${libADLMIDI_SOURCE_DIR}/src/chips/opal/opal.h
            )
        else()
            target_compile_definitions(${targetLib} PUBLIC ADLMIDI_DISABLE_OPAL_EMULATOR)
        endif()

        if(USE_JAVA_EMULATOR)
            set(HAS_EMULATOR TRUE)
            target_sources(${targetLib} PRIVATE
                ${libADLMIDI_SOURCE_DIR}/src/chips/java_opl3.cpp
            )
        else()
            target_compile_definitions(${targetLib} PUBLIC ADLMIDI_DISABLE_JAVA_EMULATOR)
        endif()

        if(USE_ESFMU_EMULATOR)
            set(HAS_EMULATOR TRUE)
            target_sources(${targetLib} PRIVATE
                ${libADLMIDI_SOURCE_DIR}/src/chips/esfmu_opl3.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/esfmu_opl3.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/esfmu/esfm.c
                ${libADLMIDI_SOURCE_DIR}/src/chips/esfmu/esfm_registers.c
                ${libADLMIDI_SOURCE_DIR}/src/chips/esfmu/esfm.h
            )
        else()
            target_compile_definitions(${targetLib} PUBLIC ADLMIDI_DISABLE_ESFMU_EMULATOR)
        endif()

        if(USE_MAME_EMULATOR)
            set(HAS_EMULATOR TRUE)
            target_sources(${targetLib} PRIVATE
                ${libADLMIDI_SOURCE_DIR}/src/chips/mame_opl2.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/mame_opl2.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/mame/opl.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/mame/mame_fmopl.cpp
            )
        else()
            target_compile_definitions(${targetLib} PUBLIC ADLMIDI_DISABLE_MAME_OPL2_EMULATOR)
        endif()

        if(USE_YMFM_EMULATOR)
            set(YMFM_SOURCES
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm_opl2.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm_opl2.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm_opl3.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm_opl3.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm/ymfm_adpcm.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm/ymfm_adpcm.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm/ymfm_fm.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm/ymfm_fm.ipp
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm/ymfm_misc.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm/ymfm_misc.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm/ymfm_opl.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm/ymfm_opl.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm/ymfm_pcm.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm/ymfm_pcm.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm/ymfm_ssg.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymfm/ymfm_ssg.h
            )
            if(DEFINED FLAG_CPP14)
                set_source_files_properties(${YMFM_SOURCES} COMPILE_FLAGS ${FLAG_CPP14})
            endif()
            target_sources(${targetLib} PRIVATE ${YMFM_SOURCES})
            set(HAS_EMULATOR TRUE)
        else()
            target_compile_definitions(${targetLib} PUBLIC -DADLMIDI_DISABLE_YMFM_EMULATOR)
        endif()

        if(USE_NUKED_OPL2_LLE_EMULATOR)
            set(HAS_EMULATOR TRUE)
            target_sources(${targetLib} PRIVATE
                ${libADLMIDI_SOURCE_DIR}/src/chips/ym3812_lle.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/ym3812_lle.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/ym3812_lle/nopl2.c
                ${libADLMIDI_SOURCE_DIR}/src/chips/ym3812_lle/nopl2.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/ym3812_lle/nuked_fmopl2.c
                ${libADLMIDI_SOURCE_DIR}/src/chips/ym3812_lle/nuked_fmopl2.h
            )
            target_compile_definitions(${targetLib} PUBLIC ADLMIDI_ENABLE_OPL2_LLE_EMULATOR)
        endif()

        if(USE_NUKED_OPL3_LLE_EMULATOR)
            set(HAS_EMULATOR TRUE)
            target_sources(${targetLib} PRIVATE
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymf262_lle.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymf262_lle.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymf262_lle/nopl3.c
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymf262_lle/nopl3.h
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymf262_lle/nuked_fmopl3.c
                ${libADLMIDI_SOURCE_DIR}/src/chips/ymf262_lle/nuked_fmopl3.h
            )
            target_compile_definitions(${targetLib} PUBLIC ADLMIDI_ENABLE_OPL3_LLE_EMULATOR)
        endif()

        if(USE_HW_SERIAL)
            set(HAS_EMULATOR TRUE)
            target_sources(${targetLib} PRIVATE
                ${libADLMIDI_SOURCE_DIR}/src/chips/opl_serial_port.cpp
            )
            target_compile_definitions(${targetLib} PRIVATE ENABLE_HW_OPL_SERIAL_PORT)
            target_compile_definitions(${targetLib} PUBLIC ADLMIDI_ENABLE_HW_SERIAL)
        endif()

        if(NOT HAS_EMULATOR)
            message(FATAL_ERROR "No emulators enabled! You must enable at least one emulator!")
        endif()
    endif()

    if(WITH_EMBEDDED_BANKS)
        target_sources(${targetLib} PRIVATE ${ADLDATA_DATABASE})
        if(WITH_GENADLDATA)
            set_property(SOURCE "${ADLDATA_DATABASE}" PROPERTY INCLUDE_DIRECTORIES "${libADLMIDI_SOURCE_DIR}/src")
            if(NOT "${GENADLDATA_CUSTOM_BANKLIST}" STREQUAL "")
                set_property(SOURCE "${ADLDATA_DATABASE}" PROPERTY GENERATED TRUE)
            endif()
        endif()
    else()
        target_compile_definitions(${targetLib} PUBLIC DISABLE_EMBEDDED_BANKS)
    endif()

    if(NOT WITH_MIDI_SEQUENCER)
        target_compile_definitions(${targetLib} PUBLIC ADLMIDI_DISABLE_MIDI_SEQUENCER)
    endif()

    if(WITH_EMBEDDED_BANKS AND WITH_GENADLDATA AND NOT ADLMIDI_DOS)
        if("${GENADLDATA_CUSTOM_BANKLIST}" STREQUAL "")
            add_dependencies(${targetLib} gen-adldata-run)
        endif()
    endif()

    if(WITH_HQ_RESAMPLER AND NOT ADLMIDI_DOS)
        find_library(ZITA_RESAMPLER_LIBRARY "zita-resampler")
        if(NOT ZITA_RESAMPLER_LIBRARY)
            message(FATAL_ERROR "zita-resampler library not found")
        endif()
        target_compile_definitions(${targetLib} PRIVATE ADLMIDI_ENABLE_HQ_RESAMPLER)
        target_link_libraries(${targetLib} PUBLIC ${ZITA_RESAMPLER_LIBRARY})
    endif()

    if(ENABLE_ADDRESS_SANITIZER)
        target_compile_options(${targetLib} PUBLIC -fsanitize=address)
        target_link_options(${targetLib} PUBLIC -fsanitize=address)
    endif()
endfunction()

set(libADLMIDI_SOURCES
    ${libADLMIDI_SOURCE_DIR}/src/adlmidi.cpp
    ${libADLMIDI_SOURCE_DIR}/src/adlmidi_load.cpp
    ${libADLMIDI_SOURCE_DIR}/src/adlmidi_midiplay.cpp
    ${libADLMIDI_SOURCE_DIR}/src/adlmidi_opl3.cpp
    ${libADLMIDI_SOURCE_DIR}/src/adlmidi_private.cpp
    ${libADLMIDI_SOURCE_DIR}/src/wopl/wopl_file.c
)

# Make headers being properly visible at IDEs
file(GLOB libADLMIDI_HEADERS
    ${libADLMIDI_SOURCE_DIR}/include/*.h
    ${libADLMIDI_SOURCE_DIR}/src/*.h
    ${libADLMIDI_SOURCE_DIR}/src/*.hpp
    ${libADLMIDI_SOURCE_DIR}/src/wopl/*.h
    ${libADLMIDI_SOURCE_DIR}/src/structures/*.hpp
    ${libADLMIDI_SOURCE_DIR}/src/chips/*.h
    ${libADLMIDI_SOURCE_DIR}/src/chips/common/*.hpp
    ${libADLMIDI_SOURCE_DIR}/src/chips/dosbox/*.h
    ${libADLMIDI_SOURCE_DIR}/src/chips/java/*.hpp
    ${libADLMIDI_SOURCE_DIR}/src/chips/nuked/*.h
    ${libADLMIDI_SOURCE_DIR}/src/chips/opal/*.h
)
list(APPEND libADLMIDI_SOURCES ${libADLMIDI_HEADERS})

# === Static library ====
if(libADLMIDI_STATIC OR WITH_VLC_PLUGIN)
    add_library(ADLMIDI_static STATIC ${libADLMIDI_SOURCES})
    if(WIN32 AND libADLMIDI_SHARED)
        set_target_properties(ADLMIDI_static PROPERTIES OUTPUT_NAME ADLMIDI-static)
    else()
        set_target_properties(ADLMIDI_static PROPERTIES OUTPUT_NAME ADLMIDI)
    endif()
    target_include_directories(ADLMIDI_static PUBLIC $<BUILD_INTERFACE:${libADLMIDI_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
    set_legacy_standard(ADLMIDI_static)
    set_visibility_hidden(ADLMIDI_static)
    handle_options(ADLMIDI_static)

    # -fPIC thing
    if(ENABLE_FPIC)
        set_target_properties(ADLMIDI_static PROPERTIES COMPILE_FLAGS "-fPIC")
    endif()

    if(NOT libADLMIDI_STATIC)
        set_target_properties(ADLMIDI_static PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endif()

# === Shared library ====
if(libADLMIDI_SHARED)
    add_library(ADLMIDI_shared SHARED ${libADLMIDI_SOURCES})
    set_target_properties(ADLMIDI_shared PROPERTIES OUTPUT_NAME ADLMIDI)
    set_target_properties(ADLMIDI_shared PROPERTIES
        VERSION ${libADLMIDI_VERSION}
        SOVERSION ${libADLMIDI_VERSION_MAJOR}
    )
    target_include_directories(ADLMIDI_shared PUBLIC $<BUILD_INTERFACE:${libADLMIDI_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
    set_legacy_standard(ADLMIDI_shared)
    set_visibility_hidden(ADLMIDI_shared)
    handle_options(ADLMIDI_shared)
    if(WIN32)
        target_compile_definitions(ADLMIDI_shared PRIVATE ADLMIDI_BUILD_DLL)
        if(CMAKE_COMPILER_IS_GNUCXX)
            set_property(TARGET ADLMIDI_shared APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -static-libstdc++")
            set_property(TARGET ADLMIDI_shared APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lpthread -Wl,-Bdynamic,--no-whole-archive")
        endif()
    endif()
    if(OPENBSD_LOCALBASE)
        set_property(TARGET ADLMIDI_shared APPEND_STRING PROPERTY LINK_FLAGS " -lc")
    endif()

    set_target_properties(ADLMIDI_shared PROPERTIES SOVERSION "1")
endif()

if(NOT libADLMIDI_STATIC AND NOT libADLMIDI_SHARED)
    message(FATAL_ERROR "Both static and shared libADLMIDI builds are disabled!
You must enable at least one of them!")
endif()

if(libADLMIDI_SHARED)
    add_library(ADLMIDI ALIAS ADLMIDI_shared)
else()
    add_library(ADLMIDI ALIAS ADLMIDI_static)
endif()

function(libADLMIDI_find_SDL2)
    if(NOT TARGET ADLMIDI_SDL2)
        find_package(SDL2 REQUIRED)
        add_library(ADLMIDI_SDL2 INTERFACE)
        if(TARGET SDL2::SDL2)
            target_link_libraries(ADLMIDI_SDL2 INTERFACE SDL2::SDL2)
            target_include_directories(ADLMIDI_SDL2 INTERFACE ${SDL2_INCLUDE_DIRS})
        else()
            string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)
            target_include_directories(ADLMIDI_SDL2 INTERFACE ${SDL2_INCLUDE_DIRS})
            target_link_libraries(ADLMIDI_SDL2 INTERFACE ${SDL2_LIBRARIES})
        endif()
    endif()
endfunction()

if(WITH_OLD_UTILS)
    # Old utility to dump AdLib bank files
    add_subdirectory(utils/dumpbank)
    # Old utility to dump AIL bank files
    add_subdirectory(utils/dumpmiles)
endif()

if(WITH_EMBEDDED_BANKS AND WITH_GENADLDATA AND NOT ADLMIDI_DOS)
    add_subdirectory(utils/gen_adldata)
endif()

if(WITH_MIDIPLAY)
    add_subdirectory(utils/midiplay)
endif()

if(WITH_ADLMIDI2)
    add_subdirectory(utils/adlmidi-2)
endif()

if(WITH_MUS2MID)
    add_subdirectory(utils/mus2mid)
endif()

if(WITH_XMI2MID)
    add_subdirectory(utils/xmi2mid)
endif()

if(EXAMPLE_SDL2_AUDIO AND NOT ADLMIDI_DOS)
    add_subdirectory(examples/sdl2_audio)
endif()

if(WITH_VLC_PLUGIN AND NOT ADLMIDI_DOS)
    add_subdirectory(utils/vlc_codec)
endif()

if(WIN32 AND WITH_WINMMDRV)
    add_subdirectory(utils/winmm_drv)
endif()

if(libADLMIDI_STATIC)
    install(TARGETS ADLMIDI_static
            EXPORT libADLMIDIStaticTargets
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")

    install(EXPORT libADLMIDIStaticTargets
            FILE libADLMIDI-static-targets.cmake
            NAMESPACE libADLMIDI::
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libADLMIDI")
endif()

if(libADLMIDI_SHARED)
    install(TARGETS ADLMIDI_shared
            EXPORT libADLMIDISharedTargets
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")

    install(EXPORT libADLMIDISharedTargets
            FILE libADLMIDI-shared-targets.cmake
            NAMESPACE libADLMIDI::
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libADLMIDI")
endif()

install(FILES
        include/adlmidi.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(libADLMIDIConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/libADLMIDIConfig.cmake"
    PATH_VARS CMAKE_INSTALL_PREFIX CMAKE_INSTALL_FULL_BINDIR CMAKE_INSTALL_FULL_INCLUDEDIR CMAKE_INSTALL_FULL_LIBDIR
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libADLMIDI"
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libADLMIDIConfig.cmake
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libADLMIDI"
)

file(GLOB DOCTXT_FILES
    "${libADLMIDI_SOURCE_DIR}/LICENSE*.txt"
    README.md AUTHORS
)
install(FILES ${DOCTXT_FILES}
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/libADLMIDI")

if(NOT ANDROID)
    configure_file(libADLMIDI.pc.in libADLMIDI.pc @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libADLMIDI.pc"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()

# === Unit tests ====
option(WITH_UNIT_TESTS "Enable unit testing" OFF)
if(WITH_UNIT_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()


# === Version check ====
set(VERSION_CHECK_SOURCE "
    #include \"adlmidi.h\"
    #if !(ADLMIDI_VERSION_MAJOR == ${PROJECT_VERSION_MAJOR} && ADLMIDI_VERSION_MINOR == ${PROJECT_VERSION_MINOR} && ADLMIDI_VERSION_PATCHLEVEL == ${PROJECT_VERSION_PATCH})
    #error Project and source code version do not match!
    #endif")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/version_check.c" "${VERSION_CHECK_SOURCE}")
add_library(ADLMIDI_version_check OBJECT "${CMAKE_CURRENT_BINARY_DIR}/version_check.c")
target_include_directories(ADLMIDI_version_check PRIVATE "include")


# === Summary ====
message("==== libADLMIDI options ====")
message("libADLMIDI_STATIC        = ${libADLMIDI_STATIC}")
message("libADLMIDI_SHARED        = ${libADLMIDI_SHARED}")
message("WITH_UNIT_TESTS          = ${WITH_UNIT_TESTS}")

message("WITH_MIDI_SEQUENCER      = ${WITH_MIDI_SEQUENCER}")
message("WITH_EMBEDDED_BANKS      = ${WITH_EMBEDDED_BANKS}")
message("WITH_HQ_RESAMPLER        = ${WITH_HQ_RESAMPLER}")
message("WITH_MUS_SUPPORT         = ${WITH_MUS_SUPPORT}")
message("WITH_XMI_SUPPORT         = ${WITH_XMI_SUPPORT}")
message("USE_DOSBOX_EMULATOR      = ${USE_DOSBOX_EMULATOR}")
message("USE_NUKED_EMULATOR       = ${USE_NUKED_EMULATOR}")
message("USE_OPAL_EMULATOR        = ${USE_OPAL_EMULATOR}")
message("USE_JAVA_EMULATOR        = ${USE_JAVA_EMULATOR}")
message("USE_ESFMU_EMULATOR       = ${USE_ESFMU_EMULATOR}")
message("USE_MAME_EMULATOR        = ${USE_MAME_EMULATOR}")
if(COMPILER_SUPPORTS_CXX14)
    message("USE_YMFM_EMULATOR        = ${USE_YMFM_EMULATOR}")
else()
    message("USE_YMFM_EMULATOR        = OFF <lack of C++14>")
endif()
message("USE_HW_SERIAL            = ${USE_HW_SERIAL}")
if(USE_NUKED_OPL2_LLE_EMULATOR OR USE_NUKED_OPL3_LLE_EMULATOR)
    message(WARNING "You enabled EXTRA-HEAVY LLE emulators, they do REALLY require \
VERY POWERFUL processor to work (For example, on Intel Core i7-11700KF (8 cores, 3.60 GHz) \
they can work in real-time only when they built with Release optimisations, Debug builds \
are choppy even single chip run). You can't use them on weak devices, don't even think to \
run them on mobile. I warned. If you want, feel free to optimize them and contribute your \
updates to @NukeYkt who created them. The only way you can use them on weaker hardware \
is record WAVs where real-time performance doesn't matter.")
endif()

message("===== Utils and extras =====")
message("WITH_GENADLDATA          = ${WITH_GENADLDATA}")
message("WITH_GENADLDATA_COMMENTS = ${WITH_GENADLDATA_COMMENTS}")

message("WITH_MIDIPLAY            = ${WITH_MIDIPLAY}")
message("MIDIPLAY_WAVE_ONLY       = ${MIDIPLAY_WAVE_ONLY}")
message("WITH_ADLMIDI2            = ${WITH_ADLMIDI2}")
message("WITH_VLC_PLUGIN          = ${WITH_VLC_PLUGIN}")
message("WITH_OLD_UTILS           = ${WITH_OLD_UTILS}")
message("WITH_MUS2MID             = ${WITH_MUS2MID}")
message("WITH_XMI2MID             = ${WITH_XMI2MID}")
message("EXAMPLE_SDL2_AUDIO       = ${EXAMPLE_SDL2_AUDIO}")
if(WIN32)
    message("WITH_WINMMDRV            = ${WITH_WINMMDRV}")
endif()
