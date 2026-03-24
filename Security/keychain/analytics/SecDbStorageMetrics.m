/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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

#include <stdlib.h>
#include <errno.h>
#import "utilities/debugging.h"
#include <sys/stat.h>
#include "SecDbStorageMetrics.h"
#include "keychain/securityd/SecItemServer.h"
#import  "utilities/SecCoreAnalytics.h"
#include "utilities/SecDb.h"
#include "keychain/securityd/SecDbItem.h"
#include "keychain/securityd/SecDbQuery.h"
#include "keychain/securityd/SecItemSchema.h"


NSDictionary * _Nullable SecDbStatsGetAccessGroupItemCounts(void) {
    // Array of keychain classes to query
    const SecDbClass* classes[] = {genp_class(), inet_class(), cert_class(), keys_class()};
    size_t classCount = sizeof(classes) / sizeof(classes[0]);
    NSMutableDictionary *allTableCounts = [[NSMutableDictionary alloc] init];

    for (size_t i = 0; i < classCount; i++) {
        const SecDbClass *dbClass = classes[i];
        NSString *tableName = (__bridge NSString *)dbClass->name;

        __block NSMutableDictionary *agrpCounts = [[NSMutableDictionary alloc] init];
        __block CFErrorRef error = NULL;

        bool ok = kc_with_dbt(false, NULL, &error, ^bool(SecDbConnectionRef dbt) {
            // SQL query to get count of regular items and tombstone items per access group
            NSString *sqlString = [NSString stringWithFormat:@"SELECT agrp, \
                                 SUM(CASE WHEN tomb = 0 THEN 1 ELSE 0 END) AS count_tomb_0, \
                                 SUM(CASE WHEN tomb = 1 THEN 1 ELSE 0 END) AS count_tomb_1 \
                                 FROM %@ \
                                 GROUP BY agrp", tableName];
            CFStringRef sql = (__bridge CFStringRef)sqlString;

            bool result = SecDbPrepare(dbt, sql, &error, ^(sqlite3_stmt *stmt) {
                SecDbStep(dbt, stmt, &error, ^(bool *stop) {
                    // Extract agrp (column 0)
                    const unsigned char *agrpText = sqlite3_column_text(stmt, 0);
                    if (agrpText) {
                        NSString *agrp = [NSString stringWithUTF8String:(const char *)agrpText];
                        // Extract count_tomb_0 (column 1) and count_tomb_1 (column 2)
                        int countTomb0 = sqlite3_column_int(stmt, 1);
                        int countTomb1 = sqlite3_column_int(stmt, 2);

                        if (agrp) {
                            agrpCounts[agrp] = @{
                                @"tomb_0": @(countTomb0),
                                @"tomb_1": @(countTomb1)
                            };
                        }
                    }
                });
            });

            return result;
        });

        if (ok) {
            allTableCounts[tableName] = agrpCounts;
        } else {
            secerror("Failed to get access group counts for %@: %@", tableName, error);
        }
        CFReleaseNull(error);
    }

    return allTableCounts;
}

