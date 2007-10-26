/*$Id: regexp.h,v 1.1 1999/09/23 17:30:07 wsanchez Exp $*/

struct eps
{ unsigned opc;struct eps*next;
  union seps {struct eps*awn;int sopc;void*irrelevoid;} sp;
}*
 bregcomp P((const char*const a,const unsigned ign_case));
char*
 bregexec Q((struct eps*code,const uchar*const text,const uchar*str,size_t len,
  const unsigned ign_case));
