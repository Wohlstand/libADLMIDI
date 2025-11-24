set(CMAKE_C_STANDARD 99)

if(APPLE)
    find_library(LIBVLC_LIBRARY vlc REQUIRED PATHS /Applications/VLC.app/Contents/MacOS/lib)
    find_library(LIBVLCCORE_LIBRARY vlccore REQUIRED PATHS /Applications/VLC.app/Contents/MacOS/lib)
    set(VLCPLUGIN_CODEC_INSTALL_PATH "/Applications/VLC.app/Contents/MacOS/plugins/")
else()
    find_package(LIBVLC REQUIRED)
endif()

add_library(adlmidi_vlc_plugin MODULE libadlmidi.c)
set_target_properties(adlmidi_vlc_plugin PROPERTIES OUTPUT_NAME adlmidi_plugin)
target_compile_definitions(adlmidi_vlc_plugin PUBLIC
    "VLC_MODULE_COPYRIGHT=\"Copyright (c) Vitaly Novichkov\""
    "VLC_MODULE_LICENSE=\"GPLv3\""
    "MODULE_STRING=\"adlmidi\""
)

if(APPLE)
    target_include_directories(adlmidi_vlc_plugin PRIVATE
        "/Applications/VLC.app/Contents/MacOS/include"
        "/Users/vitaly/Repos/vlc-3.0.21/include"
    )
    target_link_libraries(adlmidi_vlc_plugin PRIVATE ADLMIDI_static ${LIBVLCCORE_LIBRARY} ${LIBVLC_LIBRARY})
else()
    target_link_libraries(adlmidi_vlc_plugin PRIVATE ADLMIDI_static libvlc::plugin m)
endif()

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    set_property(TARGET adlmidi_vlc_plugin APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -static-libstdc++")
    set_property(TARGET adlmidi_vlc_plugin APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lpthread -Wl,-Bdynamic,--no-whole-archive")
endif()

if(NOT VLC_PLUGIN_NOINSTALL)
    message("VLC install path: ${VLCPLUGIN_CODEC_INSTALL_PATH} ${LIBVLCCORE_LIBRARY} ${LIBVLC_LIBRARY}")
    install(TARGETS adlmidi_vlc_plugin DESTINATION "${VLCPLUGIN_CODEC_INSTALL_PATH}")
else()
    install(TARGETS adlmidi_vlc_plugin DESTINATION "${CMAKE_INSTALL_LIBDIR}/vlc-codec/")
endif()
