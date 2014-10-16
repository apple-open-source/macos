/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#include <test/testmore.h>

ONE_TEST(secd_01_items)
ONE_TEST(secd_02_upgrade_while_locked)
ONE_TEST(secd_03_corrupted_items)
DISABLED_ONE_TEST(secd_04_corrupted_items)
ONE_TEST(secd_05_corrupted_items)
ONE_TEST(secd_30_keychain_upgrade) //obsolete, needs updating
ONE_TEST(secd_31_keychain_bad)
ONE_TEST(secd_31_keychain_unreadable)
ONE_TEST(secd_50_account)
ONE_TEST(secd_51_account_inflate)
ONE_TEST(secd_52_account_changed)
ONE_TEST(secd_55_account_circle)
ONE_TEST(secd_55_account_incompatibility)
ONE_TEST(secd_56_account_apply)
ONE_TEST(secd_57_account_leave)
ONE_TEST(secd_58_password_change)
ONE_TEST(secd_59_account_cleanup)
ONE_TEST(secd_60_account_cloud_identity)
ONE_TEST(secd_61_account_leave_not_in_kansas_anymore)
ONE_TEST(secd_70_engine)
ONE_TEST(secd_70_engine_corrupt)
DISABLED_ONE_TEST(secd_70_otr_remote)
