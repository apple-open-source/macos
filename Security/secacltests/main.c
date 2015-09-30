//
//  main.c
//  secacltests
//
//  Created by Vratislav Kužela on 06/05/15.
//

#include <stdio.h>
#include <test/testenv.h>

#include "testlist.h"
#include <test/testlist_begin.h>
#include "testlist.h"
#include <test/testlist_end.h>

int main(int argc, char * const *argv)
{
    int result = tests_begin(argc, argv);

    fflush(stdout);
    fflush(stderr);

    return result;
}
