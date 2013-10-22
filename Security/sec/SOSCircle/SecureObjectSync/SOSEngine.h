/*
 * Created by Michael Brouwer on 7/17/12.
 * Copyright 2012 Apple Inc. All Rights Reserved.
 */

/*!
 @header SOSEngine.h
 The functions provided in SOSEngine.h provide an interface to a
 secure object syncing engine
 */

#ifndef _SEC_SOSENGINE_H_
#define _SEC_SOSENGINE_H_

#include <SecureObjectSync/SOSTransport.h>
#include <CoreFoundation/CFRuntime.h>

__BEGIN_DECLS

enum {
    kSOSEngineInvalidMessageError = 1,
    kSOSEngineInternalError = 2,
};

typedef struct __OpaqueSOSEngine *SOSEngineRef;
typedef struct __OpaqueSOSPeer *SOSPeerRef;

/* SOSDataSource protocol (non opaque). */
typedef struct SOSDataSource *SOSDataSourceRef;

typedef struct __OpaqueSOSObject *SOSObjectRef;

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

struct SOSDataSource {
    bool (*get_manifest_digest)(SOSDataSourceRef ds, uint8_t *out_digest, CFErrorRef *error);
    SOSManifestRef (*copy_manifest)(SOSDataSourceRef ds, CFErrorRef *error);
    bool (*foreach_object)(SOSDataSourceRef ds, SOSManifestRef manifest, CFErrorRef *error, bool (^handle_object)(SOSObjectRef object, CFErrorRef *error));
    SOSMergeResult (*add)(SOSDataSourceRef ds, SOSObjectRef object, CFErrorRef *error);
    void (*release)(SOSDataSourceRef ds);

    SOSObjectRef (*createWithPropertyList)(SOSDataSourceRef ds, CFDictionaryRef plist, CFErrorRef *error);
    CFDataRef (*copyDigest)(SOSObjectRef object, CFErrorRef *error);
    CFDataRef (*copyPrimaryKey)(SOSObjectRef object, CFErrorRef *error);
    CFDictionaryRef (*copyPropertyList)(SOSObjectRef object, CFErrorRef *error);
    SOSObjectRef (*copyMergedObject)(SOSObjectRef object1, SOSObjectRef object2, CFErrorRef *error);
    CFDictionaryRef (*backupObject)(SOSObjectRef object, uint64_t handle, CFErrorRef *error);
    bool (*restoreObject)(SOSDataSourceRef ds, uint64_t handle, CFDictionaryRef item, CFErrorRef *error);
};

// Create a new engine instance for a given datasource.
SOSEngineRef SOSEngineCreate(SOSDataSourceRef dataSource, CFErrorRef *error);

// Dispose of an engine when it's no longer needed.
void SOSEngineDispose(SOSEngineRef engine);

// Handle incoming message from a remote peer.
bool SOSEngineHandleMessage(SOSEngineRef engine, SOSPeerRef peer,
                            CFDataRef message, CFErrorRef *error);

// Initiate a sync with the providied peer by sending it a message.
bool SOSEngineSyncWithPeer(SOSEngineRef engine, SOSPeerRef peer, bool force,
                           CFErrorRef *error);

/* Internal functions exposed for testability. */
CFDataRef SOSEngineCreateManifestDigestMessage(SOSEngineRef engine, SOSPeerRef peer, CFErrorRef *error);
CFDataRef SOSEngineCreateManifestMessage(SOSEngineRef engine, SOSPeerRef peer, CFErrorRef *error);
CFDataRef SOSEngineCreateManifestAndObjectsMessage(SOSEngineRef engine, SOSPeerRef peer, CFErrorRef *error);

CFStringRef SOSMessageCopyDescription(CFDataRef message);

__END_DECLS

#endif /* !_SEC_SOSENGINE_H_ */
