add_executable(adldumpmiles dumpmiles.cpp)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set_property(TARGET adldumpmiles APPEND_STRING PROPERTY LINK_FLAGS " -no-pie")
endif()

install(TARGETS adldumpmiles
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

