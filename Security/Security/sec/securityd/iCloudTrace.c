/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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


#include "iCloudTrace.h"
#include <SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecItem.h>
#include <utilities/iCloudKeychainTrace.h>
#include <securityd/SecItemServer.h>
#include <sys/stat.h>
#include <string.h>
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
#include <pwd.h>
#endif
#include <utilities/SecCFWrappers.h>

extern bool SOSCCThisDeviceDefinitelyNotActiveInCircle(void);


#define MAX_PATH 1024

/* --------------------------------------------------------------------------
	Function:		GetNumberOfItemsBeingSynced
	
	Description:	Determine the number of items being synced.  NOTE:
					This uses the SOSDataSourceFactoryRef instead of 
					calling the SecItem interface because the SecItem 
					interface requires an entitlement but the 
					SOSDataSourceFactoryRef does not.
   -------------------------------------------------------------------------- */
static int64_t GetNumberOfItemsBeingSynced()
{
	__block int64_t result = 0;
	SOSDataSourceFactoryRef dsFactRef = SecItemDataSourceFactoryGetDefault();
	CFArrayRef ds_names = dsFactRef->copy_names(dsFactRef);
    
	if (ds_names) {
        CFArrayForEach(ds_names, ^(const void *value) {
            if (isString(value)) {
                SOSManifestRef manifestRef = NULL;
                SOSDataSourceRef ds = dsFactRef->create_datasource(dsFactRef, value, NULL);
                
                if (ds) {
                    manifestRef = SOSDataSourceCopyManifest(ds, NULL);
                    if (manifestRef)
                        result += SOSManifestGetCount(manifestRef);
                }
                
                CFReleaseSafe(manifestRef);
                SOSDataSourceRelease(ds, NULL);
            }
        });
    }

	CFReleaseSafe(ds_names);
	return result;
}

/* --------------------------------------------------------------------------
	Function:		GetNumberOfPeers
	
	Description:	Determine the number of peers in the circle that this
					device is in
   -------------------------------------------------------------------------- */
static int64_t GetNumberOfPeers()
{
	int64_t result = 0;
	
	CFErrorRef error = NULL;
	CFArrayRef peers = NULL;
	
	peers = SOSCCCopyPeerPeerInfo(&error);
	if (NULL != error)
	{
		CFRelease(error);
		if (NULL != peers)
		{
			CFRelease(peers);
		}
		return result;
	}
	
	if (NULL != peers)
	{
		result = (int64_t)CFArrayGetCount(peers);
		CFRelease(peers);
	}
	
	return result;
}

static const char* kLoggingPlistPartialPath = "/Library/Preferences/com.apple.security.logging.plist";

/* --------------------------------------------------------------------------
	Function:		PathExists
	
	Description:	Utility fucntion to see if a file path exists
   -------------------------------------------------------------------------- */
static Boolean PathExists(const char* path, size_t* pFileSize)
{
	Boolean result = false;
	struct stat         sb;
	
	if (NULL != pFileSize)
	{
		*pFileSize = 0;
	}
	
	int stat_result = stat(path, &sb);
	result = (stat_result == 0);
	
    if (result)
    {
        if (S_ISDIR(sb.st_mode))
        {
            // It is a directory
            ;
        }
        else
        {
            // It is a file
            if (NULL != pFileSize)
            {
                *pFileSize = (size_t)sb.st_size;
            }
        }
    }
	return result;
}

/* --------------------------------------------------------------------------
	Function:		CopyFileContents
	
	Description:	Given a file path read the entire contents of the file 
					into a CFDataRef
   -------------------------------------------------------------------------- */
static CFDataRef CopyFileContents(const char *path)
{
    CFMutableDataRef data = NULL;
    int fd = open(path, O_RDONLY, 0666);

    if (fd == -1) 
	{
        goto badFile;
    }

    off_t fsize = lseek(fd, 0, SEEK_END);
    if (fsize == (off_t)-1) 
	{
        goto badFile;
    }

	if (fsize > (off_t)INT32_MAX) 
	{
		goto badFile;
	}

    data = CFDataCreateMutable(kCFAllocatorDefault, (CFIndex)fsize);
	if (NULL == data)
	{
		goto badFile;
	}
	
    CFDataSetLength(data, (CFIndex)fsize);
    void *buf = CFDataGetMutableBytePtr(data);
	if (NULL == buf)
	{
		goto badFile;
	}
	
    off_t total_read = 0;
    while (total_read < fsize) 
	{
        ssize_t bytes_read;

        bytes_read = pread(fd, buf, (size_t)(fsize - total_read), total_read);
        if (bytes_read == -1) 
		{
            goto badFile;
        }
        if (bytes_read == 0) 
		{
            goto badFile;
        }
        total_read += bytes_read;
    }

	close(fd);
    return data;

badFile:
    if (fd != -1) 
	{
		close(fd);
    }

    if (data)
	{	
		CFRelease(data);
	}
        
    return NULL;
}

