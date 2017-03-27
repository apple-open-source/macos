//
//  SOSAccountTransaction.h
//  sec
//
//
//

#ifndef SOSAccountTransaction_h
#define SOSAccountTransaction_h

typedef struct __OpaqueSOSAccountTransaction *SOSAccountTransactionRef;

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <CoreFoundation/CFRuntime.h>

CF_ASSUME_NONNULL_BEGIN

struct __OpaqueSOSAccountTransaction {
              CFRuntimeBase _base;

  _Nonnull    SOSAccountRef account;

              bool          initialInCircle;
  _Nullable   CFSetRef      initialViews;

  _Nullable   CFSetRef      initialUnsyncedViews;
  _Nullable   CFStringRef   initialID;

              bool          initialTrusted;
  _Nullable   CFDataRef     initialKeyParameters;

  _Nullable   CFMutableSetRef  peersToRequestSync;
};


SOSAccountTransactionRef SOSAccountTransactionCreate(SOSAccountRef account);
void SOSAccountTransactionFinish(SOSAccountTransactionRef txn);
void SOSAccountTransactionFinishAndRestart(SOSAccountTransactionRef txn);

void SOSAccountTransactionAddSyncRequestForPeerID(SOSAccountTransactionRef txn, CFStringRef peerID);
void SOSAccountTransactionAddSyncRequestForAllPeerIDs(SOSAccountTransactionRef txn, CFSetRef /* CFStringRef */ peerIDs);

CF_ASSUME_NONNULL_END

#endif /* SOSAccountTransaction_h */
