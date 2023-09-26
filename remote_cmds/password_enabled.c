#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#include <os/log.h>
#include <sys/sysctl.h>
#include <string.h>
#include <strings.h>

/* -1 unspecified
 *  0 explicitly disabled
 *  1 explicitly enabled
 */
int
password_enabled(void) {
    os_log_t logger = os_log_create("com.apple.telnetd", "default");
    int rc = -1;
    size_t n = 0;
    char *buf = NULL;

    if (0 != sysctlbyname("kern.bootargs", NULL, &n, NULL, 0)) {
        os_log_error(logger, "Could not get boot-args size");
        goto fin;
    }
    buf = malloc(n);
    if (NULL == buf || 0 != sysctlbyname("kern.bootargs", buf, &n, NULL, 0)) {
        os_log_error(logger, "Could not get boot-args");
        goto fin;
    }
    char *p = strstr(buf, "rdar102068001=");
    if (NULL == p)
        goto fin;
    n = strcspn(p, "="); // = must be present due to previous strstr
    char *val = &p[n+1];
    if ('\0' == *val) {
        rc = 0;
        goto fin;
    }
    n = strcspn(val, " \t");
    val[n] = '\0';
    rc = (0 == strcasecmp(val, "yes"));
fin:
    if (NULL != buf) {
        free(buf);
    }
    os_log_info(logger, "%s = %d", __func__, rc);
    return rc;
}
#endif
