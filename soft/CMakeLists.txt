#=============================================================================
# 5.8 GHz MIMO radar -- host software
#
# The FPGA does the high-rate work.  This builds the host side: the control
# path, the angle/tracking/classification stages, the operator display, and a
# bit-exact model of the FPGA datapath so the whole radar runs on a laptop with
# no hardware attached.
#
#   cmake -S soft -B soft/build -DCMAKE_BUILD_TYPE=Release && cmake --build soft/build -j
#=============================================================================
cmake_minimum_required(VERSION 3.16)
project(radar5g8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#-----------------------------------------------------------------------------
# Optimisation.  This machine is an Intel i5-8257U: AVX2 and FMA are present,
# AVX-512 is not.  Ask for them explicitly rather than -march=native so that a
# binary built here still runs on another authorised operator's machine of the
# same generation, which -march=native would not guarantee.
#-----------------------------------------------------------------------------
include(CheckCXXCompilerFlag)
add_library(radar_flags INTERFACE)
target_compile_options(radar_flags INTERFACE
  $<$<CONFIG:Release>:-O3 -fno-math-errno -fno-trapping-math>
  $<$<CONFIG:RelWithDebInfo>:-O2 -g>
  -Wall -Wextra -Wno-unused-parameter)

option(RADAR_NATIVE "Tune for this exact CPU (do not distribute the binary)" OFF)
if(RADAR_NATIVE)
  check_cxx_compiler_flag("-march=native" HAS_NATIVE)
  if(HAS_NATIVE)
    target_compile_options(radar_flags INTERFACE -march=native)
  endif()
else()
  foreach(flag -mavx2 -mfma)
    string(REPLACE "-" "_" flagvar ${flag})
    check_cxx_compiler_flag(${flag} HAS${flagvar})
    if(HAS${flagvar})
      target_compile_options(radar_flags INTERFACE ${flag})
    endif()
  endforeach()
endif()

#-----------------------------------------------------------------------------
# FFT backend.  Accelerate is a system framework on macOS so it costs nothing
# to link; FFTW is used when present; the built-in transform is always there so
# the stack builds on a bare machine.  radar-bench times all three.
#-----------------------------------------------------------------------------
set(RADAR_FFT_BACKEND "auto" CACHE STRING "accelerate | fftw | builtin | auto")
set(FFT_DEFINE "")
set(FFT_LIBS "")

if(RADAR_FFT_BACKEND STREQUAL "auto" OR RADAR_FFT_BACKEND STREQUAL "accelerate")
  if(APPLE)
    find_library(ACCELERATE_FRAMEWORK Accelerate)
    if(ACCELERATE_FRAMEWORK)
      set(FFT_DEFINE RADAR_FFT_ACCELERATE)
      set(FFT_LIBS ${ACCELERATE_FRAMEWORK})
    endif()
  endif()
endif()

if(NOT FFT_DEFINE AND (RADAR_FFT_BACKEND STREQUAL "auto" OR RADAR_FFT_BACKEND STREQUAL "fftw"))
  find_path(FFTW3F_INCLUDE_DIR fftw3.h HINTS /usr/local/include /opt/homebrew/include /usr/include)
  find_library(FFTW3F_LIB NAMES fftw3f HINTS /usr/local/lib /opt/homebrew/lib /usr/lib)
  if(FFTW3F_INCLUDE_DIR AND FFTW3F_LIB)
    set(FFT_DEFINE RADAR_FFT_FFTW)
    set(FFT_LIBS ${FFTW3F_LIB})
  endif()
endif()

if(NOT FFT_DEFINE)
  set(FFT_DEFINE RADAR_FFT_BUILTIN)
endif()
message(STATUS "radar: FFT backend = ${FFT_DEFINE}")

# Always compile the comparison backends into radar-bench so the choice can be
# justified with a measurement instead of an assumption.
set(EXTRA_FFT_DEFINES "")
if(APPLE AND ACCELERATE_FRAMEWORK)
  list(APPEND EXTRA_FFT_DEFINES RADAR_HAVE_ACCELERATE)
endif()
if(FFTW3F_LIB)
  list(APPEND EXTRA_FFT_DEFINES RADAR_HAVE_FFTW)
endif()

#-----------------------------------------------------------------------------
# UHD.  Optional: without it the stack still builds and runs against the
# simulator and against recordings.
#-----------------------------------------------------------------------------
option(RADAR_WITH_UHD "Build the USRP B210 driver" ON)
set(UHD_FOUND_OK FALSE)
if(RADAR_WITH_UHD)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(UHD QUIET uhd)
  endif()
  if(NOT UHD_FOUND)
    find_path(UHD_INCLUDE_DIR uhd/usrp/multi_usrp.hpp
              HINTS /usr/local/include /opt/homebrew/include /usr/include)
    find_library(UHD_LIB NAMES uhd HINTS /usr/local/lib /opt/homebrew/lib /usr/lib)
    if(UHD_INCLUDE_DIR AND UHD_LIB)
      set(UHD_INCLUDE_DIRS ${UHD_INCLUDE_DIR})
      set(UHD_LIBRARIES ${UHD_LIB})
      set(UHD_FOUND TRUE)
    endif()
  endif()
  if(UHD_FOUND)
    set(UHD_FOUND_OK TRUE)
    message(STATUS "radar: UHD found, B210 driver enabled")
  else()
    message(STATUS "radar: UHD not found, building without the B210 driver")
  endif()
endif()

#-----------------------------------------------------------------------------
# The library
#-----------------------------------------------------------------------------
set(RADAR_SOURCES
  src/log.cpp
  src/json.cpp
  src/config.cpp
  src/fft.cpp
  src/window.cpp
  src/waveform.cpp
  src/refmodel.cpp
  src/calib.cpp
  src/cfar.cpp
  src/aoa.cpp
  src/cluster.cpp
  src/track.cpp
  src/microdoppler.cpp
  src/thread.cpp
  src/sim_source.cpp
  src/file_source.cpp
  src/uhd_source.cpp
  src/proto.cpp
  src/net.cpp
  src/pipeline.cpp
)

add_library(radar STATIC ${RADAR_SOURCES})
target_include_directories(radar PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(radar PUBLIC ${FFT_DEFINE})
target_link_libraries(radar PUBLIC radar_flags ${FFT_LIBS})

# Where the web page lives, so the server can find it without an install step.
target_compile_definitions(radar PRIVATE
  RADAR_WEB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/web"
  RADAR_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")

if(UHD_FOUND_OK)
  target_compile_definitions(radar PUBLIC RADAR_HAVE_UHD)
  target_include_directories(radar PUBLIC ${UHD_INCLUDE_DIRS})
  target_link_libraries(radar PUBLIC ${UHD_LIBRARIES})
endif()

find_package(Threads REQUIRED)
target_link_libraries(radar PUBLIC Threads::Threads)

#-----------------------------------------------------------------------------
# Tools
#-----------------------------------------------------------------------------
add_executable(radard          tools/radard.cpp)
add_executable(radar-selftest  tools/radar_selftest.cpp)
add_executable(radar-bench     tools/radar_bench.cpp)
add_executable(radar-plan      tools/radar_plan.cpp)

foreach(t radard radar-selftest radar-bench radar-plan)
  target_link_libraries(${t} PRIVATE radar)
endforeach()

# radar-bench links every available backend so it can compare them.
target_compile_definitions(radar-bench PRIVATE ${EXTRA_FFT_DEFINES})
if(APPLE AND ACCELERATE_FRAMEWORK)
  target_link_libraries(radar-bench PRIVATE ${ACCELERATE_FRAMEWORK})
endif()
if(FFTW3F_LIB)
  target_link_libraries(radar-bench PRIVATE ${FFTW3F_LIB})
endif()

#-----------------------------------------------------------------------------
enable_testing()
add_test(NAME selftest COMMAND radar-selftest)
set_tests_properties(selftest PROPERTIES TIMEOUT 600)

message(STATUS "radar: build type ${CMAKE_BUILD_TYPE}")
