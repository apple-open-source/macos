BUILT_DEFFILES = \
	host.def \
	date.def \
	version.def

DEFFILES = \
	cross.def \
	site.def \
	xf86site.def \
	xorgsite.def \
	xorgversion.def

RULEFILES = \
	Imake.rules \
	Motif.rules \
	OpenBSDLib.rules \
	Win32.rules \
	X11.rules \
	bsdiLib.rules \
	bsdLib.rules \
	cde.rules \
	cross.rules \
	cygwin.rules \
	mingw.rules \
	darwinLib.rules \
	gnuLib.rules \
	hpLib.rules \
	ibmLib.rules \
	lnxLib.rules \
	lnxdoc.rules \
	necLib.rules \
	noop.rules \
	nto.rules \
	oldlib.rules \
	os2.rules \
	os2Lib.rules \
	os2Lib.rules \
	osfLib.rules \
	scoLib.rules \
	sequentLib.rules \
	sgiLib.rules \
	sunLib.rules \
	sv3Lib.rules \
	sv4Lib.rules \
	QNX4.rules \
	xf86.rules

TMPLFILES = \
	Imake.tmpl \
	Library.tmpl \
	Motif.tmpl \
	OpenBSDLib.tmpl \
	Server.tmpl \
	ServerLib.tmpl \
	Threads.tmpl \
	WinLib.tmpl \
	X11.tmpl \
	bsdiLib.tmpl \
	bsdLib.tmpl \
	cde.tmpl \
	cygwin.tmpl \
	mingw.tmpl \
	darwinLib.tmpl \
	hpLib.tmpl \
	gnuLib.tmpl \
	ibmLib.tmpl \
	lnxLib.tmpl \
	lnxdoc.tmpl \
	necLib.tmpl \
	os2Lib.tmpl \
	osfLib.tmpl \
	sgiLib.tmpl \
	sunLib.tmpl \
	sv3Lib.tmpl \
	sv4Lib.tmpl \
	xf86.tmpl \
	xorg.tmpl

CFFILES = \
	Imake.cf \
	Amoeba.cf \
	DGUX.cf \
	dmx.cf \
	DragonFly.cf \
	FreeBSD.cf \
	Mips.cf \
	NetBSD.cf \
	OpenBSD.cf \
	Oki.cf \
	cygwin.cf \
	mingw.cf \
	Win32.cf \
	apollo.cf \
	bsd.cf \
	bsdi.cf \
	convex.cf \
	cray.cf \
	fujitsu.cf \
	generic.cf \
	gnu.cf \
	hp.cf \
	ibm.cf \
	isc.cf \
	linux.cf \
	luna.cf \
	macII.cf \
	mach.cf \
	minix.cf \
	moto.cf \
	ncr.cf \
	nec.cf \
	nto.cf \
	os2.cf \
	osf1.cf \
	pegasus.cf \
	sco.cf \
	sco5.cf \
	sequent.cf \
	sgi.cf \
	sony.cf \
	sun.cf \
	svr3.cf \
	svr4.cf \
	ultrix.cf \
	usl.cf \
	lynx.cf \
	x386.cf \
	QNX4.cf \
	darwin.cf \
	xfree86.cf \
	xorg.cf

configdir = @XCONFDIR@
dist_config_DATA = $(RULEFILES) $(TMPLFILES) $(DEFFILES) $(CFFILES)
config_DATA = $(BUILT_DEFFILES)

EXTRA_DIST = \
	os2def.db \
	site.sample \
	xprint_host.def

all-local: $(BUILT_DEFFILES)

host.def:
	echo "/* dummy to placate Imake */" > host.def
date.def:
	echo "/* dummy to placate Image */" >  date.def
version.def:
	echo "/* dummy to placate Image */" >  version.def

CLEANFILES = $(BUILT_DEFFILES)
