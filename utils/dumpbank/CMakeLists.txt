add_executable(adldumpbank dumpbank.cpp)

set_nopie(adldumpbank)

install(TARGETS adldumpbank
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(WIN32)
    set_property(TARGET adldumpbank PROPERTY WIN32_EXECUTABLE OFF)
endif()
