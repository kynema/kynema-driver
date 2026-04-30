target_sources(${KYNEMA_DRIVER_LIB_NAME} PRIVATE
  SGFTiogaIface.cpp
  SGFTiogaIface.h
  KynemaSGF.cpp
  KynemaSGF.h
  KynemaSolver.h
  KynemaSolver.cpp
  MPIUtilities.h
  KynemaUGF.cpp
  KynemaUGF.h
  OversetSimulation.cpp
  OversetSimulation.h
  ParallelPrinter.h
  MemoryUsage.h
  MemoryUsage.cpp)

target_include_directories(${KYNEMA_DRIVER_LIB_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${KYNEMA_DRIVER_LIB_NAME} PUBLIC
  Kynema-SGF::kynema_sgf_api
  Kynema-SGF::buildInfokynema_sgf_obj
  AMReX::amrex
  Kynema-UGF::kynema_ugf_lib)

target_link_libraries(${KYNEMA_DRIVER_LIB_NAME} PUBLIC $<$<BOOL:${MPI_CXX_FOUND}>:MPI::MPI_CXX>)
target_link_libraries(${KYNEMA_DRIVER_LIB_NAME} PUBLIC $<$<BOOL:${MPI_Fortran_FOUND}>:MPI::MPI_Fortran>)

# Solve -fallow-argument-mismatch from fortran possibly making its way into arguments passed to clang
#if(CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|AppleClang)$")
#  set(TARGET_FLAGS "")
#  get_target_property(TARGET_FLAGS MPI::MPI_Fortran INTERFACE_COMPILE_OPTIONS)
#  message(STATUS "MPI fortran options are: ${TARGET_FLAGS}")
#  if((NOT "${TARGET_FLAGS}" STREQUAL "") AND (NOT "${TARGET_FLAGS}" STREQUAL "TARGET_FLAGS-NOTFOUND"))
#    message(STATUS "Removing -fallow-argument-mismatch from MPI fortran options if it exists...")
#    string(REPLACE "-fallow-argument-mismatch" "" TARGET_FLAGS ${TARGET_FLAGS})
#    set_target_properties(MPI::MPI_Fortran PROPERTIES INTERFACE_COMPILE_OPTIONS "${TARGET_FLAGS}")
#  endif()
#endif()
