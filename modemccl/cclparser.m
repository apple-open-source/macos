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

// ****************************************************************************
//  cclparser.m
//  libccl
//
//  Created by kevine on 3/1/06.
//  Copyright 2006-7 Apple, Inc. All rights reserved.
// *****************************************************************************
 
// #define SC_SCHEMA_DECLARATION(k,q)     extern NSString * k;      // ??
#import "cclparser.h"


// names that would otherwise cause ambiguity are expanded
#define kPersonalityExpanded @"CCLNameExpanded"
#define kPathExpanded @"CCLPathExpanded"
// localized by BTSA and network preferences


@implementation CCLParser

+ (CCLParser*) createCCLParser
{
    CCLParser* retVal= [[CCLParser alloc] init];

    return retVal;
}

/******************************************************************************
* see cclparser.h for a description of the class variables
******************************************************************************/
- (id) init
{
    self = [super init];
    mBundleData = [[NSMutableDictionary alloc] init];
    mBundlesProcessed = [[NSMutableDictionary alloc] init];
	mFlatOverrides = [[NSMutableDictionary alloc] init];
	mTypeFilter = nil;

    return self;
}

// ****************************************************************************************************
- (void) dealloc
{
	[self setTypeFilter:nil];
	[mFlatOverrides release];
	[mBundlesProcessed release];
    [mBundleData release];
    [super dealloc];

    return;
}

- (void)finalize
{
    [super finalize];
}

- (void)setTypeFilter:(NSSet*)desiredConnectTypes
{
	[desiredConnectTypes retain];
	[mTypeFilter release];

	mTypeFilter = desiredConnectTypes;
}


// ****************************************************************************************************
- (NSMutableDictionary*)buildBaseModelDict:(NSDictionary*)matchEntry
        path:(NSString*)cclPath personality:(NSString*)personality
{
    NSMutableDictionary* rval = NULL;
    NSMutableDictionary* baseDict = [NSMutableDictionary dictionaryWithCapacity:5];
    id connectType= [matchEntry objectForKey: (id)kCCLConnectTypeKey];
    id cclVars= [matchEntry objectForKey: (id)kCCLParametersKey];
    id gprsCaps = [matchEntry objectForKey: (id)kCCLGPRSCapabilitiesKey];

    if (!baseDict ||
            ![connectType isKindOfClass: [NSString class]] ||
            ![cclVars isKindOfClass: [NSDictionary class]])
        goto finish;

    [baseDict setValue:cclPath forKey:(id)kSCPropNetModemConnectionScript];
    [baseDict setValue:personality forKey:(id)kSCPropNetModemConnectionPersonality];
    [baseDict setValue:connectType forKey:(id)kCCLConnectTypeKey];
    [baseDict setValue:cclVars forKey:(id)kCCLParametersKey];

    if ([connectType isEqualTo:(id)kCCLConnectGPRS]) {
        if (![gprsCaps isKindOfClass:[NSDictionary class]])
            goto finish;
        [baseDict setValue:gprsCaps forKey:(id)kCCLGPRSCapabilitiesKey];
    }

    rval = baseDict;

finish:
    return rval;
}
   
