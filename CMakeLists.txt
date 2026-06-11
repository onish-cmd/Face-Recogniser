cmake_minimum_required(VERSION 3.21)
project(ElectronicShieldDoor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenCV cleanly. vcpkg enforces CONFIG mode on Windows automatically.
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

add_executable(door_opener src/main.cpp)

# Link using the unified variables that both vcpkg and apt-get populate perfectly
target_link_libraries(door_opener PRIVATE 
    ${OpenCV_LIBS} 
    Threads::Threads
)

# Linux: Set RPATH for portable runtime loading
if(UNIX AND NOT APPLE)
    set_target_properties(door_opener PROPERTIES
        INSTALL_RPATH "$ORIGIN/libs"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

# Windows: Automatically find and deploy target DLLs next to your .exe
if(WIN32)
    add_custom_command(TARGET door_opener POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:door_opener>" "$<TARGET_RUNTIME_DLLS:door_opener>"
        COMMAND_EXPAND_LISTS
        COMMENT "Deploying runtime DLL dependencies..."
    )
endif()
