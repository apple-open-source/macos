/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#ifndef OTLocalStore_h
#define OTLocalStore_h
#if OCTAGON

#import <Foundation/Foundation.h>
#import <Prequelite/Prequelite.h>
#import <dispatch/dispatch.h>
#import "keychain/ot/OTBottledPeerRecord.h"
#import "keychain/ot/OTContextRecord.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTLocalStore : NSObject

@property (nonatomic, readonly) NSString*         dbPath;
@property (nonatomic, readonly) PQLConnection*    pDB;
@property (nonatomic, readonly) dispatch_queue_t  serialQ;
@property (nonatomic, readonly) NSString*         contextID;
@property (nonatomic, readonly) NSString*         dsid;
@property (nonatomic, readonly) sqlite3*          _db;

-(instancetype) initWithContextID:(NSString*)contextID dsid:(NSString*)dsid path:(nullable NSString*)path error:(NSError**)error;

-(BOOL)isProposedColumnNameInTable:(NSString*)proposedColumnName tableName:(NSString*)tableName;

// OT Context Record routines
-(BOOL)initializeContextTable:(NSString*)contextID dsid:(NSString*)dsid error:(NSError**)error;
-(OTContextRecord* _Nullable)readLocalContextRecordForContextIDAndDSID:(NSString*)contextAndDSID error:(NSError**)error;
-(BOOL)insertLocalContextRecord:(NSDictionary*)attributes error:(NSError**)error;
-(BOOL)updateLocalContextRecordRowWithContextID:(NSString*)contextIDAndDSID columnName:(NSString*)columnName newValue:(void*)newValue error:(NSError**)error;
-(BOOL)deleteLocalContext:(NSString*)contextIDAndDSID error:(NSError**)error;
-(BOOL) deleteAllContexts:(NSError**)error;

//OT Bottled Peer routines
- (nullable OTBottledPeerRecord *)readLocalBottledPeerRecordWithRecordID:(NSString *)recordID
                                                                         error:(NSError**)error;
- (nullable NSArray*) readAllLocalBottledPeerRecords:(NSError**)error;
-(BOOL)deleteBottledPeer:(NSString*) recordID error:(NSError**)error;
-(BOOL) deleteBottledPeersForContextAndDSID:(NSString*)contextIDAndDSID
                               error:(NSError**)error;
-(BOOL)removeAllBottledPeerRecords:(NSError**)error;
-(BOOL)insertBottledPeerRecord:(OTBottledPeerRecord *)bp
                  escrowRecordID:(NSString *)escrowRecordID
                           error:(NSError**)error;
- (nullable NSArray*) readLocalBottledPeerRecordsWithMatchingPeerID:(NSString*)peerID error:(NSError**)error;

// generic DB routines
-(BOOL)openDBWithError:(NSError**)error;
-(BOOL)closeDBWithError:(NSError**)error;;
-(BOOL)createDirectoryAtPath:(NSString*)path error:(NSError **)error;
@end
NS_ASSUME_NONNULL_END
#endif
#endif /* OTLocalStore_h */
