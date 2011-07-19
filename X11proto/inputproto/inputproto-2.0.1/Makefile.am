inputdir = $(includedir)/X11/extensions
input_HEADERS = \
	XI.h \
	XIproto.h \
	XI2.h \
	XI2proto.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = inputproto.pc

dist_doc_DATA = XI2proto.txt XIproto.txt


MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
