/**
 * StartupItems.c - Startup Item management routines
 * Wilfredo Sanchez | wsanchez@opensource.apple.com
 * $Apple$
 **
 * Copyright (c) 1999-2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 **/

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>
#include "Log.h"
#include "main.h"
#include "StartupItems.h"

#define kStartupItemsPath "/StartupItems"
#define kParametersFile   "StartupParameters.plist"
#define kDescriptionKey   CFSTR("Description")
#define kProvidesKey      CFSTR("Provides")
#define kRequiresKey      CFSTR("Requires")
#define kUsesKey          CFSTR("Uses")
#define kPriorityKey      CFSTR("OrderPreference")
#define kBundlePathKey    CFSTR("PathToBundle")

enum {
    kPriorityLast    =  1,
    kPriorityLate    =  2,
    kPriorityNone    =  3,
    kPriorityEarly   =  4,
    kPriorityFirst   =  5,
    kPriorityNetwork = 10,
    kPriorityLocal   = 20,
};

static int priorityFromString (CFStringRef aPriority)
{
    if (aPriority)
      {
             if (CFEqual(aPriority, CFSTR("Last" ))) return kPriorityLast  ;
        else if (CFEqual(aPriority, CFSTR("Late" ))) return kPriorityLate  ;
        else if (CFEqual(aPriority, CFSTR("None" ))) return kPriorityNone  ;
        else if (CFEqual(aPriority, CFSTR("Early"))) return kPriorityEarly ;
        else if (CFEqual(aPriority, CFSTR("First"))) return kPriorityFirst ;
      }
    return kPriorityNone;
}

CFMutableArrayRef StartupItemListCreateMutable(NSSearchPathDomainMask aMask)
{
    CFMutableArrayRef anItemList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    char aPath[PATH_MAX];

    NSSearchPathEnumerationState aState = NSStartSearchPathEnumeration(NSLibraryDirectory, aMask);

    while ((aState = NSGetNextSearchPathEnumeration(aState, aPath)))
      {
        DIR* aDirectory;

        strcpy(aPath+strlen(aPath), kStartupItemsPath);

        if ((aDirectory = opendir(aPath)))
          {
            struct dirent* aBundle;

            while ((aBundle = readdir(aDirectory)))
              {
                char *aBundleName = aBundle->d_name;

                char aBundlePath[PATH_MAX];
                char aConfigFile[PATH_MAX];

                if ( !strcmp("." , aBundleName) ||
                     !strcmp("..", aBundleName) )
                    continue;

                if (gDebugFlag) debug(CFSTR("Found item: %s\n"), aBundleName);

                sprintf(aBundlePath, "%s/%s", aPath, aBundleName);
                sprintf(aConfigFile, "%s/" kParametersFile, aBundlePath);

                /* Stow away the plist data for each bundle */
                {
                    int aConfigFileDescriptor;

                    if ((aConfigFileDescriptor = open(aConfigFile, O_RDONLY, (mode_t)0)) != -1)
                      {
                        struct stat aConfigFileStatBuffer;

                        if (stat(aConfigFile, &aConfigFileStatBuffer) != -1)
                          {
                            int   aConfigFileContentsSize = aConfigFileStatBuffer.st_size;
                            char* aConfigFileContentsBuffer;

                            if ((aConfigFileContentsBuffer =
                                     mmap((caddr_t)0, aConfigFileContentsSize,
                                          PROT_READ, MAP_FILE|MAP_PRIVATE,
                                          aConfigFileDescriptor, (off_t)0)) != (caddr_t)-1)
                              {
                                CFDataRef              aConfigData = NULL;
                                CFMutableDictionaryRef aConfig     = NULL;

                                aConfigData =
                                    CFDataCreateWithBytesNoCopy(NULL, aConfigFileContentsBuffer,
								aConfigFileContentsSize, kCFAllocatorNull);

				if (aConfigData)
				  {
				    aConfig =
				      (CFMutableDictionaryRef)
				      CFPropertyListCreateFromXMLData(NULL, aConfigData,
								      kCFPropertyListMutableContainers, NULL);
				  }

				if (aConfig && CFGetTypeID(aConfig) == CFDictionaryGetTypeID())
                                  {
                                    CFStringRef aBundlePathString =
                                        CFStringCreateWithCString(NULL, aBundlePath, kCFStringEncodingUTF8);

                                    CFDictionarySetValue(aConfig, kBundlePathKey, aBundlePathString);
                                    CFArrayAppendValue(anItemList, aConfig);

                                    CFRelease(aBundlePathString);
                                  }
				else
				  error(CFSTR("Malformatted parameters file %s.\n"), aConfigFile);

                                if (aConfig    ) CFRelease(aConfig);
                                if (aConfigData) CFRelease(aConfigData);

                                if (munmap(aConfigFileContentsBuffer, aConfigFileContentsSize) == -1)
                                  { warning(CFSTR("Unable to unmap parameters file %s for item %s. (%s)\n"), aConfigFile, aBundleName, strerror(errno)); }
                              }
                            else
                              { error(CFSTR("Unable to map parameters file %s for item %s. (%s)\n"), aConfigFile, aBundleName, strerror(errno)); }
                          }
                        else
                          { error(CFSTR("Unable to stat parameters file %s for item %s. (%s)\n"), aConfigFile, aBundleName, strerror(errno)); }

                        if (close(aConfigFileDescriptor) == -1)
                          { error(CFSTR("Unable to close parameters file %s for item %s. (%s)\n"), aConfigFile, aBundleName, strerror(errno)); }
                      }
                    else
                      { error(CFSTR("Unable to open parameters file %s for item %s. (%s)\n"), aConfigFile, aBundleName, strerror(errno)); }
                }
              }
            if (closedir(aDirectory) == -1)
              { warning(CFSTR("Unable to directory bundle %s. (%s)\n"), aPath, strerror(errno)); }
          }
        else
          {
            if (errno != ENOENT)
              {
                warning(CFSTR("Open on directory %s failed. (%s)\n"), aPath, strerror(errno));
                return(NULL);
              }
          }
      }

    return (anItemList);
}

