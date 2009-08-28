/*$Id: goodies.h,v 1.26 2000/11/22 01:29:58 guenther Exp $*/

int
 readparse P((char*p,int(*const fpgetc)(),const int sarg,int skipping));
char
 *simplesplit P((char*to,const char*from,const char*fencepost,int*gotp));
void
 concatenate P((char*p)),
 metaparse P((const char*p)),
 ltstr P((const int minwidth,const long val,char*dest));

#ifdef NOstrtod
double
 strtod P((const char*str,char**const ptr));
#endif

extern const char test[];

extern const char*Tmnate,*All_args;
