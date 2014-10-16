/* NO_AUTOMATED_TESTING */

#include <stdio.h>
#include <stdlib.h>     /* getenv(3) */
#include <Security/AuthSession.h>
#include "testmore.h"

#define SSID_ENV_STR  "SECURITYSESSIONID"   /* hard-coded in Authorization.cpp */

/*
 * Not automated because SessionCreate() implicitly invokes task_for_pid(),
 * which in turn can trigger an Authorization call (and thus UI) via 
 * taskgated.  
 */
int main(__unused int ac, __unused const char *av[])
{
    char *ssid = NULL;

    plan_tests(1);

    if ((ssid = getenv(SSID_ENV_STR)) != NULL)
        printf("Current SecuritySessionID: %s\n", ssid);
    /* 
     * @@@  SessionCreate() is documented to return "noErr" on success, but 
     *      errSessionSuccess is part of the SessionStatus enum
     */
    is(SessionCreate(0/*SessionCreationFlags*/,
                     sessionHasGraphicAccess|sessionHasTTY/*SessionAttributeFlags*/), 
       errSessionSuccess, "SessionCreate()");
    if ((ssid = getenv(SSID_ENV_STR)) == NULL)
        fprintf(stderr, "Missing %s in environment!\n", SSID_ENV_STR);
    printf("New SecuritySessionID: %s\n", ssid);
    return 0;
}
