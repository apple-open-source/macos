/*
 * Copyright (c) 2009-2010,2012-2015 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/Security.h>
#import <Security/SecInternalReleasePriv.h>

#import "keychain/SecureObjectSync/CKBridge/SOSCloudKeychainClient.h"

#import <dispatch/dispatch.h>

#import <utilities/debugging.h>
#import <utilities/SecCFWrappers.h>

#import "keychain/SecureObjectSync/SOSInternal.h"
#import <Security/CKKSControlProtocol.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>

#include "secToolFileIO.h"
#include "accountCirclesViewsPrint.h"
#import "CKKSControlProtocol.h"
#import "SecItemPriv.h"
#import "supdProtocol.h"

#include <stdio.h>
#import <sqlite3.h>


@interface NSString (FileOutput)
- (void) writeToStdOut;
- (void) writeToStdErr;
@end

@implementation NSString (FileOutput)

- (void) writeToStdOut {
    fputs([self UTF8String], stdout);
}
- (void) writeToStdErr {
    fputs([self UTF8String], stderr);
}

@end

@interface NSData (Hexinization)

- (NSString*) asHexString;

@end

@implementation NSData (Hexinization)

- (NSString*) asHexString {
    return (__bridge_transfer NSString*) CFDataCopyHexString((__bridge CFDataRef)self);
}

@end

static NSString *dictionaryToString(NSDictionary *dict) {
    NSMutableString *result = [NSMutableString stringWithCapacity:0];

    NSArray* keys = [[dict allKeys] sortedArrayUsingSelector:@selector(compare:)];
    for(NSString* key in keys) {
        [result appendFormat:@"%@=%@,", key, dict[key]];
    }
    return [result substringToIndex:result.length-(result.length>0)];
}

@implementation NSDictionary (OneLiner)

- (NSString*) asOneLineString {
    return dictionaryToString(self);
}

@end

@interface SQLiteManager : NSObject
+ (NSArray<NSArray*>*)executeQuery:(NSString *)query onDatabaseAtPath:(NSString *)dbPath;

@end

@implementation SQLiteManager

+ (NSArray<NSArray*>*)executeQuery:(NSString *)query onDatabaseAtPath:(NSString *)dbPath {
    // Validate input parameters
      if (query == NULL) {
          [[NSString stringWithFormat:@"\nError: SQL query should not be null \n"] writeToStdErr];
          return NULL;
      }
      
      if (dbPath == NULL) {
          [[NSString stringWithFormat:@"\nError: DB Path should not be null \n"] writeToStdErr];
          return NULL;
      }
      
    sqlite3 *database;
    sqlite3_stmt *statement;
    
    NSMutableArray *rows = [NSMutableArray array];
    // Open database
    if (sqlite3_open_v2([dbPath UTF8String], &database, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK) {

        // Prepare the query
        if (sqlite3_prepare_v2(database, [query UTF8String], -1, &statement, NULL) == SQLITE_OK) {
            
            // Get column count
            int columnCount = sqlite3_column_count(statement);
            
            NSMutableArray *columnNames = [NSMutableArray arrayWithCapacity:columnCount];
            // get column names
            for (int i = 0; i < columnCount; i++) {
                const char *name = sqlite3_column_name(statement, i);
                if (name) {
                    [columnNames addObject:[NSString stringWithUTF8String:name]];
                } else {
                    [columnNames addObject:[NSNull null]];
                }
            }
            // Add column names
            [rows addObject:columnNames];
        
            // Execute the query and get results
            while (sqlite3_step(statement) == SQLITE_ROW) {
                NSMutableArray *outputRow = [NSMutableArray arrayWithCapacity:columnCount];
                for (int i = 0; i < columnCount; i++) {
                    const char *value = (const char *)sqlite3_column_text(statement, i);
                    if (value) {
                        [outputRow addObject:[NSString stringWithUTF8String:value]];
                        
                    } else {
                        [outputRow addObject:[NSNull null]];
                    }
                }
                [rows addObject:outputRow];
            }

            // Finalize statement
            sqlite3_finalize(statement);
        } else {
            [[NSString stringWithFormat:@"\nError: Failed to prepare statement %s \n", sqlite3_errmsg(database) ] writeToStdErr];
        }

        // Close database
        sqlite3_close(database);
    } else {
        [[NSString stringWithFormat:@"\nError: Failed to open SQL DB %s \n", sqlite3_errmsg(database)] writeToStdErr];
    }
    
    return rows;
}
@end


static void
circle_sysdiagnose(void)
{
    SOSLogSetOutputTo(NULL,NULL);
    SOSCCDumpCircleInformation();
}

static void
engine_sysdiagnose(void)
{
    SOSCCDumpEngineInformation();
}

/*
    Here are the commands to dump out all keychain entries used by HomeKit:
        security item class=genp,sync=1,agrp=com.apple.hap.pairing;
        security item class=genp,sync=0,agrp=com.apple.hap.pairing;
        security item class=genp,sync=0,agrp=com.apple.hap.metadata
*/

