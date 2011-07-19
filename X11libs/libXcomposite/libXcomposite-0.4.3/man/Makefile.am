libmandir = $(LIB_MAN_DIR)

libman_xml = Xcomposite.xml

libman_PRE = $(libman_xml:xml=man)		\
	     $(Xcomposite_shadows:=.man)

Xcomposite_shadows =	 			\
	XCompositeQueryExtension		\
	XCompositeQueryVersion			\
	XCompositeVersion			\
	XCompositeRedirectWindow		\
	XCompositeRedirectSubwindows		\
	XCompositeUnredirectWindow		\
	XCompositeUnredirectSubwindows		\
	XCompositeCreateRegionFromBorderClip	\
	XCompositeNameWindowPixmap		\
	XCompositeGetOverlayWindow		\
	XCompositeReleaseOverlayWindow

if INSTALL_MANPAGES
libman_DATA = $(libman_PRE:man=@LIB_MAN_SUFFIX@)
endif

EXTRA_DIST  = $(libman_PRE) $(libman_xml)

CLEANFILES  = $(libman_DATA)

MAINTAINERCLEANFILES = $(libman_xml:xml=man)

SUFFIXES = .$(LIB_MAN_SUFFIX) .man .xml

# String replacements in MAN_SUBSTS now come from xorg-macros.m4 via configure
.man.$(LIB_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

# Generate nroff sources from DocBook/xml

dist-hook: $(libman_PRE)

if HAVE_XMLTO
.xml.man:
	$(AM_V_GEN)$(XMLTO) man $< && mv $*.__libmansuffix__ $@
else
.xml.man:
	@echo "WARNING: configure did not find xmlto, cannot create $@ without it"
endif
