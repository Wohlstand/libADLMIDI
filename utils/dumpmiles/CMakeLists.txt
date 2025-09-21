add_executable(adldumpmiles dumpmiles.cpp)

set_nopie(adldumpmiles)

install(TARGETS adldumpmiles
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(WIN32)
    set_property(TARGET adldumpmiles PROPERTY WIN32_EXECUTABLE OFF)
    if(MSVC)
        target_compile_definitions(adldumpmiles PRIVATE -DWIN32_CONSOLE)
    endif()
endif()
