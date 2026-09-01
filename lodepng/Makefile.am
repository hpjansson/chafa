noinst_LTLIBRARIES = liblodepng.la

liblodepng_la_SOURCES = lodepng.c lodepng.h
liblodepng_la_CFLAGS = ${LODEPNG_CFLAGS} ${LIBDEFLATE_CFLAGS} -I${top_srcdir}
liblodepng_la_LDFLAGS = ${LODEPNG_LDFLAGS}
liblodepng_la_LIBADD = ${LIBDEFLATE_LIBS}

if HAVE_AVX2_INTRINSICS
noinst_LTLIBRARIES += liblodepng-avx2.la
liblodepng_la_LIBADD += liblodepng-avx2.la
liblodepng_avx2_la_SOURCES = lodepng-avx2.c lodepng-avx2.h
liblodepng_avx2_la_CFLAGS = ${LODEPNG_CFLAGS} -I${top_srcdir} -mavx2
liblodepng_avx2_la_LDFLAGS = ${LODEPNG_LDFLAGS}
endif

EXTRA_DIST = LICENSE README.md
