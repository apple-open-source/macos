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
 * SFFileVault.h
 */

#include <string>
#include <Security/SecBase.h>
#include <Security/cssmtype.h>
#include <CoreFoundation/CFURL.h>

class ExecCLITool;

class SFFileVault
{
public:
    SFFileVault() {};
    ~SFFileVault() {};
    
    OSStatus mount(CFStringRef password, CFURLRef dmgin, CFStringRef inMountPoint, CFStringRef *devicepath,
        CFTypeRef certificateOrArray);
    OSStatus mastermount(CFURLRef dmgin, CFStringRef inMountPoint, CFStringRef *devicepath);
	OSStatus unmount(CFStringRef devicepath);

    OSStatus userChangePassword(CFStringRef oldPassword,CFStringRef newPassword, CFURLRef dmgin);
	OSStatus makeMasterPassword(CFStringRef masterPasswordPassword, SecKeychainRef *keychain);
    OSStatus create(CFStringRef password, CFURLRef dmgout, CFStringRef volumeName, int64_t sectors, uid_t uid, gid_t gid,
        CFTypeRef certificateOrArray);
    Boolean masterPasswordEnabled(SecKeychainRef *keychainRef);
    OSStatus changeMasterPasswordPassword(CFStringRef oldPassword,CFStringRef newPassword);
    Boolean masterPasswordValidates(CFStringRef password);
    OSStatus recover(CFStringRef masterPassword, CFStringRef newUserPassword, CFURLRef dmgin);
    OSStatus compact(CFStringRef password, CFURLRef dmgin);
	OSStatus resize(CFStringRef password, CFURLRef dmgin, u_int64_t sectors);

    CFDataRef getCertificate();

private:
    CFStringRef extractDevicepath(CFDictionaryRef devTable);
    const char *getKeychainPath();
    const char *getCertificateFileName();
    std::string getKeychainName();
    OSStatus writeCertificateFile(CFDataRef certData);
    int writeFile(const char *fileName, const unsigned char *bytes, unsigned int numBytes);
    void convertCFString(CFStringRef inString, char outbuf[], CFIndex &usedBufLen);
    std::string convertCFString(CFStringRef inString, CFStringEncoding encoding=kCFStringEncodingUTF8);
    std::string convertCFURLRef(CFURLRef fileRef);
    bool certificateExists(SecKeychainRef keychainRef);
    bool masterPasswordUnlock(CFStringRef password);

    OSStatus mountViaDIF(CFStringRef password, CFURLRef imageURL,
        CFStringRef inMountPoint, CFStringRef *devicepath, CFTypeRef certificateOrArray);
    OSStatus chpassDIF(CFStringRef oldOrMasterPassword, CFStringRef newPassword, CFStringRef masterKeychainPath, CFURLRef dmgin);

    static const char * const _defaultMasterKeychainPath;
    static const char * const _masterKeychainName ;	
};

class SFDIHLStarter
{
public:
    SFDIHLStarter();
    ~SFDIHLStarter();
private:
    bool _dihstarted;
};
