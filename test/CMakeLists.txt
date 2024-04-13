cmake_minimum_required(VERSION 3.5)
project(sensor-logger-test C)

set(CMAKE_C_STANDARD 99)
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
endif()

add_library(sensor_logger_lib
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/slog.c
)

enable_testing()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/unity-2.5.2/src/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_executable(test_sensor_logger
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sensor_logger.c
#        ${CMAKE_CURRENT_SOURCE_DIR}/unity-2.5.2/src/unity.c
)
target_link_libraries(test_sensor_logger sensor_logger_lib)
add_test(NAME test_sensor_logger COMMAND test_sensor_logger)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_custom_target("coverage")
    get_filename_component(baseDir "${CMAKE_CURRENT_SOURCE_DIR}/.." REALPATH BASE_DIR)
    add_custom_command(TARGET "coverage" COMMAND gcov ${CMAKE_BINARY_DIR}/CMakeFiles/sensor_logger_lib.dir/${baseDir}/src/*.c.o)
endif()