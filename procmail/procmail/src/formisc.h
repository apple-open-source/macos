/*$Id: formisc.h,v 1.1.1.2 2001/07/20 19:38:16 bbraun Exp $*/

void
 loadsaved P((const struct saved*const sp)),
 loadbuf Q((const char*const text,const size_t len)),
 loadchar P((const int c)),
 elog P((const char*const a)),
 tputssn Q((const char*a,size_t l)),
 ltputssn Q((const char*a,size_t l)),
 lputcs P((const int i)),
 startprog P((const char*Const*const argv)),
 nofild P((void)),
 nlog P((const char*const a)),
 logqnl P((const char*const a)),
 closemine P((void)),
 opensink P((void));
char*
 skipwords P((char*start));
int
 getline P((void));
