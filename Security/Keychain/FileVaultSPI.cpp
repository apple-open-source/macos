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
 *  FileVaultSPI.cpp
 *  Security
 *
 *  Created by john on Wed Jul 09 2003.
 *  Copyright (c) 2003 Apple. All rights reserved.
 *
 */

#include "FileVaultSPI.h"
#include "SFFileVault.h"
#include <Security/SecBridge.h>
#include <Security/cfutilities.h>

#pragma mark -------------------- SecFileVault implementation --------------------

OSStatus SecFileVaultCreate (CFStringRef password, bool enableMasterPassword, CFURLRef dmgout, CFStringRef volumeName,
    CFStringRef sizeSpec)
{
    // Note that we do not need the master keychain password, since we can encrypt the
    // image key with the public key.
    BEGIN_SECAPI

        SFFileVault sffv;
        CFURLRef certificate = NULL;
        if (enableMasterPassword)
            sffv.getCertificate(&certificate);

        sffv.create(password, certificate, dmgout, volumeName, sizeSpec);
        if (certificate)	//@@@ leak if error thrown
            CFRelease(certificate);
            
    END_SECAPI
}

OSStatus SecFileVaultUserMount (CFStringRef password, CFURLRef dmgin, CFURLRef mountpoint, CFStringRef *devicepath)
{
    BEGIN_SECAPI

        SFFileVault sffv;
        sffv.mount(password, NULL, dmgin, mountpoint, devicepath);

    END_SECAPI
}

OSStatus SecFileVaultMasterMount(CFURLRef dmgin, CFURLRef mountpoint, CFStringRef *devicepath)
{
    BEGIN_SECAPI
    
        SFFileVault sffv;
        sffv.mastermount(dmgin,mountpoint,devicepath);

    END_SECAPI
}

OSStatus SecFileVaultUnmount(CFURLRef mountpoint, CFStringRef devicepath)
{
    BEGIN_SECAPI

        SFFileVault sffv;
        sffv.unmount(mountpoint,devicepath);

    END_SECAPI
}

OSStatus SecFileVaultUserChangePassword(CFStringRef password, CFStringRef devicepath)
{
    BEGIN_SECAPI

        SFFileVault fv;
        fv.userChangePassword(password, devicepath);

    END_SECAPI
}

OSStatus SecFileVaultMakeMasterPassword(CFStringRef masterPasswordPassword)
{
    BEGIN_SECAPI

        SFFileVault fv;
        SecKeychainRef keychainRef;
        fv.makeMasterPassword(masterPasswordPassword,&keychainRef);
    
    END_SECAPI
}

Boolean SecFileVaultMasterPasswordEnabled(SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI

        SFFileVault fv;
        return fv.masterPasswordEnabled(keychainRef);
    
    END_SECAPI1(false)
}

OSStatus SecFileVaultChangeMasterPasswordPassword(CFStringRef oldPassword,CFStringRef newPassword)
{
    BEGIN_SECAPI

        SFFileVault fv;
        fv.changeMasterPasswordPassword(oldPassword,newPassword);
    
    END_SECAPI
}

#pragma mark -------------------- SecFileVault extended implementation --------------------

OSStatus SecFileVaultMount(CFStringRef password, CFURLRef certificate, CFURLRef dmgin, CFURLRef mountpoint,
    CFStringRef *devicepath)
{
    BEGIN_SECAPI
    
        SFFileVault sffv;
        sffv.mount(password, certificate, dmgin, mountpoint, devicepath);

    END_SECAPI
}


OSStatus SecFileVaultCreateUsingCertificate (CFStringRef password, CFURLRef certificate, CFURLRef dmgout, CFStringRef volumeName,
    CFStringRef sizeSpec)
{
    BEGIN_SECAPI

        SFFileVault sffv;
        sffv.create(password, certificate, dmgout, volumeName, sizeSpec);
    
    END_SECAPI
}


