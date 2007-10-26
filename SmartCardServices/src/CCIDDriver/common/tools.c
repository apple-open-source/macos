/*
 *  tools.c
 *  ifd-CCID
 *
 *  Created by JL on Mon Feb 10 2003.
 *  Copyright (c) 2003 Jean-Luc Giraud. All rights reserved.
 *  See COPYING file for license.
 *
 */

#include "tools.h"
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>

LogLevel bGlobalLogLevel = LogLevelMaximum;
BYTE bLogging =  1;
LogType bGlobalLogType =  LogTypeStderr;	//LogTypeStderr;LogTypeSysLog

// Logging tools
void LogMessage(const char * pcFile, WORD wLine, BYTE bLogLevel, const char* pcFormat,...)
{
    va_list ap;
    int rv;
    char *pcLogMessage = NULL;
    
    // Do not log if log level > 0 and logging is disabled
    if ( (!bLogging) &&  bGlobalLogLevel )
    {
        return;
    }
    if ( bGlobalLogLevel >= bLogLevel )
    {

        va_start(ap, pcFormat);
        rv = vasprintf(&pcLogMessage, pcFormat, ap);
        if ( rv == -1 )
        {
            pcLogMessage = "Could not allocate buffer for vasprintf";
        }
        va_end(ap);
        
        if ( bGlobalLogType == LogTypeSysLog)
        {
            syslog(LOG_INFO, "%s (%d): %s",  pcFile, (int)wLine, pcLogMessage);
        }
        else
        {
            fprintf(stderr, "%s (%d): %s\n",  pcFile, (int)wLine, pcLogMessage);
        }
        if ( rv != -1 )
        {
            free(pcLogMessage);
        }
    }
}
void LogHexBuffer(const char * pcFile, WORD wLine, BYTE bLogLevel,
                  BYTE * pbData, WORD wLength, const char* pcFormat,...)
{
    WORD index;
    va_list ap;
    int rv, rvHex = -1;
    char *pcLogMessage = NULL;
    char *pcLogHexData = NULL;
    char *pcLogHexCurrent;
    
    if ( (!bLogging) &&  bGlobalLogLevel )
    {
        return;
    }
    if ( bGlobalLogLevel >= bLogLevel )
    {
        va_start(ap, pcFormat);
        rv = vasprintf(&pcLogMessage, pcFormat, ap);
        if ( rv == -1 )
        {
            pcLogMessage = "Could not allocate buffer for vasprintf";
            
        }
        else
        {
            // Allocate buffer string for printing of data
            // Each byte is 3 chars (1 per nibble + space)
            // Plus 1 for NULL byte
            pcLogHexData = malloc(wLength * 3 * sizeof(char) + 1);
            rvHex = 0;
            if ( pcLogHexData == NULL )
            {
                rvHex = -1;
                pcLogHexData = "Could not allocate buffer to print binary buffer";
            }
            else
            {
                pcLogHexCurrent = pcLogHexData;
                for (index = 0; index < wLength; index++)
                {
                    sprintf(pcLogHexCurrent, "%02X ", pbData[index]);
                    pcLogHexCurrent += 3 * sizeof(char);
                }
            }
        }
        va_end(ap);

        if ( bGlobalLogType == LogTypeSysLog)
        {
            syslog(LOG_INFO, "%s (%d): %s : %s",  pcFile, (int)wLine, pcLogMessage,
                   pcLogHexData);
        }
        else
        {
            fprintf(stderr, "%s (%d): %s : %s\n",  pcFile, (int)wLine, pcLogMessage,
                    pcLogHexData);
        }
        if ( rv != -1 )
        {
            free(pcLogMessage);
        }
        if ( rvHex != -1 )
        {
            free(pcLogHexData);
        }
    }
}

void SetLogLevel(BYTE bLogLevel)
{
    bGlobalLogLevel = bLogLevel;
}

void SetLogType(LogType bLogtype)
{
    bGlobalLogType = bLogtype;
}

void StartLogging()
{
    bLogging =  1;
}

void StopLogging()
{
    bLogging =  0;
}

