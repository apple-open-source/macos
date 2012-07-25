#ifndef __APPLEHELPERS_H__
#define __APPLEHELPERS_H__ 1

#define kDisabledByAdmin               1
#define kDisabledExpired               2
#define kDisabledInactive              3
#define kDisabledTooManyFailedLogins   4

Entry *odusers_copy_entry(Operation *op);
int odusers_remove_authdata(char *slotid);
int odusers_get_authguid(Entry *e, char *guidstr);
Entry *odusers_copy_authdata(struct berval *dn);
Attribute *odusers_copy_globalpolicy(void);
CFDictionaryRef odusers_copy_effectiveuserpoldict(struct berval *dn);
CFDictionaryRef odusers_copy_defaultglobalpolicy(void);
bool odusers_isdisabled(CFDictionaryRef policy);
bool odusers_isadmin(CFDictionaryRef policy);
struct berval *odusers_copy_dict2bv(CFDictionaryRef dict);
int odusers_increment_failedlogin(struct berval *dn);
int odusers_successful_auth(struct berval *dn, CFDictionaryRef policy);
CFDictionaryRef odusers_copy_pwsprefs(const char *suffix);

#endif /* __APPLEHELPERS_H__ */
