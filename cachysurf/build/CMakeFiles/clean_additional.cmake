# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles/cachysurf_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/cachysurf_autogen.dir/ParseCache.txt"
  "cachysurf_autogen"
  )
endif()
