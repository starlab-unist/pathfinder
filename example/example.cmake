include_directories(${PathFinderInclude})

add_library(lib_external OBJECT lib_external.cpp)
target_include_directories(lib_external PUBLIC
    ${PROJECT_SOURCE_DIR})
target_compile_options(lib_external PRIVATE -g -O0)

function(example_fuzz_target target)
  set(fuzz_target_src ${target}.cpp)

  add_executable(${target} ${fuzz_target_src} utils.cpp)
  target_compile_options(${target} PRIVATE -g -O0 -fsanitize=undefined)
  target_compile_options(${target} PRIVATE -fsanitize-coverage=edge,trace-pc-guard)
  # To prevent missing basic blocks. See https://github.com/google/sanitizers/issues/783.
  target_compile_options(${target} PRIVATE -mllvm -sanitizer-coverage-prune-blocks=0)

  target_link_libraries(${target} PRIVATE
    pathfinder
    lib_external)
  target_link_options(${target} PRIVATE -fsanitize=undefined)
endfunction()
