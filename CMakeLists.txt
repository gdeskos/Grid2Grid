###-------------------------------------------------------------------------###

cmake_minimum_required(VERSION 3.1)  #... Specify the minimum version for CMake

###-------------------------------------------------------------------------###

if(NOT WIN32)
  string(ASCII 27 Esc)
  set(ColourReset "${Esc}[m")
  set(ColourBold  "${Esc}[1m")
  set(Red         "${Esc}[31m")
  set(Green       "${Esc}[32m")
  set(Yellow      "${Esc}[33m")
  set(Blue        "${Esc}[34m")
  set(Magenta     "${Esc}[35m")
  set(Cyan        "${Esc}[36m")
  set(White       "${Esc}[37m")
  set(BoldRed     "${Esc}[1;31m")
  set(BoldGreen   "${Esc}[1;32m")
  set(BoldYellow  "${Esc}[1;33m")
  set(BoldBlue    "${Esc}[1;34m")
  set(BoldMagenta "${Esc}[1;35m")
  set(BoldCyan    "${Esc}[1;36m")
  set(BoldWhite   "${Esc}[1;37m")
endif()

###-------------------------------------------------------------------------###

set(COMPILE_LANGUAGE Fortran)                       #
set(CMAKE_Fortran_COMPILER gfortran)       #... Fortran Compiler Path

###-------------------------------------------------------------------------###

set(PROJECT_NAME Grid2Grid)                         #... Project Name
set(PROJECT_COMPILE_PATH ${CMAKE_HOME_DIRECTORY})   #... COMPILE_PATH

project(${PROJECT_NAME} ${COMPILE_LANGUAGE})        # PROJECT NAME

###-------------------------------------------------------------------------###
## GIT
# Get the current working branch
execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
  COMMAND git log -1 --format=%H
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_definitions("-D__GIT_BRANCH__=\"${GIT_BRANCH}\"")
add_definitions("-D__GIT_COMMIT_HASH__=\"${GIT_COMMIT_HASH}\"")
###-------------------------------------------------------------------------###

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_COMPILE_PATH}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_COMPILE_PATH}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_COMPILE_PATH})
set(CMAKE_Fortran_MODULE_DIRECTORY ${PROJECT_COMPILE_PATH}/lib)

if ("${CMAKE_Fortran_COMPILER_ID}" MATCHES "Intel")
    set(CMAKE_Fortran_FLAGS "-cpp -ipo -fPIC -O2 -xHOST -ipo -ip -traceback -check uninit -check bounds -heap-arrays")
    add_definitions(-D_INTEL_FORTRAN_COMPILE_RULE_)
elseif("${CMAKE_Fortran_COMPILER_ID}" MATCHES "GNU")
    set(CMAKE_Fortran_FLAGS "-cpp -fPIC -O2 -march=corei7 -msse2 -funroll-loops -fno-protect-parens -ffast-math -fbounds-check")
    add_definitions(-D_GNU_FORTRAN_COMPILE_RULE_)
endif()

option(BUILD_OF_LIB "Build <libGrid2Grid.so> in $FOAM_USER_LIBBIN" OFF)

