/*$Id: cstdio.h,v 1.1.1.1 1999/09/23 17:30:07 wsanchez Exp $*/

void
 pushrc P((const char*const name)),
 duprcs P((void)),
 closerc P((void)),
 ungetb P((const x)),
 skipline P((void));
int
 poprc P((void)),
 bopen P((const char*const name)),
 getbl P((char*p,char*end)),
 getb P((void)),
 testB P((const x)),
 sgetc P((void)),
 skipspace P((void)),
 getlline P((char*target));

extern struct dynstring*incnamed;
