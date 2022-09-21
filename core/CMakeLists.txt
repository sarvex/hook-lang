
if(MSVC)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${LIBRARY_DIR})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${LIBRARY_DIR})
endif()

add_library(mathmod SHARED
  hk_math.c
  ../src/hk_array.c
  ../src/hk_builtin.c
  ../src/hk_callable.c
  ../src/hk_chunk.c
  ../src/hk_utils.c
  ../src/hk_compiler.c
  ../src/hk_error.c
  ../src/hk_iterable.c
  ../src/hk_iterator.c
  ../src/hk_memory.c
  ../src/hk_module.c
  ../src/hk_range.c
  ../src/hk_scanner.c
  ../src/hk_string_map.c
  ../src/hk_string.c
  ../src/hk_struct.c
  ../src/hk_userdata.c
  ../src/hk_value.c
  ../src/hk_vm.c)

add_library(osmod SHARED
  hk_os.c
  ../src/hk_array.c
  ../src/hk_builtin.c
  ../src/hk_callable.c
  ../src/hk_chunk.c
  ../src/hk_utils.c
  ../src/hk_compiler.c
  ../src/hk_error.c
  ../src/hk_iterable.c
  ../src/hk_iterator.c
  ../src/hk_memory.c
  ../src/hk_module.c
  ../src/hk_range.c
  ../src/hk_scanner.c
  ../src/hk_string_map.c
  ../src/hk_string.c
  ../src/hk_struct.c
  ../src/hk_userdata.c
  ../src/hk_value.c
  ../src/hk_vm.c)

add_library(iomod SHARED
  hk_io.c
  ../src/hk_array.c
  ../src/hk_builtin.c
  ../src/hk_callable.c
  ../src/hk_chunk.c
  ../src/hk_utils.c
  ../src/hk_compiler.c
  ../src/hk_error.c
  ../src/hk_iterable.c
  ../src/hk_iterator.c
  ../src/hk_memory.c
  ../src/hk_module.c
  ../src/hk_range.c
  ../src/hk_scanner.c
  ../src/hk_string_map.c
  ../src/hk_string.c
  ../src/hk_struct.c
  ../src/hk_userdata.c
  ../src/hk_value.c
  ../src/hk_vm.c)

add_library(numbersmod SHARED
  hk_numbers.c
  ../src/hk_array.c
  ../src/hk_builtin.c
  ../src/hk_callable.c
  ../src/hk_chunk.c
  ../src/hk_utils.c
  ../src/hk_error.c
  ../src/hk_iterable.c
  ../src/hk_iterator.c
  ../src/hk_memory.c
  ../src/hk_module.c
  ../src/hk_range.c
  ../src/hk_string_map.c
  ../src/hk_string.c
  ../src/hk_struct.c
  ../src/hk_userdata.c
  ../src/hk_value.c
  ../src/hk_vm.c)

add_library(stringsmod SHARED
  hk_strings.c
  ../src/hk_array.c
  ../src/hk_builtin.c
  ../src/hk_callable.c
  ../src/hk_chunk.c
  ../src/hk_utils.c
  ../src/hk_error.c
  ../src/hk_iterable.c
  ../src/hk_iterator.c
  ../src/hk_memory.c
  ../src/hk_module.c
  ../src/hk_range.c
  ../src/hk_string_map.c
  ../src/hk_string.c
  ../src/hk_struct.c
  ../src/hk_userdata.c
  ../src/hk_value.c
  ../src/hk_vm.c)

add_library(arraysmod SHARED
  hk_arrays.c
  ../src/hk_array.c
  ../src/hk_builtin.c
  ../src/hk_callable.c
  ../src/hk_chunk.c
  ../src/hk_utils.c
  ../src/hk_error.c
  ../src/hk_iterable.c
  ../src/hk_iterator.c
  ../src/hk_memory.c
  ../src/hk_module.c
  ../src/hk_range.c
  ../src/hk_string_map.c
  ../src/hk_string.c
  ../src/hk_struct.c
  ../src/hk_userdata.c
  ../src/hk_value.c
  ../src/hk_vm.c)
