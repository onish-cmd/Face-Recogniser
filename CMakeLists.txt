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

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/copy_libs.cmake" "
        execute_process(COMMAND ldd \${BIN} OUTPUT_VARIABLE ldd_output)
        string(REPLACE \"\\n\" \";\" lines \"\${ldd_output}\")
        foreach(line \${lines})
            if(line MATCHES \"=>[ \\t]+([^ ]+)\")
                set(lib_path \"\${CMAKE_MATCH_1}\")
                if(lib_path MATCHES \"libopencv\")
                    get_filename_component(lib_name \"\${lib_path}\" NAME)
                    get_filename_component(real_path \"\${lib_path}\" REALPATH)
                    configure_file(\"\${real_path}\" \"\${LIBS_DIR}/\${lib_name}\" COPYONLY)
                endif()
            endif()
        endforeach()
    ")

    add_custom_command(TARGET door_opener POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:door_opener>/libs"
        COMMAND ${CMAKE_COMMAND} -DBIN=$<TARGET_FILE:door_opener> -DLIBS_DIR=$<TARGET_FILE_DIR:door_opener>/libs -P "${CMAKE_CURRENT_BINARY_DIR}/copy_libs.cmake"
    )
endif()

if(WIN32)
    add_custom_command(TARGET door_opener POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:door_opener>" "$<TARGET_RUNTIME_DLLS:door_opener>"
        COMMAND_EXPAND_LISTS
    )
endif()
