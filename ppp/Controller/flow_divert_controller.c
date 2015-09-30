/*
 * flow_divert_controller.c
 *
 * Copyright (c) 2012-2013, 2015 Apple Inc.
 * All rights reserved.
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include "scnc_main.h"
#include "flow_divert_controller.h"

CFDictionaryRef
flow_divert_create_dns_configuration(int control_unit, const char *if_name)
{
#pragma unused(control_unit, if_name)
	return NULL;
}

void
flow_divert_init(struct service *serv, int index)
{
#pragma unused(serv, index)
}

void
flow_divert_dispose(struct service *serv, int index)
{
#pragma unused(serv, index)
}

CFDictionaryRef
flow_divert_copy_token_parameters(struct service *serv)
{
#pragma unused(serv)
	return NULL;
}

CFNumberRef
flow_divert_copy_service_identifier(struct service *serv)
{
#pragma unused(serv)
	return NULL;
}

CFDataRef
flow_divert_copy_match_rules_result(CFDataRef request)
{
#pragma unused(request)
	return NULL;
}

void
flow_divert_set_signing_ids(CFArrayRef signing_ids)
{
#pragma unused(signing_ids)
}
