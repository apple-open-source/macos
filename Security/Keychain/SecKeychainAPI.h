/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
 *  SecKeychainAPI.h
 *  SecurityCore
 *
 *    Copyright:  (c) 2000 by Apple Computer, Inc., all rights reserved
 *
 */

/*!
	@header SecKeychainAPI The Security Core API contains all the APIs need to create a Keychain management application, minus the HI.
	 
	NOTE: Any function with Create or Copy in the name returns an object that must be released.
*/

#if !defined(__SECKEYCHAINAPI__)
#define __SECKEYCHAINAPI__ 1

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include <Security/cssmapple.h>


#if defined(__cplusplus)
extern "C" {
#endif

#ifndef __SEC_TYPES__
#define __SEC_TYPES__

/*!
@typedef SecKeychainRef
Opaque Structure to a Keychain reference.
*/
typedef struct OpaqueSecKeychainRef				*SecKeychainRef;
/*!
@typedef SecKeychainItemRef
Opaque Structure to a Keychain item reference.
*/
typedef struct OpaqueSecKeychainItemRef			*SecKeychainItemRef;
/*!
@typedef SecKeychainSearchRef
Opaque Structure to a Keychain search reference.
*/
typedef struct OpaqueSecKeychainSearchRef		*SecKeychainSearchRef;

typedef OSType	SecKeychainAttrType;
/*!
@struct SecKeychainAttribute
Security Item attributes. 
*/
struct SecKeychainAttribute {
    SecKeychainAttrType          tag;                            /* 4-byte attribute tag */
    UInt32                       length;                         /* Length of attribute data */
    void *                       data;                           /* Pointer to attribute data */
};
typedef struct SecKeychainAttribute      SecKeychainAttribute;
typedef SecKeychainAttribute *           SecKeychainAttributePtr;

/*!
@struct SecKeychainAttributeList
Security attribute list. 
*/
struct SecKeychainAttributeList {
    UInt32                       		 count;                          /* How many attributes in the array */
    SecKeychainAttribute *               attr;                           /* Pointer to first attribute in array */
};
typedef struct SecKeychainAttributeList  SecKeychainAttributeList;

typedef UInt32 SecKeychainStatus;

#endif

/*!
@enum TableIDs
*/
enum {
    kSecGenericPasswordItemTableID = CSSM_DL_DB_RECORD_GENERIC_PASSWORD,                  /* Generic password */
    kSecInternetPasswordItemTableID = CSSM_DL_DB_RECORD_INTERNET_PASSWORD,                /* Internet password */
    kSecAppleSharePasswordItemTableID = CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD             /* AppleShare password */
};

/*!
@struct SecKeychainAttributeInfo
Security attribute tag list. 
*/
struct SecKeychainAttributeInfo {
    UInt32                       		 count;                   		 /* How many items in the array */
    UInt32 *            				 tag;                            /* Pointer to first attribute tag in array */
	UInt32 *            				 format;                         /* Pointer to first attribute format in array */
};
typedef struct SecKeychainAttributeInfo  SecKeychainAttributeInfo;



/*!
@typedef SecTypeRef
Opaque pointer to one a pointer to a security referece.
Such as SecKeychainSearchRef, SecKeychainItemRef and SecKeychainItemRef.
*/
typedef void									*SecTypeRef;

/*!
@enum KeychainErrors 
*/
enum {
    errSecNotAvailable           = -25291,
    errSecReadOnly               = -25292,
    errSecAuthFailed             = -25293,
    errSecNoSuchKeychain         = -25294,
    errSecInvalidKeychain        = -25295,
    errSecDuplicateKeychain      = -25296,
    errSecDuplicateCallback      = -25297,
    errSecInvalidCallback        = -25298,
    errSecDuplicateItem          = -25299,
    errSecItemNotFound           = -25300,
    errSecBufferTooSmall         = -25301,
    errSecDataTooLarge           = -25302,
    errSecNoSuchAttr             = -25303,
    errSecInvalidItemRef         = -25304,
    errSecInvalidSearchRef       = -25305,
    errSecNoSuchClass            = -25306,
    errSecNoDefaultKeychain      = -25307,
    errSecInteractionNotAllowed  = -25308,
    errSecReadOnlyAttr           = -25309,
    errSecWrongSecVersion        = -25310,
    errSecKeySizeNotAllowed      = -25311,
    errSecNoStorageModule        = -25312,
    errSecNoCertificateModule    = -25313,
    errSecNoPolicyModule         = -25314,
    errSecInteractionRequired    = -25315,
    errSecDataNotAvailable       = -25316,
    errSecDataNotModifiable      = -25317,
    errSecCreateChainFailed      = -25318
};

/*!
@enum KeychainEvents
Events relating to the state of the default Keychain.
*/
typedef UInt16 SecKeychainEvent;
enum {
    kSecLockEvent                = 1,                            /* a keychain was locked */
    kSecUnlockEvent              = 2,                            /* a keychain was unlocked */
    kSecAddEvent                 = 3,                            /* an item was added to a keychain */
    kSecDeleteEvent              = 4,                            /* an item was deleted from a keychain */
    kSecUpdateEvent              = 5,                            /* an item was updated */
    kSecPasswordChangedEvent     = 6,                            /* the keychain password was changed */
    kSecSystemEvent              = 8,                            /* the keychain client can process events */
    kSecDefaultChangedEvent      = 9,                            /* the default keychain was changed */
    kSecDataAccessEvent          = 10,                           /* a process has accessed a keychain item's data */
    kSecKeychainListChangedEvent = 11                            /* the list of keychains has changed */
};


typedef UInt16 SecKeychainEventMask;
enum {
    kSecLockEventMask            = 1 << kSecLockEvent,
    kSecUnlockEventMask          = 1 << kSecUnlockEvent,
    kSecAddEventMask             = 1 << kSecAddEvent,
    kSecDeleteEventMask          = 1 << kSecDeleteEvent,
    kSecUpdateEventMask          = 1 << kSecUpdateEvent,
    kSecPasswordChangedEventMask = 1 << kSecPasswordChangedEvent,
    kSecSystemEventEventMask     = 1 << kSecSystemEvent,
    kSecDefaultChangedEventMask  = 1 << kSecDefaultChangedEvent,
    kSecDataAccessEventMask      = 1 << kSecDataAccessEvent,
    kSecEveryEventMask           = 0xFFFF                        /* all of the above*/
};

typedef UInt8                    SecAFPServerSignature[16];
typedef UInt8                    SecPublicKeyHash[20];

/*!
@enum KeychainStatus
The current status of the Keychain.
*/
enum {
    kSecUnlockStateStatus        = 1,
    kSecRdPermStatus             = 2,
    kSecWrPermStatus             = 4
};

typedef FourCharCode             SecItemClass;
/*!
@enum KeychainItemClasses
Keychain item classes
*/

enum {
    kSecInternetPasswordItemClass = 'inet',                   /* Internet password */
    kSecGenericPasswordItemClass = 'genp',                    /* Generic password */
    kSecAppleSharePasswordItemClass = 'ashp'                  /* AppleShare password */
};


/*!
@enum FourCharacterCodes
*/
enum {
                                                                 /* Common attributes */
    kSecCreationDateItemAttr     = 'cdat',                       /* Date the item was created (UInt32) */
    kSecModDateItemAttr          = 'mdat',                       /* Last time the item was updated (UInt32) */
    kSecDescriptionItemAttr      = 'desc',                       /* User-visible description string (string) */
    kSecCommentItemAttr          = 'icmt',                       /* User's comment about the item (string) */
    kSecCreatorItemAttr          = 'crtr',                       /* Item's creator (OSType) */
    kSecTypeItemAttr             = 'type',                       /* Item's type (OSType) */
    kSecScriptCodeItemAttr       = 'scrp',                       /* Script code for all strings (ScriptCode) */
    kSecLabelItemAttr            = 'labl',                       /* Item label (string) */
    kSecInvisibleItemAttr        = 'invi',                       /* Invisible (boolean) */
    kSecNegativeItemAttr         = 'nega',                       /* Negative (boolean) */
    kSecCustomIconItemAttr       = 'cusi',                       /* Custom icon (boolean) */
                                                                 /* Unique Generic password attributes */
    kSecAccountItemAttr          = 'acct',                       /* User account (string) - also applies to Appleshare and Generic */
    kSecServiceItemAttr          = 'svce',                       /* Service (string) */
    kSecGenericItemAttr          = 'gena',                       /* User-defined attribute (untyped bytes) */
                                                                 /* Unique Internet password attributes */
    kSecSecurityDomainItemAttr   = 'sdmn',                       /* urity domain (string) */
    kSecServerItemAttr           = 'srvr',                       /* Server's domain name or IP address (string) */
    kSecAuthTypeItemAttr         = 'atyp',                       /* Authentication Type (AuthType) */
    kSecPortItemAttr             = 'port',                       /* Port (UInt32) */
    kSecPathItemAttr             = 'path',                       /* Path (string) */
                                                                 /* Unique Appleshare password attributes */
    kSecVolumeItemAttr           = 'vlme',                       /* Volume (string) */
    kSecAddressItemAttr          = 'addr',                       /* Server address (IP or domain name) or zone name (string) */
    kSecSignatureItemAttr        = 'ssig',                       /* Server signature block (AFPServerSignature) */
                                                                 /* Unique AppleShare and Internet attributes */
    kSecProtocolItemAttr         = 'ptcl',                       /* Protocol (ProtocolType) */

};

typedef FourCharCode SecItemAttr;


/*!
@enum SecurityAuthTypeCodes
*/
enum {
    kSecAuthTypeNTLM             = 'ntlm',
    kSecAuthTypeMSN              = 'msna',
    kSecAuthTypeDPA              = 'dpaa',
    kSecAuthTypeRPA              = 'rpaa',
    kSecAuthTypeHTTPDigest       = 'httd',
    kSecAuthTypeDefault          = 'dflt'
};
typedef FourCharCode             SecAuthType;

/*!
@enum SecurityProtocolTypeCodes
*/
enum {
    kSecProtocolTypeFTP          = 'ftp ',
    kSecProtocolTypeFTPAccount   = 'ftpa',
    kSecProtocolTypeHTTP         = 'http',
    kSecProtocolTypeIRC          = 'irc ',
    kSecProtocolTypeNNTP         = 'nntp',
    kSecProtocolTypePOP3         = 'pop3',
    kSecProtocolTypeSMTP         = 'smtp',
    kSecProtocolTypeSOCKS        = 'sox ',
    kSecProtocolTypeIMAP         = 'imap',
    kSecProtocolTypeLDAP         = 'ldap',
    kSecProtocolTypeAppleTalk    = 'atlk',
    kSecProtocolTypeAFP          = 'afp ',
    kSecProtocolTypeTelnet       = 'teln'
};
typedef FourCharCode             SecProtocolType;

/*!
@typedef KCChangeSettingsInfo
Keychain Settings
*/
struct SecKeychainSettings
{ 
	UInt32			      		version; 
	Boolean	              		lockOnSleep; 
	Boolean                  	useLockInterval; 
	UInt32                		lockInterval; 
};
typedef struct SecKeychainSettings		SecKeychainSettings;

#define SEC_KEYCHAIN_SETTINGS_VERS1 1

struct SecKeychainCallbackInfo 
{
    UInt32								version;
    SecKeychainItemRef					item;
    long								processID[2];
    long								event[4]; 
    SecKeychainRef						keychain;
};
typedef struct SecKeychainCallbackInfo SecKeychainCallbackInfo;
									

/*!
    @function SecKeychainGetVersion
    Returns the version of the Keychain Manager (an unsigned 32-bit integer) in version.
    
    @param returnVers Pointer to a UNInt32 to receive the version number.
    @result noErr 0 No error.
            errSecNotAvailable -25291 Keychain Manager was not loaded.
*/
OSStatus SecKeychainGetVersion(UInt32 *returnVers);

/*!
    @function SecKeychainOpen
    Returns a referenece to the keychain specified by keychainFile.
    The memory that keychain occupies must be released by calling SecKeychainRelease when finished
    with it.
    
    @param pathName A posix path to the keychain file.
    @param keychainRef Returned keychain reference.
    @result noErr 0 No error.
            paramErr -50 The keychain parameter is invalid (NULL).
*/
OSStatus SecKeychainOpen(const char *pathName, SecKeychainRef *keychainRef);

/*!
	@function SecKeychainCreateNew
    Returns a referenece to the keychain specified by keychainFile.
    The memory that keychain occupies must be released by calling SecKeychainRelease when finished
    with it.
    
    @param pathName A posix path to the keychain file.
    @param promptUser Display a password dialog to the user.
    @param keychainRef Returned keychain reference.
    @param passwordLength Max length of the password buffer.
    @param password A pointer to buffer with the password.  Must be in canonical UTF8 encoding.
    @result noErr 0 No error.
            paramErr -50 The keychain parameter is invalid (NULL).
*/
OSStatus SecKeychainCreateNew(const char *pathName, SecKeychainRef *keychainRef, UInt32 passwordLength, const void *password, Boolean promptUser);

/*!
	@function SecKeychainDelete
    Deletes a the keychain specified by keychainRef.
     
    @param keychainRef keychain to delete reference.
    @result noErr 0 No error.
            paramErr -50 The keychain parameter is invalid (NULL).
*/
OSStatus SecKeychainDelete(SecKeychainRef keychainRef);

/*!
	@function SecKeychainSetSettings
	Changes the settings of keychain including the lockOnSleep, useLockInterval and lockInterval.
	
    @param keychainRef keychain reference of the keychain to set.
 	@param newSettings A SecKeychainSettings structure pointer.
    @result noErr 0 No error.
*/
OSStatus SecKeychainSetSettings(SecKeychainRef keychainRef, const SecKeychainSettings *newSettings);

/*!
	@function SecKeychainCopySettings
	Copy the settings of keychain including the lockOnSleep, useLockInterval and lockInterval.  Because this structure is versioned
	the caller is required to preallocate it and fill in the version of the structure.

    @param keychainRef keychain reference of the keychain settings to copy.
    @param outSettings  A SecKeychainSettings structure pointer.
	@result noErr 0 No error.
*/
OSStatus SecKeychainCopySettings(SecKeychainRef keychainRef, SecKeychainSettings *outSettings);

/*!
	@function SecKeychainUnlock
	Unlocks the specified keychain.
	
    @param keychainRef A reference to the keychain to be unlocked.
	@param passwordLength The length of the password buffer.
	@param password A buffer with the password for the keychain.
	@param usePassword By setting this flag the password parameter is either used or ignored.
    @result noErr 0 No error.
*/
OSStatus SecKeychainUnlock(SecKeychainRef keychainRef, UInt32 passwordLength, void *password, Boolean usePassword);

/*!
	@function SecKeychainLock
	Locks the specified keychain.

    @param keychainRef A reference to the keychain to be Locked.
    @result noErr 0 No error.
*/
OSStatus SecKeychainLock(SecKeychainRef	keychainRef);

/*!
	@function SecKeychainLockAll
	Locks all keychains.

    @result noErr 0 No error.
*/
OSStatus SecKeychainLockAll();

/*!
	@function SecKeychainCopyDefault
	This routine returns a SecKeychainRef which specifies the default keychain. Your application
	might call this routine to obtain the name and location of the default keychain.
	
	@param SecKeychainRef A pointer to a reference of the default keychain.
	@result noErr 0 No error.
			errSecNoDefaultKeychain -25307 There is no currently default keychain.
*/
OSStatus SecKeychainCopyDefault(SecKeychainRef *keychainRef);

/*!
	@function SecKeychainSetDefault
	This routine sets the default keychain to the keychain specified by keychain. 
	
	@param SecKeychainRef A pointer to a reference of the default keychain.
    @result noErr 0 No error.
	paramErr -50 The input specification parameter was NULL.
	errSecNoSuchKeychain -25294 The specified keychain could not be found.
	errSecInvalidKeychain -25295 The specified keychain is invalid
*/
OSStatus SecKeychainSetDefault(SecKeychainRef keychainRef);

/*!
	@function SecKeychainGetStatus
	
	Returns status information for the specified keychain in the supplied parameter. If keychain is NULL,
	the status of the default keychain is returned.
	
	The value returned in keychainStatus is a 32-bit field, the meaning of which must be determined
	by comparison with a list of predefined constants. 
	
	Currently defined bitmask values are:
		kSecUnlockStateStatus 	1 The specified keychain is unlocked if bit 0 is set.
		kSecRdPermStatus 		2 The specified keychain is unlocked with read permission if bit 1 is set.
		kSecWrPermStatus 		4 The specified keychain is unlocked with write permission if bit 2 is set.

	@param keychainRef Pointer to a keychain reference (NULL specifies the default keychain).
	@param keychainRefStatus Returned status of the specified keychain.

    @result noErr 0 No error.
			errSecNoSuchKeychain -25294 The specified keychain could not be found.
			errSecInvalidKeychain -25295 The specified keychain is invalid.	
*/
OSStatus SecKeychainGetStatus(SecKeychainRef keychainRef, SecKeychainStatus* keychainStatus);

/*!
	@function SecKeychainRelease
	Releases  keychain item references
	 
	@param keychainRef A keychain  reference to release.
    @result noErr 0 No error.
*/
OSStatus SecKeychainRelease(SecKeychainRef itemRef);

/*!
	@function SecKeychainGetPath
	Get the path location of the specified keychain.
    @param keychainRef A reference to a keychain.
    @param ioPathLength On input specifies the size or the buffer pointed to by path and on output the length of the buffer 
                        (without the zero termination which is added)
	@param pathName A posix path to the receive keychain filename.
    @result noErr 0 No error.
*/
OSStatus SecKeychainGetPath(SecKeychainRef keychainRef, UInt32 *ioPathLength, char *pathName);

/*!
	@function SecKeychainListGetCount
	This function returns the number of available keychains. This number includes all keychains within
	the "Keychains" folder, as well as any other keychains known to the Keychain Manager.
    @result the number of keychains.
*/
UInt16 SecKeychainListGetCount(void);

/*!
	@function SecKeychainListCopyKeychainAtIndex
	This routine to copies a keychain item from the default keychain to another. 
	@param index The index of the item to copy.
	@param keychainRef A keychain reference of the destination keychain.
	@result noErr 0 No error.
			errSecInvalidKeychain -25295 The specified destination keychain was invalid.
			errSecReadOnly -25292 The destination keychain is read only.
			errSecNoSuchClass -25306 item has an invalid keychain item class.
*/
OSStatus SecKeychainListCopyKeychainAtIndex(UInt16 index, SecKeychainRef *keychainRef);

/*!
	@function SecKeychainItemCreateFromContent
	Creates a new keychain item from the supplied parameters. A reference to the newly-created
	item is returned in item. A copy of the data buffer pointed to by data is stored in the item.
	When the item reference is no longer required, call SecKeychainRelease to deallocate memory occupied
	by the item.
	
	@param itemRefClass A constant identifying the class of item to be created.
	@param attrList The list of attributes of the item to be created.
	@param length Length of the data to be stored in this item.
	@param data Pointer to a buffer containing the data to be stored in this item.
    @param keychain to add the item to.
	@param itemRef A reference to the newly created keychain item (optional).
    @result noErr 0 No error.
			paramErr -50 Not enough valid parameters were supplied.
			memFullErr -108 Not enough memory in current heap zone to create the object.
*/
OSStatus SecKeychainItemCreateFromContent(SecItemClass itemClass, SecKeychainAttributeList *attrList, UInt32 length, const void *data, SecKeychainRef keychainRef, SecKeychainItemRef *itemRef);

/*!
	@function SecKeychainItemModifyContent
	This routine to update an existing keychain item after changing its attributes or data. The item is
	written to the keychain's permanent data store. If item has not previously been added to a keychain,
	SecKeychainItemModifyContent does nothing and returns noErr.

	@param itemRef A reference of the keychain item to be modified.
	@param attrList The list of attributes to be set in this item.
	@param length Length of the data to be stored in this item.
	@param data Pointer to a buffer containing the data to be stored in this item.
    @result noErr 0 No error.
			errSecNoDefaultKeychain -25307 No default keychain could be found.
			errSecInvalidItemRef -25304 The specified keychain item reference was invalid.
*/
OSStatus SecKeychainItemModifyContent(SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList, UInt32 length, const void *data);

/*!
	@function SecKeychainItemCopyContent
	Use this function to retrieve the data and/or attributes stored in the given keychain item.
		
	You must call SecKeychainItemFreeContent when you no longer need the attributes and data.

	@param itemRef A reference of the keychain item to be modified.
	@param itemClass The items class.  Pass NULL if not required.
	@param attrList The list of attributes to get in this item on input, on output the attributes are filled in.
	@param length on output the actual length of the data.
	@param outData Pointer to a buffer containing the data in this item.  Pass NULL if not required.

    @result noErr 0 No error.
			paramErr -50 Not enough valid parameters were supplied.
			errSecInvalidItemRef -25304 The specified keychain item reference was invalid.
			errSecBufferTooSmall -25301 The data was too large for the supplied buffer.
			errSecDataNotAvailable -25316 The data is not available for this item.	
*/
OSStatus SecKeychainItemCopyContent(SecKeychainItemRef itemRef, SecItemClass *itemClass, SecKeychainAttributeList *attrList, UInt32 *length, void **outData);

/*!
	@function SecKeychainItemFreeContent
*/
OSStatus SecKeychainItemFreeContent(SecKeychainAttributeList *attrList, void *data);

/*!
	@function SecKeychainAttributeInfoForItemID
	This will allow clients to obtain the tags for all possible attrs for that item class. User should call SecKeychainFreeAttributeInfo to
	release the structure when done with it.  
	
	Warning, this call returns more attributes than are support by the old style Keychain API and passing them inro older calls will
	yield an invalid attribute error.  The recommended call to retrieve the attribtute values is SecKeychainItemCopyAttributesAndData.

    @param keychainRef A reference to the keychain.
	@param itemID the relation ID of the item tags
	@param info a pointer to a SecKeychainAttributeInfo structure
	
    @result noErr 0 No error.
			paramErr -50 Not enough valid parameters were supplied.
*/
OSStatus SecKeychainAttributeInfoForItemID(SecKeychainRef keychainRef,  UInt32 itemID, SecKeychainAttributeInfo **info);

/*!
	@function SecKeychainFreeAttributeInfo
	This function free the memory aquired during the SecKeychainAttributeInfoForItemID call.
		
	@param Info a pointer to a SecKeychainAttributeInfo structure
	
    @result noErr 0 No error.
			paramErr -50 Not enough valid parameters were supplied.
*/
OSStatus SecKeychainFreeAttributeInfo(SecKeychainAttributeInfo *info);

/*!
	@function SecKeychainItemModifyContent
	This routine to update an existing keychain item after changing its attributes or data. The item is
	written to the keychain's permanent data store. If item has not previously been added to a keychain,
	SecKeychainItemModifyContent does nothing and returns noErr.

	@param itemRef A reference of the keychain item to be modified.
	@param attrList The list of attributes to be set in this item.
	@param length Length of the data to be stored in this item.
	@param data Pointer to a buffer containing the data to be stored in this item.
    @result noErr 0 No error.
			errSecNoDefaultKeychain -25307 No default keychain could be found.
			errSecInvalidItemRef -25304 The specified keychain item reference was invalid.
*/
OSStatus SecKeychainItemModifyAttributesAndData(SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList, UInt32 length, const void *data);


/*!
	@function SecKeychainItemCopyAttributesAndData
	Use this function to retrieve the data and/or attributes stored in the given keychain item.
		
	You must call SecKeychainItemFreeAttributesAndData when you no longer need the attributes and data.

	@param itemRef A reference of the keychain item to be modified.
	@param info List of tags of attributes to retrieve.
	@param itemClass The items class.  Pass NULL if not required.
	@param attrList The list of attributes to get in this item on input, on output the attributes are filled in.
	@param length on output the actual length of the data.
	@param outData Pointer to a buffer containing the data in this item.  Pass NULL if not required.

    @result noErr 0 No error.
			paramErr -50 Not enough valid parameters were supplied.
			errSecInvalidItemRef -25304 The specified keychain item reference was invalid.
			errSecBufferTooSmall -25301 The data was too large for the supplied buffer.
			errSecDataNotAvailable -25316 The data is not available for this item.	
*/
OSStatus SecKeychainItemCopyAttributesAndData(SecKeychainItemRef itemRef, SecKeychainAttributeInfo *info, SecItemClass *itemClass, SecKeychainAttributeList **attrList, UInt32 *length, void **outData);

/*!
	@function SecKeychainItemFreeAttributesAndData
	Use this function to release the data and/or attributes returned by the SecKeychainItemCopyAttributesAndData function.

	@param info List of tags of attributes to retrieve.

    @result noErr 0 No error.
*/
OSStatus SecKeychainItemFreeAttributesAndData(SecKeychainAttributeList *attrList, void *data);

/*!
	@function SecKeychainItemDelete
	Use this routine to delete a keychain item from the default keychain's permanent data store. If itemRef
	has not previously been added to the keychain, SecKeychainItemDelete does nothing and returns noErr.
	IMPORTANT: SecKeychainItemDelete does not dispose the memory occupied by the item reference itself;
	use SecKeychainItemRelease when you are completely finished with an item.	

	@param itemRef A keychain item reference of the item to be deleted.
    @result noErr 0 No error.
			errSecNoDefaultKeychain -25307 No default keychain could be found.
			errSecInvalidItemRef -25304 The specified keychain item reference was invalid.	
*/
OSStatus SecKeychainItemDelete(SecKeychainItemRef itemRef);

/*!
	@function SecKeychainItemCopyKeychain
	Use this routine to copy an existing keychain reference from a keychain item.	
	
	@param itemRef A keychain item reference of the item to be updated.
	@param keychainRef A pointer to a keychain reference returned.  Release this by calling
           SecKeychainRelease().
    @result noErr 0 No error.
			errSecInvalidItemRef -25304 The specified keychain item reference was invalid.	
*/
OSStatus SecKeychainItemCopyKeychain(SecKeychainItemRef itemRef, SecKeychainRef* keychainRef);


/*!
	@function SecKeychainItemCreateCopy
	Use this routine to copy a keychain item. The copy will be returned in itemCopy.
	
	@param itemRef A keychain item reference to copy.
	@param itemCopy The new copied item.	
    @result noErr 0 No error.
			errSecInvalidKeychain -25295 The specified destKeychain was invalid.
			errSecReadOnly -25292 The destKeychain is read only.
			errSecNoSuchClass -25306 item has an invalid keychain item class.	
*/
OSStatus SecKeychainItemCreateCopy(SecKeychainItemRef itemRef, SecKeychainItemRef *itemCopy, SecKeychainRef destKeychainRef);

/*!
	@function SecKeychainItemRelease
	Releases  keychain item references
	 
	@param itemRef A keychain item reference to release.
    @result noErr 0 No error.
*/
OSStatus SecKeychainItemRelease(SecKeychainItemRef itemRef);

/*!
	@function SecKeychainSearchCreateFromAttributes
	Creates a search reference matching a list of zero or more specified attributes in the specified keychain
	and returns a reference to the item. Pass NULL for keychain if you wish to search all unlocked
	keychains. The caller is responsible for calling SecKeychainSearchRelease to release this reference
	when finished with it. A reference to the current search criteria is also returned, for subsequent calls to
	SecKeychainCopySearchNextItem. This reference must be released by the caller when completely finished with a
	search by calling SecKeychainSearchRelease.
	
	@param keychainRef The keychain to search (NULL means search all unlocked keychains)
	@param attrList A list of zero or more SecKeychainAttribute records to be matched
					(NULL matches any keychain item).
	@param searchRef A reference to the current search is returned here.
	
    @result noErr 0 No error.
			errSecNoDefaultKeychain -25307 No default keychain could be found.
			errSecItemNotFound -25300 No matching keychain item was found.
			errSecNoSuchAttr -25303 Specified an attribute which is undefined for this item class.	
*/
OSStatus SecKeychainSearchCreateFromAttributes(SecKeychainRef keychainRef, SecItemClass itemClass, const SecKeychainAttributeList *attrList, SecKeychainSearchRef *searchRef);

/*!
	@function SecKeychainCopySearchNextItem
	Finds the next keychain item matching the given search criteria, as previously specified by a call to
	SecKeychainSearchCreateFromAttributes, and returns a reference to the item. The caller is responsible for releasing
	this reference when finished with it.
	
	@param searchRef A reference to the current search criteria.
	@param itemRef A reference to the next matching keychain item, if any, is returned here.	
    @result noErr 0 No error.
			errSecNoDefaultKeychain -25307 No default keychain could be found.
			errSecInvalidSearchRef -25305 The specified search reference was invalid.
			errSecItemNotFound -25300 No more matching keychain items were found.	
*/
OSStatus SecKeychainCopySearchNextItem(SecKeychainSearchRef searchRef, SecKeychainItemRef *itemRef);

/*!
	@function SecKeychainSearchRelease
	Releases a keychain search reference.
		
	@param searchRef A reference to the search reference.
	@result noErr 0 No error.
*/
OSStatus SecKeychainSearchRelease(SecKeychainSearchRef searchRef);

 
/*!
	@function SecKeychainListRemoveKeychain
	Removed the specified keychain from the list of availible keychains.
	
	@param keychainRef A reference to the keychain to be removed.
    @result noErr 0 No error.
*/
OSStatus SecKeychainListRemoveKeychain(SecKeychainRef *keychainRef);
 
// Keychain Callback mgr stuff
typedef OSStatus (*SecKeychainCallbackProcPtr)(SecKeychainEvent keychainEvent, SecKeychainCallbackInfo* info, void *context);


/*!
	@function SecKeychainAddCallback
	Add a callback.
	
	@param callbackFunction The callback function pointer to add
	@param eventMask
	@param userContext
    @result noErr 0 No error.
*/
OSStatus SecKeychainAddCallback(SecKeychainCallbackProcPtr callbackFunction, SecKeychainEventMask eventMask, void* userContext);


/*!
	@function SecKeychainRemoveCallback
	Remove a callback.
	
	@param callbackFunction The callback function pointer to remove 
	@result noErr 0 No error.
*/
OSStatus SecKeychainRemoveCallback(SecKeychainCallbackProcPtr callbackFunction);


/*!
	@function SecKeychainAddInternetPassword
	Add an internet password to the specified keychain.
	
	@param keychainRef
	@param serverNameLength
	@param serverName
	@param securityDomainLength
	@param securityDomain
	@param accountNameLength
	@param accountName
	@param pathLength
	@param path
	@param port
	@param protocol
	@param authType
	@param passwordLength
	@param passwordData
	@param itemRef
	
	@result noErr 0 No error.
*/
OSStatus SecKeychainAddInternetPassword(SecKeychainRef keychainRef, UInt32 serverNameLength, char *serverName, 
										UInt32 securityDomainLength, char *securityDomain, UInt32 accountNameLength, char *accountName, 
										UInt32 pathLength, char *path, UInt16 port, OSType protocol, OSType authType,
										UInt32 passwordLength, const void *passwordData, SecKeychainItemRef *itemRef);


/*!
	@function SecKeychainFindInternetPassword
	Find an internet password
		
	@param keychainRef
	@param serverNameLength
	@param serverName
	@param securityDomainLength
	@param securityDomain
	@param accountNameLength
	@param accountName
	@param pathLength
	@param path
	@param port
	@param protocol
	@param authType
	@param passwordLength
	@param passwordData
	@param itemRef

	@result noErr 0 No error.
*/
OSStatus SecKeychainFindInternetPassword(SecKeychainRef keychainRef, UInt32 serverNameLength, char *serverName, 
										UInt32 securityDomainLength, char *securityDomain, UInt32 accountNameLength, char *accountName,
										UInt32 pathLength, char *path, UInt16 port, OSType protocol, OSType authType,
										UInt32 *passwordLength, void **passwordData, SecKeychainItemRef *itemRef);


/*!
	@function SecKeychainAddGenericPassword
	Add an generic password to the specified keychain.
	
	@param  keychainRef
	@param serviceNameLength
	@param serviceName
	@param accountNameLength
	@param accountName
	@param passwordData
	@param passwordLength
	@param itemRef

	@result noErr 0 No error.
*/
OSStatus SecKeychainAddGenericPassword(SecKeychainRef keychainRef, UInt32 serviceNameLength, char *serviceName,
									   UInt32 accountNameLength, char *accountName, 
									   UInt32 passwordLength, const void *passwordData, SecKeychainItemRef *itemRef);


/*!
	@function SecKeychainFindGenericPassword
	Find a generic password

	@param keychainRef
	@param serverNameLength
	@param serverName
	@param accountNameLength
	@param accountName
	@param passwordLength
	@param passwordData
	@param itemRef

	@result noErr 0 No error.
*/
OSStatus SecKeychainFindGenericPassword(SecKeychainRef keychainRef,  UInt32 serviceNameLength, char *serviceName,
										UInt32 accountNameLength, char *accountName,
										UInt32 *passwordLength, void **passwordData, SecKeychainItemRef *itemRef);



/*!
	@function SecKeychainSetUserInteractionAllowed
	Turn on/off any optional user interface
	
	@param state true = allow user interface, false = disallow user interface

	@result noErr 0 No error.
*/
OSStatus SecKeychainSetUserInteractionAllowed(Boolean state);

/*!
	@function SecKeychainGetUserInteractionAllowed
	Get the current setting for SecKeychainSetUserInteractionAllowed
		
	@param *state true = allow user interface, false = disallow user interface

	@result noErr 0 No error.
*/
OSStatus SecKeychainGetUserInteractionAllowed(Boolean *state);

#if defined(__cplusplus)
}
#endif

#endif /* ! __SECKEYCHAINAPI__ */


