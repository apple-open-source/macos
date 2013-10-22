/*
 * Copyright (c) 2012, 2013 Apple Computer, Inc. All rights reserved.
 */

#ifndef ppp_network_detection_h
#define ppp_network_detection_h

boolean_t check_network(struct service *serv);

int copy_trigger_info(struct service *serv, CFMutableDictionaryRef *ondemand_dict_cp, CFMutableArrayRef *trigger_array_cp, CFMutableDictionaryRef *trigger_dict_cp);

#endif
