#ifndef __OVERLAYUTILS_H__
#define __OVERLAYUTILS_H__

#include "portable.h"
#include <stdio.h>

#include <ac/string.h>
#include <ac/ctype.h>
#include "slap.h"
#include "ldif.h"
#include "config.h"

void dump_berval( struct berval *bv );
void dump_berval_array(BerVarray bva);
void dump_slap_attr_desc(AttributeDescription *desc);
void dump_slap_attr(Attribute *attr);
void dump_slap_entry(Entry *ent);
void dump_req_bind_s(req_bind_s *req);
void dump_req_add_s(req_add_s *req);

#endif /* __OVERLAYUTILS_H__ */
