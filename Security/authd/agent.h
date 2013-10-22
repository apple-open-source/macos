/* Copyright (c) 2012 Apple Inc. All rights reserved. */

#ifndef _SECURITY_AUTH_AGENT_H_
#define _SECURITY_AUTH_AGENT_H_

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum _PluginState {
    init,
    created,
    current,
    deactivating,
    active,
    interrupting,
    mechinterrupting,
    dead
} PluginState;
    
typedef enum {
    privilegedAuthHost,
    securityAgent,
    userAuthHost
} AuthHostType;
    
//
// Unified reason codes transmitted to SecurityAgent (and internationalized there)
//
enum Reason {
    noReason = 0,                   // no reason (not used, used as a NULL)
    unknownReason,                  // something else (catch-all internal error)
    
    // reasons for asking for a new passphrase
    newDatabase = 11,               // need passphrase for a new database
    changePassphrase,               // changing passphrase for existing database
    
    // reasons for retrying an unlock query
    invalidPassphrase = 21,         // passphrase was wrong
    
    // reasons for retrying a new passphrase query
    passphraseIsNull = 31,          // empty passphrase
    passphraseTooSimple,            // passphrase is not complex enough
    passphraseRepeated,             // passphrase was used before (must use new one)
    passphraseUnacceptable,         // passphrase unacceptable for some other reason
    oldPassphraseWrong,             // the old passphrase given is wrong
    
    // reasons for retrying an authorization query
    userNotInGroup = 41,            // authenticated user not in needed group
    unacceptableUser,               // authenticated user unacceptable for some other reason
    
    // reasons for canceling a staged query
    tooManyTries = 61,              // too many failed attempts to get it right
    noLongerNeeded,                 // the queried item is no longer needed
    keychainAddFailed,              // the requested itemed couldn't be added to the keychain
    generalErrorCancel,              // something went wrong so we have to give up now
    
    worldChanged = 101
};
    
typedef enum {
    tool = 'TOOL',
    bundle = 'BNDL',
    unknown = 'UNKN'
} RequestorType;
    
AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
agent_t agent_create(engine_t engine, mechanism_t mech, auth_token_t auth, process_t proc, bool firstMech);

AUTH_NONNULL_ALL
uint64_t agent_run(agent_t,auth_items_t hints, auth_items_t context, auth_items_t immutable_hints);
    
AUTH_NONNULL_ALL
auth_items_t agent_get_hints(agent_t);

AUTH_NONNULL_ALL
auth_items_t agent_get_context(agent_t);

AUTH_NONNULL_ALL
void agent_deactivate(agent_t);
    
AUTH_NONNULL_ALL
void agent_destroy(agent_t);
    
AUTH_NONNULL_ALL
PluginState agent_get_state(agent_t);

AUTH_NONNULL_ALL
mechanism_t agent_get_mechanism(agent_t);
    
AUTH_NONNULL_ALL
void agent_recieve(agent_t);

AUTH_NONNULL_ALL
void
agent_notify_interrupt(agent_t agent);

AUTH_NONNULL_ALL
void
agent_clear_interrupt(agent_t agent);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_AGENT_H_ */
