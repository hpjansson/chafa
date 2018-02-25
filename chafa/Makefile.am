lib_LTLIBRARIES = libchafa.la
libchafa_la_LDFLAGS = -version-info 0:0:0

libchafa_la_SOURCES = \
	chafa.h \
	chafa-canvas.c \
	chafa-colors.c \
	chafa-symbols.c

bin_PROGRAMS = \
	chafa

chafa_LDFLAGS = $(CHAFA_LIBS) libchafa.la

INCLUDES = \
	-I$(top_srcdir)

AM_LDFLAGS = $(CHAFA_LIBS)
AM_CFLAGS  = $(CHAFA_CFLAGS)
