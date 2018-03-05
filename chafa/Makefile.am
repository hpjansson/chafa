lib_LTLIBRARIES = libchafa.la
libchafa_la_LDFLAGS = -version-info 0:0:0 -export-symbols chafa-export.sym

libchafa_la_SOURCES = \
	chafa.h \
	chafa-canvas.c \
	chafa-colors.c \
	chafa-symbols.c

EXTRA_libchafa_la_DEPENDENCIES = chafa-export.sym

bin_PROGRAMS = \
	chafa

chafa_LDFLAGS = $(CHAFA_LIBS) libchafa.la

AM_CPPFLAGS = \
	-I$(top_srcdir)

AM_LDFLAGS = $(CHAFA_LIBS)
AM_CFLAGS  = $(CHAFA_CFLAGS)
