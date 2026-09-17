cmake_minimum_required(VERSION 3.20)
project(mold VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(mold
  src/sort.cpp
  src/lexer.cpp
  src/parser.cpp
  src/ast.cpp
  src/typing.cpp
  src/compiler.cpp
  src/ranking.cpp
  src/interpreter.cpp
  src/script.cpp
  src/builtins.cpp
)

target_include_directories(mold PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(mold PUBLIC cxx_std_23)

target_compile_options(mold PRIVATE
  -Wall
  -Wextra
)

target_compile_options(mold PRIVATE
  $<$<CONFIG:Debug>:-g3 -fsanitize=address,undefined>
)

target_link_options(mold PUBLIC
  $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(test)
endif()
