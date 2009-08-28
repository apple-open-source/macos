libmandir = $(LIB_MAN_DIR)

libman_PRE = Xcursor.man
libman_DATA = $(libman_PRE:man=@LIB_MAN_SUFFIX@)

all-local: $(libman_DATA)

EXTRA_DIST = $(libman_PRE)
CLEANFILES = $(libman_DATA)

SUFFIXES = .$(LIB_MAN_SUFFIX) .man

# Strings to replace in man pages
XORGRELSTRING = @PACKAGE_STRING@

MAN_SUBSTS = \
	-e 's|__xorgversion__|"$(XORGRELSTRING)"|' \
	-e 's|__XCURSORPATH__|$(XCURSORPATH_LIST)|g' \
	-e 's|__appmansuffix__|$(APP_MAN_SUFFIX)|g' \
	-e 's|__libmansuffix__|$(LIB_MAN_SUFFIX)|g' \
	-e 's|__adminmansuffix__|$(ADMIN_MAN_SUFFIX)|g' \
	-e 's|__miscmansuffix__|$(MISC_MAN_SUFFIX)|g' \
	-e 's|__filemansuffix__|$(FILE_MAN_SUFFIX)|g'

.man.$(LIB_MAN_SUFFIX):
	-rm -f $@
	sed $(MAN_SUBSTS) < $< > $@

# Generate man page shadow files

LIB_MAN_DIR_SUFFIX = $(LIB_MAN_DIR:@mandir@/man%=%)
libman_DATA += $(Xcursor_shadows:=.@LIB_MAN_SUFFIX@)
BUILT_SOURCES = shadows.DONE
CLEANFILES += shadows.DONE

Xcursor_shadows = \
	XcursorImageCreate \
	XcursorImageDestroy \
	XcursorImagesCreate \
	XcursorImagesDestroy \
	XcursorCursorsCreate \
	XcursorCursorsDestroy \
	XcursorXcFileLoadImage \
	XcursorXcFileLoadImages \
	XcursorXcFileLoadAllImages \
	XcursorXcFileLoad \
	XcursorXcFileSave \
	XcursorFilenameLoadImage \
	XcursorFilenameLoadImages \
	XcursorFilenameLoadAllImages \
	XcursorFilenameLoad \
	XcursorFilenameSaveImages \
	XcursorFilenameSave \
	XcursorLibraryLoadImage \
	XcursorLibraryLoadImages \
	XcursorFilenameLoadCursor \
	XcursorLibraryLoadCursor \
	XcursorLibraryLoadCursors \
	XcursorShapeLoadImage \
	XcursorShapeLoadImages \
	XcursorShapeLoadCursor \
	XcursorShapeLoadCursors \
	XcursorSupportsARGB \
	XcursorSetDefaultSize \
	XcursorGetDefaultSize \
	XcursorSetTheme \
	XcursorGetTheme

shadows.DONE:
	-rm -f $(Xcursor_shadows:=.@LIB_MAN_SUFFIX@)
	(for i in $(Xcursor_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/Xcursor.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
