/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
char **ff_tokens_from_line(const char *data, const char *sep, int skip_comments);
char **ff_netgroup_tokens_from_line(const char *data);
CFMutableDictionaryRef ff_parse_user(char *data);
CFMutableDictionaryRef ff_parse_user_A(char *data);
CFMutableDictionaryRef ff_parse_group(char *data);
CFMutableDictionaryRef ff_parse_host(char *data);
CFMutableDictionaryRef ff_parse_network(char *data);
CFMutableDictionaryRef ff_parse_service(char *data);
CFMutableDictionaryRef ff_parse_protocol(char *data);
CFMutableDictionaryRef ff_parse_rpc(char *data);
CFMutableDictionaryRef ff_parse_mount(char *data);
CFMutableDictionaryRef ff_parse_printer(char *data);
CFMutableDictionaryRef ff_parse_bootparam(char *data);
CFMutableDictionaryRef ff_parse_bootp(char *data);
CFMutableDictionaryRef ff_parse_alias(char *data);
CFMutableDictionaryRef ff_parse_ethernet(char *data);
CFMutableDictionaryRef ff_parse_netgroup(char *data);
