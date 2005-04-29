#include	"vmhdr.h"

/*	Return the containing region of an allocated piece of memory.
**	Beware: this only works with Vmbest and Vmtrace.
**
**	Written by Kiem-Phong Vo, kpv@research.att.com, 01/16/94.
*/
#if __STD_C
Vmalloc_t* vmregion(reg Void_t* addr)
#else
Vmalloc_t* vmregion(addr)
reg Void_t*	addr;
#endif
{	return addr ? VM(BLOCK(addr)) : NIL(Vmalloc_t*);
}
