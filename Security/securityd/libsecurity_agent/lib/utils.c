/*
 *  utils.h
 *  libsecurity_agent
 *
 *  Copyright (c) 2010-2011 Apple Inc. All Rights Reserved.
 *
 */


#include "utils.h"

unsigned char *
uuid_init_with_sessionid(uuid_t uuid, uint32_t sessionid)
{
	uuid_t tmp = UUID_INITIALIZER_FROM_SESSIONID(sessionid);
	
	uuid_copy(uuid, tmp);
	return &uuid[0];
}

const char *
uuid_to_string(const uuid_t uuid, char *buf)
{
	uuid_unparse_lower(uuid, buf);
	return buf;
}
