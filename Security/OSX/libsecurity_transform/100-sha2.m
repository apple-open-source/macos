/*
 * Copyright (c) 2010-2011,2014 Apple Inc. All Rights Reserved.
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



#include "SecTransform.h"
#include "SecCustomTransform.h"
#include "SecDigestTransform.h"
#include <assert.h>
#include <unistd.h>

const CFStringRef kCaesarCipher = CFSTR("com.yourcompany.caesarcipher");
const CFStringRef kKeyAttributeName = CFSTR("key");

// =========================================================================
//	Registration function for a ROT-N (caesar cipher)
// =========================================================================
Boolean RegisterMyCustomTransform()
{
	static dispatch_once_t once;
	__block Boolean ok = FALSE;
	__block CFErrorRef error = NULL;
	
	SecTransformCreateBlock createCaesar = NULL;
	
	// Create the SecTransformCreateBlock block that will be used to 
	// register this custom transform
	createCaesar =^(CFStringRef name, SecTransformRef new_transform, 
					const SecTransformCreateBlockParameters* parameters) 
	{
		
		CFErrorRef result = NULL;
		
		// Some basic parameter checking.
		if (NULL == name || NULL == new_transform )
		{
			// return the error
			result = CFErrorCreate(kCFAllocatorDefault, SECTRANSFORM_ERROR_DOMAIN, 
								   kSecTransformErrorInvalidInput, NULL);
			
			return result;
		}
		// Every time a new instance of this custom transform class is 
		// created, this block will be called. This behavior means that any 
		// block variables created in this block will act like instance
		// variables for the new custom transform instance.  In this case 
		// the key variable will be in every instance of this custom 
		// caesar transform
		
		__block int _key;
		
		// Use the overrideAttribute block to have our custom transform 
		// be notified if the 'key' attribute is set
		
		parameters->overrideAttribute(
		  kSecCustomTransformAttributeSetNotification, 
		  kKeyAttributeName,
		  ^(SecTransformAttributeRef name, CFTypeRef d)
		  {
			  CFTypeRef result = NULL;
			  
			  if (NULL == d)
			  {
				  _key = 0;
				  return result;
			  }
			  
			  // Ensure the correct data type for this attribute
			  if (CFGetTypeID(d) == CFNumberGetTypeID())
			  {
				  _key = 0;
				  
				  if (!CFNumberGetValue((CFNumberRef)d, 
										kCFNumberIntType, &_key))
				  {
					  _key = 0;
					  // return the error
					  result = (CFTypeRef)CFErrorCreate(kCFAllocatorDefault, SECTRANSFORM_ERROR_DOMAIN, 
														kSecTransformErrorInvalidInput, NULL);
					  
					  return result;							
				  }
				  else 
				  {
					  result = d;
				  }
			  }
			  
			  return result;
			  
		  });
		
		// Use the overrideAttribute to change the processing of the data 
		// for this transform
		parameters->overrideAttribute(kSecCustomTransformProcessData, 
		  NULL, 
		  ^(SecTransformAttributeRef name, CFTypeRef d)
		  {
			  CFTypeRef result = NULL;
			  if (NULL == d)
			  {
				  // return the error
				  result = (CFTypeRef)CFErrorCreate(kCFAllocatorDefault, SECTRANSFORM_ERROR_DOMAIN, 
													kSecTransformErrorInvalidInput, NULL);
				  
				  return result;
			  }
			  
			  if (CFGetTypeID(d) != CFDataGetTypeID())
			  {
				  // return the error
				  result = (CFTypeRef)CFErrorCreate(kCFAllocatorDefault, SECTRANSFORM_ERROR_DOMAIN, 
													kSecTransformErrorInvalidInput, NULL);
				  
				  return result;
			  }
			  
			  CFDataRef theData = (CFDataRef)d;
			  
			  CFIndex dataLength = CFDataGetLength(theData);
			  
			  // Do the processing in memory.  There are better ways to do 
			  // this but for showing how custom transforms work this is fine.
			  char* buffer = (char*)malloc(dataLength);
			  if (NULL == buffer)
			  {
				  //return the error
				  result = (CFErrorRef)CFErrorCreate(kCFAllocatorDefault, SECTRANSFORM_ERROR_DOMAIN, 
													 kSecTransformErrorInvalidInput, NULL);
				  
				  return result;
			  }
			  
			  const char* dataPtr = (const char* )CFDataGetBytePtr(theData);
			  if (NULL == dataPtr)
			  {
				  free(buffer);
				  //return the error
				  result = (CFErrorRef)CFErrorCreate(kCFAllocatorDefault, SECTRANSFORM_ERROR_DOMAIN, 
													 kSecTransformErrorInvalidInput, NULL);
				  
				  return result;
			  }
			  // Do the work of the caesar cipher (Rot(n))
			  
			  char rotValue = (char)_key;
			  CFIndex iCnt;
			  for (iCnt = 0; iCnt < dataLength; iCnt++)
			  {
				  buffer[iCnt] = dataPtr[iCnt] + rotValue;
			  }
			  
			  result = (CFTypeRef)CFDataCreate(kCFAllocatorDefault, 
											   (const UInt8 *)buffer, dataLength);
			  free(buffer);
			  return result;
			  
		  });
	};
	
	// Make sure the custom transform is only registered once	
	dispatch_once(&once, 
				  ^{ 
					  (void)SecCustomTransformRegister(kCaesarCipher, &error, 
													   createCaesar);
				  });
	
	return (error == NULL);
} 

//The second function show how to use the this custom transform:

// =========================================================================
//	Use a custom ROT-N (caesar cipher) transform
// =========================================================================
CFStringRef DoCaesar(CFStringRef clearTest, int rotNumber)
{
	CFStringRef result = NULL;
	
	if (NULL == clearTest)
	{
		return result;
	}
	
	if (!RegisterMyCustomTransform())
	{
		return result;
	}
	
	CFErrorRef error = NULL;
	
	SecTransformRef caesarCipher = 
	SecCustomTransformCreate(kCaesarCipher, &error);
	if (NULL == caesarCipher || NULL != error)
	{
		return result;
	}
	
	CFDataRef  data = 
	CFStringCreateExternalRepresentation(kCFAllocatorDefault, 
										 clearTest, kCFStringEncodingUTF8, 0);
	if (NULL == data)		
	{
		CFRelease(caesarCipher);
		return result;
	}
	
	SecTransformSetAttribute(caesarCipher, 
							 kSecTransformInputAttributeName, data, &error);
	CFRelease(data);
	if (NULL != error)
	{
		CFRelease(caesarCipher);
		return result;
	}
	
	CFNumberRef keyNumber = 
	CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &rotNumber);
	if (NULL == keyNumber)
	{
		CFRelease(caesarCipher);
		return result;
	}
	
	SecTransformSetAttribute(caesarCipher, kKeyAttributeName, 
							 keyNumber, &error);
	CFRelease(keyNumber);
	if (NULL != error)
	{
		CFRelease(caesarCipher);
		return result;
	}
	
	CFDataRef dataResult = 
	(CFDataRef)SecTransformExecute(caesarCipher, &error);
	CFRelease(caesarCipher);
	if (NULL != dataResult && NULL == error)
	{
		result = 
		CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, 
												 dataResult, kCFStringEncodingUTF8);	
		CFRelease(dataResult);
	}
	
	return result;
}

int main(int argc, char *argv[]) 
{
	if (!RegisterMyCustomTransform())
	{
		return -1;
	}
	
	CFStringRef testStr = CFSTR("When in the course of human event");
	CFStringRef aStr = DoCaesar(testStr, 4);
	CFStringRef clearStr = DoCaesar(aStr, -4);
	if (CFEqual(testStr, clearStr))
	{
		CFShow(CFSTR("All is right with the world"));
		return 0;
	}
	
	CFShow(CFSTR("Bummer!"));
	return -1;
	
}



/*
CFReadStreamRef CFReadStreamCreateWithFD(CFAllocatorRef a, int fd) {
	char *fname;
	asprintf(&fname, "/dev/fd/%d", fd);
	CFURLRef f = CFURLCreateFromFileSystemRepresentation(a, (UInt8*)fname, strlen(fname), FALSE);
	CFReadStreamRef rd = CFReadStreamCreateWithFile(a, f);
	CFRelease(f);
	
	return rd;
}

void pair_CFReadStream_fd(CFReadStreamRef *sr, int *fd) {
	int fds[2];
	int rc = pipe(fds);
	assert(rc >= 0);
	*fd = fds[1];
	*sr = CFReadStreamCreateWithFD(NULL, fds[0]);
}

CFReadStreamRef many_zeros(uint64_t goal) {
	CFReadStreamRef rd;
	int fd;
	pair_CFReadStream_fd(&rd, &fd);
	
	// XXX: replace with a dispatch_source and non-blocking I/O...
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{
		uint64_t nwrites = 0;
		char buf[1024*8];
		bzero(buf, sizeof(buf));
		uint64_t left = goal;
		
		while (left) {
			size_t try = (sizeof(buf) < left) ? sizeof(buf) : left;
			ssize_t sz = write(fd, buf, try);
			if (sz <= 0) {
				fprintf(stderr, "Write return %zd, errno=%d\n", sz, errno);
			}
			assert(sz >= 0);
			left -= sz;
			nwrites++;
		}
		
		close(fd);
	});
	
	
	return rd;
}

int main(int argc, char *argv[]) {
	CFReadStreamRef rd = many_zeros(1024*1024 *100LL);
	Boolean ok = CFReadStreamOpen(rd);
	assert(ok);
	
	SecTransformRef dt = SecDigestTransformCreate(kSecDigestSHA2, 512, NULL);
	SecTransformSetAttribute(dt, kSecTransformInputAttributeName, rd, NULL);
	
	CFDataRef d = SecTransformExecute(dt, NULL);
	
	return 0;
}
*/
