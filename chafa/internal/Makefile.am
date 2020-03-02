SUBDIRS = smolscale

## --- Library ---

noinst_LTLIBRARIES = libchafa-internal.la

libchafa_internal_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -DCHAFA_COMPILATION
libchafa_internal_la_LDFLAGS = $(LIBCHAFA_LDFLAGS) -version-info 1:1:1
libchafa_internal_la_LIBADD = $(GLIB_LIBS) smolscale/libsmolscale.la -lm

libchafa_internal_la_SOURCES = \
	chafa-batch.c \
	chafa-batch.h \
	chafa-bitfield.h \
	chafa-color.c \
	chafa-color.h \
	chafa-color-hash.c \
	chafa-color-hash.h \
	chafa-color-table.c \
	chafa-color-table.h \
	chafa-dither.c \
	chafa-dither.h \
	chafa-indexed-image.c \
	chafa-indexed-image.h \
	chafa-palette.c \
	chafa-palette.h \
	chafa-pca.c \
	chafa-pca.h \
	chafa-private.h \
	chafa-sixel-canvas.c \
	chafa-sixel-canvas.h \
	chafa-symbols.c \
	chafa-symbols-ascii.h

if HAVE_MMX_INTRINSICS
noinst_LTLIBRARIES += libchafa-mmx.la
libchafa_internal_la_LIBADD += libchafa-mmx.la
libchafa_mmx_la_SOURCES = chafa-mmx.c
libchafa_mmx_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -mmmx -DCHAFA_COMPILATION
libchafa_mmx_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

if HAVE_SSE41_INTRINSICS
noinst_LTLIBRARIES += libchafa-sse41.la
libchafa_internal_la_LIBADD += libchafa-sse41.la
libchafa_sse41_la_SOURCES = chafa-sse41.c
libchafa_sse41_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -msse4.1 -DCHAFA_COMPILATION
libchafa_sse41_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

if HAVE_POPCNT_INTRINSICS
noinst_LTLIBRARIES += libchafa-popcnt.la
libchafa_internal_la_LIBADD += libchafa-popcnt.la
libchafa_popcnt_la_SOURCES = chafa-popcnt.c
libchafa_popcnt_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -mpopcnt -DCHAFA_COMPILATION
libchafa_popcnt_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

## -- General ---

## Include $(top_builddir)/chafa to get generated chafaconfig.h.

AM_CPPFLAGS = \
	-I$(top_srcdir)/chafa \
	-I$(top_builddir)/chafa
