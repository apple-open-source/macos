/*
 *  utils.h
 *  libsecurity_agent
 *
 *  Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 */

#include <uuid/uuid.h>
#include <bsm/audit.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define UUID_INITIALIZER_FROM_SESSIONID(sessionid) \
	{ 0,0,0,0, 0,0,0,0, 0,0,0,0, \
	(0xff000000 & (sessionid))>>24, (0x00ff0000 & (sessionid))>>16, \
	(0x0000ff00 & (sessionid))>>8,  (0x000000ff & (sessionid)) }
	
unsigned char *uuid_init_with_sessionid(uuid_t uuid, uint32_t sessionid);
const char *uuid_to_string(const uuid_t uuid, char *buf);
	
#if defined(__cplusplus)
}
#endif
