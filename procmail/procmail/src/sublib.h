/*$Id: sublib.h,v 1.1.1.1 1999/09/23 17:30:08 wsanchez Exp $*/

#ifdef NOmemmove
void
 *smemmove Q((void*To,const void*From,size_t count));
#endif

#ifdef NOstrpbrk
char
 *strpbrk P((const char*const st,const char*del));
#endif

#ifdef SLOWstrstr
char
 *sstrstr P((const char*const phaystack,const char*const pneedle));
#endif

#ifdef NOstrtol
long
 strtol P((const char*start,const char**const ptr));
#endif
