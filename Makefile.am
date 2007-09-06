SUBDIRS = include src examples

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = vidcap.pc

dist-hook:
	find $(distdir) -name ".svn" -type d -print0 | xargs -0 rm -rf
