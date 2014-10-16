/* Copyright (c) 2012-2014 Apple Inc. All Rights Reserved. */

#ifndef securityd_service_securityd_service_h
#define securityd_service_securityd_service_h

#define SECURITYD_SERVICE_NAME "com.apple.securitydservice"

#define SERVICE_XPC_REQUEST     "_request"
#define SERVICE_XPC_RC          "_rc"
#define SERVICE_XPC_KEY         "_key"
#define SERVICE_XPC_SECRET      "_secret"
#define SERVICE_XPC_SECRET_NEW  "_secret_new"
#define SERVICE_XPC_CONTEXT     "_context"
#define SERVICE_XPC_LOCKED      "_locked"
#define SERVICE_XPC_NO_PIN      "_no_pin"

enum {
    SERVICE_STASH_SET_KEY = 1,
    SERVICE_STASH_GET_KEY,
    SERVICE_STASH_BLOB,
    SERVICE_KB_LOAD,
    SERVICE_KB_UNLOCK,
    SERVICE_KB_LOCK,
    SERVICE_KB_CHANGE_SECRET,
    SERVICE_KB_CREATE,
    SERVICE_KB_IS_LOCKED,
    SERVICE_KB_RESET,
    SERVICE_STASH_LOAD_KEY,
};

#endif
