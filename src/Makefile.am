AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libvidcap.la

libvidcap_la_CPPFLAGS = -I$(top_srcdir)/include
libvidcap_la_CFLAGS = -Wall -Wextra -Wno-unused-parameter -Wno-multichar \
		      $(PTHREAD_CFLAGS)
libvidcap_la_CXXFLAGS = -Wall -Wextra -Wno-unused-parameter -Wno-multichar \
			$(PTHREAD_CFLAGS)

libvidcap_la_LIBADD = $(PTHREAD_LIBS)

libvidcap_la_LDFLAGS =							\
	-no-undefined							\
	-version-info @VC_LT_CURRENT@:@VC_LT_REVISION@:@VC_LT_AGE@	\
	-export-symbols-regex "vidcap_.*"

libvidcap_la_SOURCES =			\
	conv.c				\
	conv.h				\
	conv_to_rgb.c			\
	conv_to_i420.c			\
	conv_to_yuy2.c			\
	hotlist.c			\
	hotlist.h			\
	logging.c			\
	logging.h			\
	sapi.c				\
	sapi.h				\
	sapi_context.h			\
	sliding_window.c		\
	sliding_window.h		\
	vidcap.c

if HAVE_V4L
libvidcap_la_SOURCES += sapi_v4l.c
endif

if HAVE_QUICKTIME
libvidcap_la_SOURCES +=			\
	quicktime/gworld.c		\
	quicktime/gworld.h		\
	quicktime/sg_manager.c		\
	quicktime/sg_manager.h		\
	quicktime/sg_source.c		\
	quicktime/sg_source.h		\
	sapi_qt.c

libvidcap_la_LDFLAGS +=			\
	-framework Carbon		\
	-framework QuartzCore		\
	-framework QuickTime
endif

if HAVE_DIRECTSHOW
libvidcap_la_SOURCES +=			\
	sapi_dshow.cpp			\
	directshow/DShowSrcManager.cpp	\
	directshow/DShowSrcManager.h	\
	directshow/DevMonitor.cpp	\
	directshow/DevMonitor.h		\
	directshow/DirectShowSource.cpp	\
	directshow/DirectShowSource.h	\
	directshow/GraphMonitor.cpp	\
	directshow/GraphMonitor.h	\
	directshow/LocklessQueue.h
endif
