s/#include <syslog.h>/&\
\
extern int udp_sock;/

s/#include "clib.h"/&\
\
extern int udp_sock;\
extern int _rpcsvcdirty;/

s/switch (rqstp->rq_proc)/if (transp->xp_sock == udp_sock \&\&\
\		!(rqstp->rq_proc == NIBIND_PING ||\
\		  rqstp->rq_proc == NIBIND_GETREGISTER ||\
\		  rqstp->rq_proc == NIBIND_LISTREG ||\
\		  rqstp->rq_proc == NIBIND_BIND)) {\
\		svcerr_weakauth(transp);\
\		_rpcsvcdirty = 0;\
\		return;\
\	}\
\
\	&/
