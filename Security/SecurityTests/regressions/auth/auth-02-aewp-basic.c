/* NO_AUTOMATED_TESTING */
#include <Security/Authorization.h>
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include "testmore.h"

#define EXECUTABLE  "/bin/ls"
#define LSTARGET    "/private/var/db/shadow"

/* XXX/gh  interactive, so inappropriate for auto-regressions */
int main(__unused int ac, const char *av[])
{
    AuthorizationRef authRef = NULL;
    char *lsargs[2] = { "-l", LSTARGET };
    FILE *commPipe = NULL;
    DIR *dir = NULL;
    char lsbuf[6];       /* "total" */
    /* uint32_t total; */

    plan_tests(5);

    /* make sure LSTARGET isn't readable by mere mortals */
    dir = opendir(LSTARGET);
    is(errno, EACCES, "AEWP-basic: opendir()");
    ok_status(AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authRef), 
              "AEWP-basic: AuthCreate()");
    ok(authRef != NULL, "AEWP-basic: NULL authRef");
    ok_status(AuthorizationExecuteWithPrivileges(authRef, 
                                                 EXECUTABLE,
                                                 kAuthorizationFlagDefaults, 
                                                 lsargs,
                                                 &commPipe), 
              "AEWP-basic: AEWP()");

    /* stops at first white space */
    is_status(fscanf(commPipe, "%s", lsbuf), 1, "AEWP-basic: fscanf()");
    printf("ls output: %s\n", lsbuf);
    return 0;
}
