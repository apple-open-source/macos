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
 *  SFFileVault.h
 *  testFileVaultSPI
 *
 *  Created by john on Mon Jul 14 2003.
 *
 */

#include <Security/SecBase.h>
#include <CoreFoundation/CFURL.h>

class ExecCLITool;

class SFFileVault
{
public:
    SFFileVault() {};
    ~SFFileVault() {};
    
    OSStatus mount(CFStringRef password, CFURLRef certificate, CFURLRef dmgin,
        CFURLRef mountpoint,CFStringRef *devicepath);
    OSStatus mastermount(CFURLRef dmgin, CFURLRef mountpoint, CFStringRef *devicepath);
	OSStatus unmount(CFURLRef mountpoint,CFStringRef devicepath);

    OSStatus userChangePassword(CFStringRef password, CFStringRef devicepath);
	OSStatus makeMasterPassword(CFStringRef masterPasswordPassword, SecKeychainRef *keychain);
    OSStatus create(CFStringRef password, CFURLRef certificate, CFURLRef dmgout,
        CFStringRef volumeName, CFStringRef sizeSpec);
    Boolean masterPasswordEnabled(SecKeychainRef *keychainRef);
    OSStatus changeMasterPasswordPassword(CFStringRef oldPassword,CFStringRef newPassword);

    OSStatus getCertificate(CFURLRef *certificate);

private:
    CFStringRef extractDevicepath(const ExecCLITool& rt);
    const char *getKeychainPath();
    const char *getCertificateFileName();
    OSStatus writeCertificateFile(CFDataRef certData);
    int writeFile(const char *fileName, const unsigned char *bytes, unsigned int numBytes);

    static const char * const _defaultMasterKeychainPath;
    static const char * const _masterKeychainName ;	
};
