cmake_minimum_required(VERSION 3.15)
project(HackTerm CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB HACKTERM_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
add_executable(hackterm ${HACKTERM_SOURCES})

if(MSVC)
    target_compile_options(hackterm PRIVATE /W4 /utf-8 /EHsc)
    target_compile_definitions(hackterm PRIVATE UNICODE _UNICODE)
    target_link_libraries(hackterm PRIVATE user32)
else()
    target_compile_options(hackterm PRIVATE -Wall -Wextra)
    target_link_options(hackterm PRIVATE -static)
endif()

# Самопроверка не-UI частей: cmake --build . --target selftest && selftest child.exe
add_executable(child tests/child.cpp)
add_executable(selftest tests/selftest.cpp
    src/Common.cpp src/Runner.cpp src/Compiler.cpp src/Editor.cpp
    src/OutputPane.cpp src/Syntax.cpp src/Console.cpp)
target_link_libraries(selftest PRIVATE shell32)
if(MSVC)
    target_compile_options(selftest PRIVATE /utf-8)
else()
    target_link_options(selftest PRIVATE -static)
    target_link_options(child PRIVATE -static)
endif()
