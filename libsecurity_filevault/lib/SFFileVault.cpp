/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 *
 * SFFileVault.cpp
 */

#include "SFFileVault.h"
#include "ExecCLITool.h"
#include "SecFileVaultCert.h"
#include "FVDIHLInterface.h"

#include <Security/SecKeychain.h>
#include <Security/SecKeychainPriv.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/errors.h>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <DiskImages/DIHLInterface.h>
#include <DiskImages/DIHighLevelAPI.h>
#include <DiskImages/DIConstants.h>
#include <DiskImages/DIHLInterfacePriv.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/mount.h>

#pragma mark -------------------- Environment Variables --------------------

#define HDIUTIL_PATH	"/usr/bin/hdiutil"		// environment var -> HDIUTIL_PATH

#pragma mark -------------------- Debugging Variables --------------------

#if !defined(NDEBUG)
#define DEBUG_UNMOUNT			1
#define DEBUG_RECOVER			1
#define DEBUG_COMPACT			1
#define DEBUG_RESIZE			1
#endif

const char * const SFFileVault:: _defaultMasterKeychainPath = "/Library/Keychains/";
const char * const SFFileVault::_masterKeychainName = "FileVaultMaster";	

#pragma mark -------------------- SFFileVault implementation --------------------

OSStatus SFFileVault::mount(CFStringRef password, CFURLRef dmgin, CFStringRef inMountPoint, CFStringRef *devicepath,
    CFTypeRef certificateOrArray)
{
    //		/usr/bin/hdid -nomount -stdinpass -plist thevol.dmg
    //		/sbin/mount_hfs /dev/disk3s2 /tmp/THEVOL
    //		/usr/bin/hdiutil attach -stdinpass -nodm -mountpoint /tmp/THEVOL -plist MyFileVault.sparseimage

	mountViaDIF(password, dmgin, inMountPoint, devicepath, certificateOrArray);
 
    return noErr;
}

