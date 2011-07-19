EXTSOURCES =	xproto.py \
		bigreq.py \
		xc_misc.py
EXTENSION_XML = xproto.xml \
		bigreq.xml \
		xc_misc.xml

pkgpythondir = $(pyexecdir)/xcb

pkgpython_LTLIBRARIES = xcb.la

xcb_la_LIBADD = $(LIBXCB_LIBS)
xcb_la_CPPFLAGS = -I$(PYTHON_INCLUDE)
xcb_la_CFLAGS = -g $(CWARNFLAGS)
xcb_la_LDFLAGS = -module
xcb_la_SOURCES = conn.c constant.c cookie.c error.c event.c except.c \
		 ext.c extkey.c iter.c list.c module.c protobj.c \
		 reply.c request.c response.c struct.c union.c void.c \
		 py_client.py

noinst_HEADERS = conn.h constant.h cookie.h error.h event.h except.h \
		 ext.h extkey.h iter.h list.h module.h protobj.h \
		 reply.h request.h response.h struct.h union.h void.h

# FIXME: find a way to autogenerate this from the XML files.

if BUILD_COMPOSITE
EXTSOURCES += composite.py
EXTENSION_XML += composite.xml
endif

if BUILD_DAMAGE
EXTSOURCES += damage.py
EXTENSION_XML += damage.xml
endif

if BUILD_DPMS
EXTSOURCES += dpms.py
EXTENSION_XML += dpms.xml
endif

if BUILD_GLX
EXTSOURCES += glx.py
EXTENSION_XML += glx.xml
endif

if BUILD_RANDR
EXTSOURCES += randr.py
EXTENSION_XML += randr.xml
endif

if BUILD_RECORD
EXTSOURCES += record.py
EXTENSION_XML += record.xml
endif

if BUILD_RENDER
EXTSOURCES += render.py
EXTENSION_XML += render.xml
endif

if BUILD_RES
EXTSOURCES += res.py
EXTENSION_XML += res.xml
endif

if BUILD_SCREENSAVER
EXTSOURCES += screensaver.py
EXTENSION_XML += screensaver.xml
endif

if BUILD_SHAPE
EXTSOURCES += shape.py
EXTENSION_XML += shape.xml
endif

if BUILD_SHM
EXTSOURCES += shm.py
EXTENSION_XML += shm.xml
endif

if BUILD_SYNC
EXTSOURCES += sync.py
EXTENSION_XML += sync.xml
endif

if BUILD_XEVIE
EXTSOURCES += xevie.py
EXTENSION_XML += xevie.xml
endif

if BUILD_XF86DRI
EXTSOURCES += xf86dri.py
EXTENSION_XML += xf86dri.xml
endif

if BUILD_XFIXES
EXTSOURCES += xfixes.py
EXTENSION_XML += xfixes.xml
endif

if BUILD_XINERAMA
EXTSOURCES += xinerama.py
EXTENSION_XML += xinerama.xml
endif

if BUILD_XINPUT
EXTSOURCES += xinput.py
EXTENSION_XML += xinput.xml
endif

if BUILD_XPRINT
EXTSOURCES += xprint.py
EXTENSION_XML += xprint.xml
endif

if BUILD_SELINUX
EXTSOURCES += xselinux.py
EXTENSION_XML += xselinux.xml
endif

if BUILD_XTEST
EXTSOURCES += xtest.py
EXTENSION_XML += xtest.xml
endif

if BUILD_XV
EXTSOURCES += xv.py
EXTENSION_XML += xv.xml
endif

if BUILD_XVMC
EXTSOURCES += xvmc.py
EXTENSION_XML += xvmc.xml
endif


pkgpython_PYTHON = __init__.py $(EXTSOURCES)

BUILT_SOURCES = $(EXTSOURCES)
CLEANFILES = $(EXTSOURCES) $(EXTENSION_XML)

$(EXTSOURCES): py_client.py

SUFFIXES = .xml

.xml.py:
	$(PYTHON) $(srcdir)/py_client.py -p $(XCBPROTO_XCBPYTHONDIR) $(XCBPROTO_XCBINCLUDEDIR)/$<

$(EXTENSION_XML):
	$(LN_S) -f $(XCBPROTO_XCBINCLUDEDIR)/$@ $@
