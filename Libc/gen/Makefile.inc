#	@(#)Makefile.inc	8.6 (Berkeley) 5/4/95
# $FreeBSD: src/lib/libc/gen/Makefile.inc,v 1.80 2001/08/17 22:09:15 dd Exp $

# machine-dependent gen sources
.sinclude "${.CURDIR}/${MACHINE_ARCH}/gen/Makefile.inc"

# machine-independent gen sources
.PATH: ${.CURDIR}/gen
CWD := ${.CURDIR}/gen

CFLAGS += -I${.CURDIR}/gen

GENMIGDEFS += asl_ipc.defs
GENMIGHDRS += ${GENMIGDEFS:.defs=.h}
GENMIGSRCS += ${GENMIGDEFS:.defs=User.c}

MISRCS += ${GENMIGSRCS} NSSystemDirectories.c \
	asl.c asl_core.c asl_file.c asl_legacy1.c asl_store.c asl_util.c \
	backtrace.c \
	cache.c confstr.c crypt.c devname.c disklabel.c errlst.c \
	filesec.c fts.c \
	get_compat.c getloadavg.c getttyent.c getusershell.c getvfsbyname.c \
	isinf.c isnan.c \
	malloc.c nanosleep.c nftw.c magazine_malloc.c setlogin.c \
	sigsetops.c _simple.c stack_logging.c stack_logging_disk.c strtofflags.c syslog.c \
	thread_stack_pcs.c uname.c utmpx-darwin.c wordexp.c

.ifdef FEATURE_LEGACY_NXZONE_APIS
MISRCS += zone.c
.endif
.if !defined(LP64)
MISRCS += nlist.c
.endif
	
# DTrace USDT probes placed in magazine_malloc.c and malloc.c
${SYMROOTINC}/magmallocProvider.h: ${.CURDIR}/gen/magmallocProvider.d
	${DTRACE} -o ${.TARGET} -C -h -s ${.ALLSRC}
magazine_malloc.${OBJSUFFIX}: ${SYMROOTINC}/magmallocProvider.h
malloc.${OBJSUFFIX}: ${SYMROOTINC}/magmallocProvider.h

# Force these files to build after the mig stuff
asl.${OBJSUFFIX}: asl_ipcUser.c
syslog.${OBJSUFFIX}: asl_ipcUser.c
utmpx-darwin.${OBJSUFFIX}: asl_ipcUser.c

CLEANFILES += ${GENMIGHDRS} ${GENMIGSRCS} ${GENMIGDEFS:.defs=Server.c}

LOCALHDRS += ${.CURDIR}/gen/asl_core.h ${.CURDIR}/gen/asl_file.h ${.CURDIR}/gen/asl_legacy1.h \
	${.CURDIR}/gen/asl_private.h ${.CURDIR}/gen/asl_store.h \
	${.CURDIR}/gen/_simple.h ${.CURDIR}/gen/stack_logging.h

CFLAGS-asl_file.c += -D_DARWIN_UNLIMITED_STREAMS
CFLAGS-asl_legacy1.c += -D_DARWIN_UNLIMITED_STREAMS
CFLAGS-asl_store.c += -D_DARWIN_UNLIMITED_STREAMS
CFLAGS-confstr.c += -I${.CURDIR}/darwin
CFLAGS-fmtmsg-fbsd.c += -D_DARWIN_UNLIMITED_STREAMS
CFLAGS-getcap-fbsd.c += -D_DARWIN_UNLIMITED_STREAMS
CFLAGS-getttyent.c += -D_DARWIN_UNLIMITED_STREAMS
CFLAGS-getusershell.c += -D_DARWIN_UNLIMITED_STREAMS
CFLAGS-glob-fbsd.c += -UDEBUG

# 4840357: workaround for compiler failure building libc_debug.a
.if make(lib${LIB}_debug.a)
CFLAGS-magazine_malloc.c += -funit-at-a-time
.endif

# Malloc uses count leading zeroes, not available in thumb
#.if (${MACHINE_ARCH} == arm)
#CFLAGS-scalable_malloc.c += -mno-thumb
#.endif

CFLAGS-stack_logging_disk.c += -D_DARWIN_UNLIMITED_STREAMS
CFLAGS-utmpx-nbsd.c += -D_DARWIN_UNLIMITED_STREAMS
CFLAGS-wordexp.c += -D_DARWIN_UNLIMITED_STREAMS

