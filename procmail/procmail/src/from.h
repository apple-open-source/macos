/* $Id: from.h,v 1.2 2000/10/27 20:03:59 guenther Exp $ */

int
 eqFrom_ P((const char*const a));
const char
 *skipFrom_ P((const char*startchar,long*tobesentp));
void
 makeFrom P((const char*from,const char*const invoker)),
 checkprivFrom_ Q((uid_t euid,const char*logname,int override));
