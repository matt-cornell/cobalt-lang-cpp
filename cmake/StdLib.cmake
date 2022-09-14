# utilities for building a library with stripped symbols
function(find_static OUTPUT LIBLIBNAME)
  set(_c_orig_find_lib_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
  if (WIN32)
  list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 .lib .a)
  else() 
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
  endif()
  find_library(${OUTPUT} ${LIBLIBNAME} PARENT_SCOPE)
  set(CMAKE_FIND_LIBRARY_SUFFIXES _c_orig_find_lib_suffixes)
endfunction()
function(add_stdlib LIBNAME)
  set(opt)
  set(ova BUILD_SHARED PREFIX)
  set(mva TARGETS LIBS)
  cmake_parse_arguments(STD "${opt}" "${ova}" "${mva}" ${ARGN})
  if (NOT DEFINED STD_BUILD_SHARED)
    set(STD_BUILD_SHARED ${BUILD_SHARED_LIBS})
  endif()
  if (NOT DEFINED STD_PREFIX)
    set(STD_PREFIX "")
  endif()
  if (${BUILD_SHARED})
    add_library(${LIBNAME} SHARED ${STD_TARGETS})
    target_compile_options(${LIBNAME} -nostdlib)
    foreach(ADDED_LIB ${STD_LIBS})
      target_link_libraries(${LIBNAME} ${ADDED_LIB})
    endforeach()
    add_custom_command(TARGET ${LIBNAME} POST_BUILD COMMAND strip -wK ${PREFIX}* libco-std.so)
  else()
    set(OBJS)
    file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/${LIBNAME}_files)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${LIBNAME}_files)
    foreach(ADDED_LIB ${STD_LIBS})
      find_static(LIB ${ADDED_LIB})
      execute_process(
        COMMAND ar xv ${LIB} 
        COMMAND cut -c5-
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/${LIBNAME}_files
        OUTPUT_VARIABLE TMP_OBJS
      )
      string(REPLACE "\n" ";" TMP_OBJS "${TMP_OBJS}")
      set(OBJS ${OBJS} ${TMP_OBJS})
    endforeach()
    list(TRANSFORM OBJS PREPEND ${CMAKE_BINARY_DIR}/${LIBNAME}_files/)
    add_library(${LIBNAME} STATIC ${STD_TARGETS} ${OBJS})
    add_custom_command(TARGET ${LIBNAME} POST_BUILD COMMAND strip -wK ${STD_PREFIX}* $<TARGET_FILE:${LIBNAME}>)
  endif()
endfunction()