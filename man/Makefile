#####################################################################
#
# Makefile for building and installing man_proj
#
#               ----- ----- NeXT Confidential ----- -----
#
# History (YY-MM-DD-wd):
#
# 92-12-22-tu: Revised by Kathy Walrath, NeXT Computer, Inc.
# 97-05-29-we: Indexing support removed by Matt Rollefson, Apple
#              Computer, Inc.
#
######################################################################

######################################################################
# Macros/variables
######################################################################

NAME = man
SUBDIRS = usr/man
#MANDIR = ${DSTROOT}/NextLibrary/Documentation/ManPages
MANDIR = ${DSTROOT}/usr/share/man
#INDEXFILE= ${MANDIR}/.index.store
#INDEXFLAGS=-fsvg -LEnglish

# DSTROOT must be provided by invoker
# 	E.g., for a test:  make installsrc SRCROOT=/tmp/mansrc
# SRCROOT must be provided by invoker
# 	E.g., for a test:  make install DSTROOT=/tmp/mandst
OBJROOT = .  # overridden by Release Control when project is submitted
SYMROOT = .  # overridden by Release Control when project is submitted

#####
# The following parameters have no meaning for doc_proj, as cc isn't
# used, and there are no architecture dependencies.
#
# RC_CFLAGS
# RC_ARCHS
# RC_m68k
# RC_m88k   # R.I.P.
# RC_m98k   # aka the PowerPC architecture
# RC_i386

#####
# The following parameters have no meaning for doc_proj. ???
#
# RC_KANJI    # ??? find out if this applies
# JAPANESE    # ??? find out if this applies
# SUBLIBROOTS


######################################################################
# Targets for building man_proj
######################################################################

all default ${NAME}:
	 for i in ${SUBDIRS}; \
	 do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} $@); \
	 done

#####
# This should perhaps remove some more dot files.  Index files should probably 
# be deleted.
#
clean::
	find . \( -name '*~' -o -name '#*' -o -name '.places' -o -name '.list' \) -exec rm {} \;

#####
# The "-CWD=`pwd`..." line is a standard invocation used to convert DSTROOT 
# into a full path name.  (I don't know why, but I don't dare to take it out.)  
# You end up not changing the current working directory.
#	-CWD=`pwd`; cd ${DSTROOT}; DSTROOT=`pwd`; cd $$CWD; \
#
install:: ${DSTROOT}
	-CWD=`pwd`; cd ${DSTROOT}; DSTROOT=`pwd`; cd $$CWD; \
	 for i in ${SUBDIRS}; \
	 do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} DSTROOT=$$DSTROOT $@); \
	 done
# Create the whatis database.
#	/usr/libexec/makewhatis ${MANDIR}

# Copy the special index files.  These change from release to release....
# 5/97 MR Special index files for Librarian support commented out
#	cp icon.tiff ${MANDIR}/.dir.tiff
#	cp .index.iname ${MANDIR}/.index.iname
#	cp .index.itype ${MANDIR}/.index.itype
#	cp .displayCommand ${MANDIR}/.displayCommand
#	cp .roffArgs ${MANDIR}/.roffArgs

# Create the index when installing.
# 5/97 MR Commented out
#	-/bin/rm -rf ${INDEXFILE}
#	(cd ${MANDIR} ; ixbuild  ${INDEXFLAGS}) || exit 1

# Change permissions.
# 5/97 MR Commented out files that are no longer copied
# 10/97 MR have to reenable the whatis file when it's generated again
#	chmod 644 ${INDEXFILE}
#	chmod 644 ${MANDIR}/whatis
#	chmod 444 ${MANDIR}/.dir.tiff
#	chmod 444 ${MANDIR}/.index.iname
#	chmod 444 ${MANDIR}/.index.itype
#	chmod 444 ${MANDIR}/.displayCommand
#	chmod 444 ${MANDIR}/.roffArgs
	chown -R root.wheel ${MANDIR}

# Check for and remove any core files.
	# find /${MANDIR} -name 'core' -exec rm -rf {} \;

# Create a link that points to /usr/share/man
# 10/97 MR
# For developer documentation directory *only*
# 4/00 MR
	mkdir -p ${DSTROOT}/Developer/Documentation
	ln -s /usr/share/man ${DSTROOT}/Developer/Documentation/ManPages

#####
# Copy this directory to SRCROOT.
#
installsrc:: ${SRCROOT}
	gnutar cf - . | (cd ${SRCROOT}; tar xvfp -)

#####
# Since man_proj has no headers, the "installhdrs" target does nothing.
#
installhdrs::	# Do nothing.

#####
# Create the SRCROOT and DSTROOT directories.
#
${SRCROOT} ${DSTROOT}:; mkdir -p -m 755 $@
