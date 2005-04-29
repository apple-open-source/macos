/*
 * command library -- commands small enough to be builtins
 */

:PACKAGE: ast

LICENSE = since=1992,author=gsf+dgk

CCFLAGS = $(CC.OPTIMIZE) $(CC.DLL)

CP = $(STDCP|"cp")

HOSTTYPE == "$(CC.HOSTTYPE)"

cmd 1.2 :LIBRARY: RELEASE cmdinit.c cmdrecurse.c \
	cmd.h cmdlib.h wc.h \
	basename.c cat.c chgrp.c chmod.c chown.c cmp.c comm.c \
	cp.c cut.c dirname.c date.c expr.c fmt.c fold.c getconf.c head.c \
	id.c join.c ln.c logname.c mkdir.c mkfifo.c mv.c paste.c \
	pathchk.c rev.c rm.c rmdir.c stty.c tail.c tee.c tty.c \
	uname.c uniq.c wc.c revlib.c wclib.c \
	-lfsg

$(INCLUDEDIR) :INSTALLPROTO: cmd.h

"win32*" :NOOPTIMIZE: id.c

ignore dlldefs.h /* prevents bootstrap double build */
