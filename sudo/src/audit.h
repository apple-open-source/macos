

#ifndef _AUDIT_H
#define _AUDIT_H

#include <bsm/libbsm.h>

/*
 * Include the following tokens in the audit record for successful sudo operations
 * header
 * subject
 * return
 */
void audit_success(struct passwd *pwd);

/*
 * Include the following tokens in the audit record for failed sudo operations
 * header
 * subject
 * text
 * return
 */
void audit_fail(struct passwd *pwd, char *errmsg);

#endif /* _AUDIT_H */

