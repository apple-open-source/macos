/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */

/*
 *  SFFileVault.cpp
 *  testFileVaultSPI
 *
 *  Created by john on Mon Jul 14 2003.
 *
 */

//#include <iostream>

#include "SFFileVault.h"
#include "ExecCLITool.h"
#include <Security/SecKeychain.h>
#include <Security/KCCursor.h>
#include <Security/cfutilities.h>
#include <Security/Keychains.h>
#include <Security/KCUtilities.h>
#include <Security/globals.h>
#include <Security/Certificate.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/SecKeychainAPIPriv.h>
#include "SecFileVaultCert.h"

#pragma mark -------------------- Environment Variables --------------------

#define HDIUTIL_PATH	"/usr/bin/hdiutil"		// environment var -> HDIUTIL_PATH
#define HDID_PATH		"/usr/bin/hdid"			// environment var -> HDID_PATH
#define MOUNT_HFS_PATH	"/sbin/mount_hfs"		// environment var -> MOUNT_HFS_PATH
#define UMOUNT_PATH		"/sbin/umount"			// environment var -> UMOUNT_PATH

//	_defaultMasterKeychainPath					// environment var -> FILEVAULT_MASTER_PATH

const char * const SFFileVault:: _defaultMasterKeychainPath = "/System/Library/Keychains/";
const char * const SFFileVault::_masterKeychainName = "FileVaultMaster";	

#pragma mark -------------------- SFFileVault implementation --------------------

OSStatus SFFileVault::mount(CFStringRef password, CFURLRef certificate, CFURLRef dmgin,
        CFURLRef mountpoint, CFStringRef *devicepath)
{
    //		/usr/bin/hdid -nomount -stdinpass -plist thevol.dmg
    //		/sbin/mount_hfs /dev/disk3s2 /tmp/THEVOL
    
    const Boolean resolveAgainstBase = true;
    char imageFileString[PATH_MAX + 1];
    if (!CFURLGetFileSystemRepresentation(dmgin, resolveAgainstBase, reinterpret_cast<UInt8 *>(imageFileString), PATH_MAX))
        MacOSError::throwMe(paramErr);

    // @@@ Not implemented yet
    if (certificate)
        MacOSError::throwMe(unimpErr);

    ExecCLITool rt;
   	rt.input(password,true);	// include trailing NULL
	rt.run(HDID_PATH,"HDID_PATH", "-nomount", "-stdinpass", "-plist", imageFileString, NULL);

    CFRef<CFStringRef> devicePathString = extractDevicepath(rt);	// parse stdout from hdid --> should be plist
    if (!devicePathString)
        MacOSError::throwMe(paramErr);
    const char *devpath = CFStringGetCStringPtr(devicePathString, kCFStringEncodingMacRoman);
    if (!devpath)
        MacOSError::throwMe(ioErr);

    char mountpointString[PATH_MAX + 1];
    if (!CFURLGetFileSystemRepresentation(mountpoint, resolveAgainstBase, reinterpret_cast<UInt8 *>(mountpointString), PATH_MAX))
        MacOSError::throwMe(paramErr);

	rt.run(MOUNT_HFS_PATH,"MOUNT_HFS_PATH", devpath, mountpointString, NULL);
    *devicepath = CFStringCreateCopy(NULL, devicePathString);
 
    return noErr;
}

OSStatus SFFileVault::mastermount(CFURLRef dmgin, CFURLRef mountpoint, CFStringRef *devicepath)
{
    // convenience call to call mount with master cert
    CFStringRef password = NULL;
    CFURLRef certificate = NULL;
    getCertificate(&certificate);
    OSStatus status = mount(password, certificate, dmgin, mountpoint, devicepath);
    return status;
}

OSStatus SFFileVault::unmount(CFURLRef mountpoint, CFStringRef devicepath)
{
    //	To unmount, we do:
    //		/sbin/umount -f <mount point path>			/sbin/umount -f /tmp/THEVOL
    //		/usr/bin/hdiutil detach <device path>			/usr/bin/hdiutil detach /dev/disk3s2

    ExecCLITool rt;

    Boolean resolveAgainstBase = true;
    char mountpointString[PATH_MAX + 1];
    if (!CFURLGetFileSystemRepresentation(mountpoint, resolveAgainstBase, reinterpret_cast<UInt8 *>(mountpointString), PATH_MAX))
        MacOSError::throwMe(paramErr);

//  OSStatus status = rt.run(HDIUTIL_PATH,"HDIUTIL_PATH", "unmount", "-force", mtpt, NULL);
	/* OSStatus status = */ rt.run(UMOUNT_PATH,"UMOUNT_PATH", "-f", mountpointString, NULL);

    const char *devpath = CFStringGetCStringPtr(devicepath, kCFStringEncodingMacRoman);
    if (!devpath)
        MacOSError::throwMe(paramErr);

    return rt.run(HDIUTIL_PATH,"HDIUTIL_PATH", "detach", devpath, NULL);
}

