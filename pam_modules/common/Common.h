#include <OpenDirectory/OpenDirectory.h>

#ifndef _COMMON_H_
#define _COMMON_H_

int od_record_create(ODRecordRef*, CFStringRef);
int od_record_create_cstring(ODRecordRef*, const char*);

int od_record_check_pwpolicy(ODRecordRef);
int od_record_check_authauthority(ODRecordRef);
int od_record_check_homedir(ODRecordRef);
int od_record_check_shell(ODRecordRef);

int od_extract_home(const char *, char **, char **, char **);
int od_principal_for_user(const char *, char **);

void pam_cf_cleanup(__unused pam_handle_t *, void *, __unused int );

Boolean IsServerInstall(void);

#endif /* _COMMON_H_ */
