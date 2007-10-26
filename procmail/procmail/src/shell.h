/*$Id: shell.h,v 1.1 1999/09/23 17:30:07 wsanchez Exp $*/

#ifdef malloc
#undef malloc
#endif
#define malloc(n)	tmalloc((size_t)(n))
#define realloc(p,n)	trealloc(p,(size_t)(n))
#define free(p)		tfree(p)
#define tmemmove(t,f,n) memmove(t,f,(size_t)(n))
