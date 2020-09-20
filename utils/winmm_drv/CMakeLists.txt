
if(NOT WIN32)
    message(FATAL_ERROR "WinDriver: This component is for Windows Platform only")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(PGE_ARCHITECTURE_BITS "32")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PGE_ARCHITECTURE_BITS "64")
else()
    set(PGE_ARCHITECTURE_BITS "unk")
endif()

#====================================
#          Driver module
#====================================

set(ADLMIDI_DRIVER_SRC
    src/winmm_drv.def
    src/winmm_drv.cpp
    src/MidiSynth.cpp
    setup/regsetup.c
)

add_library(adlmididrv MODULE ${ADLMIDI_DRIVER_SRC})
set_target_properties(adlmididrv PROPERTIES PREFIX "")
target_link_libraries(adlmididrv PRIVATE winmm ADLMIDI_static)
target_include_directories(adlmididrv PRIVATE setup)
target_compile_definitions(adlmididrv PRIVATE
    -D_USRDLL
    -D_WINDLL
    -DMM_WIN32DRV_EXPORTS
    -DENABLE_REG_SERVER
)

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    set_property(TARGET adlmididrv APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -static-libstdc++ -Wl,--enable-stdcall-fixup")
    if(PGE_ARCHITECTURE_BITS EQUAL 64)
        set_property(TARGET adlmididrv APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lpthread -Wl,-Bdynamic,--no-whole-archive")
    endif()
endif()

if(NOT MSVC)
    target_compile_options(adlmididrv PRIVATE "-Wno-cast-function-type")
    target_link_libraries(adlmididrv PRIVATE pthread)
endif()



#====================================
#        Install utility
#====================================

set(ADLMIDI_INSTALLER_SRC
    installer/drvinst.c
)

if(NOT MSVC)
    list(APPEND ADLMIDI_INSTALLER_SRC
        installer/drvinst.rc
    )
endif()

add_executable(drvsetup ${ADLMIDI_INSTALLER_SRC})

target_compile_definitions(drvsetup PRIVATE
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1
    -D_CRT_SECURE_NO_WARNINGS
)

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    set_property(TARGET drvsetup APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc")
endif()


#====================================
#  A unit test to verify the driver
#====================================

add_executable(drvtest test/test.c)
target_link_libraries(drvtest PRIVATE winmm)
target_compile_options(drvtest PRIVATE "-Wno-cast-function-type")


#====================================
#       Control panel applet
#====================================

set(ADLMIDI_DRIVER_SRC
    cpl/adlsetup.def
    cpl/adlsetup.rc
    cpl/setup_dialog.c
    setup/regsetup.c
)

add_library(adlmidisetup MODULE ${ADLMIDI_DRIVER_SRC} cpl/adlsetup.c)
set_target_properties(adlmidisetup PROPERTIES PREFIX "" OUTPUT_NAME "libadlsetup" SUFFIX ".cpl")
target_include_directories(adlmidisetup PRIVATE setup)
target_compile_definitions(adlmidisetup PRIVATE
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1
    -D_CRT_SECURE_NO_WARNINGS
    -D_UNICODE
    -DUNICODE
    -DADL_IS_CPL_APPLET
)

target_link_libraries(adlmidisetup PRIVATE comctl32 gdi32 user32)

add_executable(adlmidiconfigtool ${ADLMIDI_DRIVER_SRC} cpl/adlsetup-tool.c)
set_target_properties(adlmidiconfigtool PROPERTIES WIN32_EXECUTABLE ON)
target_include_directories(adlmidiconfigtool PRIVATE setup)
target_compile_definitions(adlmidiconfigtool PRIVATE
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1
    -D_CRT_SECURE_NO_WARNINGS
    -D_UNICODE
    -DUNICODE
)

target_link_libraries(adlmidiconfigtool PRIVATE comctl32 gdi32 user32)

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(adlmidisetup PRIVATE "-Wno-cast-function-type")
    target_compile_options(adlmidiconfigtool PRIVATE "-Wno-cast-function-type")
    set_property(TARGET adlmidisetup APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -Wl,--enable-stdcall-fixup")
    set_property(TARGET adlmidiconfigtool APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -Wl,--enable-stdcall-fixup")
endif()


#install(TARGETS adlmididrv
#        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
