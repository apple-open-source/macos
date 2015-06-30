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


/*!
 @header SOSDataSource.h
 The functions provided in SOSDataSource.h provide the protocol to a
 secure object syncing data source.  This is something than can produce
 manifests and manifest digests and query objects by digest and merge
 objects into the data source.
 */

#ifndef _SEC_SOSDATASOURCE_H_
#define _SEC_SOSDATASOURCE_H_

#include <SecureObjectSync/SOSManifest.h>
#include <utilities/SecCFRelease.h>

#include <dispatch/dispatch.h>

__BEGIN_DECLS

/* SOSDataSource protocol (non opaque). */
typedef struct SOSDataSourceFactory *SOSDataSourceFactoryRef;
typedef struct SOSDataSource *SOSDataSourceRef;
typedef struct __OpaqueSOSEngine *SOSEngineRef;
typedef struct __OpaqueSOSObject *SOSObjectRef;
typedef struct __OpaqueSOSTransaction *SOSTransactionRef;

//
// MARK: - SOSDataSourceFactory protocol
//
struct SOSDataSourceFactory {
    CFArrayRef       (*copy_names)(SOSDataSourceFactoryRef factory);
    SOSDataSourceRef (*create_datasource)(SOSDataSourceFactoryRef factory, CFStringRef dataSourceName, CFErrorRef *error);
    void             (*release)(SOSDataSourceFactoryRef factory);
};

//
// MARK: - SOSDataSource protocol
//

/* Implement this if you want to create a new type of sync client.
   Currently we support keychains, but the engine should scale to
   entire filesystems. */
enum SOSMergeResult {
    kSOSMergeFailure = 0,   // CFErrorRef returned, no error returned in any other case
    kSOSMergeLocalObject,   // We choose the current object in the dataSource the manifest is still valid.
    kSOSMergePeersObject,   // We chose the peers object over our own, manifest is now dirty.
    kSOSMergeCreatedObject, // *createdObject is returned and should be released
};
typedef CFIndex SOSMergeResult;

//
// MARK: - SOSDataSource struct
//

//
// MARK: SOSDataSourceTransactionType
//
enum SOSDataSourceTransactionType {
    kSOSDataSourceNoneTransactionType = 0,
    kSOSDataSourceImmediateTransactionType,
    kSOSDataSourceExclusiveTransactionType,
    kSOSDataSourceNormalTransactionType,
    kSOSDataSourceExclusiveRemoteTransactionType,
};
typedef CFOptionFlags SOSDataSourceTransactionType;

enum SOSDataSourceTransactionPhase {
    kSOSDataSourceTransactionDidRollback = 0,   // A transaction just got rolled back
    kSOSDataSourceTransactionWillCommit,        // A transaction is about to commit.
    kSOSDataSourceTransactionDidCommit,         // A transnaction sucessfully committed.
};
typedef CFOptionFlags SOSDataSourceTransactionPhase;

enum SOSDataSourceTransactionSource {
    kSOSDataSourceSOSTransaction,        // A remotely initated transaction.
    kSOSDataSourceAPITransaction,        // A user initated transaction.
};
typedef CFOptionFlags SOSDataSourceTransactionSource;

typedef void (^SOSDataSourceNotifyBlock)(SOSDataSourceRef ds, SOSTransactionRef txn, SOSDataSourceTransactionPhase phase, SOSDataSourceTransactionSource source, struct SOSDigestVector *removals, struct SOSDigestVector *additions);

struct SOSDataSource {
    // SOSEngine - every datasource has an engine that is notified of changes
    // to the datasource.
    SOSEngineRef engine;

    // General SOSDataSource methods
    CFStringRef (*dsGetName)(SOSDataSourceRef ds);
    void (*dsSetNotifyPhaseBlock)(SOSDataSourceRef ds, dispatch_queue_t queue, SOSDataSourceNotifyBlock notifyBlock);
    SOSManifestRef (*dsCopyManifest)(SOSDataSourceRef ds, CFErrorRef *error);
    bool (*dsForEachObject)(SOSDataSourceRef ds, SOSManifestRef manifest, CFErrorRef *error, void (^handleObject)(CFDataRef key, SOSObjectRef object, bool *stop));
    CFDataRef (*dsCopyStateWithKey)(SOSDataSourceRef ds, CFStringRef key, CFStringRef pdmn, CFErrorRef *error);
    bool (*dsWith)(SOSDataSourceRef ds, CFErrorRef *error, SOSDataSourceTransactionSource source, void(^transaction)(SOSTransactionRef txn, bool *commit));
    bool (*dsRelease)(SOSDataSourceRef ds, CFErrorRef *error); // Destructor

    // SOSTransaction methods, writes to a dataSource require a transaction.
    SOSMergeResult (*dsMergeObject)(SOSTransactionRef txn, SOSObjectRef object, SOSObjectRef *createdObject, CFErrorRef *error);
    bool (*dsSetStateWithKey)(SOSDataSourceRef ds, SOSTransactionRef txn, CFStringRef pdmn, CFStringRef key, CFDataRef state, CFErrorRef *error);
    bool (*dsRestoreObject)(SOSTransactionRef txn, uint64_t handle, CFDictionaryRef item, CFErrorRef *error);

