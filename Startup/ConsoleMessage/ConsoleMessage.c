/**
 * ConsoleMessage.c - ConsoleMessage main
 * Wilfredo Sanchez  | wsanchez@opensource.apple.com
 * Kevin Van Vechten | kevinvv@uclink4.berkeley.edu
 * $Apple$
 **
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 **
 * The ConsoleMessage utility sends an IPC message to SystemStarter
 * containing the message specified on the command line.  SystemStarter
 * will perform the localization.  The message is also printed to
 * the system log.
 **/

#include <unistd.h>
#include <crt_externs.h>
#include <syslog.h>
#include <CoreFoundation/CoreFoundation.h>
#include "../SystemStarter/SystemStarterIPC.h"

static void usage() __attribute__((__noreturn__));
static void usage()
{
    /* char* aProgram = **_NSGetArgv(); */
    fprintf( stderr, "usage:\n"
		     "\tConsoleMessage [-v] <message>\n"
		     "\tConsoleMessage [-v] -S\n"
		     "\tConsoleMessage [-v] -F\n"
		     "\tConsoleMessage [-v] -s <service>\n"
		     "\tConsoleMessage [-v] -f <service>\n"
		     "\tConsoleMessage [-v] -q <setting>\n"
		     "\tConsoleMessage [-v] -b <path>\n"
                     "\tConsoleMessage [-v] -u\n"
                     "\noptions:\n"
                     "\t-v: verbose (prints errors to stdout)\n"
                     "\t-S: mark all services as successful\n"
                     "\t-F: mark all services as failed\n"
                     "\t-s: mark a specific service as successful\n"
                     "\t-f: mark a specific service as failed\n"
                     "\t-q: query a configuration setting\n"
                     "\t-b: load the display bundle at the specified path\n"
                     "\t-u: unload the display bundle\n");
    exit(1);
}

enum {
    kActionConsoleMessage,
    kActionSuccess,
    kActionFailure,
    kActionQuery,
    kActionLoadDisplayBundle,
    kActionUnloadDisplayBundle
};

