#ifndef MSG_H
#define MSG_H

#include <syslog.h>

const char *msg_init(void);
void msg_suspend(void);
void msg_resume(void);
void msg(int level,const char *func,const char *fmt,...)
#ifdef __GNUC__ 
        __attribute__ ((format (printf, 3, 4)));
#else
        ;
#endif
void parsemsg(int msg_level,const char *func,const char *fmt,...)
#ifdef __GNUC__ 
        __attribute__ ((format (printf, 3, 4)));
#else
        ;
#endif
#endif