// ****************************************************************************************************
- (BOOL) parseMatchEntry:(NSDictionary*)matchEntry path:(NSString*)cclPath personality:(NSString*)personality mergeDict:(NSMutableDictionary*)mergeDict
{
    BOOL retVal= NO;
    NSMutableDictionary *firstModelDict = nil;
	NSArray *supersedesList;

	NSArray* deviceNameList= [matchEntry objectForKey: (id)kCCLDeviceNamesKey];
	if(deviceNameList!= NULL && [deviceNameList isKindOfClass: [NSArray class]]) {
		NSEnumerator* deviceEnum= [deviceNameList objectEnumerator];
		NSDictionary* curEntry= [deviceEnum nextObject];
		
		retVal= YES;
		while(curEntry!= NULL) {
			BOOL success = NO;

			if([curEntry isKindOfClass: [NSDictionary class]]) {
				NSString* venName= [curEntry objectForKey: (id)kCCLVendorKey];
				NSString* modName= [curEntry objectForKey: (id)kCCLModelKey];

				if([venName isKindOfClass: [NSString class]] &&
						[modName isKindOfClass: [NSString class]]) {
					NSMutableDictionary *modelDict;

					// Since each UI dict has its own model/vendor keys,
					// UIs need a separate dictionary for each pair
					modelDict= [self buildBaseModelDict:matchEntry
							path:cclPath personality:personality];
					[modelDict setObject: modName forKey:(id)kCCLModelKey];
					[modelDict setObject: venName forKey:(id)kCCLVendorKey];
						
					NSMutableArray* venList= [mergeDict objectForKey: venName];
					if(venList!= NULL) {
						[venList addObject: modelDict];
					} else {
						venList= [NSMutableArray arrayWithObject: modelDict];
						[mergeDict setObject: venList forKey: venName];
					}
					success = YES;

					// stash for overrides below
					if (!firstModelDict)
						firstModelDict = modelDict;
				}
			}
			retVal&= success;
			curEntry= [deviceEnum nextObject];
		}
	}

	// for the overrides, we use firstModelDict captured above
	if (firstModelDict) {
		// .ccl bundles implicitly supersede flat scripts of the same basename
		NSString *oldName = [cclPath lastPathComponent]; 
		if ([[oldName pathExtension] isEqual:(id)kCCLFileExtension])
			oldName = [oldName stringByDeletingPathExtension];
		// if a bundle has multiple personalities, implicit override -> first
		if (![mFlatOverrides objectForKey:oldName])
			[mFlatOverrides setObject:firstModelDict forKey:oldName];
		/* all personalities one foo.ccl will all implicitly supersede foo
		else
			NSLog(@"WARNING: %@/%@ also claims to supersede %@",
					cclPath, personality, oldName);
		*/

		// if this personality overrides anything else,
		// add one description to mFlatOverrides
		supersedesList = [matchEntry objectForKey:(id)kCCLSupersedesKey];
		if (supersedesList && [supersedesList isKindOfClass:[NSArray class]]) {
			NSEnumerator *e = [supersedesList objectEnumerator];
			id flatscript;

			while ((flatscript = [e nextObject])) {
				if ([flatscript isKindOfClass:[NSString class]])
					[mFlatOverrides setObject:firstModelDict forKey:flatscript];
				else
					NSLog(@"%@'s Supersedes list: unintelligible object %@",
							cclPath, flatscript);
			}
		}
	}

    return retVal;
} 

// ****************************************************************************************************
- (BOOL) mergePersonalityDict: (NSDictionary*) mergeDict
{
    NSEnumerator* mergeVenEnum = [mergeDict keyEnumerator];
    NSString* venName;
    
    // walk list of vendors in the merge dictionary,
    // adding to existing lists in mBundleData
    while((venName = [mergeVenEnum nextObject])) {
        NSArray* mergeModels = [mergeDict objectForKey:venName];
        NSMutableArray* existingModels = [mBundleData objectForKey:venName];

        if(existingModels)
            [existingModels addObjectsFromArray:mergeModels];
        else
            [mBundleData setObject:mergeModels forKey:venName];
    }
    return YES;
}

// ****************************************************************************
- (BOOL)processFlatCCL:(NSString*)path named:(NSString*)name
{
    NSMutableDictionary *modelDict;
    NSMutableArray *otherVendor;

    // someday we could validate or auto-name, but flat CCLs are fading
    // (mutable for "cleanupMatchingModels")
    modelDict = [NSMutableDictionary dictionaryWithObjectsAndKeys:
            (id)kCCLConnectDialup, (id)kCCLConnectTypeKey,
            name, (id)kCCLModelKey,
            (id)kCCLOtherVendorName, (id)kCCLVendorKey,
            path, (id)kSCPropNetModemConnectionScript,
        NULL];
    if (!modelDict)     return NO;

    // add to existing 'Other' vendor if it exists; else create
    otherVendor = [mBundleData objectForKey:(id)kCCLOtherVendorName];
    if (otherVendor) {
        [otherVendor addObject:modelDict];
    } else {
        if(!(otherVendor = [NSMutableArray arrayWithObject:modelDict]))
            return NO;
        [mBundleData setObject:otherVendor forKey:(id)kCCLOtherVendorName];
    }

    return YES;
}

