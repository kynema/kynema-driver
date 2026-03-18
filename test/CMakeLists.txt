#=============================================================================
# Exawind Testing
#=============================================================================

# Check if mesh submodule is populated
file(GLOB RESULT ${CMAKE_SOURCE_DIR}/test/meshes/*)
list(LENGTH RESULT RES_LEN)
if(RES_LEN EQUAL 0)
  message(WARNING
"Testing is enabled, but the mesh submodule is not initialized.\
 You should 'git submodule init' and 'git submodule update'\
 before running the tests.")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CTestCustom.cmake ${CMAKE_BINARY_DIR}/CTestCustom.cmake)

if(EXAWIND_TEST_WITH_FCOMPARE)
  set(FCOMPARE_EXE "${FCOMPARE_EXE}" CACHE STRING "Path to fcompare executable for regression tests")
  if("${EXAWIND_REFERENCE_GOLDS_DIRECTORY}" STREQUAL "")
    message(FATAL_ERROR "To reference gold files, EXAWIND_REFERENCE_GOLDS_DIRECTORY must be set and exist")
  else()
    set(GOLD_FILES_DIRECTORY ${EXAWIND_REFERENCE_GOLDS_DIRECTORY}/${CMAKE_SYSTEM_NAME}/${CMAKE_CXX_COMPILER_ID}/${CMAKE_CXX_COMPILER_VERSION})
    message(STATUS "Test golds directory for fcompare: ${GOLD_FILES_DIRECTORY}")
  endif()
endif()

if(EXAWIND_SAVE_GOLDS)
  if("${EXAWIND_SAVED_GOLDS_DIRECTORY}" STREQUAL "")
    message(FATAL_ERROR "To save gold files, EXAWIND_SAVED_GOLDS_DIRECTORY must be set and the directory exist")
  else()
    if(EXISTS ${EXAWIND_SAVED_GOLDS_DIRECTORY})
      set(SAVED_GOLDS_DIRECTORY ${EXAWIND_SAVED_GOLDS_DIRECTORY}/${CMAKE_SYSTEM_NAME}/${CMAKE_CXX_COMPILER_ID}/${CMAKE_CXX_COMPILER_VERSION})
      message(STATUS "Gold files will be saved to: ${SAVED_GOLDS_DIRECTORY}")
    else()
      message(FATAL_ERROR "Specified directory for saving gold files does not exist: ${EXAWIND_SAVED_GOLDS_DIRECTORY}")
    endif()
  endif()
endif()

#=============================================================================
# Functions for adding tests / Categories of tests
#=============================================================================
macro(setup_test)
    set(CURRENT_TEST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test_files/${TEST_NAME})
    set(CURRENT_TEST_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/test_files/${TEST_NAME})
    if(NOT "${TEST_NAME}" MATCHES "^nalu-nalu")
      set(PLOT_GOLD ${GOLD_FILES_DIRECTORY}/${TEST_NAME}/plt00010)
      set(PLOT_TEST ${CURRENT_TEST_BINARY_DIR}/plt00010)
    endif()
    file(MAKE_DIRECTORY ${CURRENT_TEST_BINARY_DIR})
    file(GLOB TEST_FILES "${CURRENT_TEST_SOURCE_DIR}/*")
    file(COPY ${TEST_FILES} DESTINATION "${CURRENT_TEST_BINARY_DIR}/")
    set(TEST_NP 2)
    set(MPI_COMMANDS "${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${TEST_NP} ${MPIEXEC_PREFLAGS}")
    if(AMReX_CUDA OR AMReX_HIP OR AMReX_SYCL)
      set(FCOMPARE_TOLERANCE "-r 1e-10 --abs_tol 1.0e-12")
    endif()
    if(EXAWIND_SAVE_GOLDS AND (NOT "${TEST_NAME}" MATCHES "^nalu-nalu"))
      file(MAKE_DIRECTORY ${SAVED_GOLDS_DIRECTORY}/${TEST_NAME})
      set(SAVE_GOLDS_COMMAND "&& cp -R ${PLOT_TEST} ${SAVED_GOLDS_DIRECTORY}/${TEST_NAME}/")
    endif()
    if(EXAWIND_TEST_WITH_FCOMPARE AND (NOT "${TEST_NAME}" MATCHES "^nalu-nalu"))
      set(FCOMPARE_COMMAND "&& CUDA_LAUNCH_BLOCKING=1 ${FCOMPARE_EXE} ${FCOMPARE_TOLERANCE} ${PLOT_GOLD} ${PLOT_TEST}")
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
                    ${CMAKE_CURRENT_SOURCE_DIR}/meshes
                    ${CURRENT_TEST_BINARY_DIR}/meshes)
endmacro(setup_test)

# Standard regression test
function(add_test_r TEST_NAME)
    setup_test()
    add_test(${TEST_NAME} bash -c "set -o pipefail && ${MPI_COMMANDS} ${CMAKE_BINARY_DIR}/${EXAWIND_EXE_NAME} ${MPIEXEC_POSTFLAGS} ${CURRENT_TEST_BINARY_DIR}/${TEST_NAME}.yaml 2>&1 | tee ${TEST_NAME}.log ${SAVE_GOLDS_COMMAND} ${FCOMPARE_COMMAND}")
    # Set properties for test
    set_tests_properties(${TEST_NAME} PROPERTIES
                         TIMEOUT 14400
                         PROCESSORS ${TEST_NP}
                         WORKING_DIRECTORY "${CURRENT_TEST_BINARY_DIR}/"
                         LABELS "regression"
                         ATTACHED_FILES "${CURRENT_TEST_BINARY_DIR}/${TEST_NAME}.log")
endfunction(add_test_r)

# Regression tests excluded from CI
function(add_test_re TEST_NAME)
    add_test_r(${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES LABELS "regression;no-ci")
endfunction(add_test_re)

# Regression test with dependency
function(add_test_rd TEST_NAME TEST_DEPENDENCY)
    add_test_r(${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES FIXTURES_REQUIRED fixture_${TEST_DEPENDENCY})
    set_tests_properties(${TEST_DEPENDENCY} PROPERTIES FIXTURES_SETUP fixture_${TEST_DEPENDENCY})
endfunction(add_test_rd)

#=============================================================================
# Regression tests
#=============================================================================
add_test_r(sphere)
add_test_r(amr-nalu-cylinder)
add_test_r(amr-nalu-cylinder-motion)
add_test_r(zalesak)
add_test_r(hybrid-multi-cylinder)
add_test_r(flat-surface)
add_test_r(sloshing-tank)
add_test_r(linear-waves)
add_test_r(dam-break-block)
add_test_r(abl-bndry-output)
add_test_rd(abl-bndry-input abl-bndry-output)
add_test_r(nalu-nalu-cylinder)
add_test_r(nalu-nalu-cylinder-motion)
add_test_re(stokes-waves-cylinder)

if(NALU_USES_KYNEMA)
  add_test_r(floating-block)
endif()