# also build 64-bit long double versions (ppc only)
LDBLSRCS += asl.c err.c syslog.c

.include "Makefile.fbsd_begin"
FBSDMISRCS = _rand48.c alarm.c arc4random.c assert.c \
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

.ifdef FEATURE_LEGACY_64_APIS
# special case: getmntinfo64-fbsd.c is derived from getmntinfo.c with getmntinfo64.c.patch
.ifmake autopatch
.for _cwd in ${CWD} # This .for statement forces evaluation of ${CWD}
AUTOPATCHSRCS+= ${_cwd}/getmntinfo64-fbsd.c
${_cwd}/getmntinfo64-fbsd.c: ${_cwd}/FreeBSD/getmntinfo.c
	${CP} ${.ALLSRC} ${.TARGET}
	${PATCH} ${.TARGET} ${.ALLSRC:S/getmntinfo/getmntinfo64/}.patch
.endfor # _cwd
.else # !autopatch
MISRCS+= getmntinfo64.c
.endif # autopatch
.endif # FEATURE_LEGACY_64_APIS

# special case: scandir_b-fbsd.c is derived from scandir.c with scandir_b.c.patch
.ifmake autopatch
.for _cwd in ${CWD} # This .for statement forces evaluation of ${CWD}
AUTOPATCHSRCS+= ${_cwd}/scandir_b-fbsd.c
${_cwd}/scandir_b-fbsd.c: ${_cwd}/FreeBSD/scandir.c
	${CP} ${.ALLSRC} ${.TARGET}
	${PATCH} ${.TARGET} ${.ALLSRC:S/scandir/scandir_b/}.patch
.endfor # _cwd
.else # !autopatch
.ifdef FEATURE_BLOCKS
MISRCS+= scandir_b.c
.endif # FEATURE_BLOCKS
.endif # autopatch

PRE1050SRCS+= daemon.c
.if defined(LP64)
PRE1050SRCS+= pselect.c
.endif
.ifndef FEATURE_ONLY_UNIX_CONFORMANCE
# only patch timezone.c; don't build in base variant, but in other variants
FBSDPATCHSRCS += timezone.c
.endif

FBSDHDRS = rand48.h telldir.h
.include "Makefile.fbsd_end"

.include "Makefile.nbsd_begin"
NBSDMISRCS = utmpx.c
.include "Makefile.nbsd_end"

# private header files
INSTHDRS += ${.CURDIR}/gen/get_compat.h \
	${.CURDIR}/gen/execinfo.h
PRIV_INSTHDRS += ${.CURDIR}/gen/stack_logging.h

LEGACYSRCS += clock.c closedir.c confstr.c crypt.c fnmatch.c \
	lockf.c nanosleep.c nftw.c nice.c opendir.c \
	pause.c popen.c pselect.c rewinddir.c \
	seekdir.c setmode.c sleep.c \
	telldir.c termios.c timezone.c ttyname.c \
	usleep.c wait.c waitpid.c
INODE32SRCS += fts.c getmntinfo.c glob.c nftw.c opendir.c \
	readdir.c rewinddir.c scandir.c seekdir.c telldir.c
.ifdef FEATURE_BLOCKS
INODE32SRCS += scandir_b.c
.endif # FEATURE_BLOCKS
CANCELABLESRCS += lockf.c nanosleep.c pause.c pselect.c sleep.c termios.c \
	usleep.c wait.c waitpid.c
CANCELABLE-DARWINEXTSNSRCS += pselect.c
DARWINEXTSNSRCS += popen.c pselect.c
DYLDSRCS += _simple.c

# include __dirent.h to rename DIR structure elements
.for _src in closedir-fbsd.c opendir-fbsd.c readdir-fbsd.c rewinddir-fbsd.c \
	scandir-fbsd.c seekdir-fbsd.c telldir-fbsd.c
CFLAGS-${_src} += -I${.CURDIR}/gen -include __dirent.h
.endfor
.ifdef FEATURE_BLOCKS
CFLAGS-scandir_b-fbsd.c += -I${.CURDIR}/gen -include __dirent.h
.endif # FEATURE_BLOCKS

# 4105292/4329702 different optimizations improves performance of
# drand48 and erand48
.if !make(lib${LIB}_debug.a)
.if (${MACHINE_ARCH} == i386)
OPTIMIZE-drand48-fbsd.c += -O1
OPTIMIZE-erand48-fbsd.c += -O1
.endif
.endif

