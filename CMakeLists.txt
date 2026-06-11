cmake_minimum_required(VERSION 3.10)
project(ElectronicShieldDoor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

add_executable(door_opener src/main.cpp)
target_link_libraries(door_opener ${OpenCV_LIBS} Threads::Threads)

if(UNIX AND NOT APPLE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/libs")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

foreach(LIB ${OpenCV_LIBS})
    if(EXISTS "${LIB}")
        add_custom_command(TARGET door_opener POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LIB}" "$<TARGET_FILE_DIR:door_opener>/"
            COMMENT "Copying dependency: ${LIB}"
        )
    endif()
endforeach()
