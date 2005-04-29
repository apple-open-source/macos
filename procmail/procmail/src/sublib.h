/*$Id: sublib.h,v 1.1.1.3 2003/10/14 23:13:23 rbraun Exp $*/

#ifdef NOmemmove
void
 *smemmove Q((void*To,const void*From,size_t count));
#endif

#ifdef NOstrpbrk
char
 *sstrpbrk P((const char*const st,const char*del));
#endif

#ifdef SLOWstrstr
char
 *sstrstr P((const char*const phaystack,const char*const pneedle));
#endif

#ifdef NEEDbbzero
void
 bbzero Q((void *s,size_t n));
#endif

#ifdef NOstrlcat
size_t
 sstrlcat Q((char *dst,const char*src,size_t size)),
 sstrlcpy Q((char *dst,const char*src,size_t size));
#endif

#ifdef NOstrerror
char
 *sstrerror P((int err));
#endif

#ifdef NOstrtol
long
 sstrtol P((const char*start,const char**const ptr,int base));
#endif