// ****************************************************************************************************
- (BOOL) processCCLBundle:(NSString*)cclPath
{
    BOOL retVal= NO;
    NSDictionary* infoDict= [[NSBundle bundleWithPath: cclPath] infoDictionary];
	NSNumber *verNum;

	// check for verNum since broken plist doesn't lead to nULL infoDict
    if([infoDict isKindOfClass:[NSDictionary class]] &&
			(verNum = [infoDict objectForKey:(id)kCCLVersionKey]))
    {
		NSString *cfbundleID = [infoDict objectForKey:(id)kCFBundleIdentifierKey];
		NSString *opath;

		// ignore duplicate bundles
		// log if the duplication doesn't involve /S/L/Modem Scripts
		if ((opath = [mBundlesProcessed objectForKey:cfbundleID])) {
			if (!([opath hasPrefix:@"/System/Library"] ||
					[cclPath hasPrefix:@"/System/Library"]))
				NSLog(@"%@ appears to be a duplicate of %@ (id = %@); ignoring",
						cclPath, opath, cfbundleID);
			return YES;
		} else {
			// add the bundle ID
			[mBundlesProcessed setObject:cclPath forKey:(id)cfbundleID];
		}

        if([verNum isKindOfClass: [NSNumber class]] && [verNum isEqualToNumber:
                [NSNumber numberWithInt: kCCLBundleVersion]])
        { //Versions match, let the fun continue...
            NSDictionary* personalityList= [infoDict objectForKey: (id)kCCLPersonalitiesKey];
            if(personalityList!= NULL && [personalityList isKindOfClass: [NSDictionary class]])
            {
                NSMutableDictionary* mergeDict= [NSMutableDictionary dictionaryWithCapacity: [personalityList count]];
                NSEnumerator* personalityKeyEnum= [personalityList keyEnumerator];
                NSString* personalityKey= [personalityKeyEnum nextObject];
            
                retVal= YES;    
                while((personalityKey!= NULL) && retVal)
                {
                    NSDictionary* personalityEntry= [personalityList objectForKey: personalityKey];
                    if([personalityEntry isKindOfClass: [NSDictionary class]]) {
						BOOL interested;

						if (!mTypeFilter) {
							interested = YES;
						} else {
							NSString *connectType = [personalityEntry objectForKey:(id)kCCLConnectTypeKey];
							// see if this personality's type matches
							interested = [mTypeFilter containsObject:connectType];
						}
							
						if (interested)
							retVal= [self parseMatchEntry: personalityEntry path: cclPath personality: personalityKey mergeDict: mergeDict];
					}
                    personalityKey= [personalityKeyEnum nextObject];
                }

                if(retVal)
                    retVal= [self mergePersonalityDict: mergeDict];
            } else
                NSLog(@"skipping %@: trouble extracting personalities",cclPath);
        } else
            NSLog(@"skipping %@: incompatible CCL version number: %@",
                    cclPath, verNum);
    } else
        NSLog(@"skipping %@: malformed bundle dictionary", cclPath);

    return retVal;
}

