//
//  SecdTestKeychainUtilities.h
//  sec
//
//  Created by Mitch Adler on 6/11/13.
//
//

#ifndef _SECDTESTKEYCHAINUTILITIES_
#define _SECDTESTKEYCHAINUTILITIES_

#include <dispatch/dispatch.h>

#define kSecdTestSetupTestCount 1
void secd_test_setup_temp_keychain(const char* test_prefix, dispatch_block_t do_before_reset);

#endif
