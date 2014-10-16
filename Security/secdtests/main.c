//
//  main.c
//  secdtest
//
//  Created by Fabrice Gautier on 5/29/13.
//
//

#include <stdio.h>
#include <test/testenv.h>

#include "testlist.h"
#include <test/testlist_begin.h>
#include "testlist.h"
#include <test/testlist_end.h>

#include <securityd/spi.h>

int main(int argc, char * const *argv)
{
    securityd_init(NULL);

    int result = tests_begin(argc, argv);

    fflush(stdout);
    fflush(stderr);

    return result;
}
