cmake_minimum_required(VERSION 3.0)

project(PathSensitiveAnalysis)

enable_testing()

# Using C++11
set(CMAKE_CXX_STANDARD 11)
# The following cxxflags seem to be necessary
SET(CMAKE_CXX_FLAGS "-Wall -fno-rtti")

# Detect compiler & make sure clang/clang++ is used
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # using Clang
    message(STATUS "Your compiler is : ${CMAKE_CXX_COMPILER}")
else()
    message(FATAL_ERROR "Your compiler is : ${CMAKE_CXX_COMPILER}, please use clang/clang++")
endif()

# please make sure `llvm-config` is accessible (i.e., have been added
# to the environment path)
if(LLVM_ROOT_PATH)
    message(STATUS "Got LLVM_ROOT_PATH : ${LLVM_ROOT_PATH}")
    set(LLVM_BIN_PATH "${LLVM_ROOT_PATH}/bin")
else()
    message(STATUS "LLVM_ROOT_PATH was not provided, we assume that all llvm related commands (e.g., llvm-config, and opt) are directedly accessible")
endif()

if (LLVM_BIN_PATH)
    execute_process(COMMAND bash -c "${LLVM_BIN_PATH}/llvm-config --includedir"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE llvm_include)
else()
    execute_process(COMMAND bash -c "llvm-config --includedir"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE llvm_include)
endif()

# check results
if(result)
    message(FATAL_ERROR "llvm-config failed: ${result}")
else()
    message(STATUS "Got llvm_include : ${llvm_include}")
    include_directories(${llvm_include})
endif()


if (LLVM_BIN_PATH)
    execute_process(COMMAND bash -c "${LLVM_BIN_PATH}/llvm-config --libdir"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE llvm_lib)
else()
    execute_process(COMMAND bash -c "llvm-config --libdir"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE llvm_lib)
endif()

if(result)
    message(FATAL_ERROR "llvm-config failed: ${result}")
else()
    message(STATUS "Got llvm_lib : ${llvm_lib}")
    link_directories(${llvm_lib})
endif()


INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR})
# Function for adding test for @passName with @testFiles.
function(add_llvm_pass_tests libName passName testFiles)
    foreach(test_f ${testFiles})
        message(STATUS "Adding test for ${test_f} for pass ${passName} provided in lib ${libName} ...")
        if (LLVM_BIN_PATH)
            add_test(
                    NAME "${libname}TestOn${test_f}"
                    COMMAND bash -c "${LLVM_BIN_PATH}/opt -load ${libName} -${passName} -analyze ${test_f}"
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        else()
            add_test(
                    NAME "${libname}TestOn${test_f}"
                    COMMAND bash -c "opt -load ${libName} -${passName} -analyze ${test_f}"
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        endif()
    endforeach()
endfunction()

# Copy all "*.ll" test files to the destination directory
# Note that you can add other file types (e.g., *.bc) if you want to use them as test files.
file(GLOB TEST_FILES "${PROJECT_SOURCE_DIR}/tests/*.ll")
foreach(test_file ${TEST_FILES})
    get_filename_component(file_we ${test_file} NAME)
    message(STATUS "Copying ${file_we} ...")
    set(dest_file "${PROJECT_BINARY_DIR}/test/${file_we}")
    configure_file("${test_file}" "${dest_file}" COPYONLY)
    list(APPEND test_filenames "${dest_file}")
endforeach()
message(STATUS "Get the following test files: ${test_filenames}")


# copy benchmark to build
file(COPY "${PROJECT_SOURCE_DIR}/benchmarks" DESTINATION "${PROJECT_BINARY_DIR}")
file(COPY "${PROJECT_SOURCE_DIR}/scripts" DESTINATION "${PROJECT_BINARY_DIR}")
file(GLOB HEADERS "*.hpp" "*.h" "${PROJECT_SOURCE_DIR}/dependencies/*.hpp")


include_directories("${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/dependencies")
#
set(src "${PROJECT_SOURCE_DIR}/src/pathSensitiveAnalysis.cpp")
get_filename_component(src_we ${src} NAME_WE)
set(PASS_NAME "${src_we}")
set(libname "${src_we}")
message(STATUS "adding a library for ${src} named ${libname}")
configure_file(
        "${PROJECT_SOURCE_DIR}/configs/PathSensitiveAnalysis.h.in"
        "${PROJECT_BINARY_DIR}/PathSensitiveAnalysisConfig.h"
)
add_library(${libname} SHARED ${HEADERS} ${src})
add_llvm_pass_tests("$<TARGET_FILE:${libname}>" "${libname}" "${test_filenames}")





