## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = cygnus

updir=$(srcdir)/..
vpath % $(updir)/generic:$(updir)/unix

noinst_LIBRARIES = libtcl_cygwin.a

libtcl_cygwin_a_SOURCES = \
	panic.c \
	regexp.c \
	tclAsync.c \
	tclBasic.c \
	tclBinary.c \
	tclCkalloc.c \
	tclClock.c \
	tclCmdAH.c \
	tclCmdIL.c \
	tclCmdMZ.c \
	tclCompExpr.c \
	tclCompile.c \
	tclDate.c \
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
	tclIOSock.c \
	tclIOUtil.c \
	tclLink.c \
	tclListObj.c \
	tclLoad.c \
	tclLoadNone.c \
	tclMain.c \
	tclNamesp.c \
	tclNotify.c \
	tclObj.c \
        tclParse.c \
	tclPipe.c \
	tclPkg.c \
	tclPosixStr.c \
	tclPreserve.c \
	tclProc.c \
        tclTestProcBodyObj.c \
	tclResolve.c \
	tclStringObj.c \
	tclTest.c \
	tclTestObj.c \
	tclTimer.c \
	tclUtil.c \
	tclVar.c \
	tclAppInit.c \
	tclMtherr.c \
	tclUnixChan.c \
	tclUnixEvent.c \
	tclUnixFCmd.c \
	tclUnixFile.c \
	tclUnixNotfy.c \
	tclUnixPipe.c \
	tclUnixSock.c \
	tclUnixTest.c \
	tclUnixTime.c \
	tclUnixInit.c

# tooldir = $(exec_prefix)/$(host_alias)

TCL_LIBRARY =	@datadir@/tcl$(VERSION)
TCL_PACKAGE_PATH = @TCL_PACKAGE_PATH@

INCLUDES = -I$(updir)/generic
AM_CFLAGS = -D__TCL_UNIX_VARIANT -DTCL_LIBRARY='"$(TCL_LIBRARY)"' -DTCL_PACKAGE_PATH='"$(TCL_PACKAGE_PATH)"'

# $(OBJECTS): termcap.h pathnames.h

# install-data-local:
#	$(INSTALL_DATA) $(srcdir)/termcap $(sysconfdir)/termcap
#	$(INSTALL_DATA) $(srcdir)/termcap.h $(tooldir)/include/termcap.h

# install-exec-local: $(noinst_LIBRARIES)
# 	$(INSTALL) libtermcap.a $(tooldir)/lib/libtermcap.a
