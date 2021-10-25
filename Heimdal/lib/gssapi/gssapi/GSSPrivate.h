
#ifndef _GSS_FRAMEWORK_PRIVATE
#define _GSS_FRAMEWORK_PRIVATE 1

__nullable CFStringRef
GSSRuleGetMatch(__nonnull CFDictionaryRef rules, __nonnull CFStringRef hostname)
;

void
GSSRuleAddMatch(__nonnull CFMutableDictionaryRef rules, __nonnull CFStringRef host, __nonnull CFStringRef value);

/*
 * Cred
 */

gss_name_t
GSSCredCopyName(__nonnull gss_cred_id_t cred);

OM_uint32
GSSCredGetLifetime(__nonnull gss_cred_id_t cred);


#endif /* _GSS_FRAMEWORK_PRIVATE */