NSDictionary * _Nullable SecDbStatsGetAccessGroupStorage(void) {
    // Array of keychain classes to query
    const SecDbClass* classes[] = {genp_class(), inet_class(), cert_class(), keys_class()};
    size_t classCount = sizeof(classes) / sizeof(classes[0]);
    NSMutableDictionary *allTableStorage = [[NSMutableDictionary alloc] init];
    
    for (size_t i = 0; i < classCount; i++) {
        const SecDbClass *dbClass = classes[i];
        NSString *tableName = (__bridge NSString *)dbClass->name;
        
        __block NSMutableDictionary *agrpStorage = [[NSMutableDictionary alloc] init];
        __block CFErrorRef error = NULL;
        
        bool ok = kc_with_dbt(false, NULL, &error, ^bool(SecDbConnectionRef dbt) {
            // Build storage calculation expression dynamically using SecDbForEachAttr
            NSMutableArray *columnExpressions = [[NSMutableArray alloc] init];
            SecDbForEachAttr(dbClass, attr) {
                if ((attr->flags & kSecDbInFlag)) {
                    NSString *colName = (__bridge NSString *)attr->name;
                    NSString *colExpr = [NSString stringWithFormat:@"IFNULL(octet_length(%@), 0)", colName];
                    [columnExpressions addObject:colExpr];
                }
            }
            
            if (columnExpressions.count == 0) {
                secerror("No storage columns found for table %@", tableName);
                return false;
            }
            
            // Join all column expressions with '+'
            NSString *storageCalc = [columnExpressions componentsJoinedByString:@" + "];
            
            // SQL query to get total storage bytes per access group (without grouping by tomb)
            NSString *sqlString = [NSString stringWithFormat:@"SELECT agrp, SUM(%@) AS total_bytes FROM %@ GROUP BY agrp",
                                   storageCalc, tableName];
            CFStringRef sql = (__bridge CFStringRef)sqlString;
            
            bool result = SecDbPrepare(dbt, sql, &error, ^(sqlite3_stmt *stmt) {
                SecDbStep(dbt, stmt, &error, ^(bool *stop) {
                    // Extract agrp (column 0)
                    const unsigned char *agrpText = sqlite3_column_text(stmt, 0);
                    if (agrpText) {
                        NSString *agrp = [NSString stringWithUTF8String:(const char *)agrpText];
                        // Extract total_bytes (column 1)
                        long long totalBytes = sqlite3_column_int64(stmt, 1);
                        
                        if (agrp) {
                            agrpStorage[agrp] = @(totalBytes);
                        }
                    }
                });
            });
            
            return result;
        });
        
        if (ok) {
            allTableStorage[tableName] = agrpStorage;
        } else {
            secerror("Failed to get access group storage for %@: %@", tableName, error);
        }
        CFReleaseNull(error);
    }
    
    return allTableStorage;
}

NSDictionary * _Nullable SecDbStatsGetDatabaseFileSizeInfo(void) {
    NSString* dbPath = (__bridge_transfer NSString*)SecServerKeychainCopyPath();
    if (dbPath == nil) {
        secerror("Failed to get Keychain database path");
        return nil;
    }

    NSMutableDictionary *fileSizeInfo = [[NSMutableDictionary alloc] init];
    __block off_t totalSize = 0;

    // Helper to get file stats and add to dictionary
    void (^addFileInfo)(NSString*, NSString*) = ^(NSString *path, NSString *key) {
        struct stat fileStat;
        if (stat([path UTF8String], &fileStat) == 0) {
            fileSizeInfo[key] = @(fileStat.st_size);
            totalSize += fileStat.st_size;
        } else {
            // Store errno as negative number
            fileSizeInfo[key] = (errno > 0) ? @(-errno) : @(errno);
        }
    };

    // Get info for main DB file
    addFileInfo(dbPath, @"mainDBFileSize");

    // Get info for WAL (Write-Ahead Log) file
    NSString* walPath = [dbPath stringByAppendingString:@"-wal"];
    addFileInfo(walPath, @"walDBFileSize");

    // Get info for SHM (Shared Memory) file
    NSString* shmPath = [dbPath stringByAppendingString:@"-shm"];
    addFileInfo(shmPath, @"shmDBFileSize");

    fileSizeInfo[@"totalDBFileSize"] = @(totalSize);

    secnotice("SecDbStorageMetrics", "Database file size info: %{private}@", fileSizeInfo);

    return fileSizeInfo;
}

