## --- Library ---

lib_LTLIBRARIES = libchafa.la
libchafa_la_LIBADD =

libchafa_la_CFLAGS = $(LIBCHAFA_CFLAGS)
libchafa_la_LDFLAGS = $(LIBCHAFA_LDFLAGS) -version-info 0:0:0

libchafa_la_SOURCES = \
	chafa-canvas.c \
	chafa-colors.c \
	chafa-private.h \
	chafa-symbols.c

chafaincludedir=$(includedir)/chafa
chafainclude_HEADERS = chafa.h

if HAVE_MMX_INTRINSICS
lib_LTLIBRARIES += libchafa-mmx.la
libchafa_la_LIBADD += libchafa-mmx.la
libchafa_mmx_la_SOURCES = chafa-mmx.c
libchafa_mmx_la_CFLAGS = $(LIBCHAFA_CFLAGS) -mmmx
libchafa_mmx_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

if HAVE_SSE41_INTRINSICS
lib_LTLIBRARIES += libchafa-sse41.la
libchafa_la_LIBADD += libchafa-sse41.la
libchafa_sse41_la_SOURCES = chafa-sse41.c
libchafa_sse41_la_CFLAGS = $(LIBCHAFA_CFLAGS) -msse4.1
libchafa_sse41_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

## --- Tools ---

bin_PROGRAMS = \
	chafa

chafa_CFLAGS = $(CHAFA_CFLAGS)
chafa_LDFLAGS = $(CHAFA_LDFLAGS) libchafa.la

## -- General ---

AM_CPPFLAGS = \
	-I$(top_srcdir)
