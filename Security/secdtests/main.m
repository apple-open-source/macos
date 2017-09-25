//
//  main.c
//  secdtest
//
//  Created by Fabrice Gautier on 5/29/13.
//
//

#include <stdio.h>
#include <regressions/test/testenv.h>

#include "testlist.h"
#include <regressions/test/testlist_begin.h>
#include "testlist.h"
#include <regressions/test/testlist_end.h>

#include "keychain/ckks/CKKS.h"

#include <securityd/spi.h>

int main(int argc, char * const *argv)
{
    // secdtests should not run any CKKS. It's not entitled for CloudKit, and CKKS threading interferes with many of the tests.
    SecCKKSDisable();

    securityd_init(NULL);

    int result = tests_begin(argc, argv);

    fflush(stdout);
    fflush(stderr);

    return result;
}