# set the LIBC_ALIAS_* macros so we can decorate the symbol independent
# of other macro settings
CFLAGS-clock-fbsd.c += -DLIBC_ALIAS_CLOCK
CFLAGS-closedir-fbsd.c += -DLIBC_ALIAS_CLOSEDIR
CFLAGS-confstr.c += -DLIBC_ALIAS_CONFSTR
CFLAGS-crypt.c += -DLIBC_ALIAS_ENCRYPT -DLIBC_ALIAS_SETKEY
CFLAGS-fnmatch-fbsd.c += -DLIBC_ALIAS_FNMATCH
CFLAGS-fts.c += -DLIBC_ALIAS_FTS_CHILDREN -DLIBC_ALIAS_FTS_CLOSE -DLIBC_ALIAS_FTS_OPEN -DLIBC_ALIAS_FTS_OPEN_B -DLIBC_ALIAS_FTS_READ -DLIBC_ALIAS_FTS_SET
CFLAGS-glob-fbsd.c += -DLIBC_ALIAS_GLOB
CFLAGS-glob_b-fbsd.c += -DLIBC_ALIAS_GLOB_B
CFLAGS-lockf-fbsd.c += -DLIBC_ALIAS_LOCKF
CFLAGS-nanosleep.c += -DLIBC_ALIAS_NANOSLEEP
CFLAGS-nftw.c += -DLIBC_ALIAS_FTW -DLIBC_ALIAS_NFTW
CFLAGS-nice-fbsd.c += -DLIBC_ALIAS_NICE
CFLAGS-opendir-fbsd.c += -DLIBC_ALIAS___OPENDIR2 -DLIBC_ALIAS_OPENDIR
CFLAGS-rewinddir-fbsd.c += -DLIBC_ALIAS_REWINDDIR
CFLAGS-pause-fbsd.c += -DLIBC_ALIAS_PAUSE
CFLAGS-popen-fbsd.c += -DLIBC_ALIAS_POPEN
CFLAGS-pselect-fbsd.c += -DLIBC_ALIAS_PSELECT
CFLAGS-seekdir-fbsd.c += -DLIBC_ALIAS_SEEKDIR
CFLAGS-setmode-fbsd.c += -DLIBC_ALIAS_SETMODE
CFLAGS-sleep-fbsd.c += -DLIBC_ALIAS_SLEEP
CFLAGS-telldir-fbsd.c += -DLIBC_ALIAS__SEEKDIR -DLIBC_ALIAS_TELLDIR
CFLAGS-termios-fbsd.c += -DLIBC_ALIAS_TCDRAIN
CFLAGS-ttyname-fbsd.c += -DLIBC_ALIAS_TTYNAME_R
CFLAGS-usleep-fbsd.c += -DLIBC_ALIAS_USLEEP
CFLAGS-wait-fbsd.c += -DLIBC_ALIAS_WAIT
CFLAGS-waitpid-fbsd.c += -DLIBC_ALIAS_WAITPID

.if ${LIB} == "c"

MAN3 += asl.3 \
	backtrace.3 \
	confstr.3 crypt.3 devname.3 directory.3 fts.3 ftw.3 \
	getdomainname.3 getloadavg.3 \
	getttyent.3 getusershell.3 getvfsbyname.3 \
	malloc.3 malloc_size.3 malloc_zone_malloc.3 nlist.3 posix_memalign.3 \
	pwcache.3 setjmp.3 sigsetops.3 strtofflags.3 syslog.3 \
	tcgetpgrp.3 tcsendbreak.3 tcsetattr.3 tcsetpgrp.3 tzset.3 \
	uname.3 wordexp.3 intro.3

MAN5 += compat.5

.include "Makefile.fbsd_begin"
FBSDMAN3= alarm.3 arc4random.3 basename.3 clock.3 ctermid.3 \
	daemon.3 dirname.3 err.3 exec.3 \
	fmtcheck.3 fmtmsg.3 fnmatch.3 ftok.3 \
	getbsize.3 getcap.3 getcontext.3 getcwd.3 \
	gethostname.3 getmntinfo.3 getpagesize.3 getpass.3 \
	getpeereid.3 getprogname.3 glob.3 \
	lockf.3 makecontext.3 nice.3 \
	pause.3 popen.3 pselect.3 psignal.3 \
	raise.3 rand48.3 readpassphrase.3 \
	scandir.3 setmode.3 \
	siginterrupt.3 signal.3 signbit.3 sleep.3 \
	stringlist.3 sysconf.3 sysctl.3 \
	time.3 times.3 timezone.3 ttyname.3 \
	ualarm.3 ucontext.3 ulimit.3 unvis.3 usleep.3 utime.3 \
	vis.3
