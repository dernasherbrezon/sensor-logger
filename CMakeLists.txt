set(srcs
        "${CMAKE_CURRENT_SOURCE_DIR}/src/slog.c"
)
# When running from IDF build it as a component
if (IDF_TARGET)
    idf_component_register(SRCS "${srcs}" INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include")
else()
    add_library(sensor_logger_lib STATIC ${srcs})
    target_include_directories(sensor_logger_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
endif()
