/*$Id: fields.h,v 1.1.1.2 2001/07/20 19:38:16 bbraun Exp $*/

struct field
 *findf P((const struct field*const p,struct field**ah)),
 **addfield Q((struct field**pointer,const char*const text,
  const size_t totlen)),
 *delfield P((struct field**pointer));
void
 cleanheader P((void)),
 clear_uhead P((struct field*hdr)),
 concatenate P((struct field*const fldp)),
 flushfield P((struct field**pointer)),
 dispfield P((const struct field*p)),
 addbuf P((void));
int
 readhead P((void));