.include "Makefile.fbsd_end"

.include "Makefile.nbsd_begin"
NBSDMAN3= endutxent.3 getlastlogx.3
NBSDMAN5= utmpx.5
.include "Makefile.nbsd_end"

MLINKS+= arc4random.3 arc4random_addrandom.3	\
         arc4random.3 arc4random_stir.3

MLINKS+= asl.3 asl_add_log_file.3		\
         asl.3 asl_close.3			\
         asl.3 asl_free.3			\
         asl.3 asl_get.3			\
         asl.3 asl_key.3			\
         asl.3 asl_log.3			\
         asl.3 asl_new.3			\
         asl.3 asl_open.3			\
         asl.3 asl_remove_log_file.3		\
         asl.3 asl_search.3			\
         asl.3 asl_send.3			\
         asl.3 asl_set.3			\
         asl.3 asl_set_filter.3			\
         asl.3 asl_set_query.3			\
         asl.3 asl_unset.3			\
         asl.3 asl_vlog.3			\
         asl.3 aslresponse_free.3		\
         asl.3 aslresponse_next.3

MLINKS+= backtrace.3 backtrace_symbols.3	\
	 backtrace.3 backtrace_symbols_fd.3

MLINKS+= crypt.3 encrypt.3			\
         crypt.3 setkey.3

MLINKS+= ctermid.3 ctermid_r.3

MLINKS+= devname.3 devname_r.3

MLINKS+= directory.3 closedir.3			\
         directory.3 dirfd.3			\
         directory.3 opendir.3			\
         directory.3 readdir.3			\
         directory.3 readdir_r.3		\
         directory.3 rewinddir.3		\
         directory.3 seekdir.3			\
         directory.3 telldir.3

MLINKS+= endutxent.3 getutxent.3		\
         endutxent.3 getutxid.3			\
         endutxent.3 getutxline.3		\
         endutxent.3 pututxline.3		\
         endutxent.3 setutxent.3

MLINKS+= err.3 err_set_exit.3			\
         err.3 err_set_file.3			\
         err.3 errc.3				\
         err.3 errx.3				\
         err.3 verr.3				\
         err.3 verrc.3				\
         err.3 verrx.3				\
         err.3 vwarn.3				\
         err.3 vwarnc.3				\
         err.3 vwarnx.3				\
         err.3 warn.3				\
         err.3 warnc.3				\
         err.3 warnx.3
.ifdef FEATURE_BLOCKS
MLINKS+= err.3 err_set_exit_b.3
.endif # FEATURE_BLOCKS

MLINKS+= exec.3 execl.3				\
         exec.3 execle.3			\
         exec.3 execlp.3			\
         exec.3 execv.3				\
         exec.3 execvp.3

MLINKS+= fts.3 fts_children.3			\
         fts.3 fts_close.3			\
         fts.3 fts_open.3			\
         fts.3 fts_read.3			\
         fts.3 fts_set.3
.ifdef FEATURE_BLOCKS
MLINKS+= fts.3 fts_open_b.3
.endif # FEATURE_BLOCKS

MLINKS+= ftw.3 nftw.3

MLINKS+= getcap.3 cgetcap.3			\
         getcap.3 cgetclose.3			\
         getcap.3 cgetent.3			\
         getcap.3 cgetfirst.3			\
         getcap.3 cgetmatch.3			\
         getcap.3 cgetnext.3			\
         getcap.3 cgetnum.3			\
         getcap.3 cgetset.3			\
         getcap.3 cgetstr.3			\
         getcap.3 cgetustr.3

MLINKS+= getcontext.3 setcontext.3

MLINKS+= getcwd.3 getwd.3

MLINKS+= getdomainname.3 setdomainname.3

MLINKS+= getlastlogx.3 getlastlogxbyname.3	\
         getlastlogx.3 getutmp.3		\
         getlastlogx.3 getutmpx.3		\
         getlastlogx.3 utmpxname.3

MLINKS+= gethostname.3 sethostname.3

