SUBDIRS = smolscale

## --- Library ---

noinst_LTLIBRARIES = libchafa-internal.la

libchafa_internal_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS)
libchafa_internal_la_LDFLAGS = $(LIBCHAFA_LDFLAGS) -version-info 1:1:1
libchafa_internal_la_LIBADD = $(GLIB_LIBS) smolscale/libsmolscale.la -lm

libchafa_internal_la_SOURCES = \
	chafa-color-hash.c \
	chafa-color-hash.h \
	chafa-colors.c \
	chafa-private.h \
	chafa-sixel.c \
	chafa-symbols.c \
	chafa-symbols-ascii.h

if HAVE_MMX_INTRINSICS
noinst_LTLIBRARIES += libchafa-mmx.la
libchafa_internal_la_LIBADD += libchafa-mmx.la
libchafa_mmx_la_SOURCES = chafa-mmx.c
libchafa_mmx_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -mmmx
libchafa_mmx_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

if HAVE_SSE41_INTRINSICS
noinst_LTLIBRARIES += libchafa-sse41.la
libchafa_internal_la_LIBADD += libchafa-sse41.la
libchafa_sse41_la_SOURCES = chafa-sse41.c
libchafa_sse41_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -msse4.1
libchafa_sse41_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

if HAVE_POPCNT_INTRINSICS
noinst_LTLIBRARIES += libchafa-popcnt.la
libchafa_internal_la_LIBADD += libchafa-popcnt.la
libchafa_popcnt_la_SOURCES = chafa-popcnt.c
libchafa_popcnt_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -mpopcnt
libchafa_popcnt_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

## -- General ---

AM_CPPFLAGS = \
	-I$(top_srcdir)/chafa