    // SOSObject methods
    CFDataRef (*objectCopyDigest)(SOSObjectRef object, CFErrorRef *error);
    CFDataRef (*objectCopyPrimaryKey)(SOSObjectRef object, CFErrorRef *error);
    SOSObjectRef (*objectCreateWithPropertyList)(CFDictionaryRef plist, CFErrorRef *error);
    CFDictionaryRef (*objectCopyPropertyList)(SOSObjectRef object, CFErrorRef *error);
    CFDictionaryRef (*objectCopyBackup)(SOSObjectRef object, uint64_t handle, CFErrorRef *error);
};

//
// MARK: - SOSDataSource protocol implementation
//
static inline SOSEngineRef SOSDataSourceGetSharedEngine(SOSDataSourceRef ds, CFErrorRef *error) {
    return ds->engine;
}

static inline CFStringRef SOSDataSourceGetName(SOSDataSourceRef ds) {
    return ds->dsGetName(ds);
}

static inline void SOSDataSourceSetNotifyPhaseBlock(SOSDataSourceRef ds, dispatch_queue_t queue, SOSDataSourceNotifyBlock notifyBlock) {
    ds->dsSetNotifyPhaseBlock(ds, queue, notifyBlock);
}

static inline SOSManifestRef SOSDataSourceCopyManifest(SOSDataSourceRef ds, CFErrorRef *error) {
    return ds->dsCopyManifest(ds, error);
}

static inline bool SOSDataSourceForEachObject(SOSDataSourceRef ds, SOSManifestRef manifest, CFErrorRef *error, void (^handleObject)(CFDataRef digest, SOSObjectRef object, bool *stop)) {
    return ds->dsForEachObject(ds, manifest, error, handleObject);
}

static inline bool SOSDataSourceWith(SOSDataSourceRef ds, CFErrorRef *error,
                                     void(^transaction)(SOSTransactionRef txn, bool *commit)) {
    return ds->dsWith(ds, error, kSOSDataSourceSOSTransaction, transaction);
}

static inline bool SOSDataSourceWithAPI(SOSDataSourceRef ds, bool isAPI, CFErrorRef *error,
                                     void(^transaction)(SOSTransactionRef txn, bool *commit)) {
    return ds->dsWith(ds, error, isAPI ? kSOSDataSourceAPITransaction : kSOSDataSourceSOSTransaction, transaction);
}

static inline CFDataRef SOSDataSourceCopyStateWithKey(SOSDataSourceRef ds, CFStringRef key, CFStringRef pdmn, CFErrorRef *error)
{
    return ds->dsCopyStateWithKey(ds, key, pdmn, error);
}

static inline bool SOSDataSourceRelease(SOSDataSourceRef ds, CFErrorRef *error) {
    return !ds || ds->dsRelease(ds, error);
}

//
// MARK: - SOSTransaction
//
static inline SOSMergeResult SOSDataSourceMergeObject(SOSDataSourceRef ds, SOSTransactionRef txn, SOSObjectRef peersObject, SOSObjectRef *createdObject, CFErrorRef *error) {
    return ds->dsMergeObject(txn, peersObject, createdObject, error);
}

static inline bool SOSDataSourceSetStateWithKey(SOSDataSourceRef ds, SOSTransactionRef txn, CFStringRef key, CFStringRef pdmn, CFDataRef state, CFErrorRef *error)
{
    return ds->dsSetStateWithKey(ds, txn, key, pdmn, state, error);
}


//
// MARK: - SOSObject methods
//
static inline CFDataRef SOSObjectCopyDigest(SOSDataSourceRef ds, SOSObjectRef object, CFErrorRef *error) {
    return ds->objectCopyDigest(object, error);
}

static inline CFDataRef SOSObjectCopyPrimaryKey(SOSDataSourceRef ds, SOSObjectRef object, CFErrorRef *error) {
    return ds->objectCopyPrimaryKey(object, error);
}

static inline SOSObjectRef SOSObjectCreateWithPropertyList(SOSDataSourceRef ds, CFDictionaryRef plist, CFErrorRef *error) {
    return ds->objectCreateWithPropertyList(plist, error);
}

static inline CFDictionaryRef SOSObjectCopyPropertyList(SOSDataSourceRef ds, SOSObjectRef object, CFErrorRef *error) {
    return ds->objectCopyPropertyList(object, error);
}

static inline CFDictionaryRef SOSObjectCopyBackup(SOSDataSourceRef ds, SOSObjectRef object, uint64_t handle, CFErrorRef *error) {
    return ds->objectCopyBackup(object, handle, error);
}

static inline bool SOSObjectRestoreObject(SOSDataSourceRef ds, SOSTransactionRef txn, uint64_t handle, CFDictionaryRef item, CFErrorRef *error) {
    return ds->dsRestoreObject(txn, handle, item, error);
}


//
// MARK: SOSDataSourceFactory helpers
//

static inline SOSEngineRef SOSDataSourceFactoryGetEngineForDataSourceName(SOSDataSourceFactoryRef factory, CFStringRef dataSourceName, CFErrorRef *error)
{
    SOSDataSourceRef ds = factory->create_datasource(factory, dataSourceName, error);
    SOSEngineRef engine = ds ? SOSDataSourceGetSharedEngine(ds, error) : (SOSEngineRef) NULL;
    SOSDataSourceRelease(ds, NULL); // TODO: Log this error?!
    
    return engine;
}

__END_DECLS

#endif /* !_SEC_SOSDATASOURCE_H_ */
