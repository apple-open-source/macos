# $XdotOrg: $
#

include $(top_srcdir)/cpprules.in

xdmscriptdir = $(XDMSCRIPTDIR)
xdmconfigdir = $(XDMCONFIGDIR)

dist_xdmconfig_DATA = \
	Xaccess

xdmconfig_DATA = \
	xdm-config \
	Xresources \
	Xservers

xdmscript_SCRIPTS = \
	Xreset \
	Xsession \
	Xstartup

dist_xdmscript_SCRIPTS = \
	GiveConsole \
	TakeConsole \
	Xsetup_0 \
	Xwilling

pixmapdir = $(XDM_PIXMAPDIR)

dist_pixmap_DATA = xorg-bw.xpm xorg.xpm

BUILT_SOURCES = Xservers.ws xdm-config Xreset Xresources Xsession Xstartup

CLEANFILES = $(BUILT_SOURCES) Xservers

EXTRA_DIST = Xservers.ws.cpp Xservers.fs \
	xdm-config.cpp Xreset.cpp Xresources.cpp Xsession.cpp Xstartup.cpp

Xservers: Xservers.$(SERVERSTYPE)
	ln -s Xservers.$(SERVERSTYPE) Xservers

# Defines used when cpp'ing config files to replace settings in the files

#if USE_XPM
XPMDEFINES = -DXPM -DBITMAPDIR=$(XDM_PIXMAPDIR) -DXDM_PIXMAP=$(XDM_PIXMAP) \
        -DXDM_BWPIXMAP=$(XDM_BWPIXMAP)
#endif

#if HAVE_MKTEMP_COMMAND
MKTEMP_DEFINES = -DMKTEMP_COMMAND=$(MKTEMP_COMMAND)
#endif

CPP_FILES_FLAGS = -DBINDIR=$(bindir) -DDEFAULTVT=$(DEFAULTVT) \
    -DXDMDIR=$(XDMLIBDIR) -DXDMLOGDIR=$(XDMLOGDIR) -DXDMPIDDIR=$(XDMPIDDIR) \
    -DXDMCONFIGDIR=$(XDMCONFIGDIR) -DXDMSCRIPTDIR=$(XDMSCRIPTDIR) \
    -DSU=$(SU) -DCHOOSERPATH=$(XDMLIBDIR)/chooser $(XPMDEFINES) \
    -DSHELL_CMD=$(SHELL_CMD) $(MKTEMP_DEFINES)

Xservers.ws: $(srcdir)/Xservers.ws.cpp
	$(RAWCPP) $(RAWCPPFLAGS) $(CPP_FILES_FLAGS) < $(srcdir)/Xservers.ws.cpp | $(CPP_SED_MAGIC) > $@
