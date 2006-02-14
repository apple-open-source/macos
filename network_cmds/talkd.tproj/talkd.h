/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
extern int	announce __P((CTL_MSG *request, char *remote_machine));
extern void	do_announce __P((CTL_MSG *mp, CTL_RESPONSE *rp));
extern int	delete_invite __P((int id_num));
extern int	find_user __P((char *name, char *tty));
extern void	insert_table __P((CTL_MSG *request, CTL_RESPONSE *response));
extern int	new_id __P((void));
extern void	print_request __P((char *cp, CTL_MSG *mp));
extern void	print_response __P((char *cp, CTL_RESPONSE *rp));
extern void	process_request __P((CTL_MSG *mp, CTL_RESPONSE *rp));
