/*$Id: pipes.h,v 1.1.1.1 1999/09/23 17:30:07 wsanchez Exp $*/

void
 inittmout P((const char*const progname)),
 ftimeout P((void)),
 resettmout P((void)),
 exectrap P((const char*const tp));
int
 pipthrough P((char*line,char*source,const long len));
long
 pipin P((char*const line,char*source,long len));
char*
 readdyn P((char*bf,long*const filled)),
 *fromprog Q((char*name,char*const dest,size_t max));

#define PRDO	poutfd[0]
#define PWRO	poutfd[1]
#define PRDI	pinfd[0]
#define PWRI	pinfd[1]
#define PRDB	pbackfd[0]
#define PWRB	pbackfd[1]

extern const char exitcode[];
extern int setxit;
extern pid_t pidchild;
extern volatile time_t alrmtime;
extern volatile int toutflag;
extern int pipw;
