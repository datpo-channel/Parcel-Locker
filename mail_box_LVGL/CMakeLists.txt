cmake_minimum_required(VERSION 3.10)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-linux-gcc)

project(mail_box_pic C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(JPEG_INCLUDE_PATH "/home/gec/Worksapce/jpegsrc.v9f/jpeg-9f/include")
set(FREETYPE_INCLUDE_PATH "/home/gec/freetype-2.14.3/include")
set(FREETYPE_LIB_PATH "/home/gec/freetype-2.14.3/objs/.libs")

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lvgl
    ${JPEG_INCLUDE_PATH}
    ${FREETYPE_INCLUDE_PATH}
)

link_directories(${CMAKE_SOURCE_DIR}/lib)

file(GLOB_RECURSE LVGL_SOURCES
    "lvgl/src/*.c"
)

file(GLOB SOURCES
    "src/*.c"
)

add_executable(${PROJECT_NAME} ${SOURCES} ${LVGL_SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE)

target_link_libraries(${PROJECT_NAME}
    ${CMAKE_SOURCE_DIR}/lib/libjpeg.a
    ${CMAKE_SOURCE_DIR}/lib/libqrencode.a
    ${FREETYPE_LIB_PATH}/libfreetype.a
    pthread
    m
    dl
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY resource/ DESTINATION share/${PROJECT_NAME}/resource)

set(DEPLOY_HOST "root@192.168.33.69")
set(DEPLOY_PATH "/Workspace/mail_box_pic")

add_custom_target(deploy
    COMMAND scp ${CMAKE_BINARY_DIR}/${PROJECT_NAME} ${DEPLOY_HOST}:${DEPLOY_PATH}/${PROJECT_NAME}
    DEPENDS ${PROJECT_NAME}
    COMMENT "Deploying to ${DEPLOY_HOST}:${DEPLOY_PATH}"
)