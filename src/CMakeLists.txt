target_sources(${KYNEMA_DRIVER_LIB_NAME} PRIVATE
  Module.h
  Module.cpp
  MPIUtilities.h
  Simulation.cpp
  Simulation.h
  ParallelPrinter.h
  MemoryUsage.h
  MemoryUsage.cpp)

if(KYNEMA_DRIVER_ENABLE_FMB)
  target_sources(${KYNEMA_DRIVER_LIB_NAME} PRIVATE modules/KynemaFMB.cpp modules/KynemaFMB.h)
  target_link_libraries(${KYNEMA_DRIVER_LIB_NAME} PUBLIC KynemaFMB::kynema_fmb_library)
  target_compile_definitions(${KYNEMA_DRIVER_LIB_NAME} PUBLIC KYNEMA_DRIVER_ENABLE_FMB)
endif()
if(KYNEMA_DRIVER_ENABLE_SGF)
  target_sources(${KYNEMA_DRIVER_LIB_NAME} PRIVATE modules/SGFTiogaIface.cpp modules/SGFTiogaIface.h modules/KynemaSGF.cpp modules/KynemaSGF.h)
  target_link_libraries(${KYNEMA_DRIVER_LIB_NAME} PUBLIC Kynema-SGF::kynema_sgf_api Kynema-SGF::buildInfokynema_sgf_obj AMReX::amrex)
  target_compile_definitions(${KYNEMA_DRIVER_LIB_NAME} PUBLIC KYNEMA_DRIVER_ENABLE_SGF)
endif()
if(KYNEMA_DRIVER_ENABLE_UGF)
  target_sources(${KYNEMA_DRIVER_LIB_NAME} PRIVATE modules/KynemaUGF.cpp modules/KynemaUGF.h)
  target_link_libraries(${KYNEMA_DRIVER_LIB_NAME} PUBLIC Kynema-UGF::kynema_ugf_lib)
  target_compile_definitions(${KYNEMA_DRIVER_LIB_NAME} PUBLIC KYNEMA_DRIVER_ENABLE_UGF)
endif()

target_include_directories(${KYNEMA_DRIVER_LIB_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(${KYNEMA_DRIVER_LIB_NAME} PUBLIC yaml-cpp tioga)

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
