include_directories(${GoogleTestInclude})

function(test_target target)
  set(test_src ${target}.cpp)

  add_executable(${target} ${test_src})

  target_link_libraries(${target} PRIVATE
    pathfinder
    gtest_main)
endfunction()
