/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
extern int 		 get_if_count();
extern boolean_t 	 get_if_name(int intface, char * name);
extern boolean_t 	 get_if_addr(char * name, u_int32_t * addr);
extern boolean_t 	 get_if_option(char * name, int option_code, 
				       void * option_data, 
				       unsigned int * option_dataCnt);
extern boolean_t 	 get_if_packet(char * name, void * packet_data,
				       unsigned int * packet_dataCnt);
extern void 		 wait_all();
extern boolean_t 	 wait_if(char *);
extern ipconfig_status_t set_if(char * name, ipconfig_method_t method, 
				void * method_data, 
				unsigned int method_data_len,
				void * serviceID);



