if WANT_TOOLS

bin_PROGRAMS = \
	chafa

chafa_SOURCES = \
	chafa.c \
	chicle-file-mapping.c \
	chicle-file-mapping.h \
	chicle-font-loader.c \
	chicle-font-loader.h \
	chicle-gif-loader.c \
	chicle-gif-loader.h \
	chicle-grid-layout.c \
	chicle-grid-layout.h \
	chicle-media-loader.c \
	chicle-media-loader.h \
	chicle-media-pipeline.c \
	chicle-media-pipeline.h \
	chicle-options.c \
	chicle-options.h \
	chicle-path-queue.c \
	chicle-path-queue.h \
	chicle-placement-counter.c \
	chicle-placement-counter.h \
	chicle-png-loader.c \
	chicle-png-loader.h \
	chicle-named-colors.c \
	chicle-named-colors.h \
	qoi.h \
	chicle-qoi-loader.c \
	chicle-qoi-loader.h \
	chicle-util.c \
	chicle-util.h \
	chicle-xwd-loader.c \
	chicle-xwd-loader.h

if HAVE_AVIF
chafa_SOURCES += \
	chicle-avif-loader.c \
	chicle-avif-loader.h
endif

if HAVE_HEIF
chafa_SOURCES += \
	chicle-heif-loader.c \
	chicle-heif-loader.h
endif

if HAVE_JPEG
chafa_SOURCES += \
	chicle-jpeg-loader.c \
	chicle-jpeg-loader.h
endif

if HAVE_TIFF
chafa_SOURCES += \
	chicle-tiff-loader.c \
	chicle-tiff-loader.h
endif

if HAVE_SVG
chafa_SOURCES += \
	chicle-svg-loader.c \
	chicle-svg-loader.h
endif

if HAVE_WEBP
chafa_SOURCES += \
	chicle-webp-loader.c \
	chicle-webp-loader.h
endif

if HAVE_JXL
chafa_SOURCES += \
	chicle-jxl-loader.c \
	chicle-jxl-loader.h
endif

if HAVE_COREGRAPHICS
chafa_SOURCES += \
	chicle-coregraphics-loader.c \
	chicle-coregraphics-loader.h
endif

# We can pass -rpath so the binary knows where to find libchafa.so when
# installed outside /usr (e.g. the default /usr/local). This affects Ubuntu.
# Resolved by running ldconfig. See Github issue #32.
#
# This is disabled by default.

chafa_CFLAGS = $(CHAFA_CFLAGS) $(GLIB_CFLAGS) $(JPEG_CFLAGS) $(SVG_CFLAGS) $(TIFF_CFLAGS) $(WEBP_CFLAGS) $(JXL_CFLAGS) $(AVIF_CFLAGS) $(HEIF_CFLAGS) $(FREETYPE_CFLAGS) $(COREGRAPHICS_CFLAGS)
if ENABLE_RPATH
chafa_LDFLAGS = $(CHAFA_LDFLAGS) -rpath $(libdir)
endif
chafa_LDADD = $(GLIB_LIBS) $(JPEG_LIBS) $(SVG_LIBS) $(TIFF_LIBS) $(WEBP_LIBS) $(JXL_LIBS) $(AVIF_LIBS) $(HEIF_LIBS) $(COREGRAPHICS_LIBS) $(FREETYPE_LIBS) $(top_builddir)/chafa/libchafa.la $(top_builddir)/libnsgif/libnsgif.la $(top_builddir)/lodepng/liblodepng.la -lm $(WIN32_LDADD)

# On Microsoft Windows, we compile a resource file with windres and link it in.
# This enables UTF-8 support in filenames, environment variables, etc.

if IS_WIN32_BUILD
WIN32_LDADD = manifest.o
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
