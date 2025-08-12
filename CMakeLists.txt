cmake_minimum_required(VERSION 3.15)
set(PRJ flexptp_linux)
project(${PRJ} C)

set(CMAKE_C_STANDARD 11)

add_executable(${PRJ}
        Src/cli/cli.c
        Src/cli/cli.h
        Src/cli/term_colors.h
        Src/main.c
        Inc/flexptp_options.h
)

set(DEFS "_GNU_SOURCE=1")

target_compile_definitions(${PRJ} PUBLIC ${DEFS})
target_include_directories(${PRJ} PUBLIC Modules)

set(FLEXPTP_COMPILE_DEFS ${DEFS})
set(FLEXPTP_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/Inc ${CMAKE_CURRENT_LIST_DIR}/Src ${CMAKE_CURRENT_LIST_DIR}/Modules)
set(FLEXPTP_HWPORT LINUX)
set(FLEXPTP_NSD LINUX)
set(FLEXPTP_SERVO KALMAN)
add_subdirectory(Modules/flexPTP)
target_link_libraries(${PRJ} flexptp)
target_include_directories(${PRJ} PUBLIC ${FLEXPTP_INCLUDE_EXPORT} ${FLEXPTP_INCLUDES})