static const CFStringRef kLoggingPlistKey = CFSTR("LoggingTime");

/* --------------------------------------------------------------------------
	Function:		CopyPlistPath
	
	Description:	Return the fully qualified file path to the logging
					plist
   -------------------------------------------------------------------------- */
static const char* CopyPlistPath()
{
    const char* result = NULL;
    CFURLRef url = NULL;
    
    
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
    CFStringRef path_string = NULL;
    const char *homeDir = getenv("HOME");
    
    if (!homeDir)
    {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }
    
    path_string = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, homeDir, kCFStringEncodingUTF8, kCFAllocatorNull);
    if (NULL == path_string)
    {
        return result;
    }
    
    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path_string, kCFURLPOSIXPathStyle, true);
    CFRelease(path_string);
#endif
 
#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
    url =  CFCopyHomeDirectoryURL();
#endif
    
    if (url)
    {
        UInt8 file_path_buffer[MAX_PATH+1];
        memset(file_path_buffer, 0, MAX_PATH);
        size_t length = 0;
        
        if(!CFURLGetFileSystemRepresentation(url, false, file_path_buffer, MAX_PATH))
        {
            CFRelease(url);
            return result;  // better to log too much than not at all
        }
	
        CFRelease(url);
	
        if (strnlen((const char *)file_path_buffer, MAX_PATH) + strlen(kLoggingPlistPartialPath) >= MAX_PATH)
        {
            return result;  // better to log too much than not at all
        }
	
        strncat((char *)file_path_buffer, kLoggingPlistPartialPath, MAX_PATH);
        length  = strlen((char *)file_path_buffer) + 1;
        result = malloc(length);
        if (NULL == result)
        {
            return result;
        }
        
        memset((void *)result, 0, length);
        strncpy((char *)result, (char *)file_path_buffer, length - 1);
        return result;
    }
    
    return NULL;
    
}

/* --------------------------------------------------------------------------
	Function:		ShouldLog
	
	Description:	Get the current time and match that against the value
					in the logging plist and see if logging should be done
   -------------------------------------------------------------------------- */
static bool ShouldLog()
{
	bool result = false;
	size_t fileSize = 0;
	CFDataRef file_data = NULL;
	CFPropertyListRef logging_time_data = NULL;
	CFDataRef time_data = NULL;
    const char* plist_path = NULL;
	
	plist_path =  CopyPlistPath();
	if (NULL == plist_path)
	{
		return true;  // better to log too much than not at all
	}
	
	if (!PathExists((const char *)plist_path, &fileSize))
	{
        free((void *)plist_path);
		return true;  // better to log too much than not at all
	}
	
	file_data = CopyFileContents((const char *)plist_path);
    free((void *)plist_path);
	if (NULL == file_data)
	{
		return true;  // better to log too much than not at all
	}
	
	logging_time_data = CFPropertyListCreateWithData(kCFAllocatorDefault, file_data, kCFPropertyListMutableContainersAndLeaves, NULL, NULL);
    CFRelease(file_data);

	if (NULL == logging_time_data)
	{
		return true;  // better to log too much than not at all
	}
	
	require_action(CFDictionaryGetTypeID() == CFGetTypeID(logging_time_data), xit, result = true); // better to log too much than not at all
	
	time_data = (CFDataRef)CFDictionaryGetValue((CFDictionaryRef)logging_time_data, kLoggingPlistKey);
	require_action(time_data, xit, result = true); // better to log too much than not at all
	
	CFAbsoluteTime startTime = 0;
    memcpy(&startTime, CFDataGetBytePtr(time_data), sizeof(startTime));

    CFAbsoluteTime endTime = CFAbsoluteTimeGetCurrent();

    int days = 0;

    CFCalendarRef gregorian = CFCalendarCopyCurrent();
    CFCalendarGetComponentDifference(gregorian, startTime, endTime, 0, "d",  &days);

    CFRelease(gregorian);

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
    if (days > 6) 
    {
		result = true;
    }
#endif
            
#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
     if (days > 0)
     {
		result = true;
     }
#endif

xit:
	CFReleaseSafe(logging_time_data);
    return result;
}