static void printSecItems(NSString *subsystem, CFTypeRef result) {
    if (result) {
        if (CFGetTypeID(result) == CFArrayGetTypeID()) {
            NSArray *items = (__bridge NSArray *)(result);

            // Stringify all items, then sort them before printing
            NSMutableArray<NSString*>* itemStrings = [NSMutableArray array];
            NSObject *item;
            for (item in items) {
                if ([item respondsToSelector:@selector(asOneLineString)]) {
                    [itemStrings addObject:[NSString stringWithFormat: @"%@: %@\n", subsystem, [(NSMutableDictionary *)item asOneLineString]]];
                }
            }
            [itemStrings sortUsingSelector:@selector(compare:)];
            for(NSString* str in itemStrings) {
                [str writeToStdOut];
            }
        } else {
            NSObject *item = (__bridge NSObject *)(result);
            if ([item respondsToSelector:@selector(asOneLineString)]) {
                [[NSString stringWithFormat: @"%@: %@\n", subsystem, [(NSMutableDictionary *)item asOneLineString]] writeToStdOut];
            }
        }
    }
}

static void
homekit_sysdiagnose(void)
{
    NSString *kAccessGroupHapPairing  = @"com.apple.hap.pairing";
    NSString *kAccessGroupHapMetadata = @"com.apple.hap.metadata";

    [@"HomeKit keychain state:\n" writeToStdOut];

    // First look for syncable hap.pairing items
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : kAccessGroupHapPairing,
        (id)kSecAttrSynchronizable: (id)kCFBooleanTrue,
        (id)kSecMatchLimit : (id)kSecMatchLimitAll,
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @NO,
        (id)kSecUseDataProtectionKeychain : @YES,
    } mutableCopy];

    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &result);
    if (status == noErr) {
        printSecItems(@"HomeKit", result);
    }
    CFReleaseNull(result);

    // Now look for non-syncable hap.pairing items
    query[(id)kSecAttrSynchronizable] = @NO;
    status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &result);
    if (status == noErr) {
        printSecItems(@"HomeKit", result);
    }
    CFReleaseNull(result);

    // Finally look for non-syncable hap.metadata items
    query[(id)kSecAttrAccessGroup] = kAccessGroupHapMetadata;
    status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &result);
    if (status == noErr) {
        printSecItems(@"HomeKit", result);
    }
    CFReleaseNull(result);
}

static void
unlock_sysdiagnose(void)
{
    NSString *kAccessGroupAutoUnlock  = @"com.apple.continuity.unlock";

    [@"AutoUnlock keychain state:\n" writeToStdOut];

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : kAccessGroupAutoUnlock,
        (id)kSecAttrAccount : @"com.apple.continuity.auto-unlock.sync",
        (id)kSecAttrSynchronizable: (id)kCFBooleanTrue,
        (id)kSecMatchLimit : (id)kSecMatchLimitAll,
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @NO,
    };

    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &result);
    if (status == noErr) {
        printSecItems(@"AutoUnlock", result);
    }
    CFReleaseNull(result);
}

