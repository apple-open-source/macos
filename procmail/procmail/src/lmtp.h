/* $Id: lmtp.h,v 1.1 2001/07/20 19:38:17 bbraun Exp $ */

extern int childserverpid;

struct auth_identity
 **lmtp P((struct auth_identity***lrout,char*invoker));
void
 flushoverread P((void)),
 freeoverread P((void)),
 lmtpresponse P((int retcode));
