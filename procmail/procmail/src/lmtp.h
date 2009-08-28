/* $Id: lmtp.h,v 1.4 2001/02/20 09:35:23 guenther Exp $ */

extern int childserverpid;

struct auth_identity
 **lmtp P((struct auth_identity***lrout,char*invoker));
void
 flushoverread P((void)),
 freeoverread P((void)),
 lmtpresponse P((int retcode));
