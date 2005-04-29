/*
 *  tools.h
 *  ifd-CCID
 *
 *  Created by JL on Mon Feb 10 2003.
 *  Copyright (c) 2003 Jean-Luc Giraud. All rights reserved.
 *  See COPYING file for license.
 *
 */

#ifndef __TOOLS_H__
#define __TOOLS_H__
#include "wintypes.h"


// Get the value of a key in a bundle according to its identifier
// This function is not time-optimal if many keys are to be searched
// on the same bundle (the bundle is re-opened and re-parsed at
// every call
const char* ParseInfoPlist(const char *bundleIdentifier, const char *keyName);


DWORD CCIDToHostLong(DWORD dword);
DWORD HostToCCIDLong(DWORD dword);
WORD CCIDToHostWord(WORD word);
WORD HostToCCIDWord(WORD word);

#define LunToSlotNb(Lun)     ((WORD)((Lun) & 0x0000FFFF))
#define LunToReaderLun(Lun)  ((WORD)((Lun) >> 16))

typedef enum {
    LogTypeSysLog = 0x00,
    LogTypeStderr = 0x01
} LogType;
typedef enum {
    LogLevelCritical    = 0x00,
    LogLevelImportant   = 0x01,
    LogLevelVerbose     = 0x02,
    LogLevelVeryVerbose = 0x03,
    LogLevelMaximum     = 0xFF    
} LogLevel;


// Logging tools
// pcLine should be called as __FILE__
// wLine should be called as __LINE__
// LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Move failed, rv = %08X", rv);
void LogMessage(const char * pcFile, WORD wLine, BYTE bLogLevel, const char* pcFormat,...);
void LogHexBuffer(const char * pcFile, WORD wLine, BYTE bLogLevel,
                  BYTE * pbData, WORD wLength, const char* pcFormat,...);

void SetLogLevel(BYTE bLogLevel);
void SetLogType(LogType bLogtype);
// Logging is only stopped for log levels > 0
void StartLogging();
void StopLogging();

#endif