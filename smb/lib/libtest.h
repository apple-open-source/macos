/*
 * Copyright (c) 2007 - 2010 Apple Inc. All rights reserved.
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

#define	MAX_NUMBER_TESTSTRING 6
#define TESTSTRING1 "/George/J\\im/T:?.*est"
#define TESTSTRING2 "/George/J\\im/T:?.*est/"
#define TESTSTRING3 "George/J\\im/T:?.*est/"
#define TESTSTRING4 "George/J\\im/T:?.*est"
#define TESTSTRING5 "George"
#define TESTSTRING6 "/"
#define TESTSTRING7 "/George"

#define	MAX_URL_TO_DICT_TO_URL_TEST			4
#define URL_TO_DICT_TO_URL_TEST_STR1 "smb://local1:local@[fe80::d9b6:f149:a17c:8307%25en1]/Vista-Share"
#define URL_TO_DICT_TO_URL_TEST_STR2 "smb://local:local@colley%5B3%5D._smb._tcp.local/local"
#define URL_TO_DICT_TO_URL_TEST_STR3 "smb://BAD%3A%3B%2FBAD@colley2/badbad"
#define URL_TO_DICT_TO_URL_TEST_STR4 "smb://user:password@%7e%21%40%24%3b%27%28%29/share"


#define LIST_SHARE_CONNECT_TEST				0
#define MOUNT_WIN2003_VOLUME_TEST			1
#define URL_TO_DICT_TO_URL_TEST				3
#define DFS_MOUNT_TEST						6
#define	DFS_LOOP_TEST						7
#define URL_TO_DICTIONARY					8
#define FIND_VC_FROM_MP_TEST				9
#define NETFS_TEST							10
#define GETACCOUNTNAME_AND_SID_TEST			11
#define FORCE_GUEST_ANON_TEST				12
#define MOUNT_EXIST_TEST					13
#define LIST_DFS_REFERRALS					14
#define RUN_ALL_TEST						-1

#define START_UNIT_TEST		LIST_SHARE_CONNECT_TEST
#define END_UNIT_TEST		LIST_DFS_REFERRALS
/* Should always be greater than END_UNIT_TEST */
#define REMOUNT_UNIT_TEST	END_UNIT_TEST+1