/* --------------------------------------------------------------------------
	Function:		WriteOutLoggingTime
	
	Description:	Write out the logging plist with the time that the 
					last logging was done.
   -------------------------------------------------------------------------- */
static void WriteOutLoggingTime(void)
{
	int fd = -1;
	CFAbsoluteTime now;
	CFDataRef now_data;
	CFDictionaryRef plistData = NULL;
	CFErrorRef error = NULL;
	CFDataRef output_data = NULL;
	
	const char* filepath = CopyPlistPath();
	if (NULL == filepath)
    {
		return;
    }
	
	fd = open(filepath, (O_WRONLY | O_CREAT | O_TRUNC), 0666);
    free((void *)filepath);
	if (fd <= 0)
	{
		return;  
	}
	
	now = CFAbsoluteTimeGetCurrent();
	now_data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)&now, sizeof(now));
	if (NULL == now_data)
	{
		close(fd);
		return;
	}
	
	plistData = CFDictionaryCreate(kCFAllocatorDefault, (const void **)&kLoggingPlistKey, (const void **)&now_data, 1,
                                    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (NULL == plistData)
	{
		close(fd);
		CFRelease(now_data);
		return;
	}
	CFRelease(now_data);
	
	output_data = CFPropertyListCreateData(kCFAllocatorDefault, plistData, kCFPropertyListBinaryFormat_v1_0, 0, &error);
	CFRelease(plistData);
	if (NULL != error || NULL == output_data)
	{
		close(fd);
		if (NULL != error)
		{
			CFRelease(error);
		}
		
		if (NULL != output_data)
		{
			CFRelease(output_data);
		}
		
		return;
	}
	
	write(fd, CFDataGetBytePtr(output_data), CFDataGetLength(output_data));
	close(fd);
	
	CFRelease(output_data);
	
}

/* --------------------------------------------------------------------------
	Function:		Bucket
	
	Description:	In order to preserve annominity of a user, take an
					absolute value and return back the most significant 
					value in base 10 
   -------------------------------------------------------------------------- */
static	int64_t Bucket(int64_t value)
{
    if (value < 10)
    {
       return value;
    }

    if (value < 100)
    {
        return (value / 10) * 10;
    }

    if (value < 1000)
    {
        return (value / 100) * 100;
    }

    if (value < 10000)
    {
        return (value / 1000) * 1000;
    }

    if (value < 100000)
    {
        return (value / 10000) * 10000;
    }

    if (value < 1000000)
    {
        return (value / 100000) * 10000;
    }

    return value;
}

/* --------------------------------------------------------------------------
	Function:		DoLogging
	
	Description:	If it has been determined that logging should be done
					this function will perform the logging
   -------------------------------------------------------------------------- */
static void DoLogging()
{
	int64_t value = 1;
	
	void* token = BeginCloudKeychainLoggingTransaction();
		
	value = GetNumberOfPeers();
	value = Bucket(value);
    AddKeyValuePairToKeychainLoggingTransaction(token, kNumberOfiCloudKeychainPeers, value);

	value = GetNumberOfItemsBeingSynced();
	value = Bucket(value);
	
    AddKeyValuePairToKeychainLoggingTransaction(token, kNumberOfiCloudKeychainItemsBeingSynced, value);
	CloseCloudKeychainLoggingTransaction(token);

	WriteOutLoggingTime();
}

/* --------------------------------------------------------------------------
	Function:		InitializeCloudKeychainTracing
	
	Description:	Called when secd starts up.  It will first determine if
					the device is in a circle and if so it will see if 
					logging should be done (if enough time has expired since
					the last time logging was done) and if logging should 
					be done will perform the logging.
   -------------------------------------------------------------------------- */
void InitializeCloudKeychainTracing()
{
    if (SOSCCThisDeviceDefinitelyNotActiveInCircle())   // No circle no logging
    {
		return;
    }

	if (ShouldLog())
	{
		DoLogging();
	}	
}
