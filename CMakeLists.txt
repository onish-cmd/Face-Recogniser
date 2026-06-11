cmake_minimum_required(VERSION 3.21)
project(ElectronicShieldDoor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/permissive-)
endif()

find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

add_executable(door_opener src/main.cpp)

target_link_libraries(door_opener PRIVATE 
    ${OpenCV_LIBS} 
    Threads::Threads
)

if(UNIX AND NOT APPLE)
    set_target_properties(door_opener PROPERTIES
        INSTALL_RPATH "$ORIGIN/libs"
        BUILD_WITH_INSTALL_RPATH TRUE
    )

    add_custom_command(TARGET door_opener POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:door_opener>/libs"
        COMMAND bash -c "ldd $<TARGET_FILE:door_opener> | grep libopencv | awk '{print $$3}' | xargs -I {} cp -L {} $<TARGET_FILE_DIR:door_opener>/libs/"
    )
endif()

if(WIN32)
    add_custom_command(TARGET door_opener POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:door_opener>" "$<TARGET_RUNTIME_DLLS:door_opener>"
        COMMAND_EXPAND_LISTS
    )
endif()