CFDictionaryRef StartupItemListGetNext (CFArrayRef aWaitingList, CFDictionaryRef aStatusDict)
{
    CFDictionaryRef aNextItem = NULL;
    CFIndex aWaitingCount  = CFArrayGetCount(aWaitingList);

    if (aWaitingList && aStatusDict && aWaitingCount > 0)
      {
        int     aMaxPriority   = kPriorityLast;
        int     aMinFailedUses = INT_MAX;
	CFIndex aWaitingIndex;

	/**
	 * Iterate through the items in aWaitingList and look for an optimally ready item.
	 **/
	for (aWaitingIndex = 0; aWaitingIndex < aWaitingCount; aWaitingIndex++)
	  {
	    CFDictionaryRef anItem        = CFArrayGetValueAtIndex(aWaitingList, aWaitingIndex);
	    CFArrayRef      aRequiresList = CFDictionaryGetValue(anItem, kRequiresKey);

	    if (gDebugFlag)
	      {
		if (gDebugFlag == 2)
		  {
		    debug(CFSTR("Checking %@.\n"), CFDictionaryGetValue(anItem, kDescriptionKey));

		    if (aRequiresList)
		      debug(CFSTR("\tRequires: %@\n"), aRequiresList);
		    else
		      debug(CFSTR("\tNo requirements.\n"));
		  }
	      }

	    /**
	     * Filter out the items which have unsatisfied requirements.
	     **/
	    if (aRequiresList)
	      {
		CFIndex aRequiresCount = CFArrayGetCount(aRequiresList);
		CFIndex aRequiresIndex;

		for (aRequiresIndex = 0; aRequiresIndex < aRequiresCount; aRequiresIndex++)
		  {
		    CFStringRef aRequirement = CFArrayGetValueAtIndex(aRequiresList, aRequiresIndex);
		    CFStringRef aStatus      = CFDictionaryGetValue(aStatusDict, aRequirement);

		    if (!aStatus || CFEqual(aStatus, kRunFailure))
		      {
			if (gDebugFlag == 2) debug(CFSTR("\tFailed requirement: %@\n"), aRequirement);

			goto next_item;
		      }
		  }
	      }

	    /**
	     * anItem has all requirements met; check for soft dependancies.
	     * We'll favor the item with the fewest unmet soft dependancies here.
	     **/
	    {
	      CFArrayRef aUsesList        = CFDictionaryGetValue(anItem, kUsesKey);
	      int        aFailedUsesCount = 0;     /* Number of unmet soft depenancies */
	      Boolean    aBestPick        = FALSE; /* Is this the best pick so far?    */

	      if (gDebugFlag == 2)
		{
		  if (aUsesList)
		    debug(CFSTR("\tUses: %@\n"), aUsesList);
		  else
		    debug(CFSTR("\tNo soft dependancies.\n"));
		}

	      if (aUsesList)
		{
		  CFIndex aUsesCount = CFArrayGetCount(aUsesList);
		  CFIndex aUsesIndex;

		  for (aUsesIndex = 0; aUsesIndex < aUsesCount; aUsesIndex++)
		    {
		      CFStringRef aUses   = CFArrayGetValueAtIndex(aUsesList, aUsesIndex);
		      CFStringRef aStatus = CFDictionaryGetValue(aStatusDict, aUses);

		      if (!aStatus || CFEqual(aStatus, kRunFailure))
			{
			  if (gDebugFlag == 2) debug(CFSTR("\tUnmet dependancy: %@\n"), aUses);
			  aFailedUsesCount++;
			}
		    }

		  if (gDebugFlag == 2 && aFailedUsesCount > 0) debug(CFSTR("\tTotal: %d\n"), aFailedUsesCount);

		  if (aFailedUsesCount > aMinFailedUses) goto next_item; /* Another item already won out */
		  if (aFailedUsesCount < aMinFailedUses) aBestPick = TRUE;
		}
	      else
		if (aMinFailedUses > 0) aBestPick = TRUE;

	      {
		int aPriority = priorityFromString(CFDictionaryGetValue(anItem, kPriorityKey));

		if (aBestPick)
		  {
		    /* anItem has less unmet dependancies than any other item so far, so it wins. */
		    if (gDebugFlag == 2)
		      debug(CFSTR("\tBest pick so far, based on failed dependancies (%d->%d).\n"),
			    aMinFailedUses, aFailedUsesCount);
		  }
		else if (aPriority > aMaxPriority)
		  {
		    /* anItem has a higher priority, so it wins. */
		    if (gDebugFlag == 2)
		      debug(CFSTR("\tBest pick so far, based on Priority (%d->%d).\n"),
			    aMaxPriority, aPriority);
		  }
		else
		  goto next_item; /* No soup for you! */

		/* We have a winner!  Update success parameters to match anItem. */
		aMinFailedUses = aFailedUsesCount;
		aMaxPriority   = aPriority;
		aNextItem      = anItem;
	      }

	    } /* End of uses section. */

	  next_item:
	    continue;

	  } /* End of waiting list loop. */

      } /* if (aWaitingList && aWaitingCount > 0) */

    return (aNextItem);
}

