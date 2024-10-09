cmake_minimum_required (VERSION 3.2)

if(NOT UNIX)
    message(FATAL_ERROR "ALSASeq: This component is for UNIX and UNIX-like platforms only")
endif()

add_executable(adlalsaseq adlalsaseq.c)

target_link_libraries(adlalsaseq PRIVATE ADLMIDI)
libADLMIDI_find_SDL2()
target_link_libraries(adlalsaseq PRIVATE ADLMIDI_SDL2)

find_package(ALSA REQUIRED)
include_directories(${ALSA_INCLUDE_DIRS})
target_link_libraries(adlalsaseq PRIVATE ${ALSA_LIBRARIES})

install(TARGETS adlalsaseq
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
