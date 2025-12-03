SUBDIRS = smolscale

## --- Library ---

libchafa_internal_sources = \
	chafa-base64.c \
	chafa-base64.h \
	chafa-batch.c \
	chafa-batch.h \
	chafa-bitfield.h \
	chafa-byte-fifo.c \
	chafa-byte-fifo.h \
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
	chafa-math-util.c \
	chafa-math-util.h \
	chafa-noise.c \
	chafa-noise.h \
	chafa-palette.c \
	chafa-palette.h \
	chafa-passthrough-encoder.c \
	chafa-passthrough-encoder.h \
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
	chafa-vector.h \
	chafa-wakeup.c \
	chafa-wakeup.h \
	chafa-work-cell.c \
	chafa-work-cell.h

noinst_LTLIBRARIES = libchafa-internal.la
check_LTLIBRARIES = libchafa-internal-test.la

libchafa_internal_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(CHAFA_VISIBILITY_FLAGS) $(GLIB_CFLAGS) -DCHAFA_COMPILATION
libchafa_internal_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
libchafa_internal_la_LIBADD = $(GLIB_LIBS) smolscale/libsmolscale.la -lm
libchafa_internal_la_SOURCES = $(libchafa_internal_sources)

libchafa_internal_test_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -DCHAFA_COMPILATION
libchafa_internal_test_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
libchafa_internal_test_la_LIBADD = $(GLIB_LIBS) smolscale/libsmolscale.la -lm
libchafa_internal_test_la_SOURCES = $(libchafa_internal_sources)

if HAVE_MMX_INTRINSICS
noinst_LTLIBRARIES += libchafa-mmx.la
libchafa_internal_la_LIBADD += libchafa-mmx.la
libchafa_internal_test_la_LIBADD += libchafa-mmx.la
libchafa_mmx_la_SOURCES = chafa-mmx.c
libchafa_mmx_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -mmmx -DCHAFA_COMPILATION
libchafa_mmx_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

if HAVE_SSE41_INTRINSICS
noinst_LTLIBRARIES += libchafa-sse41.la
libchafa_internal_la_LIBADD += libchafa-sse41.la
libchafa_internal_test_la_LIBADD += libchafa-sse41.la
libchafa_sse41_la_SOURCES = chafa-sse41.c
libchafa_sse41_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -msse4.1 -DCHAFA_COMPILATION
libchafa_sse41_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

if HAVE_POPCNT_INTRINSICS
noinst_LTLIBRARIES += libchafa-popcnt.la
libchafa_internal_la_LIBADD += libchafa-popcnt.la
libchafa_internal_test_la_LIBADD += libchafa-popcnt.la
libchafa_popcnt_la_SOURCES = chafa-popcnt.c
libchafa_popcnt_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -mpopcnt -DCHAFA_COMPILATION
libchafa_popcnt_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

if HAVE_AVX2_INTRINSICS
noinst_LTLIBRARIES += libchafa-avx2.la
libchafa_internal_la_LIBADD += libchafa-avx2.la
libchafa_internal_test_la_LIBADD += libchafa-avx2.la
libchafa_avx2_la_SOURCES = chafa-avx2.c
libchafa_avx2_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -mavx2 -DCHAFA_COMPILATION
libchafa_avx2_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

## --- General ---

## Include $(top_builddir)/chafa to get generated chafaconfig.h.

AM_CPPFLAGS = \
	-I$(top_srcdir)/chafa \
	-I$(top_builddir)/chafa
