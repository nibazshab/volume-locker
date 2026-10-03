cmake_minimum_required(VERSION 4.3)
project(volume-locker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(volume-locker WIN32 main.cpp)

target_compile_options(volume-locker PRIVATE
        /W4 /permissive- /Zi
        $<$<CONFIG:Release,MinSizeRel>:/O1 /GL /Gy /GS- /MD>
        $<$<CONFIG:Debug>:/Od /RTC1 /MDd>
)

target_link_options(volume-locker PRIVATE
        $<$<CONFIG:Release,MinSizeRel>:/LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO>
)
