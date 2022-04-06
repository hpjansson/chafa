if WANT_TOOLS

bin_PROGRAMS = \
	chafa

chafa_SOURCES = \
	chafa.c \
	file-mapping.c \
	file-mapping.h \
	font-loader.c \
	font-loader.h \
	gif-loader.c \
	gif-loader.h \
	im-loader.c \
	im-loader.h \
	jpeg-loader.c \
	jpeg-loader.h \
	media-loader.c \
	media-loader.h \
	png-loader.c \
	png-loader.h \
	named-colors.c \
	named-colors.h \
	xwd-loader.c \
	xwd-loader.h

# We can pass -rpath so the binary knows where to find libchafa.so when
# installed outside /usr (e.g. the default /usr/local). This affects Ubuntu.
# Resolved by running ldconfig. See Github issue #32.
#
# This is disabled by default.

chafa_CFLAGS = $(CHAFA_CFLAGS) $(GLIB_CFLAGS) $(MAGICKWAND_CFLAGS) $(LIBJPEG_CFLAGS) $(FREETYPE_CFLAGS)
if ENABLE_RPATH
chafa_LDFLAGS = $(CHAFA_LDFLAGS) -rpath $(libdir)
endif
chafa_LDADD = $(GLIB_LIBS) $(MAGICKWAND_LIBS) $(LIBJPEG_LIBS) $(FREETYPE_LIBS) $(top_builddir)/chafa/libchafa.la $(top_builddir)/libnsgif/libnsgif.la $(top_builddir)/lodepng/liblodepng.la

endif

## -- General ---

## Include $(top_builddir)/chafa to get generated chafaconfig.h.

AM_CPPFLAGS = \
	-I$(top_srcdir)/chafa \
	-I$(top_builddir)/chafa \
	-I$(top_srcdir)/libnsgif \
	-I$(top_srcdir)/lodepng
