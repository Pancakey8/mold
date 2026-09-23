set(TEST_SOURCES
  fibonacci.cpp
  alert.cpp
  functions.cpp
  ambiguity.cpp
  readme-01.cpp
  readme-02.cpp
  builtins.cpp
  purity.cpp
  name_res.cpp
  null_compat.cpp
  annotations.cpp
  doc-30-example-1.cpp
  self-trigger.cpp
  structs.cpp
  struct_ops.cpp
  null_ops.cpp
  rank_zero.cpp
)

foreach(test_src ${TEST_SOURCES})
  get_filename_component(test_name ${test_src} NAME_WE)
  add_executable(${test_name} ${test_src})
  target_link_libraries(${test_name} PRIVATE mold)
  add_test(NAME ${test_name} COMMAND ${test_name})
  set_tests_properties(${test_name} PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
endforeach()
