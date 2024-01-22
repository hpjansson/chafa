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
	media-loader.c \
	media-loader.h \
	placement-counter.c \
	placement-counter.h \
	png-loader.c \
	png-loader.h \
	named-colors.c \
	named-colors.h \
	qoi.h \
	qoi-loader.c \
	qoi-loader.h \
	util.c \
	util.h \
	xwd-loader.c \
	xwd-loader.h

if HAVE_AVIF
chafa_SOURCES += \
	avif-loader.c \
	avif-loader.h
endif

if HAVE_JPEG
chafa_SOURCES += \
	jpeg-loader.c \
	jpeg-loader.h
endif

if HAVE_TIFF
chafa_SOURCES += \
	tiff-loader.c \
	tiff-loader.h
endif

if HAVE_SVG
chafa_SOURCES += \
	svg-loader.c \
	svg-loader.h
endif

if HAVE_WEBP
chafa_SOURCES += \
	webp-loader.c \
	webp-loader.h
endif

if HAVE_JXL
chafa_SOURCES += \
	jxl-loader.c \
	jxl-loader.h
endif

# We can pass -rpath so the binary knows where to find libchafa.so when
# installed outside /usr (e.g. the default /usr/local). This affects Ubuntu.
# Resolved by running ldconfig. See Github issue #32.
#
# This is disabled by default.

chafa_CFLAGS = $(CHAFA_CFLAGS) $(GLIB_CFLAGS) $(JPEG_CFLAGS) $(SVG_CFLAGS) $(TIFF_CFLAGS) $(WEBP_CFLAGS) $(JXL_CFLAGS) $(AVIF_CFLAGS) $(FREETYPE_CFLAGS)
if ENABLE_RPATH
chafa_LDFLAGS = $(CHAFA_LDFLAGS) -rpath $(libdir)
endif
chafa_LDADD = $(GLIB_LIBS) $(JPEG_LIBS) $(SVG_LIBS) $(TIFF_LIBS) $(WEBP_LIBS) $(JXL_LIBS) $(AVIF_LIBS) $(FREETYPE_LIBS) $(top_builddir)/chafa/libchafa.la $(top_builddir)/libnsgif/libnsgif.la $(top_builddir)/lodepng/liblodepng.la $(WIN32_LDADD)

# On Microsoft Windows, we compile a resource file with windres and link it in.
# This enables UTF-8 support in filenames, environment variables, etc.

if IS_WIN32_BUILD
WIN32_LDADD = manifest.o
chafa_SOURCES += \
	conhost.c \
	conhost.h
manifest.o: $(srcdir)/manifest.rc
	$(WINDRES) -o $@ $(srcdir)/manifest.rc
else
WIN32_LDADD =
endif

endif

## --- General ---

## Include $(top_builddir)/chafa to get generated chafaconfig.h.

AM_CPPFLAGS = \
	-I$(top_srcdir)/chafa \
	-I$(top_builddir)/chafa \
	-I$(top_srcdir)/libnsgif \
	-I$(top_srcdir)/lodepng

EXTRA_DIST = manifest.rc ms-utf8.xml
