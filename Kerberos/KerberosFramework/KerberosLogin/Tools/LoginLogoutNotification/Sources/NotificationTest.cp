#include <ConditionalMacros.h>
#include <CodeFragments.h>
#include <MacTypes.h>
#include <string.h>
#include <Memory.h>
#include <Errors.h>
#include <Dialogs.h>
#include <TextUtils.h>

#include <CFM.InitTerm.h>

#include <KerberosSupport/ErrorLib.h>

#include <KerberosLogin/KLLoginLogoutNotification.h>
#include <CredentialsCache/CredentialsCache.h>

extern "C" {

#define kNotificationTestErrorTable 128

pascal OSErr
InitializeNotificationTest (
	CFragInitBlockPtr	inInitBlock)
{
	FSSpec	libraryFile;
	OSErr err = __initialize (inInitBlock);
	
	if (err != noErr)
		return err;

	if (inInitBlock -> fragLocator.where == kDataForkCFragLocator) {
		libraryFile = *(inInitBlock -> fragLocator.u.onDisk.fileSpec);
	} else if (inInitBlock -> fragLocator.where == kResourceCFragLocator) {
		libraryFile = *(inInitBlock -> fragLocator.u.inSegs.fileSpec);
	}

	err = ::RegisterErrorTable (&libraryFile, kNotificationTestErrorTable);
	
	return err;
}

#pragma export on

KLStatus KerberosLoginNotification_InitializePlugin (
	KLN_APIVersion		inAPIVersion)
{
	if (inAPIVersion == kKLN_APIVersion_1) {
		return noErr;
	} else {
		return paramErr;
	}
}

KLStatus KerberosLoginNotification_Login (
	KLN_LoginType		inLoginType,
	const char*			inCredentialsCache)
{
	switch (inLoginType) {
		case kKLN_DialogLogin:
			AlertStdAlertParamRec alertParam;
			alertParam.movable = false;
			alertParam.helpButton = false;
			alertParam.filterProc = nil;
			alertParam.defaultText = "\pLaunch ZIG";
			alertParam.cancelText = "\pWhat you say?";
			alertParam.otherText = nil;
			alertParam.defaultButton = kAlertStdAlertOKButton;
			alertParam.cancelButton = kAlertStdAlertCancelButton;
			alertParam.position = kWindowAlertPositionParentWindow;

			SInt16	item;
			OSErr err = StandardAlert (kAlertNoteAlert,
				"\pAll your base are belong to us!", "\pSomeone set up us the bomb",
				&alertParam, &item);
			if ((err == noErr) && (item == kAlertStdAlertOKButton)) {
				KLAcquireTickets (nil, nil, nil);
				return noErr;
			} else {
				return 22221;
			}
			
		case kKLN_PasswordLogin:
			static NMRec	notification;
			static Str255	principal;
			notification.qType = nmType;
			notification.nmStr = principal;
			notification.nmResp = (NMUPP) -1;
			
			cc_context_t	ccContext = nil;
			cc_ccache_t		ccache = nil;
			cc_string_t		ccPrincipal = nil;
			
			cc_int32 ccErr = cc_initialize (&ccContext, ccapi_version_4, nil, nil);
			if (ccErr == ccNoError) {
				ccErr = cc_context_open_ccache (ccContext, inCredentialsCache, &ccache);
			}
			
			if (ccErr == ccNoError) {
				ccErr = cc_ccache_get_principal (ccache, cc_credentials_v4, &ccPrincipal);
			}
			
			if (ccErr == ccNoError) {
#if TARGET_API_MAC_OS8			
				strncpy ((char*) principal, ccPrincipal -> data, sizeof (principal));
				principal [sizeof (principal)] = '\0';
				c2pstr ((char*) principal);
#else
				CopyCStringToPascal (ccPrincipal -> data, principal);
#endif
				NMInstall (&notification);
			}
			
			if (ccPrincipal != nil) {
				cc_string_release (ccPrincipal);
			}
			
			if (ccache != nil) {
				cc_ccache_release (ccache);
			}
			
			if (ccContext != nil) {
				cc_context_release (ccContext);
			}
	}

	return noErr;
}

void KerberosLoginNotification_Logout (
	const char*			inCredentialsCache)
{
	static NMRec	notification;
	static Str255	principal;
	notification.qType = nmType;
	notification.nmStr = principal;
	notification.nmResp = (NMUPP) -1;
	
	cc_context_t	ccContext = nil;
	cc_ccache_t		ccache = nil;
	cc_string_t		ccPrincipal = nil;
	
	cc_int32 ccErr = cc_initialize (&ccContext, ccapi_version_4, nil, nil);
	if (ccErr == ccNoError) {
		ccErr = cc_context_open_ccache (ccContext, inCredentialsCache, &ccache);
	}
	
	if (ccErr == ccNoError) {
		ccErr = cc_ccache_get_principal (ccache, cc_credentials_v4, &ccPrincipal);
	}
	
	if (ccErr == ccNoError) {
#if TARGET_API_MAC_OS8			
		strncpy ((char*) principal, ccPrincipal -> data, sizeof (principal));
		principal [sizeof (principal)] = '\0';
		c2pstr ((char*) principal);
#else
		CopyCStringToPascal (ccPrincipal -> data, principal);
#endif
		NMInstall (&notification);
	}
	
	if (ccPrincipal != nil) {
		cc_string_release (ccPrincipal);
	}
	
	if (ccache != nil) {
		cc_ccache_release (ccache);
	}
	
	if (ccContext != nil) {
		cc_context_release (ccContext);
	}
}

}
