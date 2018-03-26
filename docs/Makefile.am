## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=chafa

# The top-level SGML file.
DOC_MAIN_SGML_FILE=chafa-docs.xml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR = $(top_srcdir)/chafa

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="G_DISABLE_DEPRECATED" --ignore-decorators="GLIB_VAR|G_GNUC_WARN_UNUSED_RESULT"

# Used for dependencies
HFILE_GLOB = $(top_srcdir)/chafa/chafa.h
CFILE_GLOB = $(top_srcdir)/chafa/chafa-*.c

# Ignore some private headers
IGNORE_HFILES = \
	chafa.h \
	named-colors.h \
	chafa-private.h

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--output-format=xml --name-space=chafa

FIXXREF_OPTIONS=--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib

# Images to copy into HTML directory
HTML_IMAGES =

# include common portion ...
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST +=				\
	version.xml.in

########################################################################

man_MANS =

if ENABLE_MAN

man_MANS += \
	chafa.1

XSLTPROC_FLAGS = \
        --nonet \
        --stringparam man.output.quietly 1 \
        --stringparam funcsynopsis.style ansi \
        --stringparam man.th.extra1.suppress 1 \
        --stringparam man.authors.section.enabled 0 \
        --stringparam man.copyright.section.enabled 0

.xml.1:
	$(AM_V_GEN) $(XSLTPROC) $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

dist-local-check-mans-enabled:
	if grep "Man generation disabled" $(man_MANS) >/dev/null; then $(RM) $(man_MANS); fi

else

$(man_MANS):
	echo Man generation disabled.  Creating dummy $@.  Configure with --enable-man to enable it.
	echo Man generation disabled.  Remove this file, configure with --enable-man, and rebuild > $@


dist-local-check-mans-enabled:
	echo "*** --enable-man must be used in order to make dist"
	false

endif

CLEANFILES ?=
CLEANFILES += $(man_MANS)

EXTRA_DIST += $(man_MANS)

dist-hook-local: dist-local-check-mans-enabled all-local

chafa-docs-clean: clean
	cd $(srcdir) && rm -rf xml html

