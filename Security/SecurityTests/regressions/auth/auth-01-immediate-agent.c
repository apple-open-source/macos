/* NO_AUTOMATED_TESTING */
#include <Security/Authorization.h>
#include <Security/AuthorizationTagsPriv.h>
#include <stdio.h>
#include <stdint.h>
#include "testmore.h"

/*
 * XXX/gh  These should be in AuthorizationTagsPriv.h
 */
#ifdef AUTHHOST_TYPE_AGENT
#warning AUTHHOST_TYPE_AGENT defined, clean up immediate-agent test
#else
#define AUTHHOST_TYPE_AGENT      1  // SecurityAgent
#endif

#ifdef AUTHHOST_TYPE_PRIVILEGED
#warning AUTHHOST_TYPE_PRIVILEGED defined, clean up immediate-agent test
#else
#define AUTHHOST_TYPE_PRIVILEGED 2  // authorizationhost
#endif

int main(__unused int ac, const char *av[])
{
    uint32_t hostType = AUTHHOST_TYPE_AGENT;
    AuthorizationItem item = { AGENT_HINT_IMMEDIATE_LAUNCH, sizeof(hostType), &hostType, 0 };
    AuthorizationEnvironment hints = { 1, &item };
    const char *hostTypeStr;

    plan_tests(1);

    switch(hostType)
    {
        case AUTHHOST_TYPE_AGENT: hostTypeStr = "SecurityAgent"; break;
        case AUTHHOST_TYPE_PRIVILEGED: hostTypeStr = "authorizationhost"; break;
        default: hostTypeStr = "unknown host type"; break;
    }
    ok_status(AuthorizationCreate(NULL, &hints, kAuthorizationFlagDefaults, NULL), "force immediate agent launch");

    return 0;
}
