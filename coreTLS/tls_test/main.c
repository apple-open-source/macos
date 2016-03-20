//
//  main.c
//  tls_test
//
//  Created by Fabrice Gautier on 9/3/13.
//
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <AssertMacros.h>
#include <tls_record.h>

#include "test/testenv.h"

#include "testlist.h"
#include "test/testlist_begin.h"
#include "testlist.h"
#include "test/testlist_end.h"

int record_test(void);
int handshake_self_test(void);
int handshake_client_test(void);
int handshake_server_test(void);

#if 0
static void
logger(void * __unused ctx, const char *scope, const char *function, const char *str)
{
    test_printf("[%s] %s: %s\n", scope, function, str);
}
#endif

int main(int argc, char * const argv[])
{
    printf("[TEST] %s\n", argv[0]);
    printf("Build date : %s %s\n", __DATE__, __TIME__);

    //tls_add_debug_logger(logger, NULL);

    int result = tests_begin(argc, argv);

    fflush(stderr);
    fflush(stdout);

    sleep(1);

    return result;
}
