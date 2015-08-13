include_directories(. ${CMAKE_BINARY_DIR} ../include)

#set (libvidcap_la_LIBADD ${PTHREAD_LIBS})

#set (libvidcap_la_LDFLAGS 
#	-no-undefined							
#	-version-info @VC_LT_CURRENT@:@VC_LT_REVISION@:@VC_LT_AGE@	
#	-export-symbols-regex "vidcap_.*")

set (LIBVIDCAP_SRC 			
	conv.c				
	conv_to_rgb.c			
	conv_to_i420.c			
	conv_to_yuy2.c			
	double_buffer.c			
	hotlist.c			
	logging.c			
	sapi.c				
	sliding_window.c		
	vidcap.c)

if(HAVE_V4L)
	set(LIBVIDCAP_SRC ${LIBVIDCAP_SRC} sapi_v4l.c)
endif()

if(HAVE_V4L2)
	message(FATAL_ERROR "V4L2 not yet implemented!")
endif()

if(QUICKTIME_FOUND)
	add_subdirectory(quicktime)
	include_directories(quicktime)

	set(LIBVIDCAP_SRC ${LIBVIDCAP_SRC}
		sapi_qt.c)
endif()

if(DIRECTSHOW_FOUND)
	add_subdirectory(directshow)
	include_directories(directshow)
	
	set(LIBVIDCAP_SRC ${LIBVIDCAP_SRC}
		sapi_dshow.cpp)
endif()

# Set the LIB_TYPE variable depending on the selected option
if(BUILD_SHARED_LIBS)
	set(LIB_TYPE SHARED)
else()
	set(LIB_TYPE STATIC)
endif()

add_library (vidcap ${LIB_TYPE} ${LIBVIDCAP_SRC})
#set_target_properties(vidcap ${PLATFORM_LINKFLAGS})
target_link_libraries(vidcap ${CMAKE_THREAD_LIBS_INIT})
