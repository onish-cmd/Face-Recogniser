cmake_minimum_required(VERSION 3.10)
project(AutomaticDoorOpener CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(${OpenCV_INCLUDE_DIRS})

# Define the executable
add_executable(door_opener src/main.cpp)

# Link libraries
target_link_libraries(door_opener 
    ${OpenCV_LIBS} 
    Threads::Threads
)

# Optimize for performance (Crucial for your E-350)
if(MSVC)
    target_compile_options(door_opener PRIVATE /O2)
else()
    target_compile_options(door_opener PRIVATE -O3)
endif()
