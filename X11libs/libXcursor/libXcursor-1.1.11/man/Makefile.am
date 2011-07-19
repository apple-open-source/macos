libmandir = $(LIB_MAN_DIR)

libman_PRE =				\
	Xcursor.man			\
	$(Xcursor_shadows:=.man)

Xcursor_shadows =			\
	XcursorImageCreate		\
	XcursorImageDestroy		\
	XcursorImagesCreate		\
	XcursorImagesDestroy		\
	XcursorCursorsCreate		\
	XcursorCursorsDestroy		\
	XcursorXcFileLoadImage		\
	XcursorXcFileLoadImages		\
	XcursorXcFileLoadAllImages	\
	XcursorXcFileLoad		\
	XcursorXcFileSave		\
	XcursorFilenameLoadImage	\
	XcursorFilenameLoadImages	\
	XcursorFilenameLoadAllImages	\
	XcursorFilenameLoad		\
	XcursorFilenameSaveImages	\
	XcursorFilenameSave		\
	XcursorLibraryLoadImage		\
	XcursorLibraryLoadImages	\
	XcursorFilenameLoadCursor	\
	XcursorLibraryLoadCursor	\
	XcursorLibraryLoadCursors	\
	XcursorShapeLoadImage		\
	XcursorShapeLoadImages		\
	XcursorShapeLoadCursor		\
	XcursorShapeLoadCursors		\
	XcursorSupportsARGB		\
	XcursorSetDefaultSize		\
	XcursorGetDefaultSize		\
	XcursorSetTheme			\
	XcursorGetTheme

libman_DATA = $(libman_PRE:man=@LIB_MAN_SUFFIX@)

EXTRA_DIST = $(libman_PRE)
CLEANFILES = $(libman_DATA)

SUFFIXES = .$(LIB_MAN_SUFFIX) .man

# String replacements in MAN_SUBSTS now come from xorg-macros.m4 via configure
MAN_SUBSTS += -e 's|__XCURSORPATH__|$(XCURSORPATH_LIST)|g'

.man.$(LIB_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@
