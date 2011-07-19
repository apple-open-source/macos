// See: <rdar://problem/8289209>, <rdar://problem/8351271>, <rdar://problem/8359348>

#include <dlfcn.h>

#if __i386__ || __x86_64__

#define SYM(sym) \
  __asm__(".globl R8289209$_" #sym "; R8289209$_" #sym ": jmp _" #sym);

/****************/

SYM(close);
SYM(fork);
SYM(fsync);
SYM(getattrlist);
SYM(getrlimit);
SYM(getxattr);
SYM(open);
SYM(pthread_attr_destroy);
SYM(pthread_attr_init);
SYM(pthread_attr_setdetachstate);
SYM(pthread_create);
SYM(pthread_mutex_lock);
SYM(pthread_mutex_unlock);
SYM(pthread_self);
SYM(ptrace);
SYM(read);
SYM(setattrlist);
SYM(setrlimit);
SYM(sigaction);
SYM(stat);
SYM(sysctl);
SYM(time);
SYM(unlink);
SYM(write);

#endif
