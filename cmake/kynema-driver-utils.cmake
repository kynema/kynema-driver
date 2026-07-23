function(set_cuda_build_properties target)
  # Optional additional arguments: a list of host-only source file paths that
  # must not be compiled as CUDA. These files are explicitly kept as LANGUAGE CXX
  # so they bypass nvcc/cudafe++, which crashes on GCC 13 (libstdc++ 13) headers
  # such as stl_construct.h when using nvcc 12.8.1 (cudafe++ internal assertion
  # failure at lexical.c in find_allocated_name_reference).
  set(_host_only_srcs ${ARGN})

  get_target_property(_tgt_src ${target} SOURCES)
  list(FILTER _tgt_src INCLUDE REGEX "\\.cpp")

  # Remove host-only sources from the CUDA list and mark them as plain CXX.
  if(_host_only_srcs)
    list(REMOVE_ITEM _tgt_src ${_host_only_srcs})
    set_source_files_properties(${_host_only_srcs} PROPERTIES LANGUAGE CXX)
  endif()

  set_source_files_properties(${_tgt_src} PROPERTIES LANGUAGE CUDA)
  set_target_properties(${target} PROPERTIES CUDA_ARCHITECTURES "${KYNEMA_DRIVER_CUDA_ARCH}")
  set_target_properties(${target} PROPERTIES CUDA_RESOLVE_DEVICE_SYMBOLS ON)
  #set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CUDA)
  if(KYNEMA_DRIVER_ENABLE_CUDA_RDC)
    set_target_properties(${target} PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
  endif()
endfunction(set_cuda_build_properties)
