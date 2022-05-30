SUBDIRS = smolscale

## --- Library ---

noinst_LTLIBRARIES = libchafa-internal.la

libchafa_internal_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -DCHAFA_COMPILATION
libchafa_internal_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
libchafa_internal_la_LIBADD = $(GLIB_LIBS) smolscale/libsmolscale.la -lm

libchafa_internal_la_SOURCES = \
	chafa-base64.c \
	chafa-base64.h \
	chafa-batch.c \
	chafa-batch.h \
	chafa-bitfield.h \
	chafa-canvas-internal.h \
	chafa-canvas-printer.c \
	chafa-canvas-printer.h \
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
	chafa-iterm2-canvas.c \
	chafa-iterm2-canvas.h \
	chafa-kitty-canvas.c \
	chafa-kitty-canvas.h \
	chafa-palette.c \
	chafa-palette.h \
	chafa-pca.c \
	chafa-pca.h \
	chafa-pixops.c \
	chafa-pixops.h \
	chafa-private.h \
	chafa-sixel-canvas.c \
	chafa-sixel-canvas.h \
	chafa-string-util.c \
	chafa-string-util.h \
	chafa-symbols.c \
	chafa-symbols-ascii.h \
	chafa-symbols-block.h \
	chafa-symbols-kana.h \
	chafa-symbols-latin.h \
	chafa-symbols-misc-narrow.h \
	chafa-work-cell.c \
	chafa-work-cell.h

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

## --- General ---

## Include $(top_builddir)/chafa to get generated chafaconfig.h.

AM_CPPFLAGS = \
	-I$(top_srcdir)/chafa \
	-I$(top_builddir)/chafa
