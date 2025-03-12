# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/IDS-TFG_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/IDS-TFG_autogen.dir/ParseCache.txt"
  "IDS-TFG_autogen"
  )
endif()
