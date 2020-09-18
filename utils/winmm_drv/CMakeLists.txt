
if(NOT WIN32)
    message(FATAL_ERROR "WinDriver: This component is for Windows Platform only")
endif()

set(ADLMIDI_DRIVER_SRC
    src/winmm_drv.def
    src/winmm_drv.cpp
    src/MidiSynth.cpp
)

add_library(adlmididrv MODULE ${ADLMIDI_DRIVER_SRC})
set_target_properties(adlmididrv PROPERTIES PREFIX "")
target_link_libraries(adlmididrv PRIVATE winmm ADLMIDI_static)
target_compile_definitions(adlmididrv PRIVATE
    -D_USRDLL
    -D_WINDLL
    -DMM_WIN32DRV_EXPORTS
)

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    set_property(TARGET adlmididrv APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -static-libstdc++ -Wl,--enable-stdcall-fixup")
    set_property(TARGET adlmididrv APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lpthread -Wl,-Bdynamic,--no-whole-archive")
endif()

if(NOT MSVC)
    target_link_libraries(adlmididrv PRIVATE pthread)
endif()




set(ADLMIDI_INSTALLER_SRC
    installer/libadlmidi_win32drv_setup.cpp
    installer/stdafx.cpp
)

if(NOT MSVC)
    list(APPEND ADLMIDI_INSTALLER_SRC
        installer/drvinst.rc
    )
endif()

add_executable(drvsetup ${ADLMIDI_INSTALLER_SRC})

add_executable(drvtest test/test.c)
target_link_libraries(drvtest PRIVATE winmm)

target_compile_definitions(drvsetup PRIVATE
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1
    -D_CRT_SECURE_NO_WARNINGS
)

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    set_property(TARGET drvsetup APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -static-libstdc++")
    set_property(TARGET drvsetup APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lpthread -Wl,-Bdynamic,--no-whole-archive")
endif()


#install(TARGETS adlmididrv
#        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
