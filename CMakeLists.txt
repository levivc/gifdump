cmake_minimum_required(VERSION 3.0.0)
project(gifdump VERSION 0.9.1 LANGUAGES C)
string(TOUPPER ${PROJECT_NAME} _PROJECT_NAME_UPPER)

include(TestBigEndian)
test_big_endian(_IS_BIG_ENDIAN)
if(_IS_BIG_ENDIAN)
    set(_BYTEORDER BIG_ENDIAN)
else()
    set(_BYTEORDER LIL_ENDIAN)
endif()

configure_file(config.h.in config.h)

set(_GIFDUMP_SOURCES
    main.c gifdump.c parg.c)
set(_GIFDUMP_HEADERS
    gifdump.h common.h parg.h ${PROJECT_BINARY_DIR}/config.h)

add_executable(${PROJECT_NAME}
    ${_GIFDUMP_SOURCES}
    ${_GIFDUMP_HEADERS})

set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 11
    C_STANDDARD_REQUIRED ON
    C_EXTENSIONS OFF)

if(MSVC)
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
            _CRT_SECURE_NO_WARNINGS)
endif()

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR})

install(
    TARGETS ${PROJECT_NAME})
