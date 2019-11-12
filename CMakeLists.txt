project(imagemarker)
cmake_minimum_required(VERSION 2.8)
set(CMAKE_CXX_STANDARD 14)

find_package(Boost REQUIRED
COMPONENTS date_time system thread program_options
)
find_package(OpenCV REQUIRED
COMPONENTS core highgui
)
include_directories(${Boost_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS})

aux_source_directory(. SRC_LIST)

add_executable(server server.cpp common.cpp image_processor.cpp)
add_executable(client client.cpp common.cpp)

target_link_libraries(server ${Boost_LIBRARIES} ${OpenCV_LIBRARIES})
target_link_libraries(client ${Boost_LIBRARIES})

