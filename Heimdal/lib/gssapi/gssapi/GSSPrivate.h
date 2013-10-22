
#ifndef _GSS_FRAMEWORK_PRIVATE
#define _GSS_FRAMEWORK_PRIVATE 1

CFStringRef
GSSRuleGetMatch(CFDictionaryRef rules, CFStringRef host);

void
GSSRuleAddMatch(CFMutableDictionaryRef rules, CFStringRef host, CFStringRef value);

/*
 * Cred
 */

gss_name_t
GSSCredCopyName(gss_cred_id_t cred);

OM_uint32
GSSCredGetLifetime(gss_cred_id_t cred);


#endif /* _GSS_FRAMEWORK_PRIVATE */
