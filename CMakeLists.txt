project(ShuffleMoveHelper)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")

#SET("OpenCV_DIR" "D:/usr/opencv2413/opencv/build/")
find_package(OpenCV REQUIRED)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})

add_executable(adjustBoundary src/adjustBoundary.cpp)
target_link_libraries(adjustBoundary ${OpenCV_LIBS})