void StartupItemRun (CFDictionaryRef anItem, CFMutableDictionaryRef aStatusDict)
{
    if (anItem && aStatusDict)
      {
        int aSuccess = 0;

        CFStringRef aBundlePathString = CFDictionaryGetValue(anItem, kBundlePathKey);
        size_t aBundlePathCLength =
            CFStringGetMaximumSizeForEncoding(CFStringGetLength(aBundlePathString), kCFStringEncodingUTF8) + 1;
        char *aBundlePath = (char*)malloc(aBundlePathCLength);

        if (! aBundlePath)
          {
            emergency(CFSTR("malloc() failed; out of memory while running item %s"), aBundlePathString);
            return;
          }

        if (! CFStringGetCString(aBundlePathString, aBundlePath, aBundlePathCLength, kCFStringEncodingUTF8))
          {
            emergency(CFSTR("Internal error while running item %@"), aBundlePathString);
            return;
          }

        /**
         * Run the bundle
         **/
        {
            char anExecutable[PATH_MAX] = "";

            /* Compute path to excecutable */
            {
                char *tmp;
                strcpy(anExecutable, aBundlePath);		/* .../foo     */
                tmp = rindex(anExecutable, '/');    		/* /foo        */
                strncat(anExecutable, tmp, strlen(tmp));	/* .../foo/foo */
            }

            free(aBundlePath);

            if (!access(anExecutable, X_OK))
              {
                if (gNoRunFlag)
                  {
                    aSuccess = 1;
		    sleep(1 + (random() % 3));
                  }
		else
		  {
		    pid_t aProccessID = fork();

		    switch (aProccessID)
		      {
		      case -1: /* SystemStarter (fork failed) */
			aSuccess = 0;

			error(CFSTR("Failed to fork for item %@: %s\n"),
			      aBundlePathString, strerror(errno));

			break;

		      default: /* SystemStarter (fork succeeded) */
			{
			  int aStatus;

			  if (waitpid(aProccessID, &aStatus, 0) == -1)
			    {
			      error(CFSTR("Done waiting on item %@ due to unexpected condition: %s\n"),
				    aBundlePathString, strerror(errno));

			      aSuccess = 0;
			    }
			  else
			    aSuccess = (WIFEXITED(aStatus)) ? 1 : 0;
			}
			break;

		      case 0: /* Child */
			{
			  char *const aNullEnvironment[] = { NULL };
			  int anError;
			  int i;

			  if (setsid() == -1)
			    warning(CFSTR("Unable to create session for item %@: %s\n"),
				    aBundlePathString, strerror(errno));

			  for (i = getdtablesize() - 1; i > STDERR_FILENO; i--)
				(void)close(i);

			  anError = execle(anExecutable,
					   anExecutable, "start", NULL,
					   aNullEnvironment);

			  /* We shouldn't get here. */

			  error(CFSTR("Exec failed for item %@: %s\n"),
				aBundlePathString, strerror(errno));

			  exit(anError);
			}
		      }
		  }

		if (gDebugFlag && aSuccess)
		  message(CFSTR("Ran command %s start\n"), anExecutable);
              }
            else
              {
                aSuccess = 0;
                error(CFSTR("No executable file %s\n"), anExecutable);
              }
        }

        /**
         * Update aStatusDict.
         **/
        {
            CFArrayRef aProvidesList  = CFDictionaryGetValue(anItem, kProvidesKey);
            CFIndex    aProvidesCount = CFArrayGetCount(aProvidesList);
            CFIndex    aProvidesIndex;

            for (aProvidesIndex = 0; aProvidesIndex < aProvidesCount; aProvidesIndex++)
              {
                if (aSuccess)
                    CFDictionarySetValue(aStatusDict, CFArrayGetValueAtIndex(aProvidesList, aProvidesIndex), kRunSuccess);
                else
                    CFDictionarySetValue(aStatusDict, CFArrayGetValueAtIndex(aProvidesList, aProvidesIndex), kRunFailure);
              }
        }

        return;
      }

    return;
}

