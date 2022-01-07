SUBDIRS = chafa docs libnsgif tests tools

EXTRA_DIST = \
	HACKING \
	README.md \
	autogen.sh \
	chafa.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = chafa.pc

all-local:
	@echo
	@echo ---
	@echo --- Success! You can now run tools/chafa/chafa, or install everything
	@echo --- using "make install" or "sudo make install".
	@echo ---
	@echo -e '--- \e[0;91mNOTE:\e[0m You may have to run \e[0;93msudo ldconfig\e[0m after installing.'
	@echo ---
	@echo
