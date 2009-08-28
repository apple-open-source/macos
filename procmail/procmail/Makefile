#$Id: Makefile,v 1.76 2001/07/12 01:27:19 guenther Exp $

# BASENAME should point to where the whole lot will be installed
# change BASENAME to your home directory if need be
BASENAME	= /usr
# For display in the man pages
VISIBLE_BASENAME= $(BASENAME)

# You can predefine ARCHITECTURE to a bin directory suffix
ARCHITECTURE	=
#ARCHITECTURE	=.sun4

BINDIR_TAIL	= bin$(ARCHITECTURE)
MANDIR		= $(BASENAME)/man
BINDIR		= $(BASENAME)/$(BINDIR_TAIL)
VISIBLE_BINDIR	= $(VISIBLE_BASENAME)/$(BINDIR_TAIL)
# MAN1SUFFIX for regular utility manuals
MAN1SUFFIX	=1
# MAN5SUFFIX for file-format descriptions
MAN5SUFFIX	=5
MAN1DIR		= $(MANDIR)/man$(MAN1SUFFIX)
MAN5DIR		= $(MANDIR)/man$(MAN5SUFFIX)

# Uncomment to install compressed man pages (possibly add extra suffix
# to the definitions of MAN?DIR and/or MAN?SUFFIX by hand)
#MANCOMPRESS = compress

############################*#
# Things that can be made are:
#
# help (or targets)	Displays this list you are looking at
# init (or makefiles)	Performs some preliminary sanity checks on your system
#			and generates Makefiles accordingly
# bins			Preinstalls only the binaries to ./new
# mans			Preinstalls only the man pages to ./new
# all			Does both
# install.bin		Installs the binaries from ./new to $(BINDIR)
# install.man		Installs the man pages from ./new to $(MAN[15]DIR)
# install		Does both
# recommend		Show some recommended suid/sgid modes
# install-suid		Impose the modes shown by 'make recommend'
# clean			Attempts to restore the package to pre-make state
# realclean		Attempts to restore the package to pre-make-init state
# deinstall		Removes any previously installed binaries and man
#			pages from your system by careful surgery
# autoconf.h		Will list your system's anomalies
# procmail		Preinstalls just all procmail related stuff to ./new
# formail		Preinstalls just all formail related stuff to ./new
# lockfile		Preinstalls just all lockfile related stuff to ./new
# setid			Creates the setid binary needed by the SmartList
#			installation
######################*#

# Makefile.0 - mark, don't (re)move this, a sed script needs it

LOCKINGTEST=__defaults__

#LOCKINGTEST=/tmp .	# Uncomment and add any directories you see fit.
#			If LOCKINGTEST is defined, autoconf will NOT
#			prompt you to enter additional directories.
#			See INSTALL for more information about the
#			significance of the locking tests.

########################################################################
# Only edit below this line if you *think* you know what you are doing #
########################################################################

#LOCKINGTEST=100	# Uncomment (and change) if you think you know
#			it better than the autoconf lockingtests.
#			This will cause the lockingtests to be hotwired.
#			100	to enable fcntl()
#			010	to enable lockf()
#			001	to enable flock()
#			Or them together to get the desired combination.

# Optional system libraries we search for
SEARCHLIBS = -lm -ldir -lx -lsocket -lnet -linet -lnsl_s -lnsl_i -lnsl -lsun \
 -lgen -lsockdns -ldl
#			-lresolv	# not really needed, is it?

# Informal list of directories where we look for the libraries in SEARCHLIBS
LIBPATHS=/lib /usr/lib /usr/local/lib

GCC_WARNINGS = -O2 -pedantic -Wreturn-type -Wunused -Wformat -Wtraditional \
 -Wpointer-arith -Wconversion -Waggregate-return \
 #-Wimplicit -Wshadow -Wid-clash-6 #-Wuninitialized

# The place to put your favourite extra cc flag
CFLAGS0 = -O #$(GCC_WARNINGS)
LDFLAGS0= -s
# Read my libs :-)
LIBS=

CFLAGS1 = $(CFLAGS0) #-posix -Xp
LDFLAGS1= $(LDFLAGS0) $(LIBS) #-lcposix

####CC	= cc # gcc
# object file extension
O	= o
RM	= /bin/rm -f
MV	= mv -f
LN	= ln
BSHELL	= /bin/sh
INSTALL = cp
DEVNULL = /dev/null
STRIP	= strip
MKDIRS	= new/mkinstalldirs

SUBDIRS = src man
BINSS	= procmail lockfile formail mailstat
MANS1S	= procmail formail lockfile
MANS5S	= procmailrc procmailsc procmailex

# Possible locations for the sendmail.cf file
SENDMAILCFS = /etc/mail/sendmail.cf /etc/sendmail.cf /usr/lib/sendmail.cf

# Makefile - mark, don't (re)move this, a sed script needs it

all: init
	$(MAKE) make $@

make:
	@$(BSHELL) -c "exit 0"

.PRECIOUS: Makefile

init:
	$(BSHELL) ./initmake $(BSHELL) "$(SHELL)" "$(RM)" "$(MV)" "$(LN)" \
 "$(SEARCHLIBS)" \
 "$(LIBPATHS)" \
 $(DEVNULL) "$(MAKE)" $(O) \
 "$(CC)" "$(CFLAGS1)" "$(LDFLAGS1)" "$(BINSS)" \
 "$(MANS1S)" \
 "$(MANS5S)" "$(SUBDIRS)" \
 "$(VISIBLE_BINDIR)" \
 "$(STRIP)"

makefiles makefile Makefiles Makefile: init
	@$(BSHELL) -c "exit 0"

help target targets \
bins mans install.bin install.man install recommend install-suid clean setid \
realclean veryclean clobber deinstall autoconf.h $(BINSS) multigram: init
	$(MAKE) make $@