int main (int argc, char *argv[])
{
    int		anExitCode   = 0;
    int		aVerboseFlag = 0;
    int		anAction     = kActionConsoleMessage;
    char*	aProgram     = argv[0];
    char*	anArgCStr    = NULL;
    
    /**
     * Handle command line.
     **/
    {
        char c;
        while ((c = getopt(argc, argv, "?vSFs:f:q:b:u")) != -1) {
            switch (c) {
		/* Usage */
                case '?':
                    usage();
                    break;
                    
                case 'v':
                    aVerboseFlag = 1;
                    break;
                
                case 'S':
                    anAction  = kActionSuccess;
                    anArgCStr = NULL;
                    break;

                case 'F':
                    anAction  = kActionFailure;
                    anArgCStr = NULL;
                    break;
                    
                case 's':
                    anAction  = kActionSuccess;
                    anArgCStr = optarg;
                    break;

                case 'f':
                    anAction  = kActionFailure;
                    anArgCStr = optarg;
                    break;
                    
                case 'q':
                    anAction  = kActionQuery;
                    anArgCStr = optarg;
                    break;
                    
                case 'b':
                    anAction  = kActionLoadDisplayBundle;
                    anArgCStr = optarg;
                    break;
                
                case 'u':
                    anAction  = kActionUnloadDisplayBundle;
                    break;
                    
                default:
                    fprintf(stderr, "ignoring unknown option '-%c'\n", c);
                    break;
            }
        }
	argc -= optind;
	argv += optind;
    }

    if ((anAction == kActionConsoleMessage                         && argc != 1) ||
        (anAction == kActionSuccess                                && argc != 0) ||
        (anAction == kActionFailure                                && argc != 0) ||
        (anAction == kActionQuery                                  && argc != 0) ||
        (anAction == kActionLoadDisplayBundle                      && argc != 0) ||
        (anAction == kActionUnloadDisplayBundle                    && argc != 0) )
      {
        usage();
      }

    if (getuid() != 0)
      {
        fprintf(stderr, "you must be root to run %s\n", aProgram);
        exit(1);
      }
    else
      {
        CFMutableDictionaryRef anIPCMessage  = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                                 &kCFTypeDictionaryValueCallBacks);
        
        if (anIPCMessage)
          {
            CFStringRef anArg      = NULL;
            CFIndex     aPID       = getppid();
            CFNumberRef aPIDNumber = CFNumberCreate(NULL, kCFNumberCFIndexType, &aPID);
    
            /* Parent process id is the process id of the startup item that called ConsoleMessage. */
            CFDictionarySetValue(anIPCMessage, kIPCProcessIDKey, aPIDNumber);
            CFRelease(aPIDNumber);

            if (anArgCStr)
              {
                anArg = CFStringCreateWithCString(NULL, anArgCStr, kCFStringEncodingUTF8);
              }

            if (anAction == kActionSuccess || anAction == kActionFailure)
              {
                CFBooleanRef aStatus = (anAction == kActionSuccess) ? kCFBooleanTrue : kCFBooleanFalse;
                CFDictionarySetValue(anIPCMessage, kIPCMessageKey, kIPCStatusMessage);
                CFDictionarySetValue(anIPCMessage, kIPCStatusKey, aStatus);
                if (anArg) CFDictionarySetValue(anIPCMessage, kIPCServiceNameKey, anArg);
              }
            else if (anAction == kActionQuery && anArg)
              {
                CFDictionarySetValue(anIPCMessage, kIPCMessageKey, kIPCQueryMessage);
                CFDictionarySetValue(anIPCMessage, kIPCConfigSettingKey, anArg);
              }
            else if (anAction == kActionLoadDisplayBundle && anArg)
              {
                CFDictionarySetValue(anIPCMessage, kIPCMessageKey, kIPCLoadDisplayBundleMessage);
                CFDictionarySetValue(anIPCMessage, kIPCDisplayBundlePathKey, anArg);
              }
            else if (anAction == kActionUnloadDisplayBundle)
              {
                CFDictionarySetValue(anIPCMessage, kIPCMessageKey, kIPCUnloadDisplayBundleMessage);
              }
            else if (anAction == kActionConsoleMessage)
              {
                char*       aConsoleMessageCStr = argv[0];
                CFStringRef aConsoleMessage     = CFStringCreateWithCString(NULL, aConsoleMessageCStr, kCFStringEncodingUTF8);
 
                syslog(LOG_INFO, "%s", aConsoleMessageCStr);

                CFDictionarySetValue(anIPCMessage, kIPCMessageKey, kIPCConsoleMessage);
                CFDictionarySetValue(anIPCMessage, kIPCConsoleMessageKey, aConsoleMessage);
                CFRelease(aConsoleMessage);
              }
              
            if (anArg) CFRelease(anArg);
              
            {
            CFMessagePortRef aPort = CFMessagePortCreateRemote(NULL, kSystemStarterMessagePort);
            if (aPort)
              {
                CFDataRef aData = CFPropertyListCreateXMLData(NULL, anIPCMessage);
                if (aData)
                  {
                    CFDataRef aResultData = NULL;
                    
                    int aResult = CFMessagePortSendRequest(aPort, 
                                            kIPCProtocolVersion,
                                            aData, 
                                            0.0, 
                                            30.0, 
                                            kCFRunLoopDefaultMode, 
                                            &aResultData);
                    if (aResult == kCFMessagePortSuccess)
                      {
                        /* aResultData should be ASCIZ */
                        if (aResultData)
                          {
                            fprintf(stdout, "%s", CFDataGetBytePtr(aResultData));
                            CFRelease(aResultData);
                          }
                      }
                    else
                      {
                        if (aVerboseFlag) fprintf(stderr, "Failed to send message to SystemStarter: %d\n", aResult);
                        anExitCode = 1;
                      }
                    CFRelease(aData);
                  }
                CFRelease(aPort);
              }
            else
              {
                char*       aConsoleMessageCStr = argv[0];
                fprintf(stdout, "%s\n", aConsoleMessageCStr);

                if (aVerboseFlag) fprintf(stderr, "%s could not connect to SystemStarter.\n", aProgram);
                anExitCode = 0;
              }
            }
          }
        else
          {
            if (aVerboseFlag) fprintf(stderr, "%s: not enough memory to create IPC message.\n", aProgram);
            anExitCode = 1;
          }
      }
    exit(anExitCode);
}
