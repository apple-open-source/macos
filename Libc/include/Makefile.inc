.include "${.CURDIR}/include/arpa/Makefile.inc"
.include "${.CURDIR}/include/libkern/Makefile.inc"
.include "${.CURDIR}/include/protocols/Makefile.inc"
.include "${.CURDIR}/include/malloc/Makefile.inc"
.ifdef FEATURE_LEGACY_NXZONE_APIS
.include "${.CURDIR}/include/objc/Makefile.inc"
.endif
.include "${.CURDIR}/include/secure/Makefile.inc"
.include "${.CURDIR}/include/sys/Makefile.inc"
.include "${.CURDIR}/include/xlocale/Makefile.inc"

INC_INSTHDRS += NSSystemDirectories.h \
		_locale.h \
		_structs.h \
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
		cpio.h \
		crt_externs.h \
		ctype.h \
		db.h \
		dirent.h \
		disktab.h \
		err.h \
		errno.h \
		fcntl.h \
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
		runetype.h \
		search.h \
		semaphore.h \
		setjmp.h \
		sgtty.h \
		signal.h \
		spawn.h \
		stab.h \
		standards.h \
		stdbool.h \
		stddef.h \
		stdio.h \
		stdint.h \
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
		utmpx.h \
		vis.h \
		wchar.h \
		wctype.h \
		wordexp.h \
		xlocale.h

.ifdef FEATURE_LEGACY_RUNE_APIS
INC_INSTHDRS += rune.h
.endif

.ifdef FEATURE_LEGACY_UTMP_APIS
INC_INSTHDRS += utmp.h
.endif

.PATH: ${.CURDIR}/include
CWD := ${.CURDIR}/include

MAN3 += sysexits.3

INC_INSTHDRS := ${INC_INSTHDRS:S/^/${.CURDIR}\/include\//}
INSTHDRS += ${INC_INSTHDRS}

LOCALHDRS += ${.CURDIR}/include/spawn_private.h

.include "Makefile.nbsd_begin"
NBSDHDRS = utmpx.h
.include "Makefile.nbsd_end"

.include "Makefile.fbsd_begin"
FBSDHDRS= nl_types.h
.include "Makefile.fbsd_end"