NSDictionary * _Nullable SecDbStatsGetMetrics(void) {
    // Get counts per access group
    NSDictionary *accessGroupCounts = SecDbStatsGetAccessGroupItemCounts();
    if (!accessGroupCounts) {
        secerror("Failed to get access group counts");
        return nil;
    }

    // Get storage per access group
    NSDictionary *accessGroupStorage = SecDbStatsGetAccessGroupStorage();
    if (!accessGroupStorage) {
        secerror("Failed to get access group storage");
        return nil;
    }
    
    // Store each table info
    NSMutableDictionary *tablesStorageInfo = [[NSMutableDictionary alloc] init];
    // Merge counts and storage for each table
    for (NSString *tableName in accessGroupCounts) {
        NSDictionary *tableCounts = accessGroupCounts[tableName];
        NSDictionary *tableStorage = accessGroupStorage[tableName];

        NSMutableDictionary *tableMetrics = [[NSMutableDictionary alloc] init];

        // Get all unique access groups from both counts and storage
        NSMutableSet *allAgrps = [[NSMutableSet alloc] init];
        [allAgrps addObjectsFromArray:[tableCounts allKeys]];
        [allAgrps addObjectsFromArray:[tableStorage allKeys]];

        // Merge data for each access group
        for (NSString *agrp in allAgrps) {
            NSDictionary *counts = tableCounts[agrp];
            NSNumber *storage = tableStorage[agrp];

            NSMutableDictionary *agrpInfo = [[NSMutableDictionary alloc] init];

            // Add counts (tomb_0, tomb_1)
            if (counts) {
                agrpInfo[@"tomb_0"] = counts[@"tomb_0"] ?: @(0);
                agrpInfo[@"tomb_1"] = counts[@"tomb_1"] ?: @(0);
            } else {
                agrpInfo[@"tomb_0"] = @(0);
                agrpInfo[@"tomb_1"] = @(0);
            }

            // Add storage
            if (storage != nil) {
                agrpInfo[@"storage"] = storage;
            } else {
                agrpInfo[@"storage"] = @(0);
            }

            tableMetrics[agrp] = agrpInfo;
        }

        tablesStorageInfo[tableName] = tableMetrics;
    }
    
    // Get database files size info, which will be the top level of the metrics dict sent to CoreAnalytics
    NSMutableDictionary *metrics = [SecDbStatsGetDatabaseFileSizeInfo() mutableCopy];
    if (!metrics) {
        secerror("Failed to get file size info");
        return nil;
    }
    
    // Convert tablesStorageInfo to custom delimited format; CA has issues if JSON like object is send in fields
    // Format: table_name: [agrp: (tomb_0, tomb_1, storage), ...] \n
    NSMutableString *formattedString = [[NSMutableString alloc] init];

    // Sort table names for consistent ordering
    NSArray *sortedTableNames = [[tablesStorageInfo allKeys] sortedArrayUsingSelector:@selector(compare:)];

    for (NSString *tableName in sortedTableNames) {
        NSDictionary *tableMetrics = tablesStorageInfo[tableName];

        [formattedString appendFormat:@"%@: [", tableName];

        // Sort access group names for consistent ordering
        NSArray *sortedAgrpNames = [[tableMetrics allKeys] sortedArrayUsingSelector:@selector(compare:)];

        for (NSUInteger i = 0; i < [sortedAgrpNames count]; i++) {
            NSString *agrp = sortedAgrpNames[i];
            NSDictionary *agrpInfo = tableMetrics[agrp];

            NSNumber *tomb0 = agrpInfo[@"tomb_0"] ?: @(0);
            NSNumber *tomb1 = agrpInfo[@"tomb_1"] ?: @(0);
            NSNumber *storage = agrpInfo[@"storage"] ?: @(0);

            // Add semicolon separator for all but first entry
            if (i > 0) {
                [formattedString appendString:@"; "];
            }

            [formattedString appendFormat:@"%@: (%@, %@, %@)",
                agrp, tomb0, tomb1, storage];
        }

        [formattedString appendString:@"]\n"];
    }

    metrics[@"tablesStorageInfo"] = [formattedString length] > 0 ? formattedString : @"NO_DATA";

    return metrics;
}

void SecDbStorageStatsReportMetrics(void) {
    // Get comprehensive metrics
    NSDictionary *allMetrics = SecDbStatsGetMetrics();
    if (!allMetrics) {
        secerror("Failed to get metrics for reporting");
        return;
    }
    // Send the analytics
    [SecCoreAnalytics sendEvent:@"com.apple.security.keychain.database.localStorageInfo"
                          event:allMetrics];
    
    secnotice("SecDbStorageMetrics", "Reported database storage info");
}
