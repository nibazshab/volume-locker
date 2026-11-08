cmake_minimum_required(VERSION 4.3)
project(volume-locker CXX)

set(CMAKE_WIN32_EXECUTABLE ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} main.cpp)

target_compile_options(${PROJECT_NAME} PRIVATE
    /W4 /permissive- /utf-8
    $<$<CONFIG:Release,MinSizeRel>:/O1 /GL /Gy /GS->
    $<$<CONFIG:Debug>:/Od /RTC1 /Zi>
)

target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release,MinSizeRel>:/LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    advapi32
    ole32
)
