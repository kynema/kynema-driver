cmake_minimum_required(VERSION 3.14.0 FATAL_ERROR)

project(kynema-driver CXX C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(KYNEMA_DRIVER_ENABLE_TESTS "Enable testing suite" ON)
option(KYNEMA_DRIVER_TEST_WITH_FCOMPARE "Check Kynema-SGF test plots against gold files" OFF)
option(KYNEMA_DRIVER_SAVE_GOLDS "Provide a directory in which to save golds during testing" OFF)
option(KYNEMA_DRIVER_ENABLE_CUDA "Enable CUDA" OFF)
option(KYNEMA_DRIVER_ENABLE_CUDA_RDC "Enable CUDA relocatable device code" OFF)
option(KYNEMA_DRIVER_ENABLE_ROCM "Enable HIP" OFF)
option(KYNEMA_DRIVER_ENABLE_UMPIRE "Enable Umpire GPU memory pools" OFF)

if(KYNEMA_DRIVER_ENABLE_CUDA)
  enable_language(CUDA)
  find_package(CUDAToolkit REQUIRED)
  set(KYNEMA_DRIVER_CUDA_ARCH "70" CACHE STRING "CUDA architecture (Use 'Auto' for automatic detection)")
endif()

if(KYNEMA_DRIVER_ENABLE_ROCM)
  find_package(hip REQUIRED)
  find_package(rocrand REQUIRED CONFIG)
  find_package(rocprim REQUIRED CONFIG)
  find_package(rocsparse REQUIRED CONFIG)
  find_package(hiprand REQUIRED CONFIG)
endif()

if (KYNEMA_DRIVER_ENABLE_UMPIRE)
  set(CMAKE_PREFIX_PATH ${UMPIRE_DIR} ${CMAKE_PREFIX_PATH})
  find_package(Umpire REQUIRED)
  include_directories(SYSTEM ${UMPIRE_INCLUDE_DIRS})
endif()

set(CMAKE_PREFIX_PATH ${Kynema-UGF_DIR} ${Trilinos_DIR} ${Kynema-SGF_DIR}
                      ${OpenFAST_DIR} ${TIOGA_DIR}
                      ${YAML-CPP_DIR} ${CMAKE_PREFIX_PATH})

find_package(Kynema-UGF REQUIRED)
find_package(Trilinos REQUIRED)
find_package(Kynema-SGF REQUIRED)
if(KYNEMA_UGF_USES_OPENFAST)
  enable_language(Fortran)
  find_package(OpenFAST REQUIRED)
endif()
find_package(TIOGA REQUIRED)
find_package(YAML-CPP 0.6.2 REQUIRED)
find_package(MPI REQUIRED)

# General information about machine, compiler, and build type
message(STATUS "Kynema-Driver information:")
message(STATUS "CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_CXX_COMPILER_ID = ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_CXX_COMPILER_VERSION = ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

set(KYNEMA_DRIVER_LIB_NAME "kynema_driver_lib")
add_library(${KYNEMA_DRIVER_LIB_NAME} OBJECT)
add_subdirectory(src)

set(KYNEMA_DRIVER_EXE_NAME "kynema_driver")
add_executable(${KYNEMA_DRIVER_EXE_NAME})

target_sources(${KYNEMA_DRIVER_EXE_NAME} PRIVATE
  kynema_driver.cpp)
target_link_libraries(${KYNEMA_DRIVER_EXE_NAME} PRIVATE
  ${KYNEMA_DRIVER_LIB_NAME}
  yaml-cpp)
target_include_directories(${KYNEMA_DRIVER_EXE_NAME} SYSTEM PRIVATE ${YAML_CPP_INCLUDE_DIR})
if(KYNEMA_DRIVER_ENABLE_ROCM)
  target_compile_definitions(${KYNEMA_DRIVER_EXE_NAME} PRIVATE KYNEMA_DRIVER_ENABLE_ROCM)
endif()

if(KYNEMA_DRIVER_ENABLE_CUDA)
  include(kynema-driver-utils)
  set(kyntargets "${KYNEMA_DRIVER_LIB_NAME};${KYNEMA_DRIVER_EXE_NAME}")
  foreach(tgt IN LISTS kyntargets)
    set_cuda_build_properties(${tgt})
  endforeach()
endif()

if(KYNEMA_DRIVER_ENABLE_TESTS)
  enable_testing()
  include(CTest)
  add_subdirectory(test)
endif()

install(TARGETS ${KYNEMA_DRIVER_EXE_NAME})
