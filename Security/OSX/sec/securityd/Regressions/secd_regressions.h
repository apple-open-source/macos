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
ONE_TEST(secd_20_keychain_upgrade)
ONE_TEST(secd_21_transmogrify)
DISABLED_ONE_TEST(secd_30_keychain_upgrade) //obsolete, needs updating
ONE_TEST(secd_31_keychain_bad)
ONE_TEST(secd_31_keychain_unreadable)
OFF_ONE_TEST(secd_32_restore_bad_backup)
ONE_TEST(secd_33_keychain_ctk)
ONE_TEST(secd_35_keychain_migrate_inet)
ONE_TEST(secd_36_ks_encrypt)
ONE_TEST(secd_40_cc_gestalt)
ONE_TEST(secd_50_account)
ONE_TEST(secd_49_manifests)
ONE_TEST(secd_50_message)
ONE_TEST(secd_51_account_inflate)
ONE_TEST(secd_52_account_changed)
ONE_TEST(secd_52_offering_gencount_reset)
ONE_TEST(secd_55_account_circle)
ONE_TEST(secd_55_account_incompatibility)
ONE_TEST(secd_56_account_apply)
ONE_TEST(secd_57_account_leave)
ONE_TEST(secd_57_1_account_last_standing)
ONE_TEST(secd_58_password_change)
ONE_TEST(secd_59_account_cleanup)
ONE_TEST(secd_60_account_cloud_identity)
ONE_TEST(secd_60_account_cloud_exposure)
ONE_TEST(secd_61_account_leave_not_in_kansas_anymore)
ONE_TEST(secd_62_account_hsa_join)
ONE_TEST(secd_62_account_backup)
ONE_TEST(secd_63_account_resurrection)
ONE_TEST(secd_64_circlereset)
ONE_TEST(secd_65_account_retirement_reset)
ONE_TEST(secd_66_account_recovery)
ONE_TEST(secd_67_prefixedKeyIDs)
ONE_TEST(secd_70_engine)
ONE_TEST(secd_70_engine_corrupt)
ONE_TEST(secd_70_engine_smash)
ONE_TEST(secd_71_engine_save)
ONE_TEST(secd_76_idstransport)
ONE_TEST(secd_77_ids_messaging)

DISABLED_ONE_TEST(secd_70_otr_remote)
ONE_TEST(secd_74_engine_beer_servers)
OFF_ONE_TEST(secd_75_engine_views)
ONE_TEST(secd_80_views_basic)
ONE_TEST(secd_80_views_alwayson)
ONE_TEST(secd_82_secproperties_basic)
#if TARGET_IPHONE_SIMULATOR
OFF_ONE_TEST(secd_81_item_acl_stress)
OFF_ONE_TEST(secd_81_item_acl)
#else
ONE_TEST(secd_81_item_acl_stress)
ONE_TEST(secd_81_item_acl)
#endif
ONE_TEST(secd_82_persistent_ref)
ONE_TEST(secd_83_item_match_policy)
ONE_TEST(secd_83_item_match_valid_on_date)
ONE_TEST(secd_83_item_match_trusted)
ONE_TEST(secd_90_hsa2)
ONE_TEST(secd_95_escrow_persistence)
ONE_TEST(secd_154_engine_backoff)
ONE_TEST(secd_100_initialsync)
ONE_TEST(secd_130_other_peer_views)
ONE_TEST(secd_200_logstate)
ONE_TEST(secd_201_coders)
ONE_TEST(secd_202_recoverykey)
ONE_TEST(secd_210_keyinterest)
ONE_TEST(secd_668_ghosts)

