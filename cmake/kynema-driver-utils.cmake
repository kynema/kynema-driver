function(set_cuda_build_properties target)
  get_target_property(_tgt_src ${target} SOURCES)
  list(FILTER _tgt_src INCLUDE REGEX "\\.cpp")
  set(_cuda_src ${_tgt_src})

  get_property(_has_host_only_src TARGET ${target} PROPERTY KYNEMA_DRIVER_HOST_ONLY_CXX_SOURCES SET)
  if(_has_host_only_src)
    get_target_property(_host_only_src ${target} KYNEMA_DRIVER_HOST_ONLY_CXX_SOURCES)
    # Tag host-only sources so they can be removed from the CUDA list before
    # LANGUAGE CUDA is applied to the remaining .cpp translation units.
    set_source_files_properties(${_host_only_src} PROPERTIES
      KYNEMA_DRIVER_HOST_ONLY_CXX ON
      LANGUAGE CXX)
  endif()

  foreach(_src IN LISTS _tgt_src)
    get_source_file_property(_host_only "${_src}" KYNEMA_DRIVER_HOST_ONLY_CXX)
    if(_host_only)
      list(REMOVE_ITEM _cuda_src "${_src}")
    endif()
  endforeach()

  if(_cuda_src)
    set_source_files_properties(${_cuda_src} PROPERTIES LANGUAGE CUDA)
  endif()
  set_target_properties(${target} PROPERTIES CUDA_ARCHITECTURES "${KYNEMA_DRIVER_CUDA_ARCH}")
  set_target_properties(${target} PROPERTIES CUDA_RESOLVE_DEVICE_SYMBOLS ON)
  #set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CUDA)
  if(KYNEMA_DRIVER_ENABLE_CUDA_RDC)
    set_target_properties(${target} PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
  endif()
endfunction(set_cuda_build_properties)
