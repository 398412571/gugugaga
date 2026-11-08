cmake_minimum_required(VERSION 3.12)
project(ArmorPlateVision LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # 源码里有中文注释，MSVC 需要显式指定 UTF-8，避免 C4819 / 乱码。
    add_compile_options(/utf-8)
endif()

find_package(OpenCV REQUIRED)

add_executable(task1_hsv_mask src/task1_hsv_mask.cpp)
target_include_directories(task1_hsv_mask PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(task1_hsv_mask PRIVATE ${OpenCV_LIBS})

add_executable(task2_armor_detection src/task2_armor_detection.cpp)
target_include_directories(task2_armor_detection PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(task2_armor_detection PRIVATE ${OpenCV_LIBS})