// ****************************************************************************************************
- (BOOL) processFolder:(NSString*)folderPath
{
    BOOL retVal= YES;
    NSFileManager* fileMan=  [NSFileManager defaultManager];
    NSDirectoryEnumerator* folderEnum= [fileMan enumeratorAtPath:folderPath];
    NSString *curFileName, *displayName;

    while((curFileName = [folderEnum nextObject])) {
        NSString* filePath;
        BOOL isDir = NO, exists;

		filePath = [folderPath stringByAppendingPathComponent:curFileName];
		if (![fileMan fileExistsAtPath:filePath isDirectory:&isDir]) {
			NSLog(@"Warning: %@ doesn't seem to exist", filePath);
			continue;
		}

		// if it's a new-fangled .ccl bundle, process appropriately
		if (isDir) {
			if ([[curFileName pathExtension] isEqualToString:(id)kCCLFileExtension]){
				[folderEnum skipDescendents];   // don't descend into bundle
				retVal&= [self processCCLBundle: filePath];
			} else {
				// ignore .directories
				if ([[filePath lastPathComponent] hasPrefix:@"."])
					[folderEnum skipDescendents];
			}
        } else {
			// (note: we always process flat CCLs b/c we don't know their type)
			// handle as a flat file
			CFURLRef url = (CFURLRef)[NSURL fileURLWithPath:filePath];
			LSItemInfoRecord info;
			OSStatus errn;

			// 4152940 requests invisibility info in NSFileManager
			errn = LSCopyItemInfoForURL(url, kLSRequestBasicFlagsOnly, &info);
			if (errn) {
				NSLog(@"skipping %@: error %d getting item info",filePath,errn);
				continue;
			}

			// must be visible and flat
			if ((info.flags & (kLSItemInfoIsPlainFile | kLSItemInfoIsSymlink))
					&& !(info.flags & kLSItemInfoIsInvisible)) {
				// use display name of file
				displayName = [fileMan displayNameAtPath:filePath];
				retVal&= [self processFlatCCL:filePath named:displayName];
            }
        }
    }

    return retVal;
}


// ****************************************************************************************************
- (void) cleanupNameWithPersonality: (NSMutableDictionary*) modelDict
{
    if([modelDict objectForKey: kPersonalityExpanded]== NULL)
    {
        NSString* modelName= [modelDict objectForKey: (id)kCCLModelKey];
        NSString* modelPersonality= [modelDict objectForKey: (id)kSCPropNetModemConnectionPersonality];

	if (modelName && modelPersonality) {
	    NSString* newModelName= [NSString stringWithFormat: @"%@, %@", modelName, modelPersonality];
	    [modelDict setObject: newModelName forKey: (id)kCCLModelKey];
	    [modelDict setObject: [NSNull null] forKey: kPersonalityExpanded];
	}
    }
}

// ****************************************************************************************************
- (void) cleanupNameWithCCLName: (NSMutableDictionary*) modelDict
{
    if([modelDict objectForKey: kPathExpanded]== NULL)
    {
        NSString *modelName = [modelDict objectForKey: (id)kCCLModelKey];
	NSString  *cScript = [modelDict objectForKey:(id)kSCPropNetModemConnectionScript];
        NSString *cclName = [cScript lastPathComponent];
	if ([modelName isEqual:cclName]) {
	    NSLog(@"a second copy of %@ is installed at %@?", cclName, cScript);
	    cclName = cScript;
	}

	if (![modelName isEqual:cclName]) {
	    NSString* newModelName= [NSString stringWithFormat: @"%@, %@", modelName, cclName];
	    [modelDict setObject: newModelName forKey: (id)kCCLModelKey];
	    [modelDict setObject: [NSNull null] forKey: kPathExpanded];
	}
    }
}

