#-----------------------------------------------------------------------------
# We don't allow in-source builds. This causes no end of troubles because
# all out-of-source builds will use the CMakeCache.txt file there and even
# build the libs and objects in it.

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR
		"CMake generation is not allowed within the source directory!"
		"\n Remove the CMakeCache.txt file and try again from another folder, e.g.:"
		"\n "
		"\n rm CMakeCache.txt"
		"\n cd .."
		"\n mkdir cmake-make"
		"\n cd cmake-make"
		"\n cmake ../libvidcap"
		"\n ")
endif()

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

cmake_minimum_required(VERSION 2.6)
project(libvidcap)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

#-----------------------------------------------------------------------------
# Check build environment

include(CheckFunctionExists)

check_function_exists(gettimeofday	HAVE_GETTIMEOFDAY)
check_function_exists(nanosleep		HAVE_NANOSLEEP)
check_function_exists(snprintf		HAVE_SNPRINTF)

find_package (Threads)

#-----------------------------------------------------------------------------
# Set global build flags, these will be used for the library and examples

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter ${PTHREAD_CFLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter ${PTHREAD_CFLAGS}")

#-----------------------------------------------------------------------------
# add correct video support, depending on platform and/or optional settings

#-----------------------------------------------------------------------------
# Windows
if(WIN32)
	find_package(DirectShow)
	if(DIRECTSHOW_FOUND)
		add_definitions(-DHAVE_DIRECTSHOW)
	endif()
endif()
#-----------------------------------------------------------------------------
# Mac
if(APPLE)
	find_package(QuickTime)
	if(QUICKTIME_FOUND)
		add_definitions(-DHAVE_QUICKTIME)

		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -F/Library/Frameworks -weak_framework Carbon")
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -F/Library/Frameworks -weak_framework QuartzCore")
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -F/Library/Frameworks -weak_framework QuickTime")
	endif()
endif()
#-----------------------------------------------------------------------------
# Linux
if(UNIX AND NOT APPLE)

	option(WITH_V4L "Build with V4L support" ON)
	option(WITH_V4L2 "Build with V4L2 support (not implemented yet)" OFF)

	if (WITH_V4L AND WITH_V4L2)
		message(FATAL_ERROR "Either select the V4L or V4L2 SDK")
	endif()

	#this macro will detect V4L1 and V4L2
	find_package(V4L)

	if(WITH_V4L AND V4L1_FOUND)
		set(HAVE_V4L 1)
	endif()

	if(WITH_V4L2 AND V4L2_FOUND)
		SET(HAVE_V4L2 1)
	endif()

	if(V4L1_COMPATIBILITY_HEADER)
		set(USE_V4L1_COMPAT_HEADER 1)
	endif()
endif()

#-----------------------------------------------------------------------------
# write the config file

configure_file(config.h.in ${CMAKE_BINARY_DIR}/config.h)
add_definitions(-DHAVE_CONFIG_H)

#-----------------------------------------------------------------------------
# Allow the developer to select if dynamic or static libraries are built
option(WITH_SHARED_LIBS "Build videocap as shared library" OFF)

add_subdirectory(src)

#-----------------------------------------------------------------------------
# Example files

option(WITH_EXAMPLES "Build the library examples" OFF)
if (WITH_EXAMPLES)
	add_subdirectory(examples)
endif()
