/*$Id: acommon.h,v 1.3 2001/08/04 07:15:00 guenther Exp $*/

const char
 *hostname P((void));
char
 *ultoan P((unsigned long val,char*dest)),
 *ultstr P((int minwidth,unsigned long val,char*dest));