// ****************************************************************************************************
- (bool) cleanupMatchingModels: (NSDictionary*) curModelDict modelArray: (NSArray*) modelArray changeSEL: (SEL) selector
{
    bool retVal= NO;
    NSMutableArray* matchList= [NSMutableArray array];
    NSString* matchModelName= [curModelDict objectForKey: (id)kCCLModelKey];
    NSEnumerator* modelEnum= [modelArray objectEnumerator];
    NSDictionary* curDict= [modelEnum nextObject];
    while(curDict!= NULL)
    {
        if(curDict!= curModelDict)
        {
            NSString* curModelName= [curDict objectForKey: (id)kCCLModelKey];
            if([matchModelName isEqualToString: curModelName])
            {
                [matchList addObject: curDict];
            }
            
        }
        curDict= [modelEnum nextObject];
    }
    if([matchList count]>0)
    {
        [matchList addObject: curModelDict];
        NSEnumerator* modelEnum= [matchList objectEnumerator];
        NSMutableDictionary* curDict= [modelEnum nextObject];
        while(curDict!= NULL)
        {
            [self performSelector: selector withObject: curDict];
            curDict= [modelEnum nextObject];
        }
        retVal= YES;
    }
    return retVal;
}

// ****************************************************************************************************
- (void)cleanupDuplicates
{
    NSEnumerator	*keyEnum;
    NSString		*curVenName, *modName;
	NSMutableArray	*persList;
	unsigned		persCount, i;

	// first remove overridden flat CCLs (message -> nil == no-op)
	// remove from mBundleData/Other any names in mFlatOverrides
	persList = [mBundleData objectForKey:kCCLOtherVendorName];
	persCount = [persList count];
	for (i = 0; i < persCount; i++) {
		modName = [[persList objectAtIndex:i] objectForKey:(id)kCCLModelKey];
		if ([mFlatOverrides objectForKey:modName]) {
			[persList removeObjectAtIndex:i];
			i--; persCount--;		// since everything slid
		}
	}
	// and if nothing's left, remove the Other vendor entirely
	if (persList && [persList count] == 0)
		[mBundleData removeObjectForKey:kCCLOtherVendorName];

	// check each model list against itself (XX use NSOrderedSet?)
	keyEnum = [mBundleData keyEnumerator];
    while(curVenName= [keyEnum nextObject]) {
        NSArray* modelArray= [mBundleData objectForKey: curVenName];
        NSEnumerator* modelEnum= [modelArray objectEnumerator];
        NSMutableDictionary* curModelDict;
        while((curModelDict = [modelEnum nextObject]))
            if([self cleanupMatchingModels: curModelDict modelArray: modelArray changeSEL: @selector(cleanupNameWithCCLName:)])
                [self cleanupMatchingModels: curModelDict modelArray: modelArray changeSEL: @selector(cleanupNameWithPersonality:)];
    }
}

// ****************************************************************************************************
- (NSArray*)copyVendorList
{
    NSMutableArray *vendors;

    // get mutable copy of all vendors
    vendors = [[mBundleData allKeys] mutableCopy];

    // remove 'Other'; sort; and append
    [vendors removeObject:(id)kCCLOtherVendorName];
    [vendors sortUsingSelector:@selector(caseInsensitiveCompare:)];
	if ([mBundleData objectForKey:(id)kCCLOtherVendorName])
		[vendors addObject:(id)kCCLOtherVendorName];

    return vendors;
}

// ****************************************************************************************************
- (NSArray*)getModelListForVendor: (NSString*) vendor
{
	NSMutableArray *models = [mBundleData objectForKey:vendor];
	NSSortDescriptor* sortd = [[NSSortDescriptor alloc]
			initWithKey:(id)kCCLModelKey ascending:TRUE 
			selector:@selector(caseInsensitiveCompare:)];

	// might have sorted it before, but this doesn't happen often and
	// the sort should be fast if already sorted
	[models sortUsingDescriptors:[NSArray arrayWithObject:sortd]];
	[sortd release];

    return models;
}

// ****************************************************************************************************
- (void) clearParser
{
    [mBundleData removeAllObjects];
	[mBundlesProcessed removeAllObjects];
	[mFlatOverrides removeAllObjects];
}

