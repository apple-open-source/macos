RC_OS = macos
OTHER_SRCS = Makefile
PRIVATE_NOTES = Private_CompilerTools.html FatFiles.rtf
NOTES = CompilerTools.html Prebinding.html
INSTALL_FILES = $(OTHER_SRCS) $(NOTES) $(PRIVATE_NOTES)

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

ifeq "macos" "$(RC_OS)"
  NOTESDIR := $(shell if [ "$(RC_RELEASE)" = "Beaker" ] || \
			 [ "$(RC_RELEASE)" = "Bunsen" ] || \
			 [ "$(RC_RELEASE)" = "Gonzo" ]; then \
			 echo /System/Documentation/Developer ; else \
			 echo /Developer/Documentation; fi; \
		)/ReleaseNotes
else
  NOTESDIR = /System/Documentation/Developer/ReleaseNotes
endif
PRIVATE_NOTESDIR = /usr/local/RelNotes

install: ${DSTROOT}${NOTESDIR} ${DSTROOT}${PRIVATE_NOTESDIR} ${NOTES} ${PRIVATE_NOTES}
	install -c -m 444 ${IFLAGS} ${NOTES} ${DSTROOT}${NOTESDIR}
	install -c -m 444 ${IFLAGS} ${PRIVATE_NOTES} ${DSTROOT}${PRIVATE_NOTESDIR}

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

${DSTROOT}${NOTESDIR}:; $(MKDIRS) $@

${DSTROOT}${PRIVATE_NOTESDIR}:; $(MKDIRS) $@

${DSTROOT}${PPC_ASSEMBLER_DIR}:; $(MKDIRS) $@

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
