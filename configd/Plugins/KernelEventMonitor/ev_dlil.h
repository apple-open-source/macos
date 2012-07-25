/*
 * Copyright (c) 2002, 2004, 2005, 2011 Apple Computer, Inc. All rights reserved.
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

/*
 * Modification History
 *
 * August 5, 2002	Allan Nathanson <ajn@apple.com>
 * - split code out from eventmon.c
 */


#ifndef _EV_DLIL_H
#define _EV_DLIL_H

__BEGIN_DECLS

__private_extern__
void	interface_detaching		(const char *if_name);

__private_extern__
void	interface_update_idle_state	(const char *if_name);

__private_extern__
void	interface_update_quality_metric	(const char *if_name, int quality);

__private_extern__
void	link_add			(const char *if_name);

__private_extern__
void	link_remove			(const char *if_name);

__private_extern__
void	link_update_status		(const char *if_name, boolean_t attach);

__END_DECLS

#endif /* _EV_DLIL_H */