OSStatus SFFileVault::userChangePassword(CFStringRef password, CFStringRef devicepath)
{
    // @@@ Not implemented yet, but code will be something like below
    MacOSError::throwMe(unimpErr);

    ExecCLITool rt;

    const char *devpath = CFStringGetCStringPtr(devicepath, kCFStringEncodingMacRoman);
    if (!devpath)
        MacOSError::throwMe(paramErr);

   	rt.input(password,true);	// include trailing NULL
    return rt.run(HDIUTIL_PATH,"HDIUTIL_PATH", "chpass", devpath, NULL);

    return noErr;
}

OSStatus SFFileVault::makeMasterPassword(CFStringRef masterPasswordPassword, SecKeychainRef *keychainRef)
{
    /*
        OSStatus SecFileVaultMakeMasterPassword(CFStringRef masterPasswordPassword);
    
        *** In the real code, this will be done directly rather than exec'ing a tool, since there are too many parameters to specify
        *** this needs to be done as root, since the keychain will be a system keychain
        /usr/bin/certtool y c k=/System/Library/Keychains/FileVaultMaster.keychain p=<masterPasswordPassword> 
        /usr/bin/certtool c   k=/System/Library/Keychains/FileVaultMaster.keychain o=/System/Library/Keychains/FileVaultMaster.cer 
        Two steps: create the keychain, then create the keypair
    */
    
    char masterKeychainPath[PATH_MAX + 1];
    const char *envPath = getenv("FILEVAULT_MASTER_PATH");	// must set to full path or kc will end up in ~/Library/Keychains/
    if (!envPath)
        envPath = _defaultMasterKeychainPath;
    snprintf(masterKeychainPath, sizeof(masterKeychainPath), "%s%s.keychain", envPath, _masterKeychainName);
//    std::cout << "Masterkeychain path: " << masterKeychainPath << std::endl;

    const char *mpass = CFStringGetCStringPtr(masterPasswordPassword, kCFStringEncodingMacRoman);
    if (!mpass)
        MacOSError::throwMe(paramErr);
    const UInt32 passwordLength = strlen(mpass);

    // don't add to searchlist
    KeychainCore::Keychain keychain = KeychainCore::globals().storageManager.make(Required(&masterKeychainPath),false);
    
    try
    {
		keychain->create(passwordLength, mpass);
	}
	catch (const MacOSError &err)
    {
        if (err.osStatus()!=errSecDuplicateKeychain)
            throw;
    }
    catch (const CssmCommonError &err)
    {
        if (err.cssmError()!=CSSMERR_DL_DATASTORE_ALREADY_EXISTS)
        	throw;
	}

	RequiredParam(keychainRef)=keychain->handle();

    // @@@ Need better identification for the certificate
    SecFileVaultCert fvc;
    CFStringRef hostName = CFSTR("com.apple.fv");
    CFStringRef userName = CFSTR("User Name");
    CFDataRef certData = NULL; //CFRef<>
    OSStatus status = fvc.createPair(hostName,userName,*keychainRef,&certData);
    if (status)
        MacOSError::throwMe(status);
    // Write out cert file
    status = writeCertificateFile(certData);
    if (status)
        MacOSError::throwMe(status);

    return noErr;
}