static void
rapport_sysdiagnose(void)
{
    NSString *kAccessGroupRapport  = @"com.apple.rapport";

    [@"Rapport keychain state:\n" writeToStdOut];

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : kAccessGroupRapport,
        (id)kSecAttrSynchronizable: (id)kCFBooleanTrue,
        (id)kSecMatchLimit : (id)kSecMatchLimitAll,
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @NO,
    };

    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &result);
    if (status == noErr) {
        printSecItems(@"rapport", result);
    }
    CFReleaseNull(result);
}

static void
notes_sysdiagnose(void)
{
    NSString *kAccessGroupNotes  = @"group.com.apple.notes";

    [@"Notes keychain state:\n" writeToStdOut];

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : kAccessGroupNotes,
        (id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
        (id)kSecMatchLimit : (id)kSecMatchLimitAll,
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @NO,
    };

    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &result);
    if (status == noErr) {
        // Unfortunately, Notes saves the account's DSID (or the string "local") under labl.
        // Since we don't want to log the DSID to the sysdiagnose, redact it (on external builds).
        NSMutableArray* cleanedItems = [NSMutableArray array];
        NSMutableDictionary* dsids = [NSMutableDictionary dictionary];
        uint64_t redactions = 1;

        for(NSDictionary* item in (__bridge NSDictionary*)result) {
            NSMutableDictionary* mutableItem = [item mutableCopy];

            NSString* labl = mutableItem[(id)kSecAttrLabel];
            if(!SecIsInternalRelease() && labl != nil && ![labl isEqual:@"local"]) {
                // redact!
                NSString* existing = dsids[labl];
                if(!existing) {
                    existing = [NSString stringWithFormat:@"<REDACTED-LABL-%llu>", redactions];
                    redactions += 1;
                    dsids[labl] = existing;
                }
                mutableItem[(id)kSecAttrLabel] = existing;

            } else {
                // don't change mutableItem
            }

            [cleanedItems addObject:mutableItem];
        }

        printSecItems(@"notes", (__bridge CFTypeRef)cleanedItems);
    }
    CFReleaseNull(result);
}

static void
analytics_sysdiagnose(void)
{
    NSXPCConnection* xpcConnection = [[NSXPCConnection alloc] initWithMachServiceName:@"com.apple.securityuploadd" options:0];
    if (!xpcConnection) {
        [@"failed to setup xpc connection for securityuploadd\n" writeToStdErr];
        return;
    }
    xpcConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(supdProtocol)];
    [xpcConnection resume];
    
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    [[xpcConnection remoteObjectProxyWithErrorHandler:^(NSError* rpcError) {
        [[NSString stringWithFormat:@"Error talking with daemon: %@\n", rpcError] writeToStdErr];
        dispatch_semaphore_signal(semaphore);
    }] getSysdiagnoseDumpWithReply:^(NSString* sysdiagnose) {
        if (sysdiagnose) {
            [[NSString stringWithFormat:@"\nAnalytics sysdiagnose:\n\n%@\n", sysdiagnose] writeToStdOut];
        }
        dispatch_semaphore_signal(semaphore);
    }];
    
    if (dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        [@"\n\nError: timed out waiting for response\n" writeToStdErr];
    }
}

static void
kvs_sysdiagnose(void) {
    SOSLogSetOutputTo(NULL,NULL);
    SOSCCDumpCircleKVSInformation(NULL);
}

