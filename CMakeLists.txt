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
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/copy_linux_deps.cmake"
        COMMENT "Resolving and copying OpenCV .so libraries to local libs/ folder..."
    )

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/copy_linux_deps.cmake" "
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES \"$<TARGET_FILE:door_opener>\"
            RESOLVED_DEPENDENCIES_VAR resolved_deps
            UNRESOLVED_DEPENDENCIES_VAR unresolved_deps
        )
        foreach(file \${resolved_deps})
            # Only isolate the OpenCV libraries, leave core system glibc alone
            if(file MATCHES \"libopencv\")
                file(COPY \${file} DESTINATION \"$<TARGET_FILE_DIR:door_opener>/libs\")
            endif()
        endforeach()
    ")
endif()

if(WIN32)
    add_custom_command(TARGET door_opener POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:door_opener>" "$<TARGET_RUNTIME_DLLS:door_opener>"
        COMMAND_EXPAND_LISTS
        COMMENT "Deploying runtime DLL dependencies..."
    )
endif()
