noinst_LTLIBRARIES = libsmolscale.la

SMOLSCALE_CFLAGS = $(LIBCHAFA_CFLAGS)
SMOLSCALE_LDFLAGS = $(LIBCHAFA_LDFLAGS)

if HAVE_AVX2_INTRINSICS
SMOLSCALE_CFLAGS += -DSMOL_WITH_AVX2
endif

libsmolscale_la_CFLAGS = $(SMOLSCALE_CFLAGS)
libsmolscale_la_LDFLAGS = $(SMOLSCALE_LDFLAGS)
libsmolscale_la_LIBADD =

libsmolscale_la_SOURCES = \
	smolscale.c \
	smolscale.h \
	smolscale-private.h

if HAVE_AVX2_INTRINSICS
noinst_LTLIBRARIES += libsmolscale-avx2.la
libsmolscale_la_LIBADD += libsmolscale-avx2.la
libsmolscale_avx2_la_SOURCES = smolscale-avx2.c
libsmolscale_avx2_la_CFLAGS = $(SMOLSCALE_CFLAGS) -mavx2
libsmolscale_avx2_la_LDFLAGS = $(SMOLSCALE_LDFLAGS)
endif
