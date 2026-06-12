cmake_minimum_required(VERSION 3.16)
project(electronic-shield-door VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenCV REQUIRED)

file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.hpp" "src/*.h")

add_executable(door_opener ${SRC_FILES})

target_include_directories(door_opener PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(door_opener PRIVATE ${OpenCV_LIBS})

if(WIN32)
    install(TARGETS door_opener
        RUNTIME DESTINATION .
        RUNTIME_DEPENDENCIES_SET windows_dlls
    )

    install(RUNTIME_DEPENDENCY_SET windows_dlls
        DESTINATION .
    )

elseif(UNIX AND NOT APPLE)
    set_target_properties(door_opener PROPERTIES
        INSTALL_RPATH "\$ORIGIN/libs"
        BUILD_WITH_INSTALL_RPATH TRUE
        LINK_FLAGS "-Wl,-rpath-link,${CMAKE_CURRENT_BINARY_DIR}/libs"
    )

    install(TARGETS door_opener DESTINATION .)

    foreach(lib ${OpenCV_LIBS})
        if(TARGET ${lib})
            get_target_property(lib_path ${lib} LOCATION)
        else()
            set(lib_path ${lib})
        endif()

        if(EXISTS "${lib_path}")
          get_filename_component(link_name "${lib_path}" NAME)

          get_filename_component(real_path "${lib_path}" REALPATH)
          get_filename_component(real_name "${real_path}" NAME)

          install(FILES "${real_path}" DESTINATION libs)

          if (NOT "${link_name}" STREQUAL "${real_name}")
            install(CODE "
              execute_process(
                COMMAND ${CMAKE_COMMAND} -E create_symlink \"${real_name}\" \"${link_name}\"
                WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/libs\"
              )
            ")
          endif()
        endif()
    endforeach()
endif()
