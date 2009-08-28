RC_OS = macos
OTHER_SRCS = Makefile
OPEN = -D__OPEN_SOURCE__ 
COMPILERTOOLS_NOTES = CompilerTools.html
PREBINDING_NOTES = CompilerTools.html
INSTALL_FILES = $(OTHER_SRCS) $(COMPILERTOOLS_NOTES) $(PREBINDING_NOTES) \
		$(PRIVATE_NOTES)

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

ifeq "macos" "$(RC_OS)"
  NOTESDIR := $(shell if [ "$(RC_RELEASE)" = "Beaker" ] || \
			 [ "$(RC_RELEASE)" = "Bunsen" ] || \
			 [ "$(RC_RELEASE)" = "Gonzo" ]; then \
	echo /System/Documentation/Developer/ReleaseNotes ; else \
		      if [ "$(RC_RELEASE)" = "Kodiak"    ] || \
		         [ "$(RC_RELEASE)" = "Cheetah"   ] || \
		         [ "$(RC_RELEASE)" = "Puma"      ] || \
		         [ "$(RC_RELEASE)" = "Jaguar"    ] || \
		         [ "$(RC_RELEASE)" = "Panther" ]; then \
	echo /Developer/Documentation/ReleaseNotes; else \
	echo /Developer/Documentation/DocSets/com.apple.ADC_Reference_Library.DeveloperTools.docset/Contents/Resources/Documents/documentation/DeveloperTools ; \
    fi; \
  fi;)
else
  NOTESDIR = /System/Documentation/Developer/ReleaseNotes
endif

install: ${COMPILERTOOLS_NOTES} ${PREBINDING_NOTES} ${PRIVATE_NOTES}
	$(MKDIRS) '${DSTROOT}${NOTESDIR}'
	install -c -m 444 ${IFLAGS} ${COMPILERTOOLS_NOTES} \
		'${DSTROOT}${NOTESDIR}'
	install -c -m 444 ${IFLAGS} ${PREBINDING_NOTES} \
		'${DSTROOT}${NOTESDIR}'

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
	cp CompilerTools.html $(WEBDIR)
	cp Prebinding.html $(WEBDIR)
