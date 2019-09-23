add_executable(adldumpbank dumpbank.cpp)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set_property(TARGET adldumpbank APPEND_STRING PROPERTY LINK_FLAGS " -no-pie")
endif()

install(TARGETS adldumpbank
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