static bool needs_agrp_redacting (NSString* agrp) {
    // regular expression pattern for sensitive agrp i.e third party apps. However, some of 2nd party apps also follow this thirdParty pattern but we can consider them non-sensitive
    NSString *thirdPartyApps = @"^[0-9A-Z]{10}\\.";
    NSString *secondPartyApps = @"\\b(iWork|freeform|Xcode)\\b";
    NSError *error = nil;
    NSRegularExpression *thirdPartyregex = [NSRegularExpression regularExpressionWithPattern:thirdPartyApps options:NSRegularExpressionCaseInsensitive error:&error];
    if (error) {
        [[NSString stringWithFormat:@"\nError: %@ while creating second party regex \n", error.localizedDescription] writeToStdErr];
        return false;
    } else {
        // Match the thirdParty regex pattern against the agrp
        NSRange range = NSMakeRange(0, agrp.length);
        NSTextCheckingResult *thirdPartyMatch = [thirdPartyregex firstMatchInString:agrp options:0 range:range];
        if (thirdPartyMatch) {
            // Check if it in 2nd Party apps; if yes we should return it as non-sensitive
            NSError *innerError = nil;
            NSRegularExpression * secondPartyRegex = [NSRegularExpression regularExpressionWithPattern:secondPartyApps options:NSRegularExpressionCaseInsensitive error:&innerError];
            if (innerError) {
                [[NSString stringWithFormat:@"\nError: %@ while creating second party regex \n", innerError.localizedDescription] writeToStdErr];
                return false;
            }
            NSTextCheckingResult *secondPartyMatch = [secondPartyRegex firstMatchInString:agrp options:0 range:range];
            if (secondPartyMatch) {
                return false;
            }
            return true;
        } else {
            return false;
        }
    }
}

static void agrp_item_count_sysdiagnose(void) {
    [[NSString stringWithFormat:@"\n Keychain <access Group, #items> Information \n"] writeToStdOut];
    CFErrorRef error = NULL;
    NSString* dbPath = CFBridgingRelease(SecKeychainCopyDatabasePath(&error));
    if (error != NULL || dbPath == NULL) {
        NSError *err = (NSError *)CFBridgingRelease(error);
        [[NSString stringWithFormat:@"\nError: Failed to get Keychain DB Path %@ \n", err] writeToStdErr];
        return;
    }
    NSArray *itemClasses = @[@"inet", @"genp", @"keys", @"cert"];
    // Threshold to filter access group with total item count
    NSInteger threshold = 15;
    // go over item classes and print the stats
    for (NSString *itemClass in itemClasses) {
        [[NSString stringWithFormat:@"\n -----------------------------------------------------------------\n"] writeToStdOut];
        [[NSString stringWithFormat:@"\n Keychain <access Group, (#non-tombstone, #tombstone)> information for %@ item Class\n", itemClass] writeToStdOut];
        [[NSString stringWithFormat:@"\n -----------------------------------------------------------------\n"] writeToStdOut];
        NSString *sqlQuery = [NSString stringWithFormat:@"SELECT agrp,\
                                 SUM(CASE WHEN tomb = 0 THEN 1 ELSE 0 END) AS count_tomb_0, \
                                 SUM(CASE WHEN tomb = 1 THEN 1 ELSE 0 END) AS count_tomb_1 \
                                 FROM %@ \
                                 GROUP BY agrp \
                                 ORDER BY (SUM(CASE WHEN tomb = 0 THEN 1 ELSE 0 END) + SUM(CASE WHEN tomb = 1 THEN 1 ELSE 0 END)) DESC", itemClass];
        NSArray<NSArray*>* sqlResult = [SQLiteManager executeQuery:sqlQuery onDatabaseAtPath:dbPath];
        // Go through the access groups and do redacting if necessary
        NSUInteger redactCount = 1;
        NSUInteger totalNonTombCount = 0;
        NSUInteger totalTombCount = 0;
        for (uint64_t i= 0; i < sqlResult.count; i++) {
            // Column Names
            if (i==0) {
                [[NSString stringWithFormat:@"\n%@\n",[sqlResult[0] componentsJoinedByString:@", "]] writeToStdOut];
                continue;
            }
            
            // calculate total count
            if (sqlResult[i].count>=3) {
                totalNonTombCount += [sqlResult[i][1] intValue];
                totalTombCount += [sqlResult[i][2] intValue];
            }
            
            // print only if item count meets threshold
            if(sqlResult[i].count>=3 && ([sqlResult[i][1] intValue] + [sqlResult[i][2] intValue]) >= threshold) {
                // redact agrp if necessary
                NSString *agrp = sqlResult[i].firstObject;
                if (needs_agrp_redacting(agrp)) {
                    NSString *redactedAccessGroup = [NSString stringWithFormat:@"<REDACTED-AGRP-%lu>", redactCount];
                    NSRange range = NSMakeRange(1, sqlResult[i].count-1);
                    NSArray *subArray = [sqlResult[i] subarrayWithRange:range];
                    [[NSString stringWithFormat:@"\n%@, %@\n", redactedAccessGroup, [subArray componentsJoinedByString:@", "]] writeToStdOut];
                    redactCount += 1;
                } else {
                    [[NSString stringWithFormat:@"\n%@\n",[sqlResult[i] componentsJoinedByString:@", "]] writeToStdOut];
                }
            }
        }
        [[NSString stringWithFormat:@"\n (Total agrps: %lu, Total Non-tombstone items: %lu, Total tombstone items: %lu)\n", (sqlResult.count>0 ? sqlResult.count-1 : 0), totalNonTombCount, totalTombCount] writeToStdOut];
        [[NSString stringWithFormat:@"\n -----------------------------------------------------------------\n"] writeToStdOut];
    }
}

