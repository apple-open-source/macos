/******************************************************************************
** 
**  $Id: p11x_log.c,v 1.2 2003/02/13 20:06:40 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Logging functions
** 
******************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdarg.h>
#include "cryptoki.h"

#ifdef SYS_LOG
#include <syslog.h>
#endif

/******************************************************************************
** Function: log_Start
**
** Description
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
void log_Start(char *func)
{
    log_Log(LOG_LOW, "+%s : start", func);
}

/******************************************************************************
** Function: log_End
**
** Description
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
void log_End(char *func, CK_RV rv)
{
    if (CKR_ERROR(rv));
    log_Log(LOG_LOW, " -%s : end RV(0x%lX)", func, rv);
}

/******************************************************************************
** Function: log_Err
**
** Description
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
void log_Err(char *msg, char *file, CK_LONG line)
{
    log_Log(LOG_MED, "Error (%s %ld): %s", file, line, msg);
}

/******************************************************************************
** Function: log_Log
**
** Description
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
void log_Log(CK_ULONG level, char *msg, ...)
{
    va_list args;

#ifdef SYS_LOG
    thread_MutexLock(st.log_lock);

    if (st.prefs.log_level > level)
        return;

    va_start(args, msg);
    vsyslog(0, msg, args);
    va_end(args);
#else // SYS_LOG
    FILE *fp;
    time_t t;
    struct tm *time_s;
    char format[256];

    if (st.prefs.log_level > level)
        return;

    thread_MutexLock(st.log_lock);

    fp = fopen((char *)st.prefs.log_filename, "ab");

    if (!fp)    
    {
        fp = stderr;
        fprintf(fp, "Error, could not open: %s\n", st.prefs.log_filename);
    }

    time(&t);
    time_s = localtime(&t);

    sprintf(format, "%.2d/%.2d %.2d:%.2d:%.2d %s", 
                    time_s->tm_mday, 
                    time_s->tm_mon+1, 
                    time_s->tm_hour, 
                    time_s->tm_min, 
                    time_s->tm_sec, 
                    msg);

    va_start(args, msg);
    vfprintf(fp, format, args);
    va_end(args);
    
#ifdef WIN32
    fputs("\r\n", fp);
#else
    fputs("\n", fp);
#endif // WIN32

    fflush(fp); /* Fixme: more accurate, but slows logging */

    if (fp != stderr)
      fclose(fp);

#endif // SYS_LOG

    thread_MutexUnlock(st.log_lock);
}
