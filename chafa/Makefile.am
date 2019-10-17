SUBDIRS = internal

## --- Library ---

lib_LTLIBRARIES = libchafa.la
noinst_LTLIBRARIES =

libchafa_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS)
libchafa_la_LDFLAGS = $(LIBCHAFA_LDFLAGS) -version-info 1:1:1
libchafa_la_LIBADD = $(GLIB_LIBS) internal/libchafa-internal.la -lm

libchafa_la_SOURCES = \
	chafa-canvas.c \
	chafa-canvas-config.c \
	chafa-features.c \
	chafa-symbol-map.c \
	chafa-util.c

chafaincludedir=$(includedir)/chafa
chafainclude_HEADERS = \
	chafa.h \
	chafa-canvas.h \
	chafa-canvas-config.h \
	chafa-features.h \
	chafa-symbol-map.h \
	chafa-util.h \
	chafa-version-macros.h

## -- General ---

AM_CPPFLAGS = \
	-I$(top_srcdir)
