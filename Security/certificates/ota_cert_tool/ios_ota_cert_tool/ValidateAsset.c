//
//  ValidateAsset.c
//  ios_ota_cert_tool
//
//  Created by James Murphy on 12/13/12.
//  Copyright (c) 2012 James Murphy. All rights reserved.
//

#include <CoreFoundation/CoreFoundation.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <CommonCrypto/CommonRSACryptor.h>
#include <CommonNumerics/CommonBaseXX.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>


static const char* kPublicManifestKeyData = "MIIBCgKCAQEA7eev+hip+8Vg1kj/q4qnpN37X8vaKZouAyoXZ6gy+2D2wKxR0KORuV9bCFkcyT+LST/Rhn+64YNSZ7UvkQRlU34vZcF7FWuPfEbLGcCG7e1hlshHVUUah+07Qyu82f6OAg8PBFvYwvZHZMcXlvZJQjdNtbIORQfdlrGpRN1C6xKfbX6IKE9LViGQJmljdRuaK/SxmKyMsLfsTCzh+6yxMpPtY75PuSfrVcDSlGhr108QfP5n2WQ9frtyFgdowlXr/kECWSUrj8qDk1JymVd2ZyF3dlTWdzSO17vDt6caQyjQmTyGrGRGOM6THSq9mB/fv1Q5gxEfIIb8SejTvu4GSwIDAQAB";

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


static int TearOffSignatureAndHashManifest(CFDictionaryRef manifestDict, CFDataRef* signature, CFDataRef* manifest_data)
{
	int result = -1;
	CFMutableDictionaryRef new_manifest_dict = NULL;
    CFStringRef sig_data_str = NULL;
	CFDataRef sig_data = NULL;
	CFDataRef prop_list = NULL;
    CFDataRef decoded_sig_data = NULL;
	
	if (NULL == manifestDict || NULL == signature || NULL == manifest_data)
	{
		return result;
	}
	*signature = NULL;
	*manifest_data = NULL;
	
	new_manifest_dict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(manifestDict), manifestDict);
	sig_data_str = (CFStringRef)CFDictionaryGetValue(new_manifest_dict, CFSTR("Signature"));
	if (NULL == sig_data_str)
	{
		CFRelease(new_manifest_dict);
		return result; 
	}
    
    sig_data = CFStringCreateExternalRepresentation(kCFAllocatorDefault, sig_data_str, kCFStringEncodingUTF8, 0);
    if (NULL == sig_data)
    {
        CFRelease(sig_data_str);
        CFRelease(new_manifest_dict);
		return result;
    }
    
    if (Base64Data(sig_data, 0, &decoded_sig_data))
    {
        CFRelease(sig_data);
        CFRelease(new_manifest_dict);
		return result;
    }
        
	*signature = decoded_sig_data;
	
	CFDictionaryRemoveValue(new_manifest_dict, CFSTR("Signature"));
	prop_list = CFPropertyListCreateXMLData (kCFAllocatorDefault, new_manifest_dict);
	CFRelease(new_manifest_dict);
	if (NULL == prop_list)
	{
		return result; 
	}
    
    (void)CreateHashForData(prop_list, 1, manifest_data);
	
    result = (NULL == *manifest_data) ? -1 : 0;
	return result;	
}

static int GetPublicManifestKey(CCRSACryptorRef* key_ref)
{
	int result = -1;
	
	CFStringRef encoded_key_data_str = NULL;
	CFDataRef encoded_key_data_str_data = NULL;
	CFDataRef decoded_key_data = NULL;
	CCCryptorStatus ccStatus = kCCSuccess;
	
	if (NULL == key_ref)
	{
		return result;
	}
	*key_ref = NULL;
		
	encoded_key_data_str = CFStringCreateWithCString(kCFAllocatorDefault, kPublicManifestKeyData, kCFStringEncodingUTF8);
	if (NULL == encoded_key_data_str)
	{
		return result;
	}
	
	encoded_key_data_str_data = CFStringCreateExternalRepresentation(kCFAllocatorDefault, encoded_key_data_str, kCFStringEncodingUTF8, 0);
	if (NULL == encoded_key_data_str_data)
	{
		CFRelease(encoded_key_data_str);
		return result;
	}
	CFRelease(encoded_key_data_str);
	
	if (Base64Data(encoded_key_data_str_data, 0, &decoded_key_data))
    {
		CFRelease(encoded_key_data_str_data);
        return result;
    }
	CFRelease(encoded_key_data_str_data);
	
	ccStatus = CCRSACryptorImport(CFDataGetBytePtr(decoded_key_data), CFDataGetLength(decoded_key_data), key_ref);
	CFRelease(decoded_key_data);
	
	if (kCCSuccess != ccStatus)
	{
		*key_ref = NULL;
	}
	else
	{
		result = 0;
	}
	return result;	
}