MLINKS+= getprogname.3 setprogname.3

MLINKS+= getttyent.3 endttyent.3		\
         getttyent.3 getttynam.3		\
         getttyent.3 setttyent.3

MLINKS+= getusershell.3 endusershell.3		\
         getusershell.3 setusershell.3

MLINKS+= glob.3 globfree.3

MLINKS+= malloc.3 calloc.3			\
         malloc.3 free.3			\
         malloc.3 realloc.3			\
         malloc.3 reallocf.3			\
         malloc.3 valloc.3

MLINKS+= makecontext.3 swapcontext.3

MLINKS+= malloc_size.3 malloc_good_size.3

MLINKS+= malloc_zone_malloc.3 malloc_create_zone.3 \
         malloc_zone_malloc.3 malloc_destroy_zone.3 \
         malloc_zone_malloc.3 malloc_default_zone.3 \
         malloc_zone_malloc.3 malloc_zone_from_ptr.3 \
         malloc_zone_malloc.3 malloc_zone_calloc.3 \
         malloc_zone_malloc.3 malloc_zone_valloc.3 \
         malloc_zone_malloc.3 malloc_zone_realloc.3 \
         malloc_zone_malloc.3 malloc_zone_memalign.3 \
         malloc_zone_malloc.3 malloc_zone_free.3

MLINKS+= popen.3 pclose.3

MLINKS+= psignal.3 sys_siglist.3		\
         psignal.3 sys_signame.3

MLINKS+= psignal.3 strsignal.3			\
         psignal.3 sys_siglist.3		\
         psignal.3 sys_signame.3

MLINKS+= pwcache.3 group_from_gid.3		\
         pwcache.3 user_from_uid.3

MLINKS+= rand48.3 _rand48.3			\
         rand48.3 drand48.3			\
         rand48.3 erand48.3			\
         rand48.3 jrand48.3			\
         rand48.3 lcong48.3			\
         rand48.3 lrand48.3			\
         rand48.3 mrand48.3			\
         rand48.3 nrand48.3			\
         rand48.3 seed48.3			\
         rand48.3 srand48.3

MLINKS+= scandir.3 alphasort.3
.ifdef FEATURE_BLOCKS
MLINKS+= scandir.3 scandir_b.3
.endif # FEATURE_BLOCKS

MLINKS+= strtofflags.3 fflagstostr.3

MLINKS+= setjmp.3 _longjmp.3			\
         setjmp.3 _setjmp.3			\
         setjmp.3 longjmp.3			\
         setjmp.3 longjmperr.3			\
         setjmp.3 longjmperror.3		\
         setjmp.3 siglongjmp.3			\
         setjmp.3 sigsetjmp.3

MLINKS+= setmode.3 getmode.3

MLINKS+= sigsetops.3 sigaddset.3		\
         sigsetops.3 sigdelset.3		\
         sigsetops.3 sigemptyset.3		\
         sigsetops.3 sigfillset.3		\
         sigsetops.3 sigismember.3

MLINKS+= stringlist.3 sl_add.3			\
         stringlist.3 sl_find.3			\
         stringlist.3 sl_free.3			\
         stringlist.3 sl_init.3

MLINKS+= sysctl.3 sysctlbyname.3		\
         sysctl.3 sysctlnametomib.3

MLINKS+= syslog.3 closelog.3			\
         syslog.3 openlog.3			\
         syslog.3 setlogmask.3			\
         syslog.3 vsyslog.3

MLINKS+= tcsendbreak.3 tcdrain.3		\
         tcsendbreak.3 tcflow.3			\
         tcsendbreak.3 tcflush.3

MLINKS+= tcsetattr.3 cfgetispeed.3		\
         tcsetattr.3 cfgetospeed.3		\
         tcsetattr.3 cfmakeraw.3		\
         tcsetattr.3 cfsetispeed.3		\
         tcsetattr.3 cfsetospeed.3		\
         tcsetattr.3 cfsetspeed.3		\
         tcsetattr.3 tcgetattr.3

MLINKS+= ttyname.3 isatty.3			\
         ttyname.3 ttyslot.3

MLINKS+= tzset.3 tzsetwall.3

MLINKS+= unvis.3 strunvis.3			\
         unvis.3 strunvisx.3

MLINKS+= vis.3 strvis.3				\
         vis.3 strvisx.3

MLINKS+= wordexp.3 wordfree.3
.endif