/*******************************************************************************
* mergeCCLPersonality:withDeviceConfiguration: takes a "modem" dict that would
* have come from SystemConfiguration (e.g. containing phone number and such)
* and sets various keys in a new dictionary based on data in the personality.
* Some key/value pairs are just copied (connection script and personality
* within the script bundle) or removed as appropriate, but GPRS APN and CID
* are expressed as "preferred" (aka "default") in the personality dict while
* they are hard values in the device dictionary.  If the user has already
* chosen APN or CID for this personality, we don't copy over the preferred
* values.  But if the user is choosing the personality for the first time,
* we do copy the values over.  The UI can then use them to populate the UI.
*
* We return a new NSMutableDictionary so that the UI can call us repeatedly
* with different personalities and the the same deviceConfiguration
* dictionary (from SysConfig) without any defaults from one personality
* polluting the UI when the user chooses another personality that perhaps
* doesn't have any defaults.  Only when the user saves the configuration
* should we ever see any of the things we inserted feed back to us.
*******************************************************************************/

- (NSMutableDictionary*)mergeCCLPersonality:(NSDictionary*)personality withDeviceConfiguration:(NSDictionary*)deviceConfiguration;
{
    NSMutableDictionary *rval = [deviceConfiguration mutableCopy];
    NSString *mScript, *pScript, *mPers;
    NSString *pName, *pType, *pVendor;
    BOOL newPersonality;

    // see whether the user changed personalities
    mScript = [rval objectForKey:(id)kSCPropNetModemConnectionScript];
    pScript = [personality objectForKey:(id)kSCPropNetModemConnectionScript];
    mPers = [rval objectForKey:(id)kSCPropNetModemConnectionPersonality];
    pName = [personality objectForKey:(id)kSCPropNetModemConnectionPersonality];
    newPersonality = (![pScript isEqual:mScript] || ![pName isEqual:mPers]);
    // XX any other checking of current modem dictionary needed?

    // copy vendor, model, script, personality (only model+script for flat CCL)
    pVendor = [personality objectForKey:(id)kCCLVendorKey];
    if (pVendor)
        [rval setObject:pVendor forKey:(id)kCCLVendorKey];
    else
        [rval removeObjectForKey:(id)kCCLVendorKey];
    [rval setObject:[personality objectForKey:(id)kCCLModelKey]
            forKey:(id)kCCLModelKey];
    [rval setObject:pScript forKey:(id)kSCPropNetModemConnectionScript];
    if (pName)
        [rval setObject:pName forKey:(id)kSCPropNetModemConnectionPersonality];
    else
        [rval removeObjectForKey:(id)kSCPropNetModemConnectionPersonality];

    // check to see if APN or CID need to be populated / updated
    // - if GPRS and not set; set to preferred
    // - see below for Preferred CID logic
    pType = [personality objectForKey:(id)kCCLConnectTypeKey];
    if ([pType isEqual:(id)kCCLConnectGPRS]) {
        NSDictionary *cclParms =
                [personality objectForKey:(id)kCCLParametersKey];
        NSString *savedAPN =
                [rval objectForKey:(id)kSCPropNetModemAccessPointName];
        NSString *preferredAPN =
                [cclParms objectForKey:(id)kCCLPreferredAPNKey];
        NSString *savedCID =
                [rval objectForKey:(id)kSCPropNetModemDeviceContextID];
        NSNumber *preferredCID =
                [cclParms objectForKey:(id)kCCLPreferredCIDKey];
        BOOL safeCIDs = [[[personality objectForKey:(id)kCCLGPRSCapabilitiesKey]
                            objectForKey:(id)kCCLIndependentCIDs] boolValue];
        // if !safeCIDs, perhaps we could set the preferred to max[- 1?]?
        // (we already insert values)

        // A saved CID should be overwritten if changing personalities and
        // the new personality isn't known to be safe for the old CID
        // or if there is a new preferred CID for the new personality.
        // If there is no preferred CID, the old CID should be removed if
        // CIDs are unsafe.  In other words, leave a CID alone only if
        // - it was already selected for this personality
        // - it is known to be safe for this personality && no preferred
        // unset, no preferred -> leave alone
        // unset, preferred -> set to preferred
        // set, !new personality -> leave alone
        // set, new personality, preferred -> set to preferred
        // set, new, !preferred, safe -> leave alone
        // set, new, !preferred, unsafe -> clear
        if (!savedCID || newPersonality) {
            if (preferredCID) {
                [rval setObject:[preferredCID stringValue]
                        forKey:(id)kSCPropNetModemDeviceContextID];
            } else if (savedCID && !safeCIDs) {
                [rval removeObjectForKey:(id)kSCPropNetModemDeviceContextID];
            }
        }
        if (preferredAPN && (!savedAPN || newPersonality))
            [rval setObject:preferredAPN forKey:(id)kSCPropNetModemAccessPointName];
    } else {
        // clear any existing APN/CID
        [rval removeObjectForKey:(id)kSCPropNetModemAccessPointName];
        [rval removeObjectForKey:(id)kSCPropNetModemDeviceContextID];
    }

    return rval;
}

