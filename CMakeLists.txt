cmake_minimum_required(VERSION 4.3)
project(volume-locker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(volume-locker WIN32 main.cpp)

if (MSVC)
    target_compile_options(volume-locker PRIVATE
            /O1
            /GL
            /Gy
            /GS-
            /MT
            /EHsc
            /permissive-
            /W4
    )

    target_link_options(volume-locker PRIVATE
            /LTCG
            /OPT:REF
            /OPT:ICF
            /INCREMENTAL:NO
            /SUBSYSTEM:WINDOWS
    )
endif ()

target_link_libraries(volume-locker PRIVATE
        ole32
        WindowsApp
)
