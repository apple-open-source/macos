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
 * FileVault.cpp
 */

#include "FileVaultPriv.h"
#include "SFFileVault.h"
#include <Security/SecBasePriv.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/errors.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

// Analogous to BEGIN_FVAPI, but we don't need and shouldn't hold the sec lock
#define BEGIN_FVAPI \
	try {

#define END_FVAPI \
	} \
	catch (const MacOSError &err) { return err.osStatus(); } \
	catch (const CommonError &err) { return SecKeychainErrFromOSStatus(err.osStatus()); } \
	catch (const std::bad_alloc &) { return memFullErr; } \
	catch (...) { return internalComponentErr; } \
    return noErr;
#define END_FVAPI1(bad)	} catch (...) { return bad; }

#pragma mark -------------------- SecFileVault implementation --------------------

OSStatus SecFileVaultCreate (CFStringRef password, bool enableMasterPassword, CFURLRef dmgout, CFStringRef volumeName,
    int64_t sectors, uid_t uid, gid_t gid)
{
    // Note that we do not need the master keychain password, since we can encrypt the
    // image key with the public key.
    BEGIN_FVAPI

        SFFileVault sffv;
        CFRef<CFDataRef> certificate = enableMasterPassword?sffv.getCertificate():NULL;
        sffv.create(password, dmgout, volumeName, sectors, uid, gid, certificate);
            
    END_FVAPI
}

OSStatus SecFileVaultUserMount (CFStringRef password, CFURLRef dmgin, CFStringRef mountpoint, CFStringRef *devicepath)
{
    BEGIN_FVAPI

        SFFileVault sffv;
        sffv.mount(password, dmgin, mountpoint, devicepath, NULL);

    END_FVAPI
}

OSStatus SecFileVaultMasterMount(CFURLRef dmgin, CFStringRef mountpoint, CFStringRef *devicepath)
{
    BEGIN_FVAPI
    
        SFFileVault sffv;
        sffv.mastermount(dmgin,mountpoint,devicepath);

    END_FVAPI
}

OSStatus SecFileVaultUnmount(CFStringRef devicepath)
{
    BEGIN_FVAPI

        SFFileVault sffv;
        sffv.unmount(devicepath);

    END_FVAPI
}

OSStatus SecFileVaultUserChangePassword(CFStringRef oldPassword,CFStringRef newPassword, CFURLRef dmgin)
{
    BEGIN_FVAPI

        SFFileVault fv;
        fv.userChangePassword(oldPassword, newPassword, dmgin);

    END_FVAPI
}

OSStatus SecFileVaultMakeMasterPassword(CFStringRef masterPasswordPassword)
{
    BEGIN_FVAPI

        SFFileVault fv;
        SecKeychainRef keychainRef;
        fv.makeMasterPassword(masterPasswordPassword,&keychainRef);
    
    END_FVAPI
}

Boolean SecFileVaultMasterPasswordEnabled(SecKeychainRef *keychainRef)
{
    BEGIN_FVAPI

        SFFileVault fv;
        return fv.masterPasswordEnabled(keychainRef);
    
    END_FVAPI1(false)
}

OSStatus SecFileVaultChangeMasterPasswordPassword(CFStringRef oldPassword,CFStringRef newPassword)
{
    BEGIN_FVAPI

        SFFileVault fv;
        fv.changeMasterPasswordPassword(oldPassword,newPassword);
    
    END_FVAPI
}

Boolean SecFileVaultMasterPasswordValidates(CFStringRef password)
{
    SFFileVault sffv;
	return sffv.masterPasswordValidates(password);
}

OSStatus SecFileVaultRecover(CFStringRef masterPassword, CFStringRef newUserPassword, CFURLRef dmgin)
{
    BEGIN_FVAPI

        SFFileVault sffv;
        sffv.recover(masterPassword, newUserPassword, dmgin);
    
    END_FVAPI
}

OSStatus SecFileVaultCompact(CFStringRef password, CFURLRef dmgin)
{
    BEGIN_FVAPI

        SFFileVault sffv;
        sffv.compact(password, dmgin);

    END_FVAPI
}

OSStatus SecFileVaultResize(CFStringRef password, CFURLRef dmgin, u_int64_t sectors)
{
    BEGIN_FVAPI

    SFFileVault sffv;
    sffv.resize(password, dmgin, sectors);

    END_FVAPI
}

#pragma mark -------------------- SecFileVault extended implementation --------------------

OSStatus SecFileVaultMount(CFStringRef password, CFURLRef dmgin, CFStringRef mountpoint,
    CFStringRef *devicepath, CFTypeRef certificateOrArray)
{
    BEGIN_FVAPI
    
        SFFileVault sffv;
        sffv.mount(password, dmgin, mountpoint, devicepath, certificateOrArray);

    END_FVAPI
}

OSStatus SecFileVaultCreateUsingCertificate (CFStringRef password, CFURLRef dmgout, CFStringRef volumeName, int64_t sectors,
    uid_t uid, gid_t gid, CFTypeRef certificateOrArray)
{
    BEGIN_FVAPI

        SFFileVault sffv;
        sffv.create(password, dmgout, volumeName, sectors, uid, gid, certificateOrArray);
            
    END_FVAPI
}


