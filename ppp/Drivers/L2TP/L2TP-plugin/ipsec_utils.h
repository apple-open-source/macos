/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */



int is_racoon_started(char *filename);
int start_racoon(CFBundleRef bundle, char *filename);
int stop_racoon();
int require_secure_transport(struct sockaddr *src, struct sockaddr *dst, u_int8_t proto, char *way);
int remove_secure_transport(struct sockaddr *src, struct sockaddr *dst, u_int8_t proto, char *way);
int remove_security_associations(struct sockaddr *src, struct sockaddr *dst);
void sockaddr_to_string(const struct sockaddr *address, char *buf, size_t bufLen);
int get_src_address(struct sockaddr *src, const struct sockaddr *dst);
int get_my_address(struct sockaddr *src);
int configure_racoon(struct sockaddr_in *src, struct sockaddr_in *dst, struct sockaddr_in *dst2, char proto, char *secret, char *secret_type);
int cleanup_racoon(struct sockaddr_in *src, struct sockaddr_in *dst);
