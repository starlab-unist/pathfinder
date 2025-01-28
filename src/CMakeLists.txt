find_package(Z3)
message(STATUS "Z3_FOUND: ${Z3_FOUND}")
message(STATUS "Found Z3 ${Z3_VERSION_STRING}")
message(STATUS "Z3_DIR: ${Z3_DIR}")

set(hdr_path "${PROJECT_SOURCE_DIR}/include")

configure_file (
  "${PROJECT_SOURCE_DIR}/include/duet.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/duet.h")

set(hdrs
    ${hdr_path}/branch_condition.h
    ${hdr_path}/engine.h
    ${hdr_path}/enum_solver.h
    ${hdr_path}/enumarg_bitvec.h
    ${hdr_path}/exectree.h
    ${hdr_path}/input_generator.h
    ${hdr_path}/input_signature.h
    ${hdr_path}/numeric_solver.h
    ${hdr_path}/options.h
    ${hdr_path}/pathfinder_defs.h
    ${hdr_path}/pathfinder.h
    ${hdr_path}/sygus_ast.h
    ${hdr_path}/sygus_gen.h
    ${hdr_path}/sygus_parser.h
    ${hdr_path}/trace_pc.h
    ${hdr_path}/utils.h)

set(srcs
    branch_condition.cpp
    driver.cpp
    engine.cpp
    enum_solver.cpp
    enumarg_bitvec.cpp
    exectree.cpp
    input_generator.cpp
    input_signature.cpp
    numeric_solver.cpp
    options.cpp
    sygus_ast.cpp
    sygus_gen.cpp
    sygus_parser.cpp
    trace_pc.cpp
    utils.cpp)

add_library(pathfinder STATIC ${srcs})
target_include_directories(pathfinder PUBLIC
    $<BUILD_INTERFACE:${hdr_path}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:${include_dest}>)
target_include_directories(pathfinder PUBLIC
    ${Z3_CXX_INCLUDE_DIRS})
target_link_libraries(pathfinder PRIVATE ${Z3_LIBRARIES})

install(FILES ${hdrs} DESTINATION "${include_dest}")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/duet.h DESTINATION "${include_dest}")
install(TARGETS pathfinder EXPORT pathfinder DESTINATION "${lib_dest}")
