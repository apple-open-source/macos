######################################################################
#
# Makefile called by man-proj/man-XX/Makefile to build man_proj.
#
#               ----- ----- NeXT Confidential ----- -----
#
# History (YY-MM-DD-wd):
#
# 92-12-22-tu: Revised by Kathy Walrath, NeXT Computer, Inc.
#
######################################################################

######################################################################
# Macros/variables
######################################################################

#####
# The directory we are building.
#
MANDIR = ${DSTROOT}/usr/share/man

#####
# Directories under MANDIR (and in the man_proj directory this makefile is in)
#
SUBDIRS = man3 man5 man8

#####
# Ownership and group of the files and directories installed under MANDIR
#
OWNER = root
GROUP = wheel

#####
# Empty subdirectories installed under MANDIR.  EMPTYDIRS used to include cat9, 
# catl, and catn, too, but I deleted them because we don't install the 
# corresponding man directories.
#
# 3/15/99 We now create no empty directories, because the man command no
# longer populates them and the performance in rendering the nroff is just
# no longer an issue with modern processors.

EMPTYDIRS = 
EMPTYMODE = 777

#####
# Flags for install(1)
#
#INSTALLFLAGS = -d
INSTALLFLAGS = -c


######################################################################
# Targets for building man_proj
######################################################################

all clean depend:
	@echo No $@ target, make $@ ignored

install:	ckdstroot ${MANDIR}
	-CWD=`pwd`; cd ${DSTROOT}; DSTROOT=`pwd`; cd $$CWD; \
	for i in ` echo ${EMPTYDIRS}` ; \
	  do \
		echo -n " $$i" ; \
		mkdir -p -m ${EMPTYMODE} ${MANDIR}/$$i ;\
	  done
	echo "."
	 for i in ${SUBDIRS}; \
	 do \
		 echo ==================== $$i ====================; \
		 (cd $$i; \
		 $(MAKE) ${MFLAGS} "INSTALLFLAGS=${INSTALLFLAGS}" \
		 	DSTROOT=$$DSTROOT install; \
		  chmod 755 ${MANDIR}/$$i; \
		  cd ${MANDIR}/$$i; \
		  chmod 444 * ); \
	 done

ckdstroot:
	@if [ -z "${DSTROOT}" ]; \
	then \
	  echo Must define DSTROOT; \
	  exit 1; \
	fi

${MANDIR}:
	mkdir -p -m 755 $@
