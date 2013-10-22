/*
 * Copyright (c) 2013 Apple Inc.
 * All rights reserved.
 */

#ifndef __NETWORK_DETECTION_HELPER_H__
#define __NETWORK_DETECTION_HELPER_H__

int launch_http_probe(int argc, const char * argv[]);
int detect_dns_redirect(int argc, const char * argv[]);

#endif /* __NETWORK_DETECTION_HELPER_H__ */
