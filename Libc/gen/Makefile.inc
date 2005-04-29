#	@(#)Makefile.inc	8.6 (Berkeley) 5/4/95
# $FreeBSD: src/lib/libc/gen/Makefile.inc,v 1.80 2001/08/17 22:09:15 dd Exp $

# machine-dependent gen sources
.if exists(${.CURDIR}/${MACHINE_ARCH}/gen/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/gen/Makefile.inc"
.endif

# machine-independent gen sources
.PATH: ${.CURDIR}/gen

CFLAGS += -I${.CURDIR}/pthreads

MISRCS += NSSystemDirectories.c OSSystemInfo.c arc4random.c asl.c \
	cache.c confstr.c crypt.c devname.c disklabel.c errlst.c \
	filesec.c fts.c \
	get_compat.c getloadavg.c getttyent.c getusershell.c getvfsbyname.c \
	isinf.c isnan.c \
	malloc.c nanosleep.c nftw.c nlist.c scalable_malloc.c setlogin.c \
	sigsetops.c simple_dprintf.c stack_logging.c strtofflags.c syslog.c \
	uname.c wordexp.c zone.c

CFLAGS-asl.c = -I${.CURDIR}/gen
LOCALHDRS += ${.CURDIR}/gen/asl_private.h

# also build 64-bit long double versions (ppc only)
LDBLSRCS += asl.c err.c syslog.c

.include "Makefile.fbsd_begin"
FBSDMISRCS = _rand48.c alarm.c assert.c \
	basename.c clock.c closedir.c ctermid.c \
	daemon.c dirname.c drand48.c erand48.c err.c errno_.c exec.c \
	fmtcheck.c fmtmsg.c fnmatch.c ftok.c \
	getbsize.c getcap.c getcwd.c gethostname.c getpeereid.c \
	getlogin.c getmntinfo.c getpagesize.c getprogname.c glob.c isatty.c \
	jrand48.c lcong48.c lockf.c lrand48.c mrand48.c nice.c nrand48.c \
	opendir.c pause.c popen.c pselect.c psignal.c raise.c readdir.c \
	readpassphrase.c rewinddir.c scandir.c seed48.c seekdir.c \
	sethostname.c setmode.c setprogname.c siginterrupt.c siglist.c \
	signal.c sleep.c srand48.c stringlist.c sysconf.c sysctl.c \
	sysctlbyname.c sysctlnametomib.c \
	telldir.c termios.c time.c times.c \
	ttyname.c ttyslot.c ualarm.c ulimit.c unvis.c usleep.c utime.c vis.c \
	wait.c wait3.c waitpid.c
.if (${MACHINE_ARCH} != ppc64)
FBSDMISRCS += timezone.c
.endif
FBSDHDRS = rand48.h telldir.h
.include "Makefile.fbsd_end"

.include "Makefile.nbsd_begin"
NBSDMISRCS = utmpx.c
.include "Makefile.nbsd_end"
CFLAGS-utmpx-nbsd.c += -DUTMP_COMPAT

# private header files
INSTHDRS += ${.CURDIR}/gen/get_compat.h
PRIV_INSTHDRS += ${.CURDIR}/gen/stack_logging.h

UNIX03SRCS += closedir.c crypt.c nanosleep.c nftw.c opendir.c rewinddir.c \
	seekdir.c telldir.c ttyname.c

.if ${LIB} == "c"

MAN3 += arc4random.3 asl.3 confstr.3 crypt.3 devname.3 directory.3 fts.3 ftw.3 \
	getdomainname.3 getfsent.3 getgrent.3 getgrouplist.3 getloadavg.3 \
	getnetgrent.3 getobjformat.3 getpwent.3 getttyent.3 \
	getusershell.3 getvfsbyname.3 initgroups.3 \
	malloc.3 nlist.3 pwcache.3 setjmp.3 sigsetops.3 \
	strtofflags.3 syslog.3 tcgetpgrp.3 tcsendbreak.3 \
	tcsetattr.3 tcsetpgrp.3 tzset.3 uname.3 valloc.3 wordexp.3 intro.3

MAN5 += compat.5

.include "Makefile.fbsd_begin"
FBSDMAN3= alarm.3 basename.3 clock.3 ctermid.3 daemon.3 dirname.3 err.3 exec.3 \
	fmtcheck.3 fmtmsg.3 fnmatch.3 fpclassify.3 ftok.3 \
	getbsize.3 getcap.3 getcwd.3 \
	gethostname.3 getmntinfo.3 getpagesize.3 getpass.3 \
	getpeereid.3 getprogname.3 \
	glob.3 isgreater.3 \
	lockf.3 nice.3 pause.3 popen.3 pselect.3 psignal.3 raise.3 rand48.3 \
	readpassphrase.3 scandir.3 setmode.3 \
	siginterrupt.3 signal.3 signbit.3 sleep.3 \
	stringlist.3 sysconf.3 sysctl.3 \
	time.3 times.3 timezone.3 ttyname.3 ualarm.3 \
	ulimit.3 unvis.3 usleep.3 utime.3 vis.3
.include "Makefile.fbsd_end"

.include "Makefile.nbsd_begin"
NBSDMAN3= endutxent.3
.include "Makefile.nbsd_end"

MLINKS+=arc4random.3 arc4random_addrandom.3 arc4random.3 arc4random_stir.3
MLINKS+=asl.3 asl_open.3 asl.3 asl_close.3 asl.3 asl_new.3 asl.3 asl_free.3 \
        asl.3 asl_set.3 asl.3 asl_set_query.3 asl.3 asl_get.3 asl.3 asl_unset.3 \
	asl.3 asl_log.3 asl.3 asl_vlog.3 asl.3 asl_send.3 asl.3 asl_key.3 \
	asl.3 asl_add_log_file.3 asl.3 asl_remove_log_file.3 \
	asl.3 asl_set_cutoff_level.3 asl.3 asl_search.3 asl.3 aslresponse_next.3 \
	asl.3 aslresponse_free.3
MLINKS+=ctermid.3 ctermid_r.3
MLINKS+=crypt.3 encrypt.3 crypt.3 setkey.3 crypt.3 des_setkey.3 \
	crypt.3 des_cipher.3
MLINKS+=directory.3 closedir.3 directory.3 dirfd.3 directory.3 opendir.3 \
	directory.3 readdir.3 directory.3 readdir_r.3 directory.3 rewinddir.3 \
	directory.3 seekdir.3 directory.3 telldir.3
MLINKS+=endutxent.3 getutxent.3 endutxent.3 getutxid.3 \
	endutxent.3 getutxline.3 endutxent.3 pututxline.3 \
	endutxent.3 setutxent.3
MLINKS+=err.3 err_set_exit.3 err.3 err_set_file.3 err.3 errc.3 err.3 errx.3 \
	err.3 verr.3 err.3 verrc.3 err.3 verrx.3 err.3 vwarn.3 err.3 vwarnc.3 \
	err.3 vwarnx.3 err.3 warnc.3 err.3 warn.3 err.3 warnx.3
MLINKS+=exec.3 execl.3 exec.3 execle.3 exec.3 execlp.3 \
	exec.3 execv.3 exec.3 execvp.3
MLINKS+=fpclassify.3 isfinite.3 fpclassify.3 isinf.3 fpclassify.3 isnan.3 \
	fpclassify.3 isnormal.3
MLINKS+=fts.3 fts_children.3 fts.3 fts_close.3 fts.3 fts_open.3 \
	fts.3 fts_read.3 fts.3 fts_set.3
MLINKS+=ftw.3 nftw.3
MLINKS+=getcap.3 cgetcap.3 getcap.3 cgetclose.3 getcap.3 cgetent.3 \
	getcap.3 cgetfirst.3 getcap.3 cgetmatch.3 getcap.3 cgetnext.3 \
	getcap.3 cgetnum.3 getcap.3 cgetset.3 getcap.3 cgetstr.3 \
	getcap.3 cgetustr.3
MLINKS+=getcwd.3 getwd.3
MLINKS+=getdomainname.3 setdomainname.3
MLINKS+=getfsent.3 endfsent.3 getfsent.3 getfsfile.3 getfsent.3 getfsspec.3 \
	getfsent.3 getfstype.3 getfsent.3 setfsent.3
MLINKS+=getgrent.3 endgrent.3 getgrent.3 getgrgid.3 getgrent.3 getgrnam.3 \
	getgrent.3 setgrent.3 getgrent.3 setgroupent.3
MLINKS+=gethostname.3 sethostname.3
MLINKS+=getnetgrent.3 endnetgrent.3 getnetgrent.3 innetgr.3 \
	getnetgrent.3 setnetgrent.3
MLINKS+=getprogname.3 setprogname.3
MLINKS+=getpwent.3 endpwent.3 getpwent.3 getpwnam.3 getpwent.3 getpwuid.3 \
	getpwent.3 setpassent.3 getpwent.3 setpwent.3 getpwent.3 setpwfile.3
MLINKS+=getttyent.3 endttyent.3 getttyent.3 getttynam.3 \
	getttyent.3 isdialuptty.3 getttyent.3 isnettty.3 \
	getttyent.3 setttyent.3
MLINKS+=getusershell.3 endusershell.3 getusershell.3 setusershell.3
MLINKS+=glob.3 globfree.3
MLINKS+=isgreater.3 isgreaterequal.3 isgreater.3 isless.3 \
	isgreater.3 islessequal.3 isgreater.3 islessgreater.3 \
	isgreater.3 isunordered.3
MLINKS+=malloc.3 calloc.3 malloc.3 valloc.3 malloc.3 realloc.3 malloc.3 free.3 \
	malloc.3 malloc_size.3 malloc.3 malloc_good_size.3 malloc.3 reallocf.3
MLINKS+=popen.3 pclose.3
MLINKS+=psignal.3 sys_siglist.3 psignal.3 sys_signame.3
MLINKS+=psignal.3 strsignal.3 psignal.3 sys_siglist.3 psignal.3 sys_signame.3
MLINKS+=pwcache.3 group_from_gid.3 pwcache.3 user_from_uid.3
MLINKS+=rand48.3 _rand48.3 rand48.3 drand48.3 rand48.3 erand48.3 \
	rand48.3 jrand48.3 rand48.3 lcong48.3 rand48.3 lrand48.3 \
	rand48.3 mrand48.3 rand48.3 nrand48.3 rand48.3 seed48.3 \
	rand48.3 srand48.3
MLINKS+=scandir.3 alphasort.3
MLINKS+=strtofflags.3 fflagstostr.3
MLINKS+=setjmp.3 _longjmp.3 setjmp.3 _setjmp.3 setjmp.3 longjmp.3 \
	setjmp.3 longjmperr.3 setjmp.3 longjmperror.3 \
	setjmp.3 siglongjmp.3 setjmp.3 sigsetjmp.3
MLINKS+=setmode.3 getmode.3
MLINKS+=sigsetops.3 sigaddset.3 sigsetops.3 sigdelset.3 \
	sigsetops.3 sigemptyset.3 sigsetops.3 sigfillset.3 \
	sigsetops.3 sigismember.3
MLINKS+=stringlist.3 sl_add.3 stringlist.3 sl_find.3 \
	stringlist.3 sl_free.3 stringlist.3 sl_init.3
MLINKS+=sysctl.3 sysctlbyname.3 sysctl.3 sysctlnametomib.3
MLINKS+=syslog.3 closelog.3 syslog.3 openlog.3 syslog.3 setlogmask.3 \
	syslog.3 vsyslog.3
MLINKS+=tcsendbreak.3 tcdrain.3 tcsendbreak.3 tcflow.3 tcsendbreak.3 tcflush.3
MLINKS+=tcsetattr.3 cfgetispeed.3 tcsetattr.3 cfgetospeed.3 \
	tcsetattr.3 cfmakeraw.3 tcsetattr.3 cfsetispeed.3 \
	tcsetattr.3 cfsetospeed.3 tcsetattr.3 cfsetspeed.3 \
	tcsetattr.3 tcgetattr.3
MLINKS+=ttyname.3 isatty.3 ttyname.3 ttyslot.3
MLINKS+=tzset.3 tzsetwall.3
MLINKS+=unvis.3 strunvis.3 unvis.3 strunvisx.3
MLINKS+=vis.3 strvis.3 vis.3 strvisx.3
.endif