set(GRID2GRID_LIB_OUT ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
if (BUILD_OF_LIB)

    if (NOT "$ENV{FOAM_USER_LIBBIN}" STREQUAL "")
        set(GRID2GRID_LIB_OUT $ENV{FOAM_USER_LIBBIN})
    else()
        message("${BoldRed} ")
        message("   [ERROR] Not in OpenFOAM Environment... ")
        message("${ColourReset} ${Green}")
        message("   Wrong $FOAM_USER_LIBBIN path")
        message("")
        message("       $FOAM_USER_LIBBIN : " $ENV{FOAM_USER_LIBBIN})
        message("${ColourReset} ")
        message( FATAL_ERROR )
    endif()

endif()

message("${Yellow}")
message("    GRID2GRID LIBRARY OUT PATH ")
message("${Green}")
message("       - GRID2GRID_LIB_OUT  : " ${GRID2GRID_LIB_OUT})
message("${ColourReset}")

###-------------------------------------------------------------------------###

if ( NOT HDF5_LIB_PATH )
    #... HDF5 library path ( /usr/local/lib/hdf5 is generated by soft link )
    set(HDF5_LIB_PATH /usr/local/lib/hdf5/build/bin)
endif()

#... HDF5 library header file path
set(HDF5_INC_PATH ${HDF5_LIB_PATH}/static)

#... HDF5 LIBRARY FLAG (DO NOT CHANGE ORDER !)
set(HDF5_LIB_NAME
    libhdf5_fortran.a
    libhdf5_f90cstub.a
    libhdf5.a
    libszip.a
    libz.a
)

if ( NOT FFTW3_LIB_PATH )
    #...FFTW library path ( /usr/local/lib/libfftw3.a is generated by soft link )
    set(FFTW3_LIB_PATH /usr/local/lib)
endif()

find_library(FFTW_LIB_FLAG NAMES libfftw3.a PATHS ${FFTW3_LIB_PATH} ${LIBRARY_DIRS})
if ("${FFTW_LIB_FLAG}" STREQUAL "FFTW_LIB_FLAG-NOTFOUND")
    message("${BoldRed}    [ERROR] library:  libfftw3.a not found.")
    message("${ColourReset} ${Green}")
    message("    Wrong FFTW3 library path")
    message("")
    message("       FFTW3_LIB_PATH : " ${FFTW3_LIB_PATH})
    message("${ColourReset} ")
    message( FATAL_ERROR )
endif()
#... FFTW LIBRARY FLAG
get_filename_component(FFTW3_LIB_PATH ${FFTW_LIB_FLAG} PATH)
    message("${Yellow}    Found libfftw3.a:")
    message("")
    message("${Green}       - FFTW_LIB_FLAG : ${FFTW_LIB_FLAG}")
    message("${Green}       - FFTW3_LIB_PATH : ${FFTW3_LIB_PATH}")

###-------------------------------------------------------------------------###
###                         Check Library                                   ###
###-------------------------------------------------------------------------###

set(HDF_LIBRARY "OFF" CACHE STRING "OFF")

message("${Yellow}")
message("    GRID2GRID EXTERNAL LIBRARY PATH ")
message("${Green}")
if (HDF_LIBRARY MATCHES "ON")
message("       - HDF5_LIB_PATH  : " ${HDF5_LIB_PATH})
message("       - HDF5_INC_PATH  : " ${HDF5_INC_PATH})
endif()
message("       - FFTW3_LIB_PATH : " ${FFTW3_LIB_PATH})
message("${ColourReset}")


if (HDF_LIBRARY MATCHES "ON")

#... Check HDF5 library existence and add library to HDF5_LIB_FLAG
foreach(LIB ${HDF5_LIB_NAME})
    find_library(LIBEXIST NAMES ${LIB} PATHS ${HDF5_LIB_PATH})
    if (NOT LIBEXIST)
        message("${BoldYellow}   [WARNING] library: " ${LIB} " not found. ${ColourReset}")
        set(HDF_LIBRARY "OFF")
        set(HDF5_INC_PATH "")
    endif()
    set(HDF5_LIB_FLAG ${HDF5_LIB_FLAG} ${HDF5_LIB_PATH}/${LIB})
endforeach()
message("${ColourReset}")

set(HDF5_LIB_FLAG ${HDF5_LIB_FLAG} -ldl -pthread)
add_definitions(-DENABLE_HDF5=${HDF_LIBRARY})
else()
set(HDF5_INC_PATH "")
endif()

message("${Green}")
message("    HDF LIBRARY STATUS :  " ${HDF_LIBRARY})
message("${ColourReset}")


###-------------------------------------------------------------------------###

find_library(LIBEXIST NAMES libfftw3.a PATHS ${FFTW3_LIB_PATH})
if (NOT LIBEXIST)
    message("${BoldRed} ")
    message("   [ERROR] library:  libfftw3.a not found.")
    message("${ColourReset} ${Green}")
    message("   Wrong FFTW3 library path")
    message("")
    message("       FFTW3_LIB_PATH : " ${FFTW3_LIB_PATH})
    message("${ColourReset} ")
    message( FATAL_ERROR )
endif()
#... FFTW LIBRARY FLAG
set(FFTW_LIB_FLAG ${FFTW3_LIB_PATH}/libfftw3.a )
###-------------------------------------------------------------------------###

#... -fPIC is needed to generate shared library
set(CMAKE_Fortran_FLAGS "-fPIC")

add_subdirectory(src/libBspline)
add_subdirectory(src/libFyMc)
add_subdirectory(src/libGrid2Grid)
