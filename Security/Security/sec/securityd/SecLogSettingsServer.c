//
//  SecLogSettingsServer.c
//  sec
//
//

#include <securityd/SecLogSettingsServer.h>
#include <Security/SecBase.h>
#include <Security/SecLogging.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>

CFPropertyListRef
SecCopyLogSettings_Server(CFErrorRef* error)
{
    return CopyCurrentScopePlist();
}

bool
SecSetXPCLogSettings_Server(CFTypeRef type, CFErrorRef* error)
{
    bool success = false;
    if (isString(type)) {
        ApplyScopeListForID(type, kScopeIDXPC);
        success = true;
    } else if (isDictionary(type)) {
        ApplyScopeDictionaryForID(type, kScopeIDXPC);
        success = true;
    } else {
        success = SecError(errSecParam, error, CFSTR("Unsupported CFType"));
    }

    return success;
}
