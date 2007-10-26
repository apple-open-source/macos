/*
 * command library -- commands small enough to be builtins
 */

:PACKAGE: ast

LICENSE = since=1992,author=gsf+dgk

CCFLAGS = $(CC.OPTIMIZE) $(CC.DLL)

CP = $(STDCP|"cp")
CHMOD = $(STDCHMOD|"chmod")

HOSTTYPE == "$(CC.HOSTTYPE)"

cmd 1.2 :LIBRARY: RELEASE cmdinit.c \
	cmd.h rev.h wc.h \
	basename.c cat.c chgrp.c chmod.c chown.c cksum.c cmp.c \
	comm.c cp.c cut.c dirname.c date.c expr.c fds.c fmt.c \
	fold.c getconf.c head.c id.c join.c ln.c logname.c md5sum.c \
	mkdir.c mkfifo.c mv.c paste.c pathchk.c rev.c rm.c rmdir.c \
	stty.c sum.c sync.c tail.c tee.c tty.c uname.c uniq.c wc.c \
	revlib.c wclib.c sumlib.o \
	-lfsg

sumlib.o : +lsum
	$(AR) x $(*:O=1) $(<)

$(INCLUDEDIR) :INSTALLPROTO: cmd.h cmdext.h cmdlist.h shcmd.h

"win32*" :NOOPTIMIZE: id.c

src = $(*$(*$(*cmd)):N=*.c)

cmdext.h : (src) $$(--mam:+$$(src))
	{
	cat <<!
	$("#")pragma prototyped
	$("/")*
	$(" ")* -lcmd extern function prototypes
	$(" ")*/
	
	!
	$(SED) \
		-e '/^b_[a-z_][a-z_0-9]*(/!d' \
		-e 's/^b_//' \
		-e 's/(.*//' \
		-e 's/.*/extern int	b_&(int, char**, void*);/' \
		$($(~:O=1):T=F) |
	$(SORT) -u
	} > 1.$(tmp).h
	if	$(CMP) $(CMPFLAGS) 1.$(tmp).h $(<)
	then	$(RM) $(RMFLAGS) 1.$(tmp).h
	else	$(MV) 1.$(tmp).h $(<)
	fi

cmdlist.h : (src)
	{
	cat <<!
	$("#")pragma prototyped
	$("/")*
	$(" ")* -lcmd function list -- define your own CMDLIST()
	$(" ")*/
	
	!
	$(SED) \
		-e '/^b_[a-z_][a-z_0-9]*(/!d' \
		-e 's/^b_//' \
		-e 's/(.*//' \
		-e 's/.*/CMDLIST(&)/' \
		$($(~):T=F) |
	$(SORT) -u
	} > 1.$(tmp).h
	if	$(CMP) $(CMPFLAGS) 1.$(tmp).h $(<)
	then	$(RM) $(RMFLAGS) 1.$(tmp).h
	else	$(MV) 1.$(tmp).h $(<)
	fi

if CC.HOSTTYPE == "sun4"
	pathsetlink == symlink	/* ld.so workaround */
end

ignore dlldefs.h /* prevents bootstrap double build */
