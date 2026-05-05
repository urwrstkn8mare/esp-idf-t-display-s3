cmake_minimum_required(VERSION 3.16)

if (NOT DEFINED PROJECT_NAME)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    idf_build_set_property(MINIMAL_BUILD ON)
    project(esp_idf_t_display_s3)
else()
    message(FATAL_ERROR "esp_idf_t_display_s3: This must be a project's main CMakeLists.txt.")
endif()
