cmake_minimum_required(VERSION 3.21)
project(ElectronicShieldDoor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_executable(door_opener src/main.cpp)

target_link_libraries(door_opener PRIVATE 
    opencv_core 
    opencv_imgproc
    opencv_videoio
    opencv_highgui
    opencv_objdetect
    opencv_face
    Threads::Threads
)

if(UNIX AND NOT APPLE)
    set_target_properties(door_opener PROPERTIES
        INSTALL_RPATH "$ORIGIN/libs"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

if(WIN32)
    add_custom_command(TARGET door_opener POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:door_opener>" "$<TARGET_RUNTIME_DLLS:door_opener>"
        COMMAND_EXPAND_LISTS
        COMMENT "Deploying runtime DLL dependencies automatically..."
    )
endif()
