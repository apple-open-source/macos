/*$Id: ecommon.h,v 1.4 1994/05/26 14:12:33 berg Exp $*/

void
 *tmalloc Q((const size_t len)),
 *trealloc Q((void*old,const size_t len)),
 tfree P((void*a));
