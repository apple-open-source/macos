//
//  main.c
//  securitydservicectrl
//
//  Created by Wade Benson on 12/2/12.
//  Copyright (c) 2012 Apple. All rights reserved.
//

#include "securityd_service.h"
#include "securityd_service_client.h"

#include <stdio.h>
#include <xpc/xpc.h>
#include <dispatch/dispatch.h>
#include <AssertMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecKeychainPriv.h>

static inline char *
hextostr(const uint8_t *buf, size_t len, char *hexbuf)
{
    char *s = hexbuf;
    size_t i;
    static const char hexdigits[] = "0123456789abcdef";
    for (i = 0; i < len; i++) {
        *s++ = hexdigits[buf[i]>>4];
        *s++ = hexdigits[buf[i]&0xf];
    }
    *s = '\0';
    return hexbuf;
}

int main(int argc, const char * argv[])
{
    uint64_t action = 0;
    OSStatus status = noErr;
    uint8_t testkey[128] = "\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef";
    xpc_connection_t connection = xpc_connection_create_mach_service(SECURITYD_SERVICE_NAME, NULL, XPC_CONNECTION_MACH_SERVICE_PRIVILEGED);
    xpc_object_t message = NULL, reply = NULL;

    xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
        if (xpc_get_type(event) == XPC_TYPE_ERROR) {
            printf("XPC error\n");
        }
    });
    xpc_connection_resume(connection);

    if (argc < 2) {
        printf("Usage: securityservicectrl < get | set | stash | login | loginstash | unload | load <uid> >\n");
        return 1;
    }

    if (strcmp(argv[1], "get") == 0) {
        action = SERVICE_STASH_GET_KEY;
        printf("Get key\n");

    } else if (strcmp(argv[1], "set") == 0) {
        action = SERVICE_STASH_SET_KEY;
        printf("Set key\n");

    } else if (strcmp(argv[1], "stash") == 0) {
        action = SERVICE_STASH_BLOB;
        printf("Stash\n");

    } else if (strcmp(argv[1], "login") == 0) {
        printf("SecKeychainLogin() null passwd\n");
        status = SecKeychainLogin((uint32) strlen("test"), "test", 0, NULL);
        printf("Returned: %i\n", status);
        return status ? 1 : 0;

    } else if (strcmp(argv[1], "loginstash") == 0) {
        printf("SecKeychainStash()\n");
        status = SecKeychainStash();
        printf("Returned: %i\n", status);
        return status ? 1 : 0;

    } else if (strcmp(argv[1], "unload") == 0) {
        return service_client_kb_unload(NULL);
    } else if (strcmp(argv[1], "load") == 0) {
        require_action(argc == 3, done, printf("missing <uid>\n"));
        uid_t uid = atoi(argv[2]);
        return service_client_kb_load_uid(uid);
    } else {
        printf("%s not known\n", argv[1]);
        return 1;
    }

    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, SERVICE_XPC_REQUEST, action);

    if (action == SERVICE_STASH_SET_KEY)
        xpc_dictionary_set_data(message, SERVICE_XPC_KEY, testkey, 16);

    reply = xpc_connection_send_message_with_reply_sync(connection, message);
    require_action(reply != NULL, done, status = -1);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = -1);

    if (action == SERVICE_STASH_GET_KEY) {
        size_t len = 0;
        const uint8_t *keydata = xpc_dictionary_get_data(reply, SERVICE_XPC_KEY, &len);
        if (keydata) {
            char buf[sizeof(testkey) + 1];
            printf("\tkey = %s\n", hextostr(keydata, len > sizeof(testkey) ? sizeof(testkey) : len, buf));
        }
    }

    status = (OSStatus)xpc_dictionary_get_int64(reply, SERVICE_XPC_RC);

done:
    if (message)
        xpc_release(message);
    if (reply)
        xpc_release(reply);
    if (connection)
        xpc_release(connection);

    printf("Returned: %i\n", status);

    return status ? 1 : 0;
}

