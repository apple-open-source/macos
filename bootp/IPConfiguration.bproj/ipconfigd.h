/*
 * Copyright (c) 2000-2010 Apple Inc. All rights reserved.
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
#ifndef _S_IPCONFIGD_H
#define _S_IPCONFIGD_H

#include "ipconfigd_types.h"

/**
 ** Routines in support of MiG interface
 **/
extern ipconfig_status_t
ipconfig_method_info_from_plist(CFPropertyListRef plist,
				ipconfig_method_t * method_p,
				ipconfig_method_data_t * * method_data_p);
extern int 		 get_if_count();
extern ipconfig_status_t get_if_addr(const char * name, u_int32_t * addr);
extern ipconfig_status_t get_if_option(const char * name, int option_code, 
				       void * option_data, 
				       unsigned int * option_dataCnt);
extern ipconfig_status_t get_if_packet(const char * name, void * packet_data,
				       unsigned int * packet_dataCnt);
extern ipconfig_status_t get_if_v6_packet(const char * name, void * packet_data,
					  unsigned int * packet_dataCnt);
extern ipconfig_status_t set_if(const char * name,
				ipconfig_method_t method, 
				ipconfig_method_data_t * method_data);
extern ipconfig_status_t add_service(const char * name,
				     ipconfig_method_t method, 
				     ipconfig_method_data_t * method_data,
				     void * service_id, 
				     unsigned int * service_id_len,
				     CFDictionaryRef plist, pid_t pid);
extern ipconfig_status_t set_service(const char * name,
				     ipconfig_method_t method, 
				     ipconfig_method_data_t * method_data,
				     void * service_id, 
				     unsigned int * service_id_len);
extern ipconfig_status_t remove_service_with_id(void * service_id, 
						unsigned int service_id_len);
extern ipconfig_status_t find_service(const char * name,
				      boolean_t exact,
				      ipconfig_method_t method, 
				      ipconfig_method_data_t * method_data,
				      void * service_id, 
				      unsigned int * service_id_len);
extern ipconfig_status_t remove_service(const char * name,
					ipconfig_method_t method,
					ipconfig_method_data_t * method_data);
extern ipconfig_status_t set_verbose(int verbose);

#endif /* _S_IPCONFIGD_H */
