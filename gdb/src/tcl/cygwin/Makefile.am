## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = cygnus

updir=$(srcdir)/..
vpath % $(updir)/generic:$(updir)/unix

noinst_LIBRARIES = libtcl_cygwin.a

libtcl_cygwin_a_SOURCES = \
	regcomp.c \
	regexec.c \
        regfree.c \
        regerror.c \
	tclAlloc.c \
	tclAsync.c \
	tclBasic.c \
	tclBinary.c \
	tclCkalloc.c \
	tclClock.c \
	tclCmdAH.c \
	tclCmdIL.c \
	tclCmdMZ.c \
        tclCompCmds.c \
	tclCompExpr.c \
	tclCompile.c \
	tclDate.c \
        tclEncoding.c \
	tclEnv.c \
	tclEvent.c \
	tclExecute.c \
	tclFCmd.c \
	tclFileName.c \
	tclGet.c \
	tclHash.c \
	tclHistory.c \
	tclIndexObj.c \
	tclInterp.c \
	tclIO.c \
	tclIOCmd.c \
        tclIOGT.c \
	tclIOSock.c \
	tclIOUtil.c \
	tclLink.c \
	tclListObj.c \
	tclLiteral.c \
	tclLoad.c \
	tclLoadNone.c \
	tclMain.c \
	tclNamesp.c \
	tclNotify.c \
	tclObj.c \
        tclParse.c \
	tclPanic.c \
	tclParseExpr.c \
	tclPipe.c \
	tclPkg.c \
	tclPosixStr.c \
	tclPreserve.c \
	tclProc.c \
	tclRegexp.c \
	tclTestProcBodyObj.c \
	tclResolve.c \
	tclResolve.c \
        tclResult.c \
	tclScan.c \
	tclStringObj.c \
        tclStubInit.c \
        tclStubLib.c \
        tclThread.c \
	tclTimer.c \
	tclUtf.c \
	tclUtil.c \
	tclVar.c \
	tclAppInit.c \
	tclUnixChan.c \
	tclUnixEvent.c \
	tclUnixFCmd.c \
	tclUnixFile.c \
	tclUnixNotfy.c \
	tclUnixPipe.c \
	tclUnixSock.c \
	tclUnixTest.c \
        tclUnixThrd.c \
	tclUnixTime.c \
	tclUnixInit.c

# tooldir = $(exec_prefix)/$(host_alias)

TCL_LIBRARY =	@datadir@/tcl$(VERSION)
TCL_PACKAGE_PATH = @TCL_PACKAGE_PATH@

INCLUDES = -I$(updir)/generic
AM_CFLAGS = -DTCL_LIBRARY='"$(TCL_LIBRARY)"' -DTCL_PACKAGE_PATH='"$(TCL_PACKAGE_PATH)"'

# $(OBJECTS): termcap.h pathnames.h

# install-data-local:
#	$(INSTALL_DATA) $(srcdir)/termcap $(sysconfdir)/termcap
#	$(INSTALL_DATA) $(srcdir)/termcap.h $(tooldir)/include/termcap.h

# install-exec-local: $(noinst_LIBRARIES)
# 	$(INSTALL) libtermcap.a $(tooldir)/lib/libtermcap.a
