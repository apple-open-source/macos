/*$Id: acommon.h,v 1.1.1.2 2001/07/20 19:38:14 bbraun Exp $*/

const char
 *hostname P((void));
char
 *ultoan P((unsigned long val,char*dest));
void
 ultstr P((int minwidth,unsigned long val,char*dest));
