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

target_include_directories(${PRJ} PUBLIC Modules)

set(FLEXPTP_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/Inc ${CMAKE_CURRENT_LIST_DIR}/Src ${CMAKE_CURRENT_LIST_DIR}/Modules)
set(FLEXPTP_HWPORT LINUX)
set(FLEXPTP_NSD LINUX)
set(FLEXPTP_SERVO KALMAN)
add_subdirectory(Modules/flexPTP)
target_link_libraries(${PRJ} flexptp)
target_include_directories(${PRJ} PUBLIC ${FLEXPTP_INCLUDE_EXPORT} ${FLEXPTP_INCLUDES})

if (${HAVE_CLOCK_ADJTIME})
    target_compile_definitions(flexptp PUBLIC HAVE_CLOCK_ADJTIME=1)
endif ()