static int ValidateSignature(CFDataRef signature, CFDataRef data)
{
	int result = -1;
	CCRSACryptorRef key_ref = NULL;
	CCCryptorStatus ccStatus = kCCSuccess;
	
	
	if (NULL == signature || NULL == data)
	{
		return result;
	}
	
	// Get the key
	if (GetPublicManifestKey(&key_ref))
	{
		return result;
	}
	
    const void *hash_data_ptr = CFDataGetBytePtr(data);
    size_t hash_data_len = CFDataGetLength(data);
    
    const void* sig_data_pre = CFDataGetBytePtr(signature);
    size_t sig_dat_len = CFDataGetLength(signature);
    
	ccStatus = CCRSACryptorVerify(
                key_ref,                
                ccPKCS1Padding,         
                hash_data_ptr,
                hash_data_len,
                kCCDigestSHA1,
                0,
                sig_data_pre,
                sig_dat_len);
    
    
	CCRSACryptorRelease(key_ref);
		
	result = (kCCSuccess == ccStatus) ? 0 : -1;
	return result;	
}

int ValidateAsset(const char* asset_dir_path, unsigned long current_version)
{
	const char* files[] = 
	{
		"certs.plist",
		"distrusted.plist",
		"EVRoots.plist",
		"Manifest.plist",
		"revoked.plist",
		"roots.plist"
	};
	int num_files = (sizeof(files) / sizeof(const char*));
	int iCnt = 0;
	const char* file_name = NULL;
    char wd_buf[1024];
    
	const char* current_working_directory_path = getcwd(wd_buf, 1024);
	CFDataRef file_data = NULL;
	CFDataRef hash_data = NULL;
	CFDataRef encoded_hash_data = NULL;
	CFStringRef manifest_hash_data_str = NULL;
	CFDataRef signature_data = NULL;
	CFStringRef key_name = NULL;
	CFDictionaryRef manifest_dict = NULL;
    CFNumberRef manifest_version = NULL;
    CFStringRef encoded_hash_str = NULL;
	CFDataRef manifest_data = NULL;
    unsigned long manifest_verson_number;
	int iResult = -1;
	
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
	
	if (ReadFileIntoCFDataRef("Manifest.plist", &file_data))
	{
		(void)chdir(current_working_directory_path);
		return iResult;
	}
	
	if (CreatePropertyListFromData(file_data,  CFDictionaryGetTypeID(), (CFTypeRef *)&manifest_dict))
	{
		CFRelease(file_data);
		(void)chdir(current_working_directory_path);
		return iResult;
	}
	CFRelease(file_data);
		
	// Validate the hash for the files in the manifest
	for (iCnt = 0; iCnt < num_files; iCnt++)
	{
		file_name = files[iCnt];
		// bypass the manifest file for now
		if (!strcmp("Manifest.plist", file_name))
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
			CFRelease(file_data);
			CFRelease(manifest_dict);
			(void)chdir(current_working_directory_path);
			return iResult;
		}
		CFRelease(file_data);
        
		
		if (Base64Data(hash_data, 1, &encoded_hash_data))
		{
			CFRelease(hash_data);
			CFRelease(manifest_dict);
			(void)chdir(current_working_directory_path);
			return iResult;
		}
		CFRelease(hash_data);
        
        encoded_hash_str = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, encoded_hash_data, kCFStringEncodingUTF8);
        if (NULL == encoded_hash_str)
        {
            CFRelease(encoded_hash_data);
            CFRelease(manifest_dict);
			(void)chdir(current_working_directory_path);
			return iResult;
        }
        CFRelease(encoded_hash_data);
		
		key_name = CFStringCreateWithCString(kCFAllocatorDefault, file_name, kCFStringEncodingUTF8);
		if (NULL == key_name)
		{
			CFRelease(encoded_hash_str);
			CFRelease(manifest_dict);
			(void)chdir(current_working_directory_path);
			return iResult;
		}
		
		manifest_hash_data_str = (CFStringRef)CFDictionaryGetValue(manifest_dict, key_name);
		if (NULL == manifest_hash_data_str)
		{
			CFRelease(key_name);
			CFRelease(encoded_hash_str);
			CFRelease(manifest_dict);
			(void)chdir(current_working_directory_path);
			return iResult;
		}
		CFRelease(key_name);
		
		if (!CFEqual(encoded_hash_str, manifest_hash_data_str))
		{
			CFRelease(encoded_hash_str);
			CFRelease(manifest_dict);
			(void)chdir(current_working_directory_path);
			return iResult;
		}
		CFRelease(encoded_hash_str);
	}
	
	// Get the version
    manifest_version = (CFNumberRef)CFDictionaryGetValue(manifest_dict, CFSTR("Version"));
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
    
    // Deal with the signature
	if (TearOffSignatureAndHashManifest(manifest_dict, &signature_data, &manifest_data))
	{
		CFRelease(manifest_dict);
		(void)chdir(current_working_directory_path);
		return iResult;
	}	
	
	iResult = ValidateSignature(signature_data, manifest_data);
	CFRelease(signature_data);
	CFRelease(manifest_data);
	CFRelease(manifest_dict);
	(void)chdir(current_working_directory_path);
	return iResult;    
}