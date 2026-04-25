target_sources(${KYNEMA_EXE_NAME} PRIVATE
  kynema.cpp)

target_link_libraries(${KYNEMA_EXE_NAME} PRIVATE
  ${KYNEMA_LIB_NAME}
  yaml-cpp)

target_include_directories(${KYNEMA_EXE_NAME} SYSTEM PRIVATE ${YAML_CPP_INCLUDE_DIR})

if(KYNEMA_ENABLE_ROCM)
  target_compile_definitions(${KYNEMA_EXE_NAME} PRIVATE KYNEMA_ENABLE_ROCM)
endif()
