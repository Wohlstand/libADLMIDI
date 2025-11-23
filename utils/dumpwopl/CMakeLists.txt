add_executable(adldumpwopl
    dumpwopl.c
    ../../src/wopl/wopl_file.c
    ../../src/wopl/wopl_file.h
)

set_nopie(adldumpwopl)

install(TARGETS adldumpwopl
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(WIN32)
    set_property(TARGET adldumpwopl PROPERTY WIN32_EXECUTABLE OFF)
    if(MSVC)
        target_compile_definitions(adldumpwopl PRIVATE -DWIN32_CONSOLE)
    endif()
endif()
