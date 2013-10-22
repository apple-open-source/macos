/*
 *  main.c
 *  Security
 *
 *  Created by Fabrice Gautier on 8/7/12.
 *  Copyright 2012 Apple, Inc. All rights reserved.
 *
 */

#include <stdio.h>
#include <unistd.h>

#include "test/testenv.h"

#include "testlist.h"
#include <test/testlist_begin.h>
#include "testlist.h"
#include <test/testlist_end.h>

#include <securityd/spi.h>

int main(int argc, char *argv[])
{
    printf("Build date : %s %s\n", __DATE__, __TIME__);

    /* We run this as if we are secd, so we need to initialize this */
    securityd_init(NULL);

    int result = tests_begin(argc, argv);

    fflush(stderr);
    fflush(stdout);

    sleep(1);

    return result;
}