static void print_sql_result(NSArray<NSArray*>* sqlResult) {
    for (uint64_t i = 0; i < sqlResult.count; i++) {
        [[NSString stringWithFormat:@"\n%@\n",[sqlResult[i] componentsJoinedByString:@", "]] writeToStdOut];
    }
}

static void db_table_sizes(void) {
    [[NSString stringWithFormat:@"\n -----------------------------------------------------------------\n"] writeToStdOut];
    [[NSString stringWithFormat:@"\n Keychain Database Table Size Information \n"] writeToStdOut];
    [[NSString stringWithFormat:@"\n -----------------------------------------------------------------\n"] writeToStdOut];
    CFErrorRef error = NULL;
    NSString* dbPath = CFBridgingRelease(SecKeychainCopyDatabasePath(&error));
    if (error != NULL || dbPath == NULL) {
        NSError *err = (NSError *)CFBridgingRelease(error);
        [[NSString stringWithFormat:@"\nError: Failed to get Keychain DB Path %@ \n", err] writeToStdErr];
        return;
    }
    NSString *sqlQuery = @"SELECT name, CASE \
                           WHEN sum(pgsize) >= 1073741824 THEN ROUND(sum(pgsize)/1073741824.0, 2) || ' GB' \
                           WHEN sum(pgsize) >= 1048576 THEN ROUND(sum(pgsize)/1048576.0, 2) || ' MB' \
                           WHEN sum(pgsize) >= 1024 THEN ROUND(sum(pgsize)/1024.0, 2) || ' KB' \
                           ELSE sum(pgsize) || ' bytes' \
                       END AS size \
                       FROM dbstat \
                       GROUP BY name \
                       ORDER BY sum(pgsize) DESC";
    NSArray<NSArray*>* sqlResult = [SQLiteManager executeQuery:sqlQuery onDatabaseAtPath:dbPath];
    print_sql_result(sqlResult);

    [[NSString stringWithFormat:@"\n -----------------------------------------------------------------\n"] writeToStdOut];
}

int main(int argc, const char ** argv)
{
    @autoreleasepool {
        printf("sysdiagnose keychain\n");

        circle_sysdiagnose();
        engine_sysdiagnose();
        homekit_sysdiagnose();
        unlock_sysdiagnose();
        rapport_sysdiagnose();
        notes_sysdiagnose();
        analytics_sysdiagnose();
        agrp_item_count_sysdiagnose();
        db_table_sizes();
        
        // Keep this one last
        kvs_sysdiagnose();
    }
    return 0;
}
