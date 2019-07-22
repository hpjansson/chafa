if WANT_TOOLS

bin_PROGRAMS = \
	chafa

chafa_SOURCES = \
	chafa.c \
	file-mapping.c \
	file-mapping.h \
	gif-loader.c \
	gif-loader.h \
	named-colors.c \
	named-colors.h \
	xwd-loader.c \
	xwd-loader.h

chafa_CFLAGS = $(CHAFA_CFLAGS) $(GLIB_CFLAGS) $(MAGICKWAND_CFLAGS)
chafa_LDFLAGS = $(CHAFA_LDFLAGS)
chafa_LDADD = $(GLIB_LIBS) $(MAGICKWAND_LIBS) $(top_builddir)/chafa/libchafa.la $(top_builddir)/libnsgif/libnsgif.la

endif

## -- General ---

AM_CPPFLAGS = \
	-I$(top_srcdir)/chafa \
	-I$(top_srcdir)/libnsgif
