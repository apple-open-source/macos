/*$Id: procmail.h,v 1.1.1.1 1999/09/23 17:30:07 wsanchez Exp $*/

#include "includes.h"

#ifdef console
#define DEFverbose 1
#else
#define DEFverbose 0
#endif

#ifdef GROUP_PER_USER
#define NO_CHECK_stgid 0
#else
#define NO_CHECK_stgid 1
#endif

#ifndef DEFsendmail
#define DEFsendmail SENDMAIL
#endif
#ifndef DEFflagsendmail
#define DEFflagsendmail "-oi"
#endif

#ifndef DEFPATH
#define DEFPATH		defPATH
#endif

#ifndef ETCRC
#define ETCRC	0
#endif

#define MAX32	((long)(~(unsigned long)0>>1))
#define MIN32	(-(long)MAX32)

#define XTRAlinebuf	2	     /* surplus of LINEBUF (see readparse()) */
#ifdef MAXPATHLEN
#if MAXPATHLEN>DEFlinebuf		/* to protect people from themselves */
#undef DEFlinebuf
#define DEFlinebuf MAXPATHLEN
#endif
#endif

#define rc_NOSGID	1		      /* you can forget any sgidness */
#define rc_NORMAL	2

#define MCDIRSEP	(dirsep+STRLEN(dirsep)-1)      /* most common DIRSEP */
#define MCDIRSEP_	(dirsep+STRLEN(DIRSEP)-1)

#define lck_LOCKFILE	1	  /* crosscheck the order of this with msg[] */
#define lck_ALLOCLIB	2		      /* in sterminate() in retint.c */
#define lck_MEMORY	4
#define lck_FORK	8
#define lck_FILDES	16
#define lck_KERNEL	32

extern struct varval{const char*const name;long val;}strenvvar[];
#define locksleep	(strenvvar[0].val)
#define locktimeout	(strenvvar[1].val)
#define suspendv	(strenvvar[2].val)
#define noresretry	(strenvvar[3].val)
#define timeoutv	(strenvvar[4].val)
#define verbose		(*(volatile long*)&strenvvar[5].val)
#define lgabstract	(strenvvar[6].val)

extern struct varstr{const char*const sname,*sval;}strenstr[];
#define shellmetas	(strenstr[0].sval)
#define lockext		(strenstr[1].sval)
#define msgprefix	(strenstr[2].sval)
#define scomsat		(strenstr[3].sval)
#define traps		(strenstr[4].sval)
#define shellflags	(strenstr[5].sval)
#define fdefault	(*(const char*volatile*)&strenstr[6].sval)
#define sendmail	(strenstr[7].sval)
#define flagsendmail	(strenstr[8].sval)
/* #define PM_version	(strenstr[9].sval) */

int
 eqFrom_ P((const char*const a));

extern char*buf,*buf2,*loclock,*tolock,*Stdout,*themail,*thebody;
extern const char shell[],lockfile[],newline[],binsh[],unexpeof[],*const*gargv,
 *const*restargv,*sgetcp,pmrc[],*rcfile,dirsep[],devnull[],lgname[],
 executing[],oquote[],cquote[],whilstwfor[],procmailn[],Mail[],home[],host[],
 *defdeflock,*argv0,errwwriting[],slogstr[],conflicting[],orgmail[],
 exceededlb[];
extern long filled,lastscore;
extern int sh,pwait,retval,retvl2,lcking,rcstate,rc,ignwerr,lexitcode,
 asgnlastf,accspooldir,crestarg,skiprc,savstdout,berkeley,mailfilter,erestrict;
extern size_t linebuf;
extern volatile int nextexit;
extern pid_t thepid;
extern uid_t uid;
extern gid_t gid,sgid;

/*
 *	External variables that are checked/changed by the signal handlers:
 *	volatile time_t alrmtime;
 *	pid_t pidfilt,pidchild;
 *	volatile nextexit;
 *	int lcking;
 *	static volatile mailread;	in mailfold.c
 */
