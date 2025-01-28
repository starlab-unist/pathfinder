cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(
  PathFinder
  VERSION 1.0
  LANGUAGES CXX)

if(NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  message(FATAL_ERROR "Unsupported compiler `${CMAKE_CXX_COMPILER_ID}`. Currently PathFinder only supports clang.")
endif()

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Build type not set, default is Release")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build from: Debug Release." FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (DUET_BIN_PATH)
  if (NOT EXISTS ${DUET_BIN_PATH})
    message(FATAL_ERROR "Invalid DUET bin path `${DUET_BIN_PATH}`.")
  endif()
else()
  message(WARNING "DUET_BIN_PATH is not set. Build PathFinder as run-only mode.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(GoogleTest_SRC_DIR "${PROJECT_SOURCE_DIR}/third_party/googletest" CACHE STRING "googletest directory")
set(GoogleTestInclude "${GoogleTest_SRC_DIR}/googletest/include")
add_subdirectory("${GoogleTest_SRC_DIR}")

set(include_dest "${CMAKE_INSTALL_INCLUDEDIR}/pathfinder")
set(lib_dest "${CMAKE_INSTALL_LIBDIR}/pathfinder")
add_subdirectory(src)
install(FILES pathfinder-config.cmake DESTINATION "${lib_dest}")
install(EXPORT pathfinder DESTINATION "${lib_dest}")

if("${CMAKE_SOURCE_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
  add_subdirectory(example)
  add_subdirectory(test)
endif()
