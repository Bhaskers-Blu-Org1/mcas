cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(threadipc CXX C)

add_subdirectory(unit_test)

set(CMAKE_CXX_STANDARD 14)
add_compile_options(-g -pedantic -Wall -Wextra -Werror -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings -Wformat=2)
add_compile_options("$<$<CONFIG:Debug>:-O0>")
add_definitions(-DCONFIG_DEBUG)

include_directories(./include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")
include_directories(${CMAKE_INSTALL_PREFIX}/include) # daxctl

link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) # tbb tbbmalloc
link_directories(${CMAKE_INSTALL_PREFIX}/lib) # tbb tbbmalloc

#set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

file(GLOB SOURCES src/*.c*)

add_library(threadipc SHARED ${SOURCES})

target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Debug>:-O0>")
target_link_libraries(${PROJECT_NAME} common)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
install(DIRECTORY "src/" DESTINATION "include/threadipc" FILES_MATCHING PATTERN "./include/threadipc/*")


# # force early install because others depend on this.  this seems a hack. is there
# # a better way?
# #
# add_custom_command(
#   TARGET ${PROJECT_NAME} POST_BUILD
#   COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/include/ccpm
#   COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/lib
#   COMMAND cp ${PROJECT_SOURCE_DIR}/src/*.h ${CMAKE_INSTALL_PREFIX}/include/ccpm
#   COMMAND cp -r ${PROJECT_SOURCE_DIR}/src/x86_64 ${CMAKE_INSTALL_PREFIX}/include/ccpm
#   COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/libccpm.so ${CMAKE_INSTALL_PREFIX}/lib
#   )

