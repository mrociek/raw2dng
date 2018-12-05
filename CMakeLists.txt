PROJECT( raw2dng )

macro(use_cxx11)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
    endif ()
  else ()
    set (CMAKE_CXX_STANDARD 11)
  endif ()
endmacro(use_cxx11)

CMAKE_MINIMUM_REQUIRED( VERSION 2.8 )

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake-modules")

ADD_COMPILE_OPTIONS( -Wall )

use_cxx11()

ADD_SUBDIRECTORY( libdng )
ADD_SUBDIRECTORY( raw2dng )
