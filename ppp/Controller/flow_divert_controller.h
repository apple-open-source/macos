/*
 * flow_divert_controller.h
 *
 * Copyright (c) 2012-2013 Apple Inc.
 * All rights reserved.
 */

#ifndef __FLOW_DIVERT_CONTROLLER_H__
#define __FLOW_DIVERT_CONTROLLER_H__

void flow_divert_init(struct service *serv, int index);
void flow_divert_dispose(struct service *serv, int index);
CFDictionaryRef flow_divert_copy_token_parameters(struct service *serv);
CFNumberRef flow_divert_copy_service_identifier(struct service *serv);
CFDataRef flow_divert_copy_match_rules_result(CFDataRef request);
void flow_divert_set_signing_ids(CFArrayRef signing_ids);

#endif /* __FLOW_DIVERT_CONTROLLER_H__ */
