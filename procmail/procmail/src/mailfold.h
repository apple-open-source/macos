/*$Id: mailfold.h,v 1.1.1.2 2001/07/20 19:38:17 bbraun Exp $*/

long
 dump P((const int s,const int type,const char*source,long len));
int
 writefolder P((char*boxname,char*linkfolder,const char*source,long len,
  const int ignwerr,const int dolock));
void
 logabstract P((const char*const lstfolder)),
 concon P((const int ch)),
 readmail P((int rhead,const long tobesent));

extern int logopened,rawnonl;
extern off_t lasttell;
