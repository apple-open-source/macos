#include <Security/SecBase.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc/malloc.h>
#include <unistd.h>
#include <Security/Security.h>
#include <Security/SecKeychainPriv.h>
#include "SecKeychainFuzzer.h"

#define TEMPFILE_TEMPLATE "/tmp/keychain_parser_fuzzer.XXXXXX"

int SecKeychainFuzzer(const uint8_t *Data, size_t Size) {
    char* temppath = (char*)malloc(strlen(TEMPFILE_TEMPLATE));
    strcpy(temppath, TEMPFILE_TEMPLATE);

    int fd = mkstemp(temppath);
    if (fd < 0) {
        fprintf(stderr, "Unable to create tempfile: %d\n", errno);
        free(temppath);
        return 0;
    }

    size_t written = write(fd, Data, Size);
    if (written != Size) {
        fprintf(stderr, "Failed to write all bytes to tempfile\n");
    } else {
        SecKeychainRef keychain_ref = NULL;
        OSStatus status = SecKeychainOpen(temppath, &keychain_ref);
        if(status == errSecSuccess && keychain_ref != NULL) {
            SecKeychainStatus kcStatus;
            SecKeychainGetStatus(keychain_ref, &kcStatus);

            UInt32 version = 0;
            SecKeychainGetKeychainVersion(keychain_ref, &version);

            Boolean is_valid = false;
            SecKeychainIsValid(keychain_ref, &is_valid);

            UInt32 passwordLength = 0;
            void *passwordData = NULL;
            SecKeychainItemRef itemRef = NULL;
            SecKeychainFindGenericPassword(keychain_ref, 10, "SurfWriter", 10, "MyUserAcct", &passwordLength, &passwordData, &itemRef);

            if(passwordData != NULL) {
                SecKeychainItemFreeContent(NULL, passwordData);
            }

            if(itemRef != NULL) {
                CFRelease(itemRef);
            }

            CFRelease(keychain_ref);
        } else {
            fprintf(stderr, "Keychain parsing error! %d %p\n", status, keychain_ref);
        }
    }

    if (remove(temppath) != 0) {
        fprintf(stderr, "Unable to remove tempfile: %s\n", temppath);
    }

    if (temppath) {
        free(temppath);
    }

    return 0;
}
