#
# makefile for architecture project.
#
EXPORT_DSTDIR=/usr/include/architecture
LOCAL_DSTDIR=/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/architecture
ANSI_DSTDIR=/usr/include
INSTALL_FLAGS= -p -m 444
ANSI_HDRS = _limits.h limits.h

# -- Compatible paths --
# EXPORT_DSTDIR=/NextLibrary/Frameworks/System.framework/Versions/B/Headers/architecture
# LOCAL_DSTDIR= /NextLibrary/Frameworks/System.framework/Versions/B/PrivateHeaders/architecture
# ANSI_DSTDIR=/NextLibrary/Frameworks/System.framework/Versions/B/Headers/bsd

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

OBJROOT=.

EXPORT_SOURCE= . i386 ppc arm
LOCAL_SOURCE=  . i386 ppc arm
TAGS_ARCH= ppc

ifneq "" "$(wildcard /usr/ucb/unifdef)"
	UNIFDEF = /usr/ucb/unifdef
else
	UNIFDEF = /usr/bin/unifdef
endif
	DECOMMENT = /usr/local/bin/decomment

all:

debug kern :

install:	all installhdrs

installhdrs: all DSTROOT $(DSTROOT)$(LOCAL_DSTDIR) \
	$(DSTROOT)$(EXPORT_DSTDIR)
#
#    First, LOCAL_DSTDIR
#
	-@for i in ${LOCAL_SOURCE};					\
	do								\
	    DSTDIR=$(DSTROOT)$(LOCAL_DSTDIR)/$$i;			\
	    (cd $$i;							\
                $(MKDIRS) $$DSTDIR;					\
                install $(INSTALL_FLAGS) *.[hs] $$DSTDIR;		\
                for j in $(ANSI_HDRS);					\
                do							\
                    if [ -f $$j ]; then					\
                        ANSIDSTDIR=$(DSTROOT)$(ANSI_DSTDIR)/$$i;	\
                        $(MKDIRS) $$ANSIDSTDIR;				\
                        mv $$DSTDIR/$$j $$ANSIDSTDIR/$$j;		\
                    fi;							\
                done);							\
	done
#
#    Now   EXPORT_DSTDIR
#
#  10/3/94 jdoenias - changed make sure original version of file
#  gets installed so that mod times don't change and invalidate the
#  precomps.  Only really affects m68k/zsreg.h
#
	-@if [ -n "$(OBJROOT)" ]; then					\
	   EXPORTSDIR=$(OBJROOT);					\
	else								\
	   EXPORTSDIR=`pwd`;						\
	fi;								\
	CWD=`pwd`;							\
	cd $$EXPORTSDIR;						\
	EXPORTSDIR_FULL=`pwd`;						\
	cd $$CWD;							\
	for i in ${EXPORT_SOURCE};					\
	do								\
	    EXPDIR=$$EXPORTSDIR_FULL/exports;				\
	    [ -d $$EXPDIR ] || $(MKDIRS) $$EXPDIR;			\
	    rm -f $$EXPDIR/*;						\
	    DSTDIR=$(DSTROOT)$(EXPORT_DSTDIR)/$$i;			\
	    [ -d $$DSTDIR ] || $(MKDIRS) $$DSTDIR;			\
	    (cd $$i;							\
	        hdrs=`echo *.[hs]`;					\
		for j in $$hdrs; 					\
		do							\
		    echo garbage > $$EXPDIR/$$j.strip;			\
		    $(UNIFDEF) -UARCH_PRIVATE $$j > $$EXPDIR/$$j ||	\
		    $(DECOMMENT) $$EXPDIR/$$j  r > $$EXPDIR/$$j.strip; 	\
		    if [ -s $$EXPDIR/$$j.strip ]; 			\
		    then (						\
			install $(INSTALL_FLAGS) $$j $$DSTDIR;		\
		    );							\
		    else 						\
			echo Header file $$i/$$j not exported;		\
		    fi;							\
		    rm -f $$EXPDIR/$$j*;				\
		done;							\
		for j in $(ANSI_HDRS);					\
		do							\
                    if [ -f $$DSTDIR/$$j ]; then			\
			/bin/rm -f $$DSTDIR/$$j;			\
                    fi;							\
		done;							\
	    );								\
	done;								\
	rm -r $$EXPDIR

clean:	ALWAYS
	rm -f *~ */*~ 
	rm -rf exports

installsrc: SRCROOT $(SRCROOT)
ifneq "" "$(wildcard /bin/pax)"
	pax -rw . ${SRCROOT}
else
	tar cf - . | (cd $(SRCROOT); tar xfBp -)
endif

tags:	ALWAYS
	@for arch in `echo $(TAGS_ARCH)`;				\
	do								\
		$(RM) -f TAGS.$$arch tags.$$arch;			\
		echo Making TAGS.$$arch;				\
		etags -et -f TAGS.$$arch *.h $$arch/*.h;		\
		echo Making tags.$$arch;				\
		ctags -w -o tags.$$arch *.h $$arch/*.h;			\
	done

$(SRCROOT) $(DSTROOT)$(EXPORT_DSTDIR) $(DSTROOT)$(LOCAL_DSTDIR):
	$(MKDIRS) $@

SRCROOT DSTROOT:	ALWAYS
	@if [ -n "${$@}" ]; \
	then \
		exit 0; \
	else \
		echo Must define $@; \
		exit 1; \
	fi

ALWAYS:
