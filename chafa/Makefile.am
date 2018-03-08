lib_LTLIBRARIES = libchafa.la
libchafa_la_LIBADD =

libchafa_la_CFLAGS = $(AM_CFLAGS) $(CHAFA_HIDDEN_VISIBILITY_CFLAGS)
libchafa_la_LDFLAGS = $(CHAFA_LINK_FLAGS) -version-info 0:0:0

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
libchafa_mmx_la_CFLAGS = $(AM_CFLAGS) $(CHAFA_HIDDEN_VISIBILITY_FLAGS) -mmmx
endif

if HAVE_SSE41_INTRINSICS
lib_LTLIBRARIES += libchafa-sse41.la
libchafa_la_LIBADD += libchafa-sse41.la
libchafa_sse41_la_SOURCES = chafa-sse41.c
libchafa_sse41_la_CFLAGS = $(AM_CFLAGS) $(CHAFA_HIDDEN_VISIBILITY_FLAGS) -msse4.1
endif

## --- Tools ---

bin_PROGRAMS = \
	chafa

chafa_LDFLAGS = $(CHAFA_LIBS) libchafa.la

AM_CPPFLAGS = \
	-I$(top_srcdir)

AM_LDFLAGS = $(CHAFA_LIBS)
AM_CFLAGS  = $(CHAFA_CFLAGS)
