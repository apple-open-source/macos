/*$Id: regexp.h,v 1.13 1994/10/07 15:25:09 berg Exp $*/

struct eps
{ unsigned opc;struct eps*next;
  union seps {struct eps*awn;int sopc;void*irrelevoid;} sp;
}*
 bregcomp P((const char*const a,const unsigned ign_case));
char*
 bregexec Q((struct eps*code,const uchar*const text,const uchar*str,size_t len,
  const unsigned ign_case));
