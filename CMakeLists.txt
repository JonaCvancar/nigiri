cmake_minimum_required(VERSION 3.22)
project(nigiri)

if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
  if(NIGIRI_MIMALLOC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
endif()

include(cmake/buildcache.cmake)
include(cmake/pkg.cmake)


# --- GTFS-RT PROTOBUF ---
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/gtfsrt/gtfs-realtime.pb.h
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/gtfsrt/gtfs-realtime.pb.cc
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:protoc>
    --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/generated/gtfsrt
    --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gtfs-realtime.proto
    DEPENDS protoc
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gtfs-realtime.proto
    COMMENT "Generating GTFS RT protocol buffers C++ code"
)
add_library(gtfsrt ${CMAKE_CURRENT_BINARY_DIR}/generated/gtfsrt/gtfs-realtime.pb.cc)
target_compile_features(gtfsrt PUBLIC cxx_std_23)
target_include_directories(gtfsrt SYSTEM PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(gtfsrt protobuf)


# --- LINT ---
option(NIGIRI_LINT "Run clang-tidy with the compiler." OFF)
if(NIGIRI_LINT)
  # clang-tidy will be run on all targets defined hereafter
  include(cmake/clang-tidy.cmake)
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(nigiri-compile-options
      -Weverything
      -Wno-c++98-compat
      -Wno-c++98-compat-pedantic
      -Wno-newline-eof
      -Wno-missing-prototypes
      -Wno-padded
      -Wno-double-promotion
      -Wno-undef
      -Wno-undefined-reinterpret-cast
      -Wno-float-conversion
      -Wno-global-constructors
      -Wno-exit-time-destructors
      -Wno-switch-enum
      -Wno-c99-designator
      -Wno-zero-as-null-pointer-constant
      -Wno-missing-noreturn
      -Wno-undefined-func-template
      -Wno-unsafe-buffer-usage
          -Wno-unused-parameter
          -Wno-unused-variable
      -Wno-c++20-compat
      -Werror
      -march=native
      -mtune=native
  )
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
  set(nigiri-compile-options -Wall -Wextra -Werror)
elseif(MSVC)
  set(nigiri-compile-options /WX)
else()
  set(nigiri-compile-options
      -Wall
      -Wextra
      -Wno-maybe-uninitialized)
  if(NOT CMAKE_CROSSCOMPILING)
    set(nigiri-compile-options ${nigiri-compile-options} -Werror)
  endif()
endif()


# --- LIB ---
file(GLOB_RECURSE nigiri-files src/*.cc)
add_library(nigiri ${nigiri-files})
target_include_directories(nigiri PUBLIC include)
target_link_libraries(nigiri PUBLIC cista geo utl fmt date miniz date-tz wyhash unordered_dense gtfsrt)
target_compile_features(nigiri PUBLIC cxx_std_23)
target_compile_options(nigiri PRIVATE ${nigiri-compile-options})


# --- MAIN ---
file(GLOB_RECURSE nigiri-server-files server/main.cc)
add_executable(nigiri-server ${nigiri-server-files})
target_link_libraries(nigiri-server PRIVATE nigiri)
target_compile_features(nigiri-server PUBLIC cxx_std_23)


# --- TEST ---
add_library(nigiri-generated INTERFACE)
target_include_directories(nigiri-generated INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/generated)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test/test_dir.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/test_dir.h
)
#file(GLOB_RECURSE nigiri-test-files test/*.cc)
set(nigiri-test-files
        test/main.cc
        test/routing/tripbased/tb_query_test.cc
        test/routing/tripbased/test_data.cc
)
add_executable(nigiri-test ${nigiri-test-files})
target_link_libraries(nigiri-test nigiri gtest ianatzdb-res nigiri-generated)
target_compile_options(nigiri-test PRIVATE ${nigiri-compile-options})

# --- TEST ---
set(nigiri-test-files2
        test/main.cc
        test/routing/tripbased/test_data.cc
        test/routing/tripbased/tb_correctness_check.cc
        test/raptor_search.cc
)
add_executable(nigiri-test-all ${nigiri-test-files2})
target_link_libraries(nigiri-test-all nigiri gtest ianatzdb-res nigiri-generated)
target_compile_options(nigiri-test-all PRIVATE ${nigiri-compile-options})

# -- PERFORMANCE --
file(GLOB_RECURSE aachen_files tripbased_performance/aachen.cc)
add_executable(aachen ${aachen_files})
target_link_libraries(aachen PRIVATE nigiri)
target_compile_features(aachen PUBLIC cxx_std_23)
file(GLOB_RECURSE vbb_files tripbased_performance/vbb.cc)
add_executable(vbb ${vbb_files})
target_link_libraries(vbb PRIVATE nigiri)
target_compile_features(vbb PUBLIC cxx_std_23)
file(GLOB_RECURSE germany_files tripbased_performance/germany.cc)
add_executable(germany ${germany_files})
target_link_libraries(germany PRIVATE nigiri)
target_compile_features(germany PUBLIC cxx_std_23)

# -- ONETOALL --
file(GLOB_RECURSE aachen_files_oa tripbased_onetoall/aachen_oa.cc)
add_executable(aachen_oa ${aachen_files_oa})
target_link_libraries(aachen_oa PRIVATE nigiri)
target_compile_features(aachen_oa PUBLIC cxx_std_23)

file(GLOB_RECURSE aachen_query_files_oa tripbased_onetoall/aachen_oa_query.cc)
add_executable(aachen_oa_query ${aachen_query_files_oa})
target_link_libraries(aachen_oa_query PRIVATE nigiri)
target_compile_features(aachen_oa_query PUBLIC cxx_std_23)

file(GLOB_RECURSE vbb_files_oa tripbased_onetoall/vbb_oa.cc)
add_executable(vbb_oa ${vbb_files_oa})
target_link_libraries(vbb_oa PRIVATE nigiri)
target_compile_features(vbb_oa PUBLIC cxx_std_23)

file(GLOB_RECURSE vbb_query_files_oa tripbased_onetoall/vbb_oa_query.cc)
add_executable(vbb_oa_query ${vbb_query_files_oa})
target_link_libraries(vbb_oa_query PRIVATE nigiri)
target_compile_features(vbb_oa_query PUBLIC cxx_std_23)

file(GLOB_RECURSE generate_test_stop_files tripbased_onetoall/generate_test_stops.cpp)
add_executable(generate_test_stop ${generate_test_stop_files})
target_compile_features(generate_test_stop PUBLIC cxx_std_23)
