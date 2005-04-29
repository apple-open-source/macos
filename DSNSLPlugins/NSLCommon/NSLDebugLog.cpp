/*
 *  NSLDebugLog.cpp
 *  NSLPlugins
 *
 *  Created by Kevin Arnold on Tue May 14 2002.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */

#include "NSLDebugLog.h"

#if BUILDING_NSLDEBUG
pthread_mutex_t	nslSysLogLock = PTHREAD_MUTEX_INITIALIZER;

void ourLog(const char* format, ...)
{
    va_list ap;
    
	pthread_mutex_lock( &nslSysLogLock );

    va_start( ap, format );
    newlog( format, ap );
    va_end( ap );

	pthread_mutex_unlock( &nslSysLogLock );
}

void newlog(const char* format, va_list ap )
{
    char	pcMsg[MAXLINE +1];
    
    vsnprintf( pcMsg, MAXLINE, format, ap );

#if LOG_ALWAYS
    syslog( LOG_ALERT, "T:[0x%x] %s", pthread_self(), pcMsg );
#else
    syslog( LOG_ERR, "T:[0x%x] %s", pthread_self(), pcMsg );
#endif
}

#endif

#if LOG_ALWAYS
	int			gDebuggingNSL = 1;
#else
	int			gDebuggingNSL = (getenv("NSLDEBUG") != NULL);
#endif

int IsNSLDebuggingEnabled( void )
{
	return gDebuggingNSL;
}
