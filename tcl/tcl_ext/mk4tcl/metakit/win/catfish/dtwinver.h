/*
Module : DTWINVER.H
Purpose: Interface of a function to perform
         version detection on OS
Created: PJN / DATE/2 / 11-05-1996
History: None

Copyright (c) 1996 by PJ Naughter.  
All rights reserved.

*/

#ifndef __DTWINVER_H__                                          

////////////////////////////////// Includes ///////////////////////////////////


////////////////////////////////// defines ////////////////////////////////////

//values which get stored in [OS_VERSION_INFO].dwEmulatedPlatformId 
//and [OS_VERSION_INFO].dwUnderlyingPlatformId 
const DWORD PLATFORM_WIN32S =            0;
const DWORD PLATFORM_WINDOWS   =         1;
const DWORD PLATFORM_NT_WORKSTATION =    2; 
const DWORD PLATFORM_WINDOWS31 =         3;  
const DWORD PLATFORM_WINDOWSFW =         4;
const DWORD PLATFORM_DOS =               5;
const DWORD PLATFORM_NT_SERVER =         6;
const DWORD PLATFORM_NT_ADVANCEDSERVER = 7;


typedef struct _OS_VERSION_INFO
{
  DWORD dwOSVersionInfoSize;
                  
  //What version of OS is being emulated
  DWORD dwEmulatedMajorVersion;
  DWORD dwEmulatedMinorVersion;
  DWORD dwEmulatedBuildNumber;
  DWORD dwEmulatedPlatformId;
#ifdef _WIN32                    
  TCHAR szEmulatedCSDVersion[128];
#else  
  char szEmulatedCSDVersion[128];
#endif  

  //What version of OS is really running                 
  DWORD dwUnderlyingMajorVersion;
  DWORD dwUnderlyingMinorVersion;
  DWORD dwUnderlyingBuildNumber;
  DWORD dwUnderlyingPlatformId;   
#ifdef _WIN32                      
  TCHAR szUnderlyingCSDVersion[128];
#else  
  char szUnderlyingCSDVersion[128];
#endif  
} OS_VERSION_INFO, *POS_VERSION_INFO, FAR *LPOS_VERSION_INFO;
                                          
                                          
/////////////////////////////// Functions /////////////////////////////////////
BOOL GetOSVersion(LPOS_VERSION_INFO lpVersionInformation);


#endif //__DTWINVER_H__