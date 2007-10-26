/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

// *****************************************************************************
//  cclparser.h
//
//  Created by kevine on 3/1/06.
//  Recreated by sspies 15 January 2007.
// *****************************************************************************

#import <Foundation/Foundation.h>
#import "cclkeys.h"

#define kCCLOtherVendorName @"Other"

@interface CCLParser : NSObject 
{
    NSMutableDictionary *mBundleData;       // vendors->models->personalities
    NSMutableDictionary *mBundlesProcessed; // bundle IDs -> paths
    NSMutableDictionary *mFlatOverrides;    // flat names -> personalities
                                            // (contains "Supersedes" data)

    NSSet   *mTypeFilter;
}

// allocate a parser
+ (CCLParser*)createCCLParser;

// sets a filter for CCL bundle-based personalities.  Specifically, 
// desiredConnectTypes causes the -process* routines to silently ignore
// personalities with Connect Type values other than those listed.
// (e.g. "GPRS" or "Dialup"; someday WWAN?)
// Flat CCL bundles (which have no type information) are still included.
// Be sure to call -clearParser if changing the list for an existing object.
- (void)setTypeFilter:(NSSet*)desiredConnectTypes;

// recursively searches directory (i.e. /Library/Modem Scripts) for CCLs.
// Additional invocations add to the store.
//
// A CCL bundle is a directory with the .ccl extension.
// processFolder: returns NO if it finds any directory that looks like a
// CCL but isn't a properly formed (doesn't have the right files, bad
// version, etc).  It does not give up just because it found one malformed.
// CCL.  Conforming bundles still have their personality data added.  
// Any files are assumed to be flat CCL scripts and are gathered together
// under the 'Other' vendor.  "Flat" CCL personalities have their DeviceVendor
// property set to the English string "Other" (kCCLOtherVendorName).
// Their model is the CCL filename.
- (BOOL)processFolder:(NSString*)folderPath;

// add a single bundle or flat CCL script to the store
- (BOOL)processCCLBundle:(NSString*)path;
- (BOOL)processFlatCCL:(NSString*)path named:(NSString*)name;

// expands names to remove ambiguity; will leave expanded dups.
// Call after adding all CCLs.
- (void)cleanupDuplicates;

// returns a new array of vendor keys sorted alphabetically except for
// 'Other' which will be appended to the list (if there were flat CCLs)
// 'Other' should appear in a separate segment of the Vendor popup and
// be localized by the callers of copyVendorList.  Additionally, OS X
// contains a number of bundles with DeviceVendor = "Generic".  Generic
// should also be localized.
- (NSArray*)copyVendorList;

// returns a reference to a sorted (by model name) list of personalities for
// one of the 'copyVendorList' vendors.  dictionary keys from cclkeys.h.
- (NSArray*)getModelListForVendor:(NSString*)vendor;

// attempts to upgrade a pre-Leopard deviceConfiguration dictionary to have a 
// vendor, model, connection script, and personality if needed.
// Only needed if vendor/model missing from stored device configuration.
// If vendor/model are present, they are validated and the ConnectionScript
// updated or nil returned if there was no match.  Beware -setTypeFilter:.
- (NSMutableDictionary*)upgradeDeviceConfiguration:(NSDictionary*)deviceConf;

// merges personality data (e.g. preferred APN/CID) with provided
// SystemConfiguration device (e.g. modem) configuration dictionary.
// returns autoreleased NSMutableDictionary on success; NULL on failure.
// (The extra copy makes sure we only store what the user chooses or types.
// i.e. One personality's defaults don't end up in the wrong personality.)
- (NSMutableDictionary*)mergeCCLPersonality:(NSDictionary*)personality withDeviceConfiguration:(NSDictionary*)deviceConfiguration;


// empties the store, retaining any type filters
- (void)clearParser;

@end
