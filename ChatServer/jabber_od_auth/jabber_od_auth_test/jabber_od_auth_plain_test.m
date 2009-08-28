//
//  jabber_od_auth_plain_test.m
//  ChatServer2
//
//  Created by korver@apple.com on 2/3/09.
//  Copyright 2009 Apple. All rights reserved.
//

static char *argv0 = 0;

#include <stdio.h>
#include <ctype.h>

#include "../apple_authenticate.h"


// APPLE_CHAT_SACL_NAME is defined in apple_patch/pre_configure/jabberd2/c2s/c2s.h.patch,
// so this code doesn't have access to it when compiled in Xcode, because the patches
// are applied by an external Makefile
#ifndef APPLE_CHAT_SACL_NAME
#define APPLE_CHAT_SACL_NAME "chat"
#endif


/*
 *
 * the following _ar functions were taken more or less verbatim from authreg_sqlite.c.patch
 *
 */

/* -----------------------------------------------------------------
 int _ar_od_user_exists()

 RETURNS:
 0 = user not found
 1 = user exists
 ----------------------------------------------------------------- */
static int _ar_od_user_exists(char *username, char *realm)
{
    int iResult = od_auth_check_user_exists((const char *) username);
    if (0 > iResult) /* error? */
        iResult = 0; /* return "not found" */

    return iResult;
}

/* -----------------------------------------------------------------
 int _ar_od_check_password()

 RETURNS:
 0 = password is authenticated
 1 = authentication failed
 ----------------------------------------------------------------- */
static int _ar_od_check_password(char *username, char *realm, char password[257])
{
    /* Verify the password */
    int iResult = od_auth_check_plain_password(username, password);
    if (0 != iResult) /* error? */
        iResult = 1; /* return "auth failed" */
    else {
        /* Now that we know the user is legit, verify service access */
        int iErr = od_auth_check_service_membership(username, APPLE_CHAT_SACL_NAME);
        iResult = (1 == iErr) ? 0 : 1; /* return success/fail */
    }

    return iResult;
}

#if 0
/* -----------------------------------------------------------------
    int _ar_od_create_challenge()

    RETURNS:
       -1 = CRAM-MD5 unsupported for this user
        0 = operation failed
        1 = operation succeeded
   ----------------------------------------------------------------- */
static int _ar_od_create_challenge(char *username, char *challenge, int maxlen)
{
    int iResult = od_auth_supports_cram_md5(username);
    if (0 == iResult) /* auth method not available for this user */
        iResult = -1; /* return "failed" */

    /* create a unique challenge for this request */
    iResult = od_auth_create_crammd5_challenge(challenge, maxlen);
    if (0 < iResult) /* ok? */
        iResult = 1; /* return "success" */

    return iResult;
}

/* -----------------------------------------------------------------
    int _ar_od_check_response()

    RETURNS:
        0 = response is authenticated
        1 = authentication failed
   ----------------------------------------------------------------- */
static int _ar_od_check_response(char *username, char *realm, char *challenge, char *response)
{
    /* Verify the response */
    int iResult = od_auth_check_crammd5_response(username, challenge, response);
    if (0 != iResult) /* error? */
        iResult = 1; /* return "auth failed" */
    else {
        /* Now that we know the user is legit, verify service access */
        int iErr = od_auth_check_service_membership(username, APPLE_CHAT_SACL_NAME);
        iResult = (1 == iErr) ? 0 : 1; /* return success/fail */
    }

    return iResult;
}
#endif

int usage()
{
    fprintf(stderr, "usage: %s iterations username realm [password]\n", argv0);
    fprintf(stderr, "       username, realm, and password are passed to sprintf, where %%d is the index\n");
    fprintf(stderr, "       (eg %s 1000 'testuser%%d' 'od%%d.apple.com' 'pass%%d')\n", argv0);
    fprintf(stderr, "       if password is not specified, only _ar_od_user_exists is tested\n");
    exit(1);
}

int main (int argc, const char * argv[])
{
    argv0 = (char*)argv[0];
    if (strrchr(argv0, '/'))
        argv0 = strrchr(argv0, '/') + 1;

    if (argc != 4 && argc != 5)
        usage();

    if (! isdigit(argv[1][0]))
        usage();

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    int iterations = atoi(argv[1]);
    fprintf(stderr, "Starting %d iterations\n", iterations);

    char username[1024];
    char realm[1024];
    char password[1024];
    int i;
    int users_found = 0;
    int auths_succeeded = 0;
    for (i = 0; i < iterations; ++i) {
        // lots of i in case %d is used multiple times
        snprintf(username, sizeof(username), argv[2], i, i, i, i, i, i);
        snprintf(realm, sizeof(realm), argv[3], i, i, i, i, i, i);

        users_found += (_ar_od_user_exists(username, realm) ? 1 : 0);

        if (argc == 5) {
            // lots of i in case %d is used multiple times
              snprintf(password, sizeof(password), argv[4], i, i, i, i, i, i);
            auths_succeeded += (_ar_od_check_password(username, realm, password) ? 0 : 1);
        }
    }

#if 0
/* maybe implement CRAMMD5 someday */
static int _ar_od_create_challenge(char *username, char *challenge, int maxlen)
static int _ar_od_check_response(char *username, char *realm, char *challenge, char *response)
#endif

    printf("users_found: %d (of %d)\n", users_found, iterations);
    if (argc == 5)
        printf("auths_succeeded: %d (of %d)\n", auths_succeeded, iterations);

    [pool drain];

    return 0;
}