OSStatus SFFileVault::create(CFStringRef password, CFURLRef certificate, CFURLRef dmgout,  
    CFStringRef volumeName, CFStringRef sizeSpec)
{
    // /usr/bin/hdiutil create -encryption -stdinpass -type SPARSE -fs "HFS+" -volname <vol name> -size 20g <path to disk image>
    
    ExecCLITool rt;
    
    // Construct the "-volname" parameter
    if (!volumeName)
        MacOSError::throwMe(paramErr);
    const char *volname = CFStringGetCStringPtr(volumeName, kCFStringEncodingMacRoman);
    if (!volname)
        MacOSError::throwMe(paramErr);

    // Construct the "-size" parameter
    if (!sizeSpec)
        MacOSError::throwMe(paramErr);
    const char *sizestr = CFStringGetCStringPtr(sizeSpec, kCFStringEncodingMacRoman);
    if (!sizestr)
        MacOSError::throwMe(paramErr);
    
    // Construct the file name parameter
    CFRef<CFStringRef> fileString = CFURLCopyFileSystemPath(dmgout, kCFURLPOSIXPathStyle);
    if (!fileString)
        MacOSError::throwMe(paramErr);
    const char *fname = CFStringGetCStringPtr(fileString, kCFStringEncodingMacRoman);
    if (!fname)
        MacOSError::throwMe(paramErr);
    
    // Construct the "-certificate" parameter
    const char *certificateParamString = certificate?"-certificate":"-layout";	// @@@ what is a safe empty param?
    CFStringRef certificateFileString = certificate?CFURLCopyFileSystemPath(certificate, kCFURLPOSIXPathStyle):NULL;
    if (certificate && !certificateFileString)
        MacOSError::throwMe(paramErr);
    const char *certFileString = certificate?CFStringGetCStringPtr(certificateFileString, kCFStringEncodingMacRoman):"SPUD";
    if (certificate && !certFileString)
        MacOSError::throwMe(paramErr);
    
   	rt.input(password,true);	// include trailing NULL
	OSStatus status = rt.run(HDIUTIL_PATH,"HDIUTIL_PATH", "create", "-encryption", "CEncryptedEncoding", 
        "-stdinpass", "-type", "SPARSE", "-fs", "HFS+", "-volname", volname, "-size", sizestr, 
        certificateParamString, certFileString, fname, NULL);
    
    if (certificateFileString)
        CFRelease(certificateFileString);
        
    return status;
}

Boolean SFFileVault::masterPasswordEnabled(SecKeychainRef *keychainRef)
{
    char masterKeychain[PATH_MAX + 1];
    snprintf(masterKeychain, sizeof(masterKeychain), "%s.keychain", getKeychainPath());	//@@@  leak

	SecKeychainRef tmpKeychainRef=KeychainCore::globals().storageManager.make(masterKeychain, false)->handle();
    if (tmpKeychainRef == NULL)
       return false; 
    
    if (keychainRef)
        *keychainRef = tmpKeychainRef;
    else
        CFRelease(tmpKeychainRef);
    return true;
}

OSStatus SFFileVault::changeMasterPasswordPassword(CFStringRef oldPassword,CFStringRef newPassword)
{
    // Essentially SecKeychainChangePassword for the FileVault Master Password keychain
    SecKeychainRef keychainRef;
    if (!masterPasswordEnabled(&keychainRef))
        MacOSError::throwMe(errSecNoSuchKeychain);
    
    std::string oldpw = cfString(oldPassword);	//UInt32
    std::string newpw = cfString(newPassword);

	KeychainCore::Keychain keychain = KeychainCore::Keychain::optional(keychainRef);
    keychain->changePassphrase (oldpw.length(), oldpw.c_str(), newpw.length(), newpw.c_str());
    CFRelease(keychainRef);
	return noErr;
}

/*
    Shouldn't cfString being using code like this?
    
    const Boolean isExternalRepresentation = false;
    const CFStringEncoding encoding = kCFStringEncodingUTF8;
    CFIndex usedBufLen = 0;
    UInt8 lossByte = 0;

    if (!theString)
        MacOSError::throwMe(paramErr);

    CFRange stringRange = CFRangeMake(0,CFStringGetLength(theString));
    // Call once first just to get length
    CFIndex length = CFStringGetBytes(theString, stringRange, encoding, lossByte,
        isExternalRepresentation, NULL, 0, &usedBufLen);
*/

#pragma mark -------------------- Helpers --------------------

#define SYSTEM_ENTITIES_KEY		CFSTR("system-entities")
#define CONTENT_HINT_KEY		CFSTR("content-hint")
#define DEV_ENTRY_KEY			CFSTR("dev-entry")
#define APPLE_HFS_KEY			CFSTR("Apple_HFS")

