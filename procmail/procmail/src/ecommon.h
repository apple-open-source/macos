/*$Id: ecommon.h,v 1.1 1999/09/23 17:30:07 wsanchez Exp $*/

void
 *tmalloc Q((const size_t len)),
 *trealloc Q((void*old,const size_t len)),
 tfree P((void*a));
