s/#include "clib.h"/&\
#include <NetInfo\/socket_lock.h>\
\
extern int udp_sock;\
extern int _rpcsvcdirty;\
\
\static unsigned time_usec(void);\
\static void time_record(int, unsigned);/

s/#include <syslog.h>/&\
#include <NetInfo\/socket_lock.h>\
\
extern int udp_sock;\
\
\static unsigned time_usec(void);\
\static void time_record(int, unsigned);/

s/char \*result;/&\
\	unsigned time;/

s/switch (rqstp->rq_proc)/if (transp->xp_sock == udp_sock \&\&\
\		!(rqstp->rq_proc == _NI_BIND ||\
\		  rqstp->rq_proc == _NI_PING)) {\
\		svcerr_weakauth(transp);\
\		_rpcsvcdirty = 0;\
\		return;\
\	}\
\
\	&/
	
s/result = (\*local)(&argument, rqstp);/socket_unlock();\
\	time = time_usec();\
\	&\
\	time_record(rqstp->rq_proc, time_usec() - time);\
\	socket_lock();/

s/if (!svc_freeargs/if (result != NULL) { (*local)(\&argument, NULL); }\
	if (!svc_freeargs/

$a\
/*\
\ * This code is shamelessly ripped off from lookupd, though mucked to be\
\ * in microseconds, instead of milleseconds (else, too coarse).\
\ */\
static unsigned\
time_usec(void)\
{\
\	static struct timeval base;\
\	static int initialized;\
\	struct timeval now;\
\
\	if (!initialized) {\
\		gettimeofday(&base, NULL);\
\		initialized = 1;\
\	}\
\	gettimeofday(&now, NULL);\
\	if (now.tv_usec < base.tv_usec) {\
\		return (((now.tv_sec - 1) - base.tv_sec) * 1000000 +\
\			((now.tv_usec + 1000000) - base.tv_usec));\
\	} else {\
\		return ((now.tv_sec - base.tv_sec) * 1000000 +\
\			(now.tv_usec - base.tv_usec));\
\	}\
}\
\
/*\
\ * XXX This definition really belongs in ni.x, but we're not\
\ * going to muck with the protocol definition file in this patch.\
\ * This definition must be shared with ni_prot_svc.c\
\ */\
struct ni_stats {\
\	unsigned long ncalls;\
\	unsigned long time;\
\    } netinfod_stats[_NI_LOOKUPREAD+1]; /* XXX Assumes LOOKUPREAD is last! */\
\
static void\
time_record(int procnum, unsigned msecs)\
{\
\    static unsigned char init = FALSE;\
\
\    if (!init) {\
\	int i;\
\	init = TRUE;\
\	for (i = 0; i <= _NI_LOOKUPREAD; i++) {\
\	    netinfod_stats[i].ncalls = netinfod_stats[i].time = 0;\
\	}\
\    }\
\    netinfod_stats[procnum].ncalls++;\
\    netinfod_stats[procnum].time += msecs;\
}
