/*$Id: acommon.h,v 1.1.1.3 2003/10/14 23:13:23 rbraun Exp $*/

const char
 *hostname P((void));
char
 *ultoan P((unsigned long val,char*dest)),
 *ultstr P((int minwidth,unsigned long val,char*dest));
