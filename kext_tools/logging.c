#include <libc.h>
#include <syslog.h>
#include "logging.h"
#include "globals.h"

void (*kextd_log)(const char * format, ...) = kextd_print;
void (*kextd_error_log)(const char * format, ...) = kextd_error_print;

/*******************************************************************************
*
*******************************************************************************/
void kextd_openlog(const char * name)
{
    int fd;

    openlog(name, LOG_CONS | LOG_NDELAY | LOG_PID,
        LOG_DAEMON);

    kextd_log = kextd_syslog;
    kextd_error_log = kextd_error_syslog;

    fd = open("/dev/null", O_RDWR, 0);
    if (fd == -1) {
        // FIXME: print error? return error code? what kind of recovery is possible?
        goto finish;
    }
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2) {
        close(fd);
    }

finish:    
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_syslog(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);
    vsyslog(LOG_INFO, format, ap);
    va_end(ap);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_error_syslog(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);
    vsyslog(LOG_ERR, format, ap);
    va_end(ap);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_print(const char * format, ...)
{
    va_list ap;
    char fake_buffer[2];
    int output_length;
    char * output_string;

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        fprintf(stderr, "malloc failure\n");
        return;
    }

    va_start(ap, format);
    vsprintf(output_string, format, ap);
    va_end(ap);

    va_start(ap, format);
    fprintf(stdout, "%s: %s\n", progname, output_string);
    va_end(ap);

    fflush(stdout);

    free(output_string);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_error_print(const char * format, ...)
{
    va_list ap;
    char fake_buffer[2];
    int output_length;
    char * output_string;

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        fprintf(stderr, "malloc failure\n");
        return;
    }

    va_start(ap, format);
    vsprintf(output_string, format, ap);
    va_end(ap);

    va_start(ap, format);
    fprintf(stderr, "%s: %s\n", progname, output_string);
    va_end(ap);

    fflush(stderr);

    free(output_string);

    return;
}
