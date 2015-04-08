//
//  ValidateAsset.c
//  CertificateTool
//
//  Copyright (c) 2012-2013 Apple Inc. All Rights Reserved.
//

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <CommonCrypto/CommonRSACryptor.h>
#include <CommonNumerics/CommonBaseXX.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>


static int ValidateFilesInDirectory(const char* dir_path, int num_files, const char * files[])
{
    
	int result = 0; 	// Assume all is well
	DIR* dirp = NULL;
	struct dirent* dp = NULL;
	
	dirp = opendir(dir_path);
	if (NULL == dirp)
	{
		return -1;
	}
	
	for (int iCnt = 0; iCnt < num_files; iCnt++)
	{
		int name_length = (int)strlen(files[iCnt]);
		int found = 0;
		while (NULL != (dp = readdir(dirp)))
		{
			if (dp->d_namlen == name_length && 0 == strcmp(dp->d_name, files[iCnt]))
			{
				found = 1;
                break;
			}
		}
		if (0 == found)
		{
			(void)closedir(dirp);
            
			return -1;
		}
		rewinddir(dirp);
	}
	(void)closedir(dirp);
	return result;
}

static int ReadFileIntoCFDataRef(const char* file_path, CFDataRef* out_data)
{
	int result = -1; // guilt until proven
	FILE* infile = NULL;
	void* buffer = NULL;
	int numbytes = 0;
	
	if (NULL == file_path || NULL == out_data)
	{
		return result;
	}
	
	infile = fopen(file_path, "r");
	if (NULL == infile)
	{
		return result;
	}
	
	fseek(infile, 0L, SEEK_END);
	numbytes = (int)ftell(infile);
	
	fseek(infile, 0L, SEEK_SET);
	buffer = calloc(numbytes, sizeof(char));
	if (NULL == buffer)
	{
		fclose(infile);
		return result;
	}
	
	fread(buffer, sizeof(char), numbytes, infile);
	fclose(infile);
	
	*out_data =  CFDataCreate(kCFAllocatorDefault, (const UInt8 *)buffer, numbytes);
	free(buffer);
	result = (NULL != *out_data) ? 0 : -1;
	return result;
}

static int CreateHashForData(CFDataRef cfData, int useSHA1, CFDataRef* out_hash)
{
	int result = -1; // Guilty until proven
	CCDigestAlgorithm algo = (useSHA1) ? kCCDigestSHA1 :kCCDigestSHA256;
	size_t digest_length = (useSHA1) ? CC_SHA1_DIGEST_LENGTH : CC_SHA256_DIGEST_LENGTH;
	UInt8 buffer[digest_length];
    
	if (NULL == cfData || NULL == out_hash)
	{
		return result;
	}
	
	*out_hash  = NULL;
	
	memset(buffer, 0, digest_length);
	
	if (!CCDigest(algo, CFDataGetBytePtr(cfData), CFDataGetLength(cfData), buffer))
	{
		*out_hash = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)buffer, digest_length);
	}
	else
	{
		return result;
	}
	
	result = (NULL == *out_hash) ? -1 : 0;
	return result;
}

static int Base64Data(CFDataRef cfData, int for_encoding, CFDataRef* encoded_data)
{
	int result = -1; // Guilty until proven
	CNEncodings encoding = kCNEncodingBase64;
	CNStatus status = kCCSuccess;
	CNEncodingDirection direction = (for_encoding) ? kCNEncode : kCNDecode;
	unsigned char buffer[1024];
	size_t encoded_data_length = 1024;
	
	if (NULL == cfData || NULL == encoded_data)
	{
		return result;
	}
	memset(buffer, 0, 1024);
	*encoded_data = NULL;
	
	status = CNEncode(encoding, direction, CFDataGetBytePtr(cfData), CFDataGetLength(cfData), buffer, &encoded_data_length);
	if (kCCSuccess == status)
	{
		*encoded_data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)buffer, encoded_data_length);
		result = (NULL == *encoded_data) ? -1 : 0;
	}
	return result;
}