CFStringRef StartupItemCreateLocalizedString (CFDictionaryRef anItem, CFStringRef aString)
{
    CFStringRef aResult = NULL;
    char *aLanguage = getenv("LANGUAGE");

    if (anItem && aString && aLanguage)
      {
        CFStringRef aBundlePathString = CFDictionaryGetValue(anItem, kBundlePathKey);
        size_t aBundlePathCLength =
            CFStringGetMaximumSizeForEncoding(CFStringGetLength(aBundlePathString), kCFStringEncodingUTF8) + 1;
        char  *aBundlePath = (char*)malloc(aBundlePathCLength);
        int    aStringsFileDescriptor;
        char   aStringsFilePath[PATH_MAX];

        if (! aBundlePath)
          {
            emergency(CFSTR("malloc() failed; out of memory while running item %s"), aBundlePathString);
            return CFRetain(aString);
          }

        if (! CFStringGetCString(aBundlePathString, aBundlePath, aBundlePathCLength, kCFStringEncodingUTF8))
          {
            emergency(CFSTR("internal error while running item %@"), aBundlePathString);
            return CFRetain(aString);
          }

        sprintf(aStringsFilePath, "%s/Resources/%s.lproj/Localizable.strings", aBundlePath, aLanguage);
        
        if ((aStringsFileDescriptor = open(aStringsFilePath, O_RDONLY, (mode_t)0)) != -1)
          {
            struct stat aStringsFileStatBuffer;

            if (stat(aStringsFilePath, &aStringsFileStatBuffer) != -1)
              {
                int   aStringsFileContentsSize = aStringsFileStatBuffer.st_size;
                char* aStringsFileContentsBuffer;

                if ((aStringsFileContentsBuffer =
		     mmap((caddr_t)0, aStringsFileContentsSize,
			  PROT_READ, MAP_FILE|MAP_PRIVATE,
			  aStringsFileDescriptor, (off_t)0)) != (caddr_t)-1)
                  {
                    CFDataRef              aStringsData = NULL;
                    CFMutableDictionaryRef aStringsDict = NULL;

                    aStringsData =
                        CFDataCreateWithBytesNoCopy(NULL, aStringsFileContentsBuffer,
						    aStringsFileContentsSize, kCFAllocatorNull);

                    if (aStringsData)
		      {
			aStringsDict =
			  (CFMutableDictionaryRef)
			  CFPropertyListCreateFromXMLData(NULL, aStringsData,
							  kCFPropertyListMutableContainers, NULL);
		      }

                    if (aStringsDict && CFGetTypeID(aStringsDict) == CFDictionaryGetTypeID())
                      {
                        CFDictionaryRef aDictStringValue = CFDictionaryGetValue(aStringsDict, aString);
                        if (aDictStringValue)
                          {
                            aResult = CFRetain(aDictStringValue);
                          }
                        else
                          {
                            error(CFSTR("Unable to get Localized string from valid string file %s\n"), aStringsFilePath);
                          }
                      }
                    else
		      error(CFSTR("Malformatted strings file %s.\n"), aStringsFilePath);

                    if (aStringsDict) CFRelease(aStringsDict);
                    if (aStringsData) CFRelease(aStringsData);

                    if (munmap(aStringsFileContentsBuffer, aStringsFileContentsSize) == -1)
                      { error(CFSTR("Unable to unmap strings file %s for item %s. (%s)\n"), aStringsFilePath, aBundlePath, strerror(errno)); }
                  }
                else
                  { error(CFSTR("Unable to map strings file %s for item %s. (%s)\n"), aStringsFilePath, aBundlePath, strerror(errno)); }
              }
            else
              { error(CFSTR("Unable to stat strings file %s for item %s. (%s)\n"), aStringsFilePath, aBundlePath, strerror(errno)); }

            if (close(aStringsFileDescriptor) == -1)
              { error(CFSTR("Unable to close strings file %s for item %s. (%s)\n"), aStringsFilePath, aBundlePath, strerror(errno)); }
          }
        else
          { debug(CFSTR("Unable to open strings file %s for item %s. (%s)\n"), aStringsFilePath, aBundlePath, strerror(errno)); }
    }

    return ((aResult) ? aResult : CFRetain(aString));
}
