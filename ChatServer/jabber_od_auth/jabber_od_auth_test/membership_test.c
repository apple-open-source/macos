//
//  membership_test.c
//
//  ChatServer2
//
//  Created by korver@apple.com on 2/3/09.
//  Copyright 2009 Apple. All rights reserved.
//

static char *argv0 = 0;

#include <stdio.h>
#include <ctype.h>

#include "../apple_membership.c"


// APPLE_CHAT_SACL_NAME is defined in apple_patch/pre_configure/jabberd2/c2s/c2s.h.patch,
// so this code doesn't have access to it when compiled in Xcode, because the patches
// are applied by an external Makefile
#ifndef APPLE_CHAT_SACL_NAME
#define APPLE_CHAT_SACL_NAME "chat"
#endif

/* returns number of successes */
int
membership_test(int iterations, const char *username_fmt, const char *service)
{
    int i;
    int success = 0;

    if (service == 0)
        service = APPLE_CHAT_SACL_NAME;

    for (i = 1; i <= iterations; ++i) {
        char username[1024];
        // lots of i in case %d is used multiple times
        snprintf(username, sizeof(username), username_fmt, i, i, i, i, i, i);

        /* Now that we know the user is legit, verify service access */
        if (od_auth_check_service_membership(username, service) != 0)
            ++success;
    }

    return success;
}

int
usage()
{
    fprintf(stderr, "usage: %s iterations username [service]\n", argv0);
    fprintf(stderr, "       username is passed to sprintf, where %%d is the index (1..iterations)\n");
    fprintf(stderr, "       (eg %s 1000 'testuser%%d')\n", argv0);
    exit(1);
}

int
main(int argc, const char * argv[])
{
    argv0 = (char*)argv[0];
    if (strrchr(argv0, '/'))
        argv0 = strrchr(argv0, '/') + 1;

    if (argc != 3 && argc != 4)
        usage();

    if (! isdigit(argv[1][0]))
        usage();

    int iterations = atoi(argv[1]);
    fprintf(stderr, "Starting %d iterations\n", iterations);

    int success = membership_test(iterations, argv[2], (argc == 4) ? argv[3] : 0);

    printf("%s: %d successful checks (of %d)\n", argv0, success, iterations);

    return 0;
}
