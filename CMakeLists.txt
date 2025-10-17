cmake_minimum_required(VERSION 3.20)
project(volume-locker)

set(CMAKE_WIN32_EXECUTABLE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} main.cpp)

target_compile_options(${PROJECT_NAME} PRIVATE
    /MT
    /EHsc
    /D_WIN32_WINNT=0x0A00
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    WindowsApp.lib
    user32.lib
)
