AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter

noinst_PROGRAMS = simplegrab

simplegrab_LDADD = $(PTHREAD_LIBS) $(top_builddir)/src/libvidcap.la
simplegrab_CFLAGS = $(PTHREAD_CFLAGS)
AM_LDFLAGS = $(PTHREAD_CFLAGS)

simplegrab_SOURCES = simplegrab.c

EXTRA_DIST = vidcapTester
