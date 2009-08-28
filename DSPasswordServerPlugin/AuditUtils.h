#ifndef _PWSF_AUDITUTILS_H_
#define _PWSF_AUDITUTILS_H_
#include <bsm/libbsm.h>
#include <bsm/audit_uevents.h>

#ifdef __cplusplus
extern "C" {
#endif

int audit_disabled_user(const char *authedSlot, const char *targetSlot, short event);

#ifdef __cplusplus
};
#endif

#endif /*_PWSF_AUDITUTILS_H_ */