static int CreatePropertyListFromData(CFDataRef prop_data,  CFTypeID output_type, CFTypeRef* plistRef)
{
	int result = -1; // Guilt until proven
	CFPropertyListRef aPlistRef = NULL;
	CFPropertyListFormat list_format = kCFPropertyListXMLFormat_v1_0;
	CFErrorRef error = NULL;
	
	if (NULL == prop_data || NULL == plistRef)
	{
		return result;
	}
	
	*plistRef = NULL;
	
	aPlistRef = CFPropertyListCreateWithData(kCFAllocatorDefault, prop_data, 0, &list_format, &error);
	if (NULL != error || NULL == aPlistRef)
	{
        if (NULL != aPlistRef)
        {
            CFRelease(aPlistRef);
        }
        
		if (NULL != error)
		{
			CFRelease(error);
		}
		return result;
	}
	
	if (CFGetTypeID(aPlistRef) != output_type)
	{
		CFRelease(aPlistRef);
		return result;
	}
	
	*plistRef = aPlistRef;
    result = (NULL == *plistRef) ? -1 : 0;
	return result;
}



int ValidateAsset(const char* asset_dir_path, unsigned long current_version)
{
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
    const char* files[] =
	{
        "AssertVersion.plist",
		"Blocked.plist",
		"EVRoots.plist",
		"certsIndex.data",
		"certsTable.data",
		"manifest.data"
	};
	int num_files = (sizeof(files) / sizeof(const char*));
	int iCnt = 0;
	const char* file_name = NULL;
    char wd_buf[1024];
    
	const char* current_working_directory_path = getcwd(wd_buf, 1024);
	CFDataRef file_data = NULL;
	CFDataRef hash_data = NULL;
	CFDataRef manifest_hash_data = NULL;
	CFStringRef key_name = NULL;
	CFDictionaryRef manifest_dict = NULL;
    CFNumberRef manifest_version = NULL;
    unsigned long manifest_verson_number;
	int iResult = -1;
	OSStatus err = errSecSuccess;
	CMSDecoderRef cmsDecoder = NULL;
	size_t numSigners = 0;
	SecPolicyRef x509Policy = NULL;
	CMSSignerStatus signedStatus = kCMSSignerUnsigned;
	OSStatus resultCode = errSecSuccess;
	CFDataRef cmsMsg = NULL;
	
	// parameter check
	if (NULL == asset_dir_path)
	{
		return iResult;
	}
	
	if (ValidateFilesInDirectory(asset_dir_path, num_files, files))
	{
		return iResult;
	}
	
	if (chdir(asset_dir_path))
	{
		return iResult;
	}
	
	if (ReadFileIntoCFDataRef("manifest.data", &file_data))
	{
		(void)chdir(current_working_directory_path);
		return iResult;
	}
    
    if (NULL == file_data)
    {
        return iResult;
    }
	
	err = CMSDecoderCreate(&cmsDecoder);
	if (errSecSuccess != err)
	{
		CFRelease(file_data);
		(void)chdir(current_working_directory_path);
		return iResult;
	}
	
	err = CMSDecoderUpdateMessage(cmsDecoder, CFDataGetBytePtr(file_data), CFDataGetLength(file_data));
	CFRelease(file_data);
	if (errSecSuccess != err)
	{
		CFRelease(cmsDecoder);
		(void)chdir(current_working_directory_path);
		return iResult;
	}
	
	err = CMSDecoderFinalizeMessage(cmsDecoder);
	if (errSecSuccess != err)
	{
		CFRelease(cmsDecoder);
		(void)chdir(current_working_directory_path);
		return iResult;
	}
	
	err = CMSDecoderGetNumSigners(cmsDecoder, &numSigners);
	if (errSecSuccess != err || 0 == numSigners)
	{
		CFRelease(cmsDecoder);
		(void)chdir(current_working_directory_path);
		return iResult;
	}
	
	x509Policy = SecPolicyCreateBasicX509();
	if (NULL == x509Policy)
	{
		CFRelease(cmsDecoder);
		(void)chdir(current_working_directory_path);
		return iResult;
	}
	
	err = CMSDecoderCopySignerStatus(cmsDecoder, 0, x509Policy, true, &signedStatus, NULL, &resultCode);
	CFRelease(x509Policy);
	if (errSecSuccess != err || kCMSSignerValid != signedStatus || errSecSuccess != resultCode)
	{
		CFRelease(cmsDecoder);
		(void)chdir(current_working_directory_path);
		return iResult;
	}
	
	err = CMSDecoderCopyContent(cmsDecoder, &cmsMsg);
	CFRelease(cmsDecoder);
	if (errSecSuccess != err)
	{
		(void)chdir(current_working_directory_path);
		return iResult;
	}
	
	if (CreatePropertyListFromData(cmsMsg,  CFDictionaryGetTypeID(), (CFTypeRef *)&manifest_dict))
	{
        if (NULL != manifest_dict)
        {
            CFRelease(manifest_dict);
        }
		CFRelease(cmsMsg);
		(void)chdir(current_working_directory_path);
		return iResult;
	}
	CFRelease(cmsMsg);
    
	// Validate the hash for the files in the manifest
	for (iCnt = 0; iCnt < num_files; iCnt++)
	{
		file_name = files[iCnt];
		// bypass the manifest file for now
		if (!strcmp("manifest.data", file_name))
		{
			continue;
		}
		
		if (ReadFileIntoCFDataRef(file_name, &file_data))
		{
			CFRelease(manifest_dict);
			(void)chdir(current_working_directory_path);
			return iResult;
		}
		
		if (CreateHashForData(file_data, 0, &hash_data))
		{
            if (NULL != file_data)
            {
                CFRelease(file_data);
            }
			CFRelease(manifest_dict);
			(void)chdir(current_working_directory_path);
			return iResult;
		}
		CFRelease(file_data);
        
		key_name = CFStringCreateWithCString(kCFAllocatorDefault, file_name, kCFStringEncodingUTF8);
		if (NULL == key_name)
		{
			CFRelease(manifest_dict);
			(void)chdir(current_working_directory_path);
			return iResult;
		}
		
		manifest_hash_data = (CFDataRef)CFDictionaryGetValue(manifest_dict, key_name);
		if (NULL == manifest_hash_data)
		{
			CFRelease(key_name);
			CFRelease(hash_data);
			CFRelease(manifest_dict);
			(void)chdir(current_working_directory_path);
			return iResult;
		}
		CFRelease(key_name);
		
		if (!CFEqual(hash_data, manifest_hash_data))
		{
			CFRelease(hash_data);
			CFRelease(manifest_dict);
			(void)chdir(current_working_directory_path);
			return iResult;
		}
		CFRelease(hash_data);
	}

	
	// Get the version
    manifest_version = (CFNumberRef)CFDictionaryGetValue(manifest_dict, CFSTR("VersionNumber"));
    if (NULL == manifest_version)
    {
        CFRelease(manifest_dict);
		(void)chdir(current_working_directory_path);
		return iResult;
    }
    
    if (!CFNumberGetValue(manifest_version, kCFNumberLongType, &manifest_verson_number))
    {
        CFRelease(manifest_version);
        CFRelease(manifest_dict);
		(void)chdir(current_working_directory_path);
		return iResult;
    }
    
    CFRelease(manifest_version);
    if (manifest_verson_number < current_version)
    {
        CFRelease(manifest_dict);
		(void)chdir(current_working_directory_path);
		return iResult;
    }
   
    iResult = 0;
	return iResult;
#else
    return -1;
#endif
}
