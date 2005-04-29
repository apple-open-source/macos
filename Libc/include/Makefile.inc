.include "${.CURDIR}/include/arpa/Makefile.inc"
.include "${.CURDIR}/include/libkern/Makefile.inc"
.include "${.CURDIR}/include/protocols/Makefile.inc"
.include "${.CURDIR}/include/machine/Makefile.inc"
.include "${.CURDIR}/include/malloc/Makefile.inc"
.include "${.CURDIR}/include/objc/Makefile.inc"
.include "${.CURDIR}/include/sys/Makefile.inc"
.include "${.CURDIR}/include/xlocale/Makefile.inc"

INC_INSTHDRS += NSSystemDirectories.h \
		_locale.h \
		_types.h \
		_wctype.h \
		_xlocale.h \
		aio.h \
		alloca.h \
		ar.h \
		asl.h \
		assert.h \
		asm.h \
		bitstring.h \
		c.h \
		cpio.h \
		crt_externs.h \
		ctype.h \
		db.h \
		dirent.h \
		disktab.h \
		err.h \
		errno.h \
		fcntl.h \
		float.h \
		fmtmsg.h \
		fnmatch.h \
		fsproperties.h \
		fstab.h \
		fts.h \
		ftw.h \
		getopt.h \
		glob.h \
		grp.h \
		inttypes.h \
		iso646.h \
		kvm.h \
		langinfo.h \
		libc.h \
		libgen.h \
		limits.h \
		locale.h \
		memory.h \
		monetary.h \
		monitor.h \
		mpool.h \
		ndbm.h \
		nl_types.h \
		nlist.h \
		paths.h \
		poll.h \
		pwd.h \
		ranlib.h \
		readpassphrase.h \
		regex.h \
		rune.h \
		runetype.h \
		search.h \
		semaphore.h \
		setjmp.h \
		sgtty.h \
		signal.h \
		stab.h \
		standards.h \
		stdarg.h \
		stdbool.h \
		stddef.h \
		stdio.h \
		stdlib.h \
		strhash.h \
		string.h \
		stringlist.h \
		strings.h \
		struct.h \
		sysexits.h \
		syslog.h \
		tar.h \
		termios.h \
		time.h \
		timeconv.h \
		ttyent.h \
		ucontext.h \
		ulimit.h \
		unistd.h \
		util.h \
		utime.h \
		utmp.h \
		varargs.h \
		vis.h \
		wchar.h \
		wctype.h \
		wordexp.h \
		xlocale.h

.PATH: ${.CURDIR}/include
MAN3 += sysexits.3

INC_INSTHDRS := ${INC_INSTHDRS:S/^/${.CURDIR}\/include\//}
INSTHDRS += ${INC_INSTHDRS}

STRIP_HDRS += ctype.h

.include "Makefile.nbsd_begin"
NBSDHDRS = utmpx.h
.include "Makefile.nbsd_end"
INSTHDRS_AUTOPATCH += utmpx.h
