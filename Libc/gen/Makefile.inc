#	@(#)Makefile.inc	8.6 (Berkeley) 5/4/95
# $FreeBSD: src/lib/libc/gen/Makefile.inc,v 1.80 2001/08/17 22:09:15 dd Exp $

# machine-independent gen sources
.PATH: ${.CURDIR}/${MACHINE_ARCH}/gen ${.CURDIR}/gen 

CFLAGS+= -I${.CURDIR}/pthreads

SRCS += NSSystemDirectories.c  getpagesize.c          siginterrupt.c \
	alarm.c                getpass.c              siglist.c \
	assert.c               getttyent.c            signal.c \
	                       getusershell.c         sigsetops.c \
	clock.c                getvfsbyname.c         sleep.c \
	closedir.c                                    stack_logging.c \
	confstr.c                                     strchr.c \
	crypt.c                                       sysconf.c \
	ctermid.c              isatty.c               sysctl.c \
	                       isnan.c                sysctlbyname.c \
	daemon.c               malloc.c               syslog.c \
	devname.c              nanosleep.c            telldir.c \
	difftime.c             nice.c                 termios.c \
	disklabel.c            nlist.c                time.c \
	err.c                  opendir.c              times.c \
	errlst.c               pause.c                timezone.c \
	exec.c                 popen.c                ttyname.c \
	fnmatch.c              psignal.c              ttyslot.c \
	                                              ualarm.c \
	ftok.c                 raise.c                uname.c \
	fts.c                                         unvis.c \
	getbsize.c             readdir.c              usleep.c \
	getcap.c               rewinddir.c            utime.c \
	getcwd.c               scalable_malloc.c      vis.c \
	                       scandir.c              wait.c \
	gethostname.c          seekdir.c              wait3.c \
	getloadavg.c           sethostname.c          waitpid.c \
	getlogin.c             setlogin.c             zone.c \
	getmntinfo.c           setmode.c \
	_rand48.c erand48.c lcong48.c mrand48.c seed48.c drand48.c jrand48.c \
	nrand48.c srand48.c lrand48.c basename.c dirname.c arc4random.c \
	strtofflags.c lockf.c readdir_r.c ulimit.c


# machine-dependent gen sources
.if exists(${.CURDIR}/${MACHINE_ARCH}/gen/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/gen/Makefile.inc"
.endif

.if ${LIB} == "c"
MAN3+=	alarm.3 arc4random.3 clock.3 \
	basename.3 \
	confstr.3 ctermid.3 daemon.3 \
	devname.3 directory.3 dirname.3 \
	err.3 exec.3 fmtcheck.3 fnmatch.3 frexp.3 ftok.3 fts.3 \
	getbootfile.3 getbsize.3 getcap.3 getcwd.3 \
	getdomainname.3 getfsent.3 \
	getgrent.3 getgrouplist.3 gethostname.3 getloadavg.3 \
	getmntinfo.3 getnetgrent.3 getobjformat.3 \
	getpagesize.3 getpass.3 getpeereid.3 getpwent.3 \
	getttyent.3 getusershell.3 getvfsbyname.3 getvfsent.3 \
	glob.3 initgroups.3 isinf.3 \
	ldexp.3 lockf.3 modf.3 msgctl.3 msgget.3 msgrcv.3 msgsnd.3 \
	nice.3 nlist.3 pause.3 popen.3 psignal.3 pwcache.3 \
	raise.3 rand48.3 rfork_thread.3 \
	scandir.3 setjmp.3 setmode.3 \
	siginterrupt.3 signal.3 sigsetops.3 sleep.3 stringlist.3 \
	strtofflags.3 sysconf.3 sysctl.3 syslog.3 tcgetpgrp.3 \
	tcsendbreak.3 tcsetattr.3 tcsetpgrp.3 time.3 times.3 timezone.3 \
	ttyname.3 tzset.3 ualarm.3 uname.3 unvis.3 usleep.3 utime.3 \
	valloc.3 vis.3

MLINKS+=arc4random.3 arc4random_addrandom.3 arc4random.3 arc4random_stir.3
MLINKS+=ctermid.3 ctermid_r.3
MLINKS+=directory.3 closedir.3 directory.3 dirfd.3 directory.3 opendir.3 \
	directory.3 readdir.3 directory.3 rewinddir.3 directory.3 seekdir.3 \
	directory.3 telldir.3
MLINKS+=err.3 err_set_exit.3 err.3 err_set_file.3 err.3 errc.3 err.3 errx.3 \
	err.3 verr.3 err.3 verrc.3 err.3 verrx.3 err.3 vwarn.3 err.3 vwarnc.3 \
	err.3 vwarnx.3 err.3 warnc.3 err.3 warn.3 err.3 warnx.3
MLINKS+=exec.3 execl.3 exec.3 execle.3 exec.3 execlp.3 exec.3 exect.3 \
	exec.3 execv.3 exec.3 execvp.3
MLINKS+=fts.3 fts_children.3 fts.3 fts_close.3 fts.3 fts_open.3 \
	fts.3 fts_read.3 fts.3 fts_set.3
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
MLINKS+=getpwent.3 endpwent.3 getpwent.3 getpwnam.3 getpwent.3 getpwuid.3 \
	getpwent.3 setpassent.3 getpwent.3 setpwent.3 getpwent.3 setpwfile.3
MLINKS+=getttyent.3 endttyent.3 getttyent.3 getttynam.3 \
	getttyent.3 isdialuptty.3 getttyent.3 isnettty.3 \
	getttyent.3 setttyent.3
MLINKS+=getusershell.3 endusershell.3 getusershell.3 setusershell.3
MLINKS+=getvfsent.3 endvfsent.3 getvfsent.3 getvfsbytype.3 \
	getvfsent.3 setvfsent.3 getvfsent.3 vfsisloadable.3 \
	getvfsent.3 vfsload.3
MLINKS+=glob.3 globfree.3
MLINKS+=isinf.3 isnan.3 isinf.3 isnanf.3
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
MLINKS+=readdir.3 readdir_r.3
.endif