/*******************************************************************************
* -upgradeDeviceConfiguration takes a system configuration modem dictionary
* that doesn't have a device/vendor pair and adds one.  It may or may not
* do smart things like use keys in the CCL bundles to tell it which
* personalities are equivalent to old flat scripts.  :)
*******************************************************************************/
- (NSMutableDictionary*)upgradeDeviceConfiguration:(NSDictionary*)deviceConf
{
    NSMutableDictionary *rval = nil;
    NSString *vendor, *model, *cScript, *csName, *persName = nil;
    NSArray *persList;
	NSEnumerator *e;
	NSDictionary *pers;
	BOOL knownModel = NO;


    // use the kSCProp constants since these are SC dicts
    csName = [[deviceConf objectForKey:(id)kSCPropNetModemConnectionScript] lastPathComponent];

    // let's see if there's anything here (we'll validate below)
    vendor = [deviceConf objectForKey:(id)kSCPropNetModemDeviceVendor];
    model = [deviceConf objectForKey:(id)kSCPropNetModemDeviceModel];

	if (model && vendor) {
		persList = [mBundleData objectForKey:vendor];
		e = [persList objectEnumerator];
		while ((pers = [e nextObject])) {
			if ([[pers objectForKey:(id)kCCLModelKey] isEqual:model]) {
				knownModel = YES;
				break;
			}
		}
	} else {
        // maybe overridden (implicitly or otherwise)
        if ((pers = [mFlatOverrides objectForKey:csName])) {
            vendor = [pers objectForKey:(id)kCCLVendorKey];
            model = [pers objectForKey:(id)kCCLModelKey];
            persName = [pers objectForKey:(id)kSCPropNetModemConnectionPersonality];
			knownModel = YES;
        } else {
            // fall back to trying "other"/<connectionscript>
            vendor = kCCLOtherVendorName;
			if ([[csName pathExtension] isEqual:(id)kCCLFileExtension])
				csName = [csName stringByDeletingPathExtension];
            model = csName;

			// validate with 'other' vendor list
			persList = [mBundleData objectForKey:vendor];
			e = [persList objectEnumerator];
			while ((pers = [e nextObject])) {
				if ([[pers objectForKey:(id)kCCLModelKey] isEqual:model]) {
					knownModel = YES;
					break;
				}
			}
        }
    }

	if (knownModel) {
        // yay; go ahead and create result dictionary
        rval = [deviceConf mutableCopy];
        [rval setObject:vendor forKey:(id)kSCPropNetModemDeviceVendor];
        [rval setObject:model forKey:(id)kSCPropNetModemDeviceModel];
		cScript = [pers objectForKey:(id)kSCPropNetModemConnectionScript];
		if (cScript)
			[rval setObject:cScript forKey:(id)kSCPropNetModemConnectionScript];
        if (persName)
            [rval setObject:persName forKey:(id)kSCPropNetModemConnectionPersonality];
    }

    return rval;        // still nil if the vendor/model didn't work out
}

@end
