/*
    $Id: NSLDebugLog.h,v 1.6 2002/04/20 01:30:24 imlucid Exp $

    Contains:	Definition of simple DebugLog controlled by environment variable

    Copyright:	© 2000 by Apple Computer, Inc., all rights reserved.

    DRI:		Al Begley

    $Log: NSLDebugLog.h,v $
    Revision 1.6  2002/04/20 01:30:24  imlucid
    take off the LOG_ALL flag

    Revision 1.5  2002/04/19 21:39:26  imlucid
    Added support for the server sending us a CFRunLoopRef

    Revision 1.4  2002/04/03 19:59:58  imlucid
    fixes for DNS and network transitions

    Revision 1.3  2002/03/22 02:31:45  imlucid
    Fixed registration stuff

    Revision 1.2  2002/01/30 00:17:26  imlucid
    Tweeks for registration issues

    Revision 1.1  2001/12/21 17:17:15  imlucid
    Newly added files

    Revision 1.1  2001/12/19 20:09:02  snsimon
    first checked in

    Revision 1.1  2001/09/10 20:19:13  imlucid
    Adding DSNSLCommon files

    Revision 1.1  2000/06/24 04:35:29  albegley
    Reorganized project: added plugins

    Revision 1.1.1.1  2000/06/04 18:45:50  albegley
    Importing NSLCore, formerly NSLManager

    Revision 1.1.1.1  2000/05/27 03:10:54  albegley
    NSL Manager organized for new PB

    Revision 1.4  2000/03/29 22:09:00  albegley
    Turn on the flag

    Revision 1.3  2000/03/29 03:38:07  albegley
    Change DebugLog to DBGLOG

    Revision 1.2  2000/03/24 04:33:23  albegley
    Fix Keywords


*/

#ifndef _DEBUGLOG_H_
#define _DEBUGLOG_H_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>			// getpid
#include <syslog.h>			// syslog

// Set compile flag BUILDING_NSLDEBUG=1 to enable debug logging
// Set environment variable NSLDEBUG at run time to see messages

#define BUILDING_NSLDEBUG	1
#define LOG_ALL				0

#if BUILDING_NSLDEBUG
    #if LOG_ALL
        #define DBGLOG(format, args...) \
            if (true) \
            { \
                syslog( LOG_ERR, format , ## args); \
                fflush(NULL); \
            } \
            else
    #elif DEBUG
        #define DBGLOG(format, args...) \
            if (getenv( "NSLDEBUG" )) \
            { \
                syslog( LOG_ERR, format , ## args, getpid()); \
                fflush(NULL); \
            } \
            else
    #else
        #define DBGLOG(format, args...) \
            if (getenv( "NSLDEBUG" )) \
            { \
                fprintf (stderr, format , ## args); \
                fflush(NULL); \
            } \
            else
    #endif
#else
    #define DBGLOG(format, args...)
#endif


#endif