OSStatus SFFileVault::mountViaDIF(CFStringRef password, CFURLRef imageURL, CFStringRef inMountPoint,
        CFStringRef *outMountPoint, CFTypeRef certificateOrArray)
{
	CFRef<CFMutableDictionaryRef> input(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
	CFRef<CFMutableDictionaryRef> imageOptions(CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
	CFRef<CFMutableDictionaryRef> driveOptions(CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
	
	if (!outMountPoint)
        MacOSError::throwMe(paramErr);
	*outMountPoint = NULL;

	// Don't show in Finder as a separate volume	
	CFDictionarySetValue(input, kDIHLAttachMountNoBrowseKey, kCFBooleanTrue);
	
	// To not ignore permissions on the mounted image
	CFDictionarySetValue(input, kDIHLAttachMountEnablePermKey, kCFBooleanTrue);
	
	// if you want -nomount
//	CFDictionarySetValue(input, kDIHLAttachMountRequiredKey, kCFBooleanFalse);
//	CFDictionarySetValue(input, kDIHLAttachMountAttemptedKey, kCFBooleanFalse);
	
	// if you want -kernel
//	CFDictionarySetValue(input, kDIHLAttachForceInKernelKey, kCFBooleanTrue);
	
	// if you want -nokernel
//	CFDictionarySetValue(input, kDIHLAttachForceInKernelKey, kCFBooleanTrue);
	
	// if you want to supporess auto-opening the root of the volume in Finder
//	CFDictionarySetValue(input, kDIHLAttachAutoOpenROKey, kCFBooleanFalse);
//	CFDictionarySetValue(input, kDIHLAttachAutoOpenRWKey, kCFBooleanFalse);
		
	// if you want to specify mountpoint
    if (inMountPoint)
		CFDictionarySetValue(input, kDIHLAttachMountPointKey, inMountPoint);

	// to specify image
	CFDictionarySetValue(input, kDIHLAttachURLKey, imageURL);

    // Mark as a system-image
    CFDictionarySetValue(driveOptions, kDIHLAttachDriveOptionSystemImageKey, kCFBooleanTrue);

    // Mark as being used as a FileVault image
    CFDictionarySetValue(driveOptions, CFSTR("filevault-image"), kCFBooleanTrue);

	// to specify passphrase
    if (password)
        CFDictionarySetValue(imageOptions, CFSTR(kDIPassphraseKey), password);

	// to specify image options
	if (CFDictionaryGetCount(imageOptions) > 0)
        CFDictionarySetValue(input, kDIHLAttachImageOptionsKey, imageOptions);
    
	// to specify drive options
	if (CFDictionaryGetCount(driveOptions) > 0)
        CFDictionarySetValue(input, kDIHLAttachDriveOptionsKey, driveOptions);
    
    if (certificateOrArray)
        CFDictionarySetValue(input, CFSTR(kDICertificatesKey), certificateOrArray);
    
	CFDictionaryRef output = NULL;	// This is created with CFDictionaryCreateCopy, so we are responsible for release
    SFDIHLStarter dihlstarter;
//	OSStatus status = (*FVDIHLInterface::DIHLDiskImageAttach()((const CFDictionaryRef)input, (DIHLStatusProcPtr)NULL /* statusProc */, (void *)0L, &output);
	OSStatus status = (*FVDIHLInterface::DIHLDiskImageAttach())(input, (DIHLStatusProcPtr)NULL /* statusProc */, NULL, &output);
    if (status)
    {
		if (output)
			CFRelease(output);
	    MacOSError::throwMe(status);
	}
	
    if (outMountPoint)
        *outMountPoint = extractDevicepath(static_cast<CFDictionaryRef>(output));

	if (output)
		CFRelease(output);
	
	return status;
}

OSStatus SFFileVault::mastermount(CFURLRef dmgin, CFStringRef inMountPoint, CFStringRef *devicepath)
{
    // convenience call to call mount with master cert
    CFStringRef password = NULL;
    CFRef<CFDataRef> certificate(getCertificate());
    OSStatus status = mount(password, dmgin, inMountPoint, devicepath, certificate);
    return status;
}

OSStatus SFFileVault::unmount(CFStringRef devicepath)
{
    //	To unmount, we do:
    //		/usr/bin/hdiutil detach <device path>			/usr/bin/hdiutil detach /dev/disk2s2

    ExecCLITool rt;

    if (!devicepath)
        MacOSError::throwMe(paramErr);
    const char *devpath = CFStringGetCStringPtr(devicepath, kCFStringEncodingMacRoman);
    if (!devpath)
        MacOSError::throwMe(paramErr);
    rt.addargs(1, "detach", NULL);
#if !defined(NDEBUG)
	if (DEBUG_UNMOUNT)
        rt.addargs(2, "-debug", "-verbose", NULL);
#endif
    int rx = rt.run(HDIUTIL_PATH,"HDIUTIL_PATH", devpath, NULL);
    if (rx)
        MacOSError::throwMe(rx);
    return 0;
}


OSStatus SFFileVault::userChangePassword(CFStringRef oldPassword, CFStringRef newPassword, CFURLRef dmgin)
{
    //	In user mode, need oldPassword, newPassword:
    //		chpassDIF(CFSTR("theOldPassword"), CFSTR("theNewPassword"), NULL, CFURLRef dmgin);

	return chpassDIF(oldPassword, newPassword, NULL, dmgin);
}

//In user mode, need oldPassword, newPassword:
//		sffv.chpassDIF(CFSTR("theOldPassword"), CFSTR("theNewPassword"), NULL, CFURLRef dmgin);
//
// In recovery mode, need path to master keychain, and new user password
//		sffv.chpassDIF(CFSTR("masterKeychainPassword"), CFSTR("theNewUserPassword"), "FileVaultMaster.keychain", CFURLRef dmgin);

OSStatus SFFileVault::chpassDIF(CFStringRef oldPassword, CFStringRef newPassword, CFStringRef masterKeychainPath, CFURLRef dmgin)
{
    // In user mode, need oldPassword, newPassword
    // In recovery mode, need password for master keychain file, path to master keychain, and new user password
    if (!dmgin)
        MacOSError::throwMe(paramErr);
    
	CFRef<CFMutableDictionaryRef> inOptions(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
	CFRef<CFMutableDictionaryRef> imageOptions(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));

    CFDictionarySetValue(inOptions, kDIHLChangePassphraseImageURLKey, dmgin);
	CFDictionarySetValue(inOptions, kDIHLChangePassphraseImageOptionsKey, imageOptions);		// DI seems to need at least an empty one
    if (oldPassword)
        CFDictionarySetValue(inOptions, kDIHLChangePassphraseOldPassphraseKey, oldPassword);
    if (newPassword)
        CFDictionarySetValue(inOptions, kDIHLChangePassphraseNewPassphraseKey, newPassword);

    if (masterKeychainPath)
        CFDictionarySetValue(inOptions, kDIHLChangePassphraseRecoveryKey, masterKeychainPath);

#if !defined(NDEBUG)
    const char *debugVal = getenv("FILEVAULT_DEBUG");
	if (debugVal)
    {
        CFDictionarySetValue(inOptions, kDIHLCreateDebugKey, kCFBooleanTrue);
        CFDictionarySetValue(inOptions, kDIHLCreateVerboseKey, kCFBooleanTrue);
	}
#endif

    SFDIHLStarter dihlstarter;
    
    OSStatus status = (*FVDIHLInterface::DIHLDiskImageChangePassword())(inOptions);
    if (status)
        MacOSError::throwMe(status);
    return noErr;
}

OSStatus SFFileVault::makeMasterPassword(CFStringRef masterPasswordPassword, SecKeychainRef *keychainRef)
{
    /*
        OSStatus SecFileVaultMakeMasterPassword(CFStringRef masterPasswordPassword);
    
        *** In the real code, this will be done directly rather than exec'ing a tool, since there are too many parameters to specify
        *** this needs to be done as root, since the keychain will be a system keychain
        /usr/bin/certtool y c k=/Library/Keychains/FileVaultMaster.keychain p=<masterPasswordPassword> 
        /usr/bin/certtool c   k=/Library/Keychains/FileVaultMaster.keychain o=/Library/Keychains/FileVaultMaster.cer 
        Two steps: create the keychain, then create the keypair
    */
    std::string masterKeychainPath(getKeychainName());

    char mpass[PATH_MAX + 1];
	CFIndex passwordLength;
	convertCFString(masterPasswordPassword, mpass, passwordLength);

    SecAccessRef initialAccess = NULL;
    OSStatus status = SecKeychainCreate(masterKeychainPath.c_str(), passwordLength, mpass, false, initialAccess, keychainRef);
    if (status!=noErr && status!=errSecDuplicateKeychain && status!=CSSMERR_DL_DATASTORE_ALREADY_EXISTS)
        MacOSError::throwMe(status);

    if (certificateExists(*keychainRef))
        return noErr;

    // Create the key pair
    char host[1024];
	gethostname(host, sizeof(host));

    SecFileVaultCert fvc;
    CFStringRef hostName = CFSTR("FileVault Recovery Key");		// This is what shows up in Keychain Access display
    CFStringRef userName = CFStringCreateWithCString(kCFAllocatorDefault, host, kCFStringEncodingUTF8);
    CFDataRef certData = NULL; //CFRef<>
    status = fvc.createPair(hostName,userName,*keychainRef,&certData);
    if (status)
        MacOSError::throwMe(status);
    // Write out cert file
    status = writeCertificateFile(certData);
    if (status)
        MacOSError::throwMe(status);

    return noErr;
}

bool SFFileVault::certificateExists(SecKeychainRef keychainRef)
{
    SecKeychainSearchRef searchRef;
    SecKeychainItemRef itemRef;

    OSStatus status = SecKeychainSearchCreateFromAttributes(keychainRef, kSecCertificateItemClass, NULL, &searchRef);
    if (status) return false;

    // This will return DL_INVALID_RECORDTYPE if no certificate has 
    // been added before or errSecItemNotFound
    status = SecKeychainSearchCopyNext(searchRef, &itemRef);
    return status == noErr;
}

static CFStringRef	_SFFileVaultCopyFileSystemPersonalityForImage(CFURLRef inURL);
OSStatus SFFileVault::create(CFStringRef password, CFURLRef dmgout, CFStringRef volumeName, int64_t sectors, uid_t uid, gid_t gid,
    CFTypeRef certificateOrArray)
{
    // e.g. "password", "MyFileVault", "filevault", 10204, NULL  (10240 -> 5M)
    if (!dmgout)
        MacOSError::throwMe(paramErr);
    
	// See startNewImageOp in DiskUtility
	CFRef<CFMutableDictionaryRef> operation(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
    
    // Make the image-options key
    CFRef<CFMutableDictionaryRef> createTargetOptions(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
 
//  CFDictionarySetValue(createTargetOptions, kDIHLCreateTargetPasswordKey, password);
//  CFDictionarySetValue(createTargetOptions, kDIHLCreateTargetEncryptionKey, CFSTR("CEncryptedEncoding"));
    if (password)
        CFDictionarySetValue(createTargetOptions, CFSTR(kDIPassphraseKey), password);
    CFDictionarySetValue(createTargetOptions, CFSTR(kDIEncryptionClassKey), CFSTR("CEncryptedEncoding"));

    // ---------------------------------
    // Make the create-target-spec key
    // ---------------------------------
    CFRef<CFMutableDictionaryRef> createTargetSpec(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
	CFRef<CFNumberRef> numSectors(CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &sectors));

    CFDictionarySetValue(createTargetSpec, kDIHLCreateTargetSectorsKey, numSectors);
    CFDictionarySetValue(createTargetSpec, kDIHLCreateTargetURLKey, dmgout);
    CFDictionarySetValue(createTargetSpec, kDIHLCreateTargetImageTypeKey, kDIHLCreateTargetSparseValue);
    CFDictionarySetValue(createTargetSpec, kDIHLCreateTargetOptionsKey, createTargetOptions);
    
    if (certificateOrArray)
		CFDictionarySetValue(createTargetSpec, kDIHLCreateTargetCertificatesKey, certificateOrArray);	// CFData or CFArray of CFData's
    
    CFDictionarySetValue(operation, kDIHLCreateTargetSpecKey, createTargetSpec);	// Put createTarget dictionary in operation dictionary

    // ---------------------------------
    // Make the create-content-spec key
    // ---------------------------------
    CFRef<CFMutableDictionaryRef> createContentSpec(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
    CFRef<CFMutableDictionaryRef> nbiSpec(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
	CFRef<CFNumberRef> userID (CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &uid));
	CFRef<CFNumberRef> groupID(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &gid));

	CFRef<CFStringRef> filesystem(_SFFileVaultCopyFileSystemPersonalityForImage(dmgout));
	CFDictionarySetValue(nbiSpec, kDIHLCreateNBIFileSystemKey, filesystem);	 		// rdar://problem/3969147
    CFDictionarySetValue(nbiSpec, kDIHLCreateNBIVolNameKey, volumeName);
    CFDictionarySetValue(nbiSpec, kDIHLCreateNBIUIDKey, userID);
    CFDictionarySetValue(nbiSpec, kDIHLCreateNBIGIDKey, groupID);
    CFDictionarySetValue(createContentSpec, kDIHLCreateContentNBIKey, nbiSpec);		// Put nbiSpec dictionary in createContent dictionary
    CFDictionarySetValue(operation, kDIHLCreateContentSpecKey, createContentSpec);	// Put createContent dictionary in operation dictionary

    // ---------------------------------
    // Add in some random global keys
    // ---------------------------------
    CFDictionarySetValue(operation, CFSTR(kDICreateImageType), CFSTR("SPARSE"));
    if (password)
        CFDictionarySetValue(operation, CFSTR(kDIPassphraseKey), password);			// needed to prevent SecAgent UI (yes, password is there twice)
    CFDictionarySetValue(operation, kDIHLAgentKey, kDIHLHDIUTILAgentValue);
    
#if !defined(NDEBUG)
    const char *debugVal = getenv("FILEVAULT_DEBUG");
	if (debugVal)
    {
        CFDictionarySetValue(operation, kDIHLCreateDebugKey, kCFBooleanTrue);
        CFDictionarySetValue(operation, kDIHLCreateVerboseKey, kCFBooleanTrue);
	}
#endif

    CFDictionaryRef outResults = NULL;
    SFDIHLStarter dihlstarter;
    
    OSStatus status = (*FVDIHLInterface::DIHLDiskImageCreate())(operation, NULL, NULL, &outResults);
    if (outResults)
        CFRelease(outResults);
    if (status)
        MacOSError::throwMe(status);
        
    return status;
}

CFStringRef	_SFFileVaultCopyFileSystemPersonalityForImage(CFURLRef inURL)			// rdar://problem/3969147
{
	CFStringRef		rval		= CFSTR("Journaled HFS+");
	CFURLRef		parentURL	= NULL;
	char			fullpath[MAXPATHLEN];
	struct statfs	sfsbuf;
	
	do {
		if (!inURL)				break;
		
		parentURL = CFURLCreateCopyDeletingPathExtension(kCFAllocatorDefault, inURL);
		if (!parentURL)			break;

		// get path and statfs it
		if (!CFURLGetFileSystemRepresentation(parentURL, false /* resolveAgainstBase */, (UInt8 *)fullpath, MAXPATHLEN))
								break;								
		if (statfs(fullpath, &sfsbuf))
								break;
								
		if (pathconf(fullpath, _PC_CASE_SENSITIVE))
			rval = CFSTR("Case-sensitive Journaled HFS+");
			
		// special case here for UFS
		if (sfsbuf.f_fstypename && strlen(sfsbuf.f_fstypename) && !strcmp(sfsbuf.f_fstypename, "ufs"))
			rval = CFSTR("UFS");
		
	} while (0);
	
	if (parentURL)				CFRelease(parentURL);
	return rval;
}


Boolean SFFileVault::masterPasswordEnabled(SecKeychainRef *keychainRef)
{
    // Callers must release the keychain ref
	std::string masterKeychain(getKeychainName());

	if (access(masterKeychain.c_str(), F_OK))
        return false;

    SecKeychainRef tmpKeychainRef = NULL;
    OSStatus status = SecKeychainOpen(masterKeychain.c_str(), &tmpKeychainRef);
    if (status)
        MacOSError::throwMe(status);
    if (tmpKeychainRef == NULL)
        MacOSError::throwMe(paramErr);
    
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

    OSStatus status = SecKeychainChangePassword(keychainRef, oldpw.length(), oldpw.c_str(), newpw.length(), newpw.c_str());
    if (status==noErr)
        status = SecKeychainLock(keychainRef);
    CFRelease(keychainRef);
    if (status)
        MacOSError::throwMe(status);

	return noErr;
}

Boolean SFFileVault::masterPasswordValidates(CFStringRef password)
{
    std::string masterKeychain(getKeychainName());

    SecKeychainRef keychain = NULL;
    OSStatus status = SecKeychainOpen(masterKeychain.c_str(), &keychain);
    if (status)
        return false;
	status = SecKeychainLock(keychain);
    if (status)
        return false;

    char masterPassword[PATH_MAX + 1];
	CFIndex passwordLength;
	convertCFString(password, masterPassword, passwordLength);

	status = SecKeychainUnlock(keychain,static_cast<UInt32>(passwordLength), masterPassword, true);

	SecKeychainLock(keychain);
    CFRelease(keychain);
    return status == noErr;		// status of unlock
}

bool SFFileVault::masterPasswordUnlock(CFStringRef password)
{
    if (!password)
        return false;
    
    SecKeychainRef keychainRef = NULL;
    if (!masterPasswordEnabled(&keychainRef))
        return false;
        
    char masterPassword[PATH_MAX + 1];
	CFIndex passwordLength;
	convertCFString(password, masterPassword, passwordLength);

    OSStatus status = SecKeychainUnlock(keychainRef, passwordLength, masterPassword, true);
    CFRelease(keychainRef);
    if (status)
        MacOSError::throwMe(status);

	return true;
}

OSStatus SFFileVault::recover(CFStringRef oldUserPassword, CFStringRef newUserPassword, CFURLRef dmgin)
{
    // Note that oldUserPassword may be NULL, but newUserPassword can't be
    if (!newUserPassword)
        MacOSError::throwMe(paramErr);
#if 0
    CFRef<CFStringRef> masterKeychainPath(CFStringCreateWithBytes(kCFAllocatorDefault,reinterpret_cast<UInt8 *>(masterKeychain),
        strlen(masterKeychain), kCFStringEncodingUTF8, false));
	return chpassDIF(oldUserPassword, newUserPassword, masterKeychainPath, dmgin);
#else
//	hdiutil chpass -recover $BUILDDIR/FileVaultMaster.keychain -newstdinpass MyFileVault.sparseimage

    ExecCLITool rt;
    std::string imageFileString(convertCFURLRef(dmgin));
    std::string masterKeychain(getKeychainName());

    // In the recover case, we use oldUserPassword as the password for the master keychain
    masterPasswordUnlock(oldUserPassword);
    
   	rt.addincfs(newUserPassword,true);	// include trailing NULL
    rt.addarg("chpass");				// add the command first
#if !defined(NDEBUG)
	if (DEBUG_RECOVER)
        rt.addargs(2, "-debug", "-verbose", NULL);
#endif
    int rx = rt.run(HDIUTIL_PATH,"HDIUTIL_PATH", "-recover", masterKeychain.c_str(), "-newstdinpass", imageFileString.c_str(), NULL);
    if (rx)
        MacOSError::throwMe(rx);
    return 0;
#endif
}

OSStatus SFFileVault::compact(CFStringRef password, CFURLRef dmgin)
{
    if (!dmgin)
        MacOSError::throwMe(paramErr);
    
	CFRef<CFMutableDictionaryRef> inOptions(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
	CFRef<CFMutableDictionaryRef> imageOptions(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));

    CFDictionarySetValue(inOptions, kDIHLAgentKey, kDIHLAttachHDIUTILAgentValue);

	// kDIHLCompactPassphraseKey is defined, but never used by DIHL code
	CFDictionarySetValue(imageOptions, CFSTR(kDIPassphraseKey), password?password:CFSTR(""));

#if !defined(NDEBUG)
	const char *debugVal = getenv("FILEVAULT_DEBUG");
	if (debugVal)
    {
        CFDictionarySetValue(inOptions, kDIHLCreateDebugKey, kCFBooleanTrue);
        CFDictionarySetValue(inOptions, kDIHLCreateVerboseKey, kCFBooleanTrue);
	}
#endif

	CFDictionarySetValue(imageOptions, CFSTR(kDIBackingStoreWriteableKey), kCFBooleanTrue);

    CFDictionarySetValue(inOptions, kDIHLCompactImageURLKey, dmgin);
	// to specify image options
	if (CFDictionaryGetCount(imageOptions) > 0)
        CFDictionarySetValue(inOptions, kDIHLCompactImageOptionsKey, imageOptions);

    SFDIHLStarter dihlstarter;
    
    OSStatus status = (*FVDIHLInterface::DIHLDiskImageCompact())(inOptions, NULL, NULL);
    if (status)
	{
		syslog(LOG_ERR,"SFFileVault::compact DIHLDiskImageCompact result: %ld",status);
        MacOSError::throwMe(status);
	}
    return noErr;
}

OSStatus SFFileVault::resize(CFStringRef password, CFURLRef dmgin, u_int64_t sectors)
{
    ExecCLITool rt;
    std::string imageFileString(convertCFURLRef(dmgin));
    char sectorsString[32];
    snprintf(sectorsString,sizeof(sectorsString),"%llu",sectors);

   	rt.addincfs(password,true);		// include trailing NULL
    rt.addarg("resize");			// add the command first
#if !defined(NDEBUG)
	if (DEBUG_RESIZE)
        rt.addargs(2, "-debug", "-verbose", NULL);
#endif
    int rx = rt.run(HDIUTIL_PATH,"HDIUTIL_PATH", "-stdinpass", "-sectors", sectorsString, imageFileString.c_str(), NULL);
    if (rx)
        MacOSError::throwMe(rx);
    return 0;
}

#pragma mark -------------------- Helpers --------------------

CFStringRef SFFileVault::extractDevicepath(CFDictionaryRef devTable)
{
    CFArrayRef sysEntities = static_cast<CFArrayRef>(CFDictionaryGetValue(devTable,kDIHLAttachNewSystemEntitiesKey));
    if (sysEntities == NULL)
        return NULL;

    CFIndex dictionaryCount = CFArrayGetCount(sysEntities);
    for (CFIndex ix=0;ix < dictionaryCount;ix++)
    {
		CFDictionaryRef dict = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(sysEntities, ix));
		
		// rdar://problem/3969147 - don't assume that we are looking for Apple_HFS
		// since the filevault image could have been reformatted to be partitionless
		// and/or we could be using Apple_HFSX
		// 
		// the proper thing to do is to look for the dev node corresponding to a mount point
		if (CFDictionaryGetValue(dict, kDIHLAttachNewMountPointKey)) 
        	return CFStringCreateCopy(kCFAllocatorDefault, (CFStringRef)CFDictionaryGetValue(dict,kDIHLAttachNewDevEntryKey));
    }
    return NULL;
}

CFDataRef SFFileVault::getCertificate()
{
    // do a find in the master keychain
    std::string masterKeychain(getKeychainName());

    SecKeychainRef keychainRef = NULL;
    OSStatus status = SecKeychainOpen(masterKeychain.c_str(), &keychainRef);
    if (status)
        MacOSError::throwMe(status);

    SecKeychainSearchRef searchRef = NULL;
    status = SecKeychainSearchCreateFromAttributes(keychainRef, kSecCertificateItemClass, NULL, &searchRef);
    if (status)
    {
	    CFRelease(keychainRef);
        MacOSError::throwMe(status);
	}
	
    SecKeychainItemRef itemRef;
    status = SecKeychainSearchCopyNext(searchRef, &itemRef);
    if (status)
    {
        CFRelease(searchRef);
	    CFRelease(keychainRef);
        MacOSError::throwMe(status);
    }

    CSSM_DATA certData;
    status = SecCertificateGetData((SecCertificateRef)itemRef, &certData);
    CFDataRef theCertData = CFDataCreate(kCFAllocatorDefault, const_cast<const UInt8 *>(static_cast<UInt8 *>(certData.Data)), certData.Length);
    
    CFRelease(searchRef);
    CFRelease(itemRef);
	CFRelease(keychainRef);
    if (status)
        MacOSError::throwMe(status);
    return theCertData;
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
#if defined(NDEBUG)
    const char *envPath = _defaultMasterKeychainPath;
#else
    const char *envPath = getenv("FILEVAULT_MASTER_PATH");	// must set to full path or kc will end up in ~/Library/Keychains/
    if (!envPath)
        envPath = _defaultMasterKeychainPath;
#endif
    snprintf(masterKeychainPath, sizeof(masterKeychainPath), "%s%s", envPath, _masterKeychainName);
//    std::cout << "Masterkeychain path: " << masterKeychainPath << std::endl;
    size_t sz = strlen(masterKeychainPath)+1;
    char *path = static_cast<char *>(malloc(sz));
    strncpy(path,masterKeychainPath,sz);
    return static_cast<const char *>(path);
}

std::string SFFileVault::getKeychainName()
{
	char masterKeychain[PATH_MAX + 1];
    const auto_ptr<const char> kcpath(getKeychainPath());
    snprintf(masterKeychain, sizeof(masterKeychain), "%s.keychain", kcpath.get());
    return std::string(masterKeychain,strlen(masterKeychain));
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
	int fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd <= 0)
		return errno;

	if (lseek(fd, 0, SEEK_SET) < 0)
		return errno;
	
	int rtn = write(fd, bytes, (size_t)numBytes);
	rtn = (rtn != static_cast<int>(numBytes))?EIO:0;
	close(fd);
	return rtn;
}

void SFFileVault::convertCFString(CFStringRef inString, char outbuf[], CFIndex &usedBufLen)
{
    if (!inString)
    {
        outbuf[0]=0;
        return;
    }
   
    usedBufLen = 0;
    /*CFIndex length = */ CFStringGetBytes(inString, CFRangeMake(0,CFStringGetLength(inString)), kCFStringEncodingUTF8, 0,
        false, reinterpret_cast<UInt8 *>(outbuf), PATH_MAX, &usedBufLen);
    if (usedBufLen >= 0)
        outbuf[usedBufLen]=0;
}

std::string SFFileVault::convertCFString(CFStringRef inString, CFStringEncoding encoding)
{
    if (!inString)
    	return std::string();

    // Call once first just to get length
    const Boolean isExternalRepresentation = false;
    CFIndex usedBufLen = 0;
    UInt8 lossByte = 0;
    CFRange stringRange = CFRangeMake(0,CFStringGetLength(inString));
    CFIndex length = CFStringGetBytes(inString, stringRange, encoding, lossByte, isExternalRepresentation, NULL, 0, &usedBufLen);
        
    auto_ptr<char> stringbuffer(new char [length]);
    length = CFStringGetBytes(inString, stringRange, encoding, lossByte, isExternalRepresentation, 
        reinterpret_cast<UInt8 *>(stringbuffer.get()), length, &usedBufLen);
   
    usedBufLen = 0;
	length = CFStringGetBytes(inString, stringRange, encoding, lossByte, isExternalRepresentation, 
        reinterpret_cast<UInt8 *>(stringbuffer.get()), length, &usedBufLen);

    return (usedBufLen > 0)?(std::string (stringbuffer.get(),usedBufLen)):std::string();
}

std::string SFFileVault::convertCFURLRef(CFURLRef fileRef)
{
    if (!fileRef)
        MacOSError::throwMe(paramErr);
    const Boolean resolveAgainstBase = true;
    char fileString[PATH_MAX + 1];
    if (!CFURLGetFileSystemRepresentation(fileRef, resolveAgainstBase, reinterpret_cast<UInt8 *>(fileString), PATH_MAX))
        MacOSError::throwMe(paramErr);
    return std::string(fileString,strlen(fileString));
}

#pragma mark -------------------- SFDIHLStarter implementation --------------------

SFDIHLStarter::SFDIHLStarter() : _dihstarted(false)
{
    if (_dihstarted)
        return;
    int status = (*FVDIHLInterface::DIInitialize())();
    if (status)
        MacOSError::throwMe(status);
    _dihstarted = true;
}

SFDIHLStarter::~SFDIHLStarter()
{
    if (_dihstarted)
        (*FVDIHLInterface::DIDeinitialize())();
}
