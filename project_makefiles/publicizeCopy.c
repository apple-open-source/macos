/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 */

#ifdef __APPLE__
#import "make.h"
#include <stdlib.h>
#include <string.h>
#include <mach/mach_init.h>
#include <servers/netname.h>
#include <stdio.h>

typedef int BOOL;

static char * tentative_path;  // The one we are currently potentially copying
static char cumulative_copies[4096]; // All copied targets so far
static BOOL alreadyCommit;  // Keeps us from putting things out multiple times

static void reallyPublicizeCopy(char *path)
{
    char *portName = getenv("MAKEPORT");
    char *hostName = getenv("MAKEHOST");
    mach_port_t port;
    static BOOL port_set = FALSE;
    char * fileName = rindex(path,'/')+1;

#ifdef BE_NOISY_ON_STDOUT    
    printf("Copying %s\n",fileName);
#else
    strcat(cumulative_copies, " ");
    strcat(cumulative_copies, fileName);
#endif

    if (!port_set) {
      if (!portName)
	  return;
	    
      if (!hostName)
	  hostName = "";
	    
      if ((netname_look_up (name_server_port, hostName, portName, &port)
      	  != KERN_SUCCESS)
	  || (port == MACH_PORT_NULL))
	  return;
	  
      port_set = TRUE;
    }
	
    make_alert(port,
               -1,
	       NULL, 0,
	       fileName, strlen(fileName) + 1,
	       NULL, 0,
	       0,
	       "Copying", 8);
}
    

void initializeCopyPublicity()
{
  tentative_path = NULL;
  cumulative_copies[0] = '\0';
}


void finalizeCopyPublicity()
{
  if (cumulative_copies[0] != '\0') {
    printf("Copied%s\n",cumulative_copies);
    cumulative_copies[0] = '\0';
  }
}


void publicizeCopy(char *path, int tentative)
{
    if (!tentative_path) {
      /* Want to ignore lower levels if we've already set the 
         top level tentative directory we are copying */
      if (tentative)
         tentative_path = path;
      else 
         reallyPublicizeCopy(path);
    }
}



void commitTentativePublicity()
{
   if ((tentative_path) && !alreadyCommit) {
     reallyPublicizeCopy(tentative_path);  // Only commit once
     alreadyCommit = TRUE;
   }
}

void abortTentativePublicity()
{
   tentative_path = NULL;
   alreadyCommit = FALSE;  // reset commit flag
}

#else

void initializeCopyPublicity() {}
void finalizeCopyPublicity() {}
void publicizeCopy(const char* fileName, int tentative) {}
void commitTentativePublicity() {}
void abortTentativePublicity() {}

#endif
