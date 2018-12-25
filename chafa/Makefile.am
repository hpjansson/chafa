SUBDIRS = libnsgif

## --- Library ---

lib_LTLIBRARIES = libchafa.la
noinst_LTLIBRARIES =

libchafa_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS)
libchafa_la_LDFLAGS = $(LIBCHAFA_LDFLAGS) -version-info 1:1:1
libchafa_la_LIBADD = $(GLIB_LIBS)

libchafa_la_SOURCES = \
	chafa-canvas.c \
	chafa-canvas-config.c \
	chafa-colors.c \
	chafa-features.c \
	chafa-private.h \
	chafa-symbol-map.c \
	chafa-symbols.c \
	chafa-symbols-ascii.h \
	chafa-util.c

chafaincludedir=$(includedir)/chafa
chafainclude_HEADERS = \
	chafa.h \
	chafa-canvas.h \
	chafa-canvas-config.h \
	chafa-features.h \
	chafa-symbol-map.h \
	chafa-util.h

if HAVE_MMX_INTRINSICS
noinst_LTLIBRARIES += libchafa-mmx.la
libchafa_la_LIBADD += libchafa-mmx.la
libchafa_mmx_la_SOURCES = chafa-mmx.c
libchafa_mmx_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -mmmx
libchafa_mmx_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

if HAVE_SSE41_INTRINSICS
noinst_LTLIBRARIES += libchafa-sse41.la
libchafa_la_LIBADD += libchafa-sse41.la
libchafa_sse41_la_SOURCES = chafa-sse41.c
libchafa_sse41_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -msse4.1
libchafa_sse41_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

if HAVE_POPCNT_INTRINSICS
noinst_LTLIBRARIES += libchafa-popcnt.la
libchafa_la_LIBADD += libchafa-popcnt.la
libchafa_popcnt_la_SOURCES = chafa-popcnt.c
libchafa_popcnt_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -mpopcnt
libchafa_popcnt_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

## --- Tools ---

if WANT_TOOLS

bin_PROGRAMS = \
	chafa

chafa_SOURCES = \
	chafa.c \
	gif-loader.c \
	gif-loader.h \
	named-colors.c \
	named-colors.h

chafa_CFLAGS = $(CHAFA_CFLAGS) $(GLIB_CFLAGS) $(MAGICKWAND_CFLAGS)
chafa_LDFLAGS = $(CHAFA_LDFLAGS)
chafa_LDADD = $(GLIB_LIBS) $(MAGICKWAND_LIBS) libchafa.la libnsgif/libnsgif.la -lm

endif

## -- General ---

AM_CPPFLAGS = \
	-I$(top_srcdir)
