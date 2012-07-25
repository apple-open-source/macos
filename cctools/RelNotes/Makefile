RC_OS = macos
OTHER_SRCS = Makefile
PRIVATE_NOTES = Private_CompilerTools.html FatFiles.rtf
COMPILERTOOLS_NOTES = CompilerTools.html
PREBINDING_NOTES = CompilerTools.html
INSTALL_FILES = $(OTHER_SRCS) $(COMPILERTOOLS_NOTES) $(PREBINDING_NOTES) \
		$(PRIVATE_NOTES)

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

PRIVATE_NOTESDIR = /usr/local/RelNotes

install: ${PRIVATE_NOTES}
	$(MKDIRS) ${DSTROOT}${PRIVATE_NOTESDIR}
	install -c -m 444 ${IFLAGS} ${PRIVATE_NOTES} \
		${DSTROOT}${PRIVATE_NOTESDIR}

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

depend:

clean:
shlib_clean:

all:

ifeq "macos" "$(RC_OS)"
  WEBDIR = /Library/WebServer/Documents
else
  WEBDIR = /Local/Library/WebServer/Documents
endif

local_install:
	cp Private_CompilerTools.html $(WEBDIR)
	cp CompilerTools.html $(WEBDIR)
	cp Prebinding.html $(WEBDIR)
