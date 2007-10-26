#	@(#)Makefile.inc	8.3 (Berkeley) 10/24/94
# $FreeBSD: src/lsys/Makefile.inc,v 1.91 2001/09/21 21:35:22 rwatson Exp $

# Include machine dependent definitions.
#
# MDASM names override the default syscall names in MIASM.
# NOASM will prevent the default syscall code from being generated.
#
.ifnmake autopatch
.if exists(${.CURDIR}/${MACHINE_ARCH}/sys/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/sys/Makefile.inc"
.endif
.endif # !autopatch

# sys sources
.PATH: ${.CURDIR}/sys

# Include the generated makefile containing the *complete* list
# of syscall names in MIASM.
#.include "${.CURDIR}/../../sys/sys/syscall.mk"

# Sources common to both syscall interfaces:

# 3375657: patches for sem_open() sem_unlink() shm_open() shm_unlink()
MISRCS+= chmod.c chmodx_np.c crt_externs.c \
	errno.c fchmod.c fix-3375657.c \
	getiopolicy_np.c getrlimit.c gettimeofday.c \
	kill.c __libc_init.c \
	mmap.c \
	openx_np.c \
	posix_spawn.c \
	select.c sem_open.c sem_unlink.c semctl.c \
	setrlimit.c \
	shm_open.c shm_unlink.c sigaction.c sigcatch.c sigsuspend.c \
	sigtramp.c statx_np.c \
	umaskx_np.c

.if (${MACHINE_ARCH} != ppc)
.if (${MACHINE_ARCH} != i386)
MISRCS+= context-stubs.c
.endif
.endif

.ifdef LP64
MISRCS+= fcntl.c ioctl.c
PRE1050SRCS+= select.c
.endif

.include "Makefile.obsd_begin"
OBSDMISRCS= stack_protector.c
.include "Makefile.obsd_end"

.for _src in sem_open.c sem_unlink.c shm_open.c shm_unlink.c fix-3375657.c
CFLAGS-${_src} += -D__APPLE_PR3375657_HACK__
.endfor

INODE32SRCS += statx_np.c

CANCELABLESRCS += select.c sigsuspend.c

.ifdef LP64
CANCELABLESRCS+= fcntl.c
.endif

LEGACYSRCS += accept.c bind.c connect.c \
	getattrlist.c getpeername.c getsockname.c \
	kill.c lchown.c listen.c \
	mprotect.c msgctl.c msync.c munmap.c \
	open.c \
	recvfrom.c recvmsg.c \
	select.c semctl.c sendmsg.c sendto.c setattrlist.c \
	shmctl.c sigsuspend.c socketpair.c

# we need to create open.h, which just contains a definition for O_NOCTTY
open.do open.o open.po open.So: open.h
open.h :
	${CC} -E -dD ${CFLAGS} -include fcntl.h -x c /dev/null | grep O_NOCTTY > ${.TARGET}

.for _src in msgctl.c semctl.c shmctl.c
CFLAGS-${_src} += -DKERNEL
.endfor

# sigtramp.c can only compile with __DARWIN_UNIX03=0 (for ppc64) because
# the structure field names are renamed with __ prefix when __DARWIN_UNIX03=1.
# If sigtramp.c ever needs to build variant, this will have to be fix properly
CFLAGS-sigtramp.c = -U__DARWIN_UNIX03 -D__DARWIN_UNIX03=0

# set the LIBC_ALIAS_* macros so we can decorate the symbol independent
# of other macro settings
CFLAGS-accept.c += -DLIBC_ALIAS_ACCEPT
#CFLAGS-aio_suspend.c += -DLIBC_ALIAS_AIO_SUSPEND
CFLAGS-bind.c += -DLIBC_ALIAS_BIND
CFLAGS-chmod.c += -DLIBC_ALIAS_CHMOD
#CFLAGS-close.c += -DLIBC_ALIAS_CLOSE
CFLAGS-connect.c += -DLIBC_ALIAS_CONNECT
CFLAGS-fchmod.c += -DLIBC_ALIAS_FCHMOD
CFLAGS-fcntl.c += -DLIBC_ALIAS_FCNTL
#CFLAGS-fsync.c += -DLIBC_ALIAS_FSYNC
CFLAGS-getattrlist.c += -DLIBC_ALIAS_GETATTRLIST
CFLAGS-getpeername.c += -DLIBC_ALIAS_GETPEERNAME
CFLAGS-getrlimit.c += -DLIBC_ALIAS_GETRLIMIT
CFLAGS-getsockname.c += -DLIBC_ALIAS_GETSOCKNAME
CFLAGS-kill.c += -DLIBC_ALIAS_KILL
CFLAGS-lchmod.c += -DLIBC_ALIAS_LCHMOD
CFLAGS-listen.c += -DLIBC_ALIAS_LISTEN
CFLAGS-mmap.c += -DLIBC_ALIAS_MMAP
CFLAGS-mprotect.c += -DLIBC_ALIAS_MPROTECT
CFLAGS-msgctl.c += -DLIBC_ALIAS_MSGCTL
#CFLAGS-msgrcv.c += -DLIBC_ALIAS_MSGRCV
#CFLAGS-msgsnd.c += -DLIBC_ALIAS_MSGSND
CFLAGS-msync.c += -DLIBC_ALIAS_MSYNC
CFLAGS-munmap.c += -DLIBC_ALIAS_MUNMAP
CFLAGS-open.c += -DLIBC_ALIAS_OPEN
#CFLAGS-poll.c += -DLIBC_ALIAS_POLL
#CFLAGS-pread.c += -DLIBC_ALIAS_PREAD
#CFLAGS-pwrite.c += -DLIBC_ALIAS_PWRITE
#CFLAGS-read.c += -DLIBC_ALIAS_READ
#CFLAGS-readv.c += -DLIBC_ALIAS_READV
CFLAGS-recvfrom.c += -DLIBC_ALIAS_RECVFROM
CFLAGS-recvmsg.c += -DLIBC_ALIAS_RECVMSG
CFLAGS-select.c += -DLIBC_ALIAS_SELECT
#CFLAGS-sem_wait.c += -DLIBC_ALIAS_SEM_WAIT
CFLAGS-semctl.c += -DLIBC_ALIAS_SEMCTL
CFLAGS-sendmsg.c += -DLIBC_ALIAS_SENDMSG
CFLAGS-sendto.c += -DLIBC_ALIAS_SENDTO
CFLAGS-setattrlist.c += -DLIBC_ALIAS_SETATTRLIST
CFLAGS-setrlimit.c += -DLIBC_ALIAS_SETRLIMIT
CFLAGS-shmctl.c += -DLIBC_ALIAS_SHMCTL
CFLAGS-sigsuspend.c += -DLIBC_ALIAS_SIGSUSPEND
CFLAGS-socketpair.c += -DLIBC_ALIAS_SOCKETPAIR
#CFLAGS-waitid.c += -DLIBC_ALIAS_WAITID
#CFLAGS-write.c += -DLIBC_ALIAS_WRITE
#CFLAGS-writev.c += -DLIBC_ALIAS_WRITEV

# Add machine dependent asm sources:
SRCS+=${MDASM}

# Look though the complete list of syscalls (MIASM) for names that are
# not defined with machine dependent implementations (MDASM) and are
# not declared for no generation of default code (NOASM).  Add each
# syscall that satisfies these conditions to the ASM list.
.for _asm in ${MIASM}
.if (${MDASM:R:M${_asm:R}} == "")
.if (${NOASM:R:M${_asm:R}} == "")
ASM+=$(_asm)
.endif
.endif
.endfor

OBJS+=	${ASM} ${PSEUDO}

SASM=	${ASM:S/.o/.S/}

SPSEUDO= ${PSEUDO:S/.o/.S/}

SRCS+=	${SASM} ${SPSEUDO}

# Generated files
CLEANFILES+=	${SASM} ${SPSEUDO}

${SASM}:
	printf '#include <SYS.h>\nRSYSCALL(${.PREFIX})\n' > ${.TARGET}

${SPSEUDO}:
	printf '#include <SYS.h>\nPSEUDO(${.PREFIX:S/_//})\n' \
	    > ${.TARGET}

COPYFILES+= ${.CURDIR}/sys/libc.syscall

.if ${LIB} == "c"
MAN2+= pthread_kill.2 pthread_sigmask.2 \
	sem_close.2 sem_open.2 sem_post.2 \
	sem_unlink.2 sem_wait.2 shm_open.2 shm_unlink.2 \
	sigwait.2 getdtablesize.2 setreuid.2 setregid.2 \
	nanosleep.2 undelete.2
MAN3+= atomic.3 barrier.3 getiopolicy_np.3 spinlock.3 cache.3
MLINKS+= atomic.3 OSAtomicAdd32.3
MLINKS+= atomic.3 OSAtomicAdd32Barrier.3
MLINKS+= atomic.3 OSAtomicAdd64.3
MLINKS+= atomic.3 OSAtomicAdd64Barrier.3
MLINKS+= atomic.3 OSAtomicAnd32.3
MLINKS+= atomic.3 OSAtomicAnd32Barrier.3
MLINKS+= atomic.3 OSAtomicAnd32Orig.3
MLINKS+= atomic.3 OSAtomicAnd32OrigBarrier.3
MLINKS+= atomic.3 OSAtomicCompareAndSwap32.3
MLINKS+= atomic.3 OSAtomicCompareAndSwap32Barrier.3
MLINKS+= atomic.3 OSAtomicCompareAndSwap64.3
MLINKS+= atomic.3 OSAtomicCompareAndSwap64Barrier.3
MLINKS+= atomic.3 OSAtomicCompareAndSwapInt.3
MLINKS+= atomic.3 OSAtomicCompareAndSwapIntBarrier.3
MLINKS+= atomic.3 OSAtomicCompareAndSwapLong.3
MLINKS+= atomic.3 OSAtomicCompareAndSwapLongBarrier.3
MLINKS+= atomic.3 OSAtomicCompareAndSwapPtr.3
MLINKS+= atomic.3 OSAtomicCompareAndSwapPtrBarrier.3
MLINKS+= atomic.3 OSAtomicDecrement32.3
MLINKS+= atomic.3 OSAtomicDecrement32Barrier.3
MLINKS+= atomic.3 OSAtomicDecrement64.3
MLINKS+= atomic.3 OSAtomicDecrement64Barrier.3
MLINKS+= atomic.3 OSAtomicDequeue.3
MLINKS+= atomic.3 OSAtomicEnqueue.3
MLINKS+= atomic.3 OSAtomicIncrement32.3
MLINKS+= atomic.3 OSAtomicIncrement32Barrier.3
MLINKS+= atomic.3 OSAtomicIncrement64.3
MLINKS+= atomic.3 OSAtomicIncrement64Barrier.3
MLINKS+= atomic.3 OSAtomicOr32.3
MLINKS+= atomic.3 OSAtomicOr32Barrier.3
MLINKS+= atomic.3 OSAtomicOr32Orig.3
MLINKS+= atomic.3 OSAtomicOr32OrigBarrier.3
MLINKS+= atomic.3 OSAtomicTestAndClear.3
MLINKS+= atomic.3 OSAtomicTestAndClearBarrier.3
MLINKS+= atomic.3 OSAtomicTestAndSet.3
MLINKS+= atomic.3 OSAtomicTestAndSetBarrier.3
MLINKS+= atomic.3 OSAtomicXor32.3
MLINKS+= atomic.3 OSAtomicXor32Barrier.3
MLINKS+= atomic.3 OSAtomicXor32Orig.3
MLINKS+= atomic.3 OSAtomicXor32OrigBarrier.3
MLINKS+= barrier.3 OSMemoryBarrier.3

MLINKS+= getiopolicy_np.3 setiopolicy_np.3

MLINKS+= sem_wait.2 sem_trywait.2
MLINKS+= cache.3 sys_cache_control.3
MLINKS+= cache.3 sys_icache_invalidate.3
MLINKS+= cache.3 sys_dcache_flush.3

MLINKS+= spinlock.3 OSSpinLockLock.3			\
         spinlock.3 OSSpinLockTry.3			\
         spinlock.3 OSSpinLockUnlock.3

#MAN+=	_exit.2 accept.2 access.2 acct.2 adjtime.2 \
#	aio_cancel.2 aio_error.2 aio_read.2 aio_return.2 \
#	aio_suspend.2 aio_waitcomplete.2 aio_write.2 \
#	bind.2 brk.2 chdir.2 chflags.2 \
#	chmod.2 chown.2 chroot.2 clock_gettime.2 close.2 \
#	connect.2 dup.2 execve.2 extattr_get_file.2 \
#	fcntl.2 fhopen.2 flock.2 fork.2 fsync.2 \
#	getdirentries.2 getdtablesize.2 \
#	getfh.2 getfsstat.2 getgid.2 getgroups.2 getitimer.2 getlogin.2 \
#	getpeername.2 getpgrp.2 getpid.2 getpriority.2 getrlimit.2 \
#	getrusage.2 getsid.2 getsockname.2 \
#	getsockopt.2 gettimeofday.2 getuid.2 \
#	intro.2 ioctl.2 issetugid.2 jail.2 kill.2 \
#	kldfind.2 kldfirstmod.2 kldload.2 kldnext.2 kldstat.2 kldsym.2 \
#	kldunload.2 kqueue.2 link.2 listen.2 lseek.2 \
#	madvise.2 mincore.2 minherit.2 mkdir.2 mkfifo.2 mknod.2 mlock.2 mmap.2 \
#	modfind.2 modnext.2 modstat.2 \
#	mount.2 mprotect.2 msync.2 munmap.2 nanosleep.2 \
#	nfssvc.2 open.2 pathconf.2 pipe.2 poll.2 profil.2 ptrace.2 quotactl.2 \
#	read.2 readlink.2 reboot.2 recv.2 rename.2 revoke.2 rfork.2 rmdir.2 \
#	rtprio.2 select.2 semctl.2 semget.2 semop.2 send.2 sendfile.2 \
#	setgroups.2 setpgid.2 setregid.2 setresuid.2 setreuid.2 setsid.2 \
#	setuid.2 shmat.2 shmctl.2 shmget.2 shutdown.2 \
#	sigaction.2 sigaltstack.2 sigpending.2 sigprocmask.2 sigreturn.2 \
#	sigstack.2 sigsuspend.2 socket.2 socketpair.2 stat.2 statfs.2 \
#	swapon.2 symlink.2 sync.2 sysarch.2 syscall.2 \
#	truncate.2 umask.2 undelete.2 \
#	unlink.2 utimes.2 vfork.2 wait.2 write.2
#.if !defined(NO_P1003_1B)
#MAN+=	sched_get_priority_max.2 sched_setparam.2 \
#	sched_setscheduler.2 sched_yield.2
#.endif

#MLINKS+=access.2 eaccess.2
#MLINKS+=brk.2 sbrk.2
#MLINKS+=chdir.2 fchdir.2
#MLINKS+=chflags.2 fchflags.2
#MLINKS+=chmod.2 fchmod.2 chmod.2 lchmod.2
#MLINKS+=chown.2 fchown.2 chown.2 lchown.2
#MLINKS+=clock_gettime.2 clock_getres.2 clock_gettime.2 clock_settime.2
#MLINKS+=dup.2 dup2.2
#MLINKS+=extattr_get_file.2 extattr_set_file.2 \
#	extattr_get_file.2 extattr_delete_file.2 \
#	extattr_get_file.2 extattr_get_fd.2 \
#	extattr_get_file.2 extattr_set_fd.2 \
#	extattr_get_file.2 extattr_delete_fd.2 \
#	extattr_get_file.2 extattr.2
#MLINKS+=fhopen.2 fhstat.2 fhopen.2 fhstatfs.2
#MLINKS+=getdirentries.2 getdents.2
#MLINKS+=getgid.2 getegid.2
#MLINKS+=getitimer.2 setitimer.2
#MLINKS+=getlogin.2 getlogin_r.3
#MLINKS+=getlogin.2 setlogin.2
#MLINKS+=getpgrp.2 getpgid.2
#MLINKS+=getpid.2 getppid.2
#MLINKS+=getpriority.2 setpriority.2
#MLINKS+=getrlimit.2 setrlimit.2
#MLINKS+=getsockopt.2 setsockopt.2
#MLINKS+=gettimeofday.2 settimeofday.2
#MLINKS+=getuid.2 geteuid.2
#MLINKS+=intro.2 errno.2
#MLINKS+=kqueue.2 kevent.2
#MLINKS+=mlock.2 munlock.2
#MLINKS+=modnext.2 modfnext.2
#MLINKS+=mount.2 unmount.2
#MLINKS+=pathconf.2 fpathconf.2
#MLINKS+=read.2 pread.2 read.2 readv.2
#MLINKS+=recv.2 recvfrom.2 recv.2 recvmsg.2
#MLINKS+=send.2 sendmsg.2 send.2 sendto.2
#MLINKS+=setpgid.2 setpgrp.2
#MLINKS+=setresuid.2 setresgid.2 setresuid.2 getresuid.2 setresuid.2 getresgid.2
#MLINKS+=setuid.2 setegid.2 setuid.2 seteuid.2 setuid.2 setgid.2
#MLINKS+=shmat.2 shmdt.2
#MLINKS+=stat.2 fstat.2 stat.2 lstat.2
#MLINKS+=statfs.2 fstatfs.2
#MLINKS+=syscall.2 __syscall.2
#MLINKS+=truncate.2 ftruncate.2
#MLINKS+=utimes.2 futimes.2 utimes.2 lutimes.2
#MLINKS+=wait.2 wait3.2 wait.2 wait4.2 wait.2 waitpid.2
#MLINKS+=write.2 pwrite.2 write.2 writev.2
#.if !defined(NO_P1003_1B)
#MLINKS+=sched_get_priority_max.2 sched_get_priority_min.2 \
#	sched_get_priority_max.2 sched_rr_get_interval.2
#MLINKS+=sched_setparam.2 sched_getparam.2
#MLINKS+=sched_setscheduler.2 sched_getscheduler.2
#.endif
.endif
