/*$Id: locking.h,v 1.1.1.2 2001/07/20 19:38:17 bbraun Exp $*/

void
 unlock P((char**const lockp));
int
 lockit P((char*name,char**const lockp)),
 lcllock P((const char*const noext,const char*const withext)),
 xcreat Q((const char*const name,const mode_t mode,time_t*const tim,
  const chownit));

#ifdef NOfcntl_lock
#ifndef USElockf
#ifndef USEflock
#define fdlock(fd)	0
#define fdunlock()	0
#endif
#endif
#endif
#ifndef fdlock
int
 fdlock P((int fd)),
 fdunlock P((void));
#endif

extern char*globlock;
