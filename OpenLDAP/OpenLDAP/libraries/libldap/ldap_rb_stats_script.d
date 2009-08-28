/*
 * Copyright (c) 2008 Apple Inc.  All Rights Reserved.
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
 * 
 * Does this need to be released under the OpenLDAP License instead of, or in
 * addition to the APSL?
 * 
 */

/*
 * Dtrace script to see the response message rb tree stats
 *
 * To run this script:
 *    sudo dtrace -s ldap_rb_stats_script.d -p <pid of some ldap using process>
 *    sudo dtrace -s ldap_rb_stats_script.d -c "some ldap using command"
 */

#pragma D option quiet
#pragma D option bufsize=2m
#pragma D option switchrate=150hz


/* High water mark for the count */
int hwmcount;

ldap_rb_stats$target:::count
{
	printf("\n%s: msg id: %d, msg ptr %p nodes in rb tree %d", probefunc, arg1, arg2, arg0);
	ustack();
}

ldap_rb_stats$target:::count
/ arg0 > hwmcount /
{
	hwmcount = arg0;
}

END
{
	printf("nodes in rb tree high water mark: %d\n", hwmcount);
}