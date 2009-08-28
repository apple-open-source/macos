xextdir = $(includedir)/X11/extensions
xext_HEADERS = \
	dpms.h \
	dpmsstr.h \
	extutil.h \
        ge.h \
        geproto.h \
        Xge.h \
	lbxbuf.h \
	lbxbufstr.h \
	lbxdeltastr.h \
	lbximage.h \
	lbxopts.h \
	lbxstr.h \
	lbxzlib.h \
	MITMisc.h \
	mitmiscstr.h \
	multibuf.h \
	multibufst.h \
	security.h \
	securstr.h \
	shape.h \
	shapestr.h \
	shmstr.h \
	sync.h \
	syncstr.h \
	Xag.h \
	Xagstr.h \
	Xcup.h \
	Xcupstr.h \
	Xdbe.h \
	Xdbeproto.h \
	XEVI.h \
	XEVIstr.h \
	Xext.h \
	XLbx.h \
	XShm.h \
	xtestext1.h \
	XTest.h \
	xteststr.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xextproto.pc

EXTRA_DIST = xextproto.pc.in geproto.txt

EXTRA_DIST += ChangeLog
MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog
