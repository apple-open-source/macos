#ifndef __LOGGING_H__
#define __LOGGING_H__

extern void (*kextd_log)(const char * format, ...);
extern void (*kextd_error_log)(const char * format, ...);

void kextd_openlog(const char * name);

void kextd_syslog(const char * format, ...);
void kextd_error_syslog(const char * format, ...);
void kextd_print(const char * format, ...);
void kextd_error_print(const char * format, ...);

#endif __LOGGING_H__
