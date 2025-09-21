add_executable(mus2mid mus2mid.cpp)
# TODO: Use own library
target_include_directories(mus2mid PRIVATE ${PROJECT_SOURCE_DIR}/src)

install(TARGETS mus2mid
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(WIN32)
    set_property(TARGET mus2mid PROPERTY WIN32_EXECUTABLE OFF)
    if(MSVC)
        target_compile_definitions(mus2mid PRIVATE -DWIN32_CONSOLE)
    endif()
endif()
