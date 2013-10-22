//
//  secd_regressions.h
//  sec
//
//  Created by Fabrice Gautier on 5/29/13.
//
//

#include <test/testmore.h>

ONE_TEST(secd_01_items)
ONE_TEST(secd_02_upgrade_while_locked)
ONE_TEST(secd_03_corrupted_items)
ONE_TEST(secd_04_corrupted_items)
ONE_TEST(secd_05_corrupted_items)

DISABLED_ONE_TEST(secd_30_keychain_upgrade) //obsolete, needs updating
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
