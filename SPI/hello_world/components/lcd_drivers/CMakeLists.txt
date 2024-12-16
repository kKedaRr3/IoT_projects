idf_component_register(
    SRCS "ST7735.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)