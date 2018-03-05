lib_LTLIBRARIES = libchafa.la

libchafa_la_CFLAGS = $(AM_CFLAGS) $(CHAFA_HIDDEN_VISIBILITY_CFLAGS)
libchafa_la_LDFLAGS = $(CHAFA_LINK_FLAGS) -version-info 0:0:0

libchafa_la_SOURCES = \
	chafa.h \
	chafa-canvas.c \
	chafa-colors.c \
	chafa-symbols.c

bin_PROGRAMS = \
	chafa

chafa_LDFLAGS = $(CHAFA_LIBS) libchafa.la

AM_CPPFLAGS = \
	-I$(top_srcdir)

AM_LDFLAGS = $(CHAFA_LIBS)
AM_CFLAGS  = $(CHAFA_CFLAGS)
