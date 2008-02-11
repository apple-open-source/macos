## A very generic makefile for recursing in sub-directories that respect the usual release control rules

NAME = SMM.doc

SUBDIRS = timed timed

SOURCES = Makefile

FASTCP = /bin/cp
RM     = /bin/rm
MKDIRS = /bin/mkdir -p
CHOWN = /usr/sbin/chown
ECHO   = echo

all::
	@$(MAKE) build \
		"TARGET = all" \
		"OBJROOT = ./Objects" \
		"SYMROOT = ./Symbols" \
		"RC_CFLAGS = $(RC_CFLAGS)" \
		"RC_ARCHS = $(RC_ARCHS)"

build:: $(OBJROOT) $(SYMROOT) recurse

recurse::
	@echo '====' make $(TARGET) for: $(SUBDIRS) '===='
	@HERE=`pwd`;						\
	cd ${OBJROOT};						\
	ABSOROOT=`pwd`;						\
	cd $$HERE;						\
	cd ${SYMROOT};						\
	ABSSROOT=`pwd`;						\
	cd $$HERE;						\
	for i in ${SUBDIRS}; \
	do \
		echo == make $(TARGET) for $$i ==; \
		(cd $$i; ${MAKE} $(TARGET) 			\
			OBJROOT="$$ABSOROOT/$$i"		\
			SYMROOT="$$ABSSROOT/$$i"		\
			DSTROOT="$(DSTROOT)"			\
			SRCROOT="$(SRCROOT)/$$i"		\
			RC_CFLAGS="$(RC_CFLAGS)"		\
			RC_ARCHS="$(RC_ARCHS)"			\
			|| exit 1); 				\
	done
	@echo '====' make $(TARGET) for $(SUBDIRS) done '===='

installhdrs::

install:: $(DSTROOT)
	@$(MAKE) build \
		"TARGET = install" \
		"DSTROOT = $(DSTROOT)" \
		"OBJROOT = $(OBJROOT)" \
		"SYMROOT = $(SYMROOT)" \
		"RC_CFLAGS = $(RC_CFLAGS)" \
		"RC_ARCHS = $(RC_ARCHS)"

installsrc::
	$(MKDIRS) $(SRCROOT)
	@$(FASTCP) $(SOURCES) $(SRCROOT)
	@$(MAKE) recurse "TARGET = $@"

clean:
	@$(RM) -rf Objects Symbols *_obj sym
	@$(MAKE) recurse "TARGET = $@"

copy::	DEST
	@$(MAKE) installsrc "SRCROOT = $(DEST)"

$(DSTROOT):
	$(MKDIRS) -m 755 $@
	$(CHOWN) root:wheel $@

$(SYMROOT):
	@$(MKDIRS) $(SYMROOT)

$(OBJROOT):
	@$(MKDIRS) $(OBJROOT)

SRCROOT DEST:
	@if [ -n "${$@}" ]; then exit 0; \
	else $(ECHO) Must define $@; exit 1; fi

BACKUPROOT = $(PROJECT_BACKUP_DIR)/$(NAME)_proj/$(USER)/Archives

backup::
	$(MAKE) installsrc SRCROOT=`backupname $(BACKUPROOT)/$(NAME)`

help::
	@$(ECHO) 'Makefile targets are:'
	@$(ECHO) 'install:	makes install (fat)'
	@$(ECHO) 'copy:		copies the sources to DEST'
	@$(ECHO) 'installsrc:	copies the sources to SRCROOT'
	
