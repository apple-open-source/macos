/*$Id: robust.h,v 1.13 2001/06/21 09:43:53 guenther Exp $*/

void
 nomemerr Q((const size_t len))	 __attribute__((noreturn)),
 *tmalloc Q((const size_t len)),
 *trealloc Q((void*const old,const size_t len)),
 *fmalloc Q((const size_t len)),
 *frealloc Q((void*const old,const size_t len)),
 tfree P((void*const p)),
 opnlog P((const char*file)),
 ssleep P((const unsigned seconds)),
 doumask Q((const mode_t mask));
pid_t
 sfork P((void));
int
 opena P((const char*const a)),
 ropen Q((const char*const name,const mode,const mode_t mask)),
 rpipe P((int fd[2])),
 rdup P((const int p)),
 rclose P((const int fd)),
 rread P((const int fd,void*const a,const int len)),
 rwrite P((const int fd,const void*const a,const int len));

extern mode_t cumask;
