/* 
 * Based on sample code from Marc Majka 
 */
#include <notify.h>
#include <string.h>	/* strerror() */
#include <sys/errno.h>	/* errno */
#include <stdint.h>	/* uint32_t */
#include <syslog.h>	/* syslog() */
#include <stdarg.h>	/* syslog() */
#include "libbsm.h"

/* if 1, assumes a kernel that sends the right notification */
#define AUDIT_NOTIFICATION_ENABLED	1

#if AUDIT_NOTIFICATION_ENABLED
static int token = 0;
#endif	/* AUDIT_NOTIFICATION_ENABLED */

static long au_cond = AUC_UNSET;	/* <bsm/audit.h> */

uint32_t
au_notify_initialize(void)
{
#if AUDIT_NOTIFICATION_ENABLED
    uint32_t status, ignore_first;

    status = notify_register_check(__BSM_INTERNAL_NOTIFY_KEY, &token);
    if (status != NOTIFY_STATUS_OK) return status;

    status = notify_check(token, &ignore_first);
    if (status != NOTIFY_STATUS_OK) return status;
#endif

    if (auditon(A_GETCOND, &au_cond, sizeof(long)) < 0)
    {
	syslog(LOG_ERR, "Initial audit status check failed (%s)", 
	       strerror(errno));
	if (errno == ENOSYS)		/* auditon() unimplemented */
	    return AU_UNIMPL;
	return NOTIFY_STATUS_FAILED;	/* is there a better code? */
    }
    return NOTIFY_STATUS_OK;
}

int
au_notify_terminate(void)
{
#if AUDIT_NOTIFICATION_ENABLED
    return (notify_cancel(token) == NOTIFY_STATUS_OK) ? 0 : -1;
#else
    return 0;
#endif
}

/* 
 * On error of any notify(3) call, reset 'au_cond' to ensure we re-run 
 * au_notify_initialize() next time 'round--but assume auditing is on.  
 * This is a slight performance hit if auditing is off, but at least the 
 * system will behave correctly.  The notification calls are unlikely to 
 * fail, anyway.  
 */
int 
au_get_state(void)
{
#if AUDIT_NOTIFICATION_ENABLED
    uint32_t did_notify;
#endif
    int status;

    /* 
     * Don't make the client initialize this set of routines, but 
     * take the slight performance hit by checking ourselves every 
     * time.  
     */
    if (au_cond == AUC_UNSET)
    {
	status = au_notify_initialize();
	if (status != NOTIFY_STATUS_OK) 
	{
	    if (status == AU_UNIMPL)
		return AU_UNIMPL;
	    return AUC_AUDITING;
	}
	else
	    return au_cond;
    }
#if AUDIT_NOTIFICATION_ENABLED
    status = notify_check(token, &did_notify);
    if (status != NOTIFY_STATUS_OK)
    {
	au_cond = AUC_UNSET;
	return AUC_AUDITING;
    }

    if (did_notify == 0) return au_cond;
#endif

    if (auditon(A_GETCOND, &au_cond, sizeof(long)) < 0)
    {
	/* XXX  reset au_cond to AUC_UNSET? */
	syslog(LOG_ERR, "Audit status check failed (%s)", 
	       strerror(errno));
	if (errno == ENOSYS)	/* function unimplemented */
	    return AU_UNIMPL;
	return errno;
    }
    switch (au_cond)
    {
	case AUC_NOAUDIT:	/* auditing suspended */
	case AUC_DISABLED:	/* auditing shut off */
	    return AUC_NOAUDIT;
	case AUC_UNSET:	/* uninitialized; shouldn't get here */
	case AUC_AUDITING:	/* audit on */
	default:
	    return AUC_AUDITING;
    }
}

