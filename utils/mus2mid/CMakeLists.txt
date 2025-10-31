add_executable(mus2mid mus2mid.cpp cvt_mus2mid.hpp)

install(TARGETS mus2mid
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(WIN32)
    set_property(TARGET mus2mid PROPERTY WIN32_EXECUTABLE OFF)
    if(MSVC)
        target_compile_definitions(mus2mid PRIVATE -DWIN32_CONSOLE)
    endif()
endif()

if(ADLMIDI_DOS)
    set_target_properties(mus2mid PROPERTIES SUFFIX ".exe")
endif()