CFStringRef SFFileVault::extractDevicepath(const ExecCLITool& rt)
{
    CFRef<CFDataRef> tableData = CFDataCreate(NULL,reinterpret_cast<const UInt8 *>(rt.data()),rt.length());
    CFStringRef errorString = NULL;
    CFRef<CFDictionaryRef> devTable = static_cast<CFDictionaryRef>(CFPropertyListCreateFromXMLData(NULL,
        tableData, kCFPropertyListImmutable, &errorString));
    if (errorString != NULL)
    {
        CFRelease(errorString);
        return NULL;
    }
				
    CFRef<CFArrayRef> sysEntities = static_cast<CFArrayRef>(CFDictionaryGetValue(devTable,SYSTEM_ENTITIES_KEY));
    if (sysEntities == NULL)
        return NULL;

    CFIndex dictionaryCount = CFArrayGetCount(sysEntities);
    for (CFIndex ix=0;ix < dictionaryCount;ix++)
    {
        CFRef<CFDictionaryRef> dict = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(sysEntities, ix));
        CFRef<CFStringRef> deviceEntryString = static_cast<CFStringRef>(CFDictionaryGetValue(dict,CONTENT_HINT_KEY));
        if (CFEqual(deviceEntryString, APPLE_HFS_KEY))	// found it
        	return static_cast<CFStringRef>(CFDictionaryGetValue(dict,DEV_ENTRY_KEY));
    }
    return NULL;
}

OSStatus SFFileVault::getCertificate(CFURLRef *certificateFile)
{
    //@@@ to be done
    MacOSError::throwMe(unimpErr);
    // do a find in the master keychain
    char masterKeychain[PATH_MAX + 1];
    snprintf(masterKeychain, sizeof(masterKeychain), "%s.keychain", getKeychainPath());	//@@@  leak

    // don't add to searchlist
    KeychainCore::Keychain keychain = KeychainCore::globals().storageManager.make(Required(&masterKeychain),false);
	KeychainCore::StorageManager::KeychainList keychains;
	KeychainCore::globals().storageManager.optionalSearchList(keychain, keychains);
    
	// Code basically copied from SecKeychainSearchCreateFromAttributes and SecKeychainSearchCopyNext:
	KeychainCore::KCCursor cursor(keychains, kSecCertificateItemClass, NULL);
	KeychainCore::Item item;
	if (!cursor->next(item))
		CssmError::throwMe(errSecItemNotFound);

//    KeychainCore::Certificate *certificate = static_cast<KeychainCore::Certificate *>(&*item);
//    CSSM_DATA_PTR certData = static_cast<CSSM_DATA_PTR>(certificate->data());

    return noErr;
}

OSStatus SFFileVault::writeCertificateFile(CFDataRef certData)
{
    const char *certFile = getCertificateFileName();
    OSStatus status = writeFile(certFile, CFDataGetBytePtr(certData), CFDataGetLength(certData));
    if (certFile)
        ::free(const_cast<char *>(certFile));
    return status;
}

const char *SFFileVault::getKeychainPath()
{
    // Append ".keychain to get keychain name; append .cer to get certificate
    char masterKeychainPath[PATH_MAX + 1];
    const char *envPath = getenv("FILEVAULT_MASTER_PATH");	// must set to full path or kc will end up in ~/Library/Keychains/
    if (!envPath)
        envPath = _defaultMasterKeychainPath;
    snprintf(masterKeychainPath, sizeof(masterKeychainPath), "%s%s", envPath, _masterKeychainName);
//    std::cout << "Masterkeychain path: " << masterKeychainPath << std::endl;
    size_t sz = strlen(masterKeychainPath)+1;
    char *path = static_cast<char *>(malloc(sz));
    strncpy(path,masterKeychainPath,sz);
    return static_cast<const char *>(path);
}

const char *SFFileVault::getCertificateFileName()
{
    char certFile[PATH_MAX + 1];
    snprintf(certFile, sizeof(certFile), "%s.cer", getKeychainPath());
    size_t sz = strlen(certFile)+1;
    char *path = static_cast<char *>(malloc(sz));
    strncpy(path,certFile,sz);
    return static_cast<const char *>(path);
}

int SFFileVault::writeFile(const char *fileName, const unsigned char *bytes, unsigned int numBytes)
{
	int fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (fd <= 0)
		return errno;

	if (lseek(fd, 0, SEEK_SET) < 0)
		return errno;
	
	int rtn = write(fd, bytes, (size_t)numBytes);
	rtn = (rtn != static_cast<int>(numBytes))?EIO:0;
	close(fd);
	return rtn;
}
	
#pragma mark -------------------- Unused --------------------

