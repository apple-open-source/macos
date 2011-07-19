/*
 * Copyright (c) 2006-2010 Apple Inc. All rights reserved.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <TargetConditionals.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include "symbol_scope.h"
#include "EAPSecurity.h"
#include "EAPKeychainUtil.h"
#include "EAPKeychainUtilInternal.h"
#include "myCFUtil.h"

#if TARGET_OS_EMBEDDED

OSStatus
EAPSecKeychainPasswordItemRemove(SecKeychainRef keychain,
				 CFStringRef unique_id_str)
{
    const void *	keys[] = {
	kSecClass,
	kSecAttrService
    };
    CFDictionaryRef	query;
    OSStatus		status;

    const void *	values[] = {
	kSecClassGenericPassword,
	unique_id_str
    };

    query = CFDictionaryCreate(NULL, keys, values,
			       sizeof(keys) / sizeof(*keys),
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks);
    status = SecItemDelete(query);
    CFRelease(query);
    if (status != noErr) {
	fprintf(stderr, "SecItemDelete failed: %s (%d)\n", 
		EAPSecurityErrorString(status), (int)status);
    }
    return (status);
}

OSStatus
EAPSecKeychainPasswordItemCopy(SecKeychainRef keychain, 
			       CFStringRef unique_id_str, 
			       CFDataRef * ret_password)
{
    const void *	keys[] = {
	kSecClass,
	kSecAttrService,
	kSecReturnData
    };
    CFDictionaryRef	query;
    CFTypeRef		results;
    OSStatus		status;
    const void *	values[] = {
	kSecClassGenericPassword,
	unique_id_str,
	kCFBooleanTrue
    };

    query = CFDictionaryCreate(NULL, keys, values,
			       sizeof(keys) / sizeof(*keys),
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks);
    status = SecItemCopyMatching(query, &results);
    CFRelease(query);
    if (status == noErr) {
	*ret_password = results;
    }
    else {
	*ret_password = NULL;
    }
    return (status);
}

OSStatus
EAPSecKeychainPasswordItemCreateWithAccess(SecKeychainRef keychain,
					   SecAccessRef access,
					   CFStringRef unique_id_str,
					   CFDataRef label,
					   CFDataRef description,
					   CFDataRef user,
					   CFDataRef password)
{
    CFMutableDictionaryRef	attrs;
    OSStatus			status;

    attrs = CFDictionaryCreateMutable(NULL, 0,
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attrs, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(attrs, kSecAttrService, unique_id_str);
    if (label != NULL) {
	CFDictionarySetValue(attrs, kSecAttrLabel, label);
    }
    if (description != NULL) {
	CFDictionarySetValue(attrs, kSecAttrDescription, description);
    }
    if (user != NULL) {
	CFDictionarySetValue(attrs, kSecAttrAccount, user);
    }
    CFDictionarySetValue(attrs, kSecValueData, password);
    status = SecItemAdd(attrs, NULL);
    CFRelease(attrs);
    return (status);
}

OSStatus
EAPSecKeychainPasswordItemSet(SecKeychainRef keychain,
			      CFStringRef unique_id_str,
			      CFDataRef password)
{
    const void *	keys[] = {
	kSecClass,
	kSecAttrService,
	kSecReturnData
    };
    CFTypeRef		existing_password = NULL;
    CFDictionaryRef	query;
    CFDictionaryRef	pass_dict;
    OSStatus		status;
    const void *	values[] = {
	kSecClassGenericPassword,
	unique_id_str,
	kCFBooleanTrue
    };

    query = CFDictionaryCreate(NULL, keys, values,
			       sizeof(keys) / sizeof(*keys),
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks);
    status = SecItemCopyMatching(query, &existing_password);
    CFRelease(query);
    if (status != noErr) {
	goto done;
    }
    if (existing_password != NULL
	&& CFEqual(password, existing_password)) {
	/* nothing to do */
	goto done;
    }
    query = CFDictionaryCreate(NULL, keys, values,
			       (sizeof(keys) / sizeof(*keys)) - 1,
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks);
    pass_dict = CFDictionaryCreate(NULL,
				   (const void * *)&kSecValueData,
				   (const void * *)&password,
				   1,
				   &kCFTypeDictionaryKeyCallBacks,
				   &kCFTypeDictionaryValueCallBacks);
    status = SecItemUpdate(query, pass_dict);
    CFRelease(query);
    CFRelease(pass_dict);

 done:
    my_CFRelease(&existing_password);
    return (status);
}

#else /* TARGET_OS_EMBEDDED */
#include <Security/SecAccess.h>
#include <Security/SecACL.h>
#include <Security/SecTrustedApplication.h>
#include <Security/SecTrustedApplicationPriv.h>

const CFStringRef kEAPSecKeychainPropPassword = CFSTR("Password");
const CFStringRef kEAPSecKeychainPropLabel = CFSTR("Label");
const CFStringRef kEAPSecKeychainPropDescription = CFSTR("Description");
const CFStringRef kEAPSecKeychainPropAccount = CFSTR("Account");
const CFStringRef kEAPSecKeychainPropTrustedApplications = CFSTR("TrustedApplications");
const CFStringRef kEAPSecKeychainPropAllowRootAccess = CFSTR("AllowRootAccess");

#include <Security/cssmtype.h>
#include <Security/cssmapple.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>

#define MY_KEYCHAIN_ATTR_MAX		6

typedef struct {
    SecKeychainAttributeInfo	info;
    UInt32			tag[MY_KEYCHAIN_ATTR_MAX];
    UInt32			format[MY_KEYCHAIN_ATTR_MAX];
} mySecKeychainAttributeInfo;

typedef struct {
    SecKeychainAttributeList	list;
    SecKeychainAttribute 	attr[MY_KEYCHAIN_ATTR_MAX];
} mySecKeychainAttributeList;

typedef struct CFStringSecItemTag {
    const CFStringRef *	key;
    UInt32		tag;
} CFStringSecItemTag;

STATIC const CFStringSecItemTag prop_tag_tbl[] = {
    { &kEAPSecKeychainPropAccount, kSecAccountItemAttr },
    { &kEAPSecKeychainPropLabel, kSecLabelItemAttr },
    { &kEAPSecKeychainPropDescription, kSecDescriptionItemAttr },
};
STATIC const int		prop_tag_tbl_size = (sizeof(prop_tag_tbl)
						     / sizeof(prop_tag_tbl[0]));

STATIC void
mySecKeychainAttributeInfoInit(mySecKeychainAttributeInfo * attr_info)
{
    attr_info->info.count = 0;
    attr_info->info.tag = attr_info->tag;
    attr_info->info.format = attr_info->format;
    return;
}

STATIC Boolean
mySecKeychainAttributeInfoAdd(mySecKeychainAttributeInfo * attr_info,
			      UInt32 tag)
{
    int		count = attr_info->info.count;

    if (count >= MY_KEYCHAIN_ATTR_MAX) {
	fprintf(stderr, "Trying to add attribute %d but list is full\n",
		(int)tag);
	return (FALSE);
    }
    attr_info->format[count] = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
    attr_info->tag[count] = tag;
    attr_info->info.count++;
    return (TRUE);
}

STATIC void
mySecKeychainAttributeListInit(mySecKeychainAttributeList * attr_list)
{
    attr_list->list.count = 0;
    attr_list->list.attr = attr_list->attr;
    return;
}

STATIC Boolean
mySecKeychainAttributeListAdd(mySecKeychainAttributeList * attr_list,
			      UInt32 attr_tag, CFDataRef attr_data)
{
    SecKeychainAttribute * 	attr;
    int				count = attr_list->list.count;

    if (count >= MY_KEYCHAIN_ATTR_MAX) {
	fprintf(stderr, "Trying to add attribute %d but list is full\n",
		(int)attr_tag);
	return (FALSE);
    }
    attr = attr_list->attr + count;
    attr->tag = attr_tag;
    attr->length = CFDataGetLength(attr_data);
    attr->data = (void *)CFDataGetBytePtr(attr_data);
    attr_list->list.count++;
    return (TRUE);
}

STATIC Boolean
mySecKeychainAttributeListAddFromDict(mySecKeychainAttributeList * attr_list,
				      CFDictionaryRef attrs)
{
    int				i;
    Boolean			ret = TRUE;
    const CFStringSecItemTag *	tbl;

    for (i = 0, tbl = prop_tag_tbl; i < prop_tag_tbl_size; i++, tbl++) {
	CFDataRef	val = CFDictionaryGetValue(attrs, *tbl->key);

	if (val != NULL) {
	    ret = mySecKeychainAttributeListAdd(attr_list, tbl->tag, val);
	    if (ret == FALSE) {
		fprintf(stderr, "mySecKeychainAttributeListAddFromDict() "
			"failed to add %d\n", (int)tbl->tag);
		break;
	    }
	}
    }
    return (ret);
}

PRIVATE_EXTERN OSStatus
EAPSecAccessCreateWithUid(uid_t uid, SecAccessRef * ret_access)
{
    /* make the "uid/gid" ACL subject, this is a CSSM_LIST_ELEMENT chain */
    CSSM_ACL_PROCESS_SUBJECT_SELECTOR	selector = {
	CSSM_ACL_PROCESS_SELECTOR_CURRENT_VERSION,
	CSSM_ACL_MATCH_UID,	/* active fields mask: match uids (only) */
	uid,			/* effective user id to match */
	0			/* effective group id to match */
    };
    CSSM_LIST_ELEMENT 		subject2 = {
	NULL,			/* NextElement */
	0,			/* WordID */
	CSSM_LIST_ELEMENT_DATUM	/* ElementType */
    };
    CSSM_LIST_ELEMENT 		subject1 = {
	&subject2,		/* NextElement */
	CSSM_ACL_SUBJECT_TYPE_PROCESS, /* WordID */
	CSSM_LIST_ELEMENT_WORDID /* ElementType */
    };
    /* rights granted (replace with individual list if desired) */
    CSSM_ACL_AUTHORIZATION_TAG	rights[] = {
	CSSM_ACL_AUTHORIZATION_ANY
    };
    /* owner component (right to change ACL) */
    CSSM_ACL_OWNER_PROTOTYPE	owner = {
	{ // TypedSubject
	    CSSM_LIST_TYPE_UNKNOWN,	/* type of this list */
	    &subject1,			/* head of the list */
	    &subject2			/* tail of the list */
	},
	FALSE				/* Delegate */
    };
    /* ACL entry */
    CSSM_ACL_ENTRY_INFO		acls[] = {
	{
	    { /* EntryPublicInfo */
		{ /* TypedSubject */
		    CSSM_LIST_TYPE_UNKNOWN, /* type of this list */
		    &subject1,		/* head of the list */
		    &subject2		/* tail of the list */
		},
		FALSE,			/* Delegate */
		{			/* Authorization */
		    sizeof(rights) / sizeof(rights[0]), /* NumberOfAuthTags */
		    rights		/* AuthTags */
		},
		{			/* TimeRange */
		},
		{			/* EntryTag */
		}
	    },
	    0				/* EntryHandle */
	}
    };

    subject2.Element.Word.Data = (UInt8 *)&selector;
    subject2.Element.Word.Length = sizeof(selector);
    return (SecAccessCreateFromOwnerAndACL(&owner,
					   sizeof(acls) / sizeof(acls[0]),
					   acls,
					   ret_access));
}

PRIVATE_EXTERN OSStatus
EAPSecAccessCreateWithTrustedApplications(CFArrayRef trusted_apps,
					  CFDataRef label_data,
					  SecAccessRef * ret_access)
{
    CFStringRef		label = NULL;
    OSStatus		status;
        
    if (label_data != NULL) {
	label = my_CFStringCreateWithData(label_data);
    }
    else {
	label = CFSTR("--unspecified--");
	CFRetain(label);
    }
    status = SecAccessCreate(label, trusted_apps, ret_access);
    my_CFRelease(&label);
    return (status);
}

PRIVATE_EXTERN OSStatus
EAPSecKeychainItemSetAccessForTrustedApplications(SecKeychainItemRef item,
						  CFArrayRef trusted_apps)
{
    CFArrayRef 		app_list = NULL;
    CFMutableArrayRef	app_list_data = NULL;
    SecACLRef		acl;
    CFArrayRef		acl_list = NULL;
    SecAccessRef	access = NULL;
    CFStringRef 	prompt_description = NULL;
    CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR prompt_selector;
    OSStatus		status;

    status = SecKeychainItemCopyAccess(item, &access);
    if (status != noErr) {
	status = SecAccessCreate(CFSTR("--unspecified--"),
				 trusted_apps, &access);
	if (status != noErr) {
	    goto done;
	}
    }
    else {
	status = SecAccessCopySelectedACLList(access,
					      CSSM_ACL_AUTHORIZATION_DECRYPT,
					      &acl_list);
	if (status != noErr) {
	    goto done;
	}
	acl = (SecACLRef)CFArrayGetValueAtIndex(acl_list, 0);
	status = SecACLCopySimpleContents(acl, &app_list, &prompt_description,
					  &prompt_selector);
	if (status == noErr && app_list != NULL) {
	    int			count;
	    int			i;
	    CFRange		r;
	    CFMutableArrayRef	new_list = NULL;
	    
	    r.location = 0;
	    r.length = CFArrayGetCount(app_list);
	    app_list_data = CFArrayCreateMutable(NULL, r.length,
						 &kCFTypeArrayCallBacks);
	    for (i = 0; i < r.length; i++) {
		SecTrustedApplicationRef	app;
		CFDataRef			data = NULL;

		app = (SecTrustedApplicationRef)
		    CFArrayGetValueAtIndex(app_list, i);
		(void)SecTrustedApplicationCopyExternalRepresentation(app,
								      &data);
		if (data != NULL) {
		    CFArrayAppendValue(app_list_data, data);
		    CFRelease(data);
		}
	    }
	    /* in case it changed */
	    r.length = CFArrayGetCount(app_list_data);
	    count = CFArrayGetCount(trusted_apps);
	    for (i = 0; i < count; i++) {
		bool				already_there;
		SecTrustedApplicationRef	app;
		CFDataRef			data = NULL;

		app = (SecTrustedApplicationRef)
		    CFArrayGetValueAtIndex(trusted_apps, i);

		(void)SecTrustedApplicationCopyExternalRepresentation(app,
								      &data);
		if (data == NULL) {
		    continue;
		}
		already_there = CFArrayContainsValue(app_list_data, r, data);
		CFRelease(data);
		if (already_there) {
		    continue;
		}
		if (new_list == NULL) {
		    new_list = CFArrayCreateMutableCopy(NULL, 0, app_list);
		}
		CFArrayAppendValue(new_list, app);
	    }
	    if (new_list == NULL) {
		/* no modifications required */
		goto done;
	    }
	    status = SecACLSetSimpleContents(acl, new_list, prompt_description,
					     &prompt_selector);
	    CFRelease(new_list);
	    if (status != noErr) {
		goto done;
	    }
	}
	else {
	    status = SecACLSetSimpleContents(acl, trusted_apps,
					     prompt_description,
					     &prompt_selector);
	    if (status != noErr) {
		goto done;
	    }
	}
    }
    status = SecKeychainItemSetAccess(item, access);

 done:
    my_CFRelease(&access);
    my_CFRelease(&app_list);
    my_CFRelease(&app_list_data);
    my_CFRelease(&prompt_description);
    my_CFRelease(&acl_list);
    return (status);
}

STATIC OSStatus
KeychainPasswordItemCopy(SecKeychainRef keychain,
			 CFStringRef unique_id_str,
			 SecKeychainItemRef * ret_item)
{
    SecKeychainItemRef	item;
    OSStatus		status;
    CFDataRef		unique_id;

    unique_id = CFStringCreateExternalRepresentation(NULL,
						     unique_id_str,
						     kCFStringEncodingUTF8,
						     0);
    status 
	= SecKeychainFindGenericPassword(keychain,
					 CFDataGetLength(unique_id),
					 (const char *)CFDataGetBytePtr(unique_id),
					 0,
					 NULL,
					 NULL,
					 NULL,
					 &item);
    CFRelease(unique_id);
    *ret_item = NULL;
    if (status != noErr) {
#if TEST_EAPKEYCHAINUTIL
	fprintf(stderr, "SecKeychainFindGenericPassword failed: %s (%d)\n", 
		EAPSecurityErrorString(status), (int)status);
#endif /* TEST_EAPKEYCHAINUTIL */
    }
    else {
	*ret_item = item;
    }
    return (status);
}

STATIC CFStringRef
SecKeychainAttrTypeGetCFString(SecKeychainAttrType type)
{
    int				i;
    const CFStringSecItemTag *	tbl;

    for (i = 0, tbl = prop_tag_tbl; i < prop_tag_tbl_size; i++, tbl++) {
	if (tbl->tag == type) {
	    return (*tbl->key);
	}
    }
    return (NULL);
}

STATIC OSStatus
KeychainItemCopyInfo(SecKeychainItemRef item,
		     CFArrayRef keys,
		     CFDictionaryRef * ret_attrs)
{
    CFMutableDictionaryRef 	attrs = NULL;
    int				i;
    void *			password_data = NULL;
    UInt32 			password_length = 0;
    void * *			password_data_p;
    UInt32 *			password_length_p;
    CFRange			range = CFRangeMake(0, CFArrayGetCount(keys));
    mySecKeychainAttributeInfo 	req_attr_info;
    SecKeychainAttributeInfo * 	req_attr_info_p;
    SecKeychainAttributeList * 	ret_attr_list = NULL;
    SecKeychainAttributeList * *ret_attr_list_p;
    OSStatus			status;

    mySecKeychainAttributeInfoInit(&req_attr_info);
    if (CFArrayContainsValue(keys, range, kEAPSecKeychainPropAccount)) {
	mySecKeychainAttributeInfoAdd(&req_attr_info, 
				      kSecAccountItemAttr);
    }
    if (CFArrayContainsValue(keys, range, kEAPSecKeychainPropLabel)) {
	mySecKeychainAttributeInfoAdd(&req_attr_info, 
				      kSecLabelItemAttr);
    }
    if (CFArrayContainsValue(keys, range, kEAPSecKeychainPropDescription)) {
	mySecKeychainAttributeInfoAdd(&req_attr_info, 
				      kSecDescriptionItemAttr);
    }
    if (CFArrayContainsValue(keys, range, kEAPSecKeychainPropPassword)) {
	password_data_p = &password_data;
	password_length_p = &password_length;
    }
    else {
	password_data_p = NULL;
	password_length_p = NULL;
    }
    if (req_attr_info.info.count != 0) {
	req_attr_info_p = &req_attr_info.info;
	ret_attr_list_p = &ret_attr_list;
    }
    else {
	req_attr_info_p = NULL;
	ret_attr_list_p = NULL;
    }
    status = SecKeychainItemCopyAttributesAndData(item, 
						  req_attr_info_p,
						  NULL,
						  ret_attr_list_p,
						  password_length_p,
						  password_data_p);
    if (status != noErr) {
	goto done;
    }
    attrs = CFDictionaryCreateMutable(NULL, 0,
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);
    if (password_data != NULL && password_length != 0) {
	CFDataRef	password;

	password = CFDataCreate(NULL, password_data, password_length);
	CFDictionarySetValue(attrs, kEAPSecKeychainPropPassword, password);
	CFRelease(password);
    }
    if (ret_attr_list != NULL) {
	SecKeychainAttribute *	attr = ret_attr_list->attr;

	for (i = 0; i < ret_attr_list->count; i++, attr++) {
	    CFStringRef		key;
	    CFDataRef		val;

	    if (attr->data == NULL || attr->length == 0) {
		continue;
	    }
	    key = SecKeychainAttrTypeGetCFString(attr->tag);
	    if (key == NULL) {
		/* shouldn't happen */
		continue;
	    }
	    val = CFDataCreate(NULL, attr->data, attr->length);
	    CFDictionarySetValue(attrs, key, val);
	    CFRelease(val);
	}
    }
    SecKeychainItemFreeAttributesAndData(ret_attr_list, password_data);

 done:
    *ret_attrs = attrs;
    return (status);

}

OSStatus
EAPSecKeychainPasswordItemRemove(SecKeychainRef keychain,
				 CFStringRef unique_id_str)
{
    SecKeychainItemRef	item;
    OSStatus		status;

    status = KeychainPasswordItemCopy(keychain, unique_id_str, &item);
    if (status != noErr) {
	goto done;
    }
    status = SecKeychainItemDelete(item);
    CFRelease(item);
    if (status != noErr) {
	fprintf(stderr, "SecKeychainItemDelete() failed: %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);

    }
 done:
    return (status);
}

OSStatus
EAPSecKeychainPasswordItemCopy2(SecKeychainRef keychain, 
				CFStringRef unique_id_str,
				CFArrayRef keys,
				CFDictionaryRef * ret_values)
{
    SecKeychainItemRef		item = NULL;
    OSStatus			status;

    *ret_values = NULL;
    status = KeychainPasswordItemCopy(keychain, unique_id_str, &item);
    if (status == noErr) {
	status = KeychainItemCopyInfo(item, keys, ret_values);
    }
    if (item != NULL) {
	CFRelease(item);
    }
    return (status);
}

OSStatus
EAPSecKeychainPasswordItemCopy(SecKeychainRef keychain, 
			       CFStringRef unique_id_str, 
			       CFDataRef * ret_password)
{
    CFDataRef			password;
    CFDictionaryRef		props;
    CFArrayRef			req_props;
    OSStatus			status;

    *ret_password = NULL;
    req_props = CFArrayCreate(NULL,
			      (const void * *)&kEAPSecKeychainPropPassword, 1,
			      &kCFTypeArrayCallBacks);
    status = EAPSecKeychainPasswordItemCopy2(keychain, 
					     unique_id_str,
					     req_props,
					     &props);
    CFRelease(req_props);
    if (status != noErr) {
	return (status);
    }
    password = CFDictionaryGetValue(props, kEAPSecKeychainPropPassword);
    if (password == NULL) {
	status = errSecItemNotFound;
    }
    else {
	*ret_password = CFRetain(password);
    }
    CFRelease(props);
    return (status);
}

OSStatus
EAPSecKeychainPasswordItemCreateWithAccess(SecKeychainRef keychain,
					   SecAccessRef access,
					   CFStringRef unique_id_str,
					   CFDataRef label,
					   CFDataRef description,
					   CFDataRef user,
					   CFDataRef password)
{
    mySecKeychainAttributeList	attr_list;
    OSStatus			status;
    CFDataRef			unique_id;

    unique_id = CFStringCreateExternalRepresentation(NULL,
						     unique_id_str,
						     kCFStringEncodingUTF8,
						     0);
    mySecKeychainAttributeListInit(&attr_list);
    mySecKeychainAttributeListAdd(&attr_list, kSecServiceItemAttr, unique_id);
    if (label != NULL) {
	mySecKeychainAttributeListAdd(&attr_list, kSecLabelItemAttr, label);
    }
    if (description != NULL) {
	mySecKeychainAttributeListAdd(&attr_list, kSecDescriptionItemAttr,
				      description);
    }
    if (user != NULL) {
	mySecKeychainAttributeListAdd(&attr_list, kSecAccountItemAttr, user);
    }
    status = SecKeychainItemCreateFromContent(kSecGenericPasswordItemClass,
					      &attr_list.list,
					      CFDataGetLength(password),
					      CFDataGetBytePtr(password),
					      keychain,
					      access,
					      NULL);
    CFRelease(unique_id);
    return (status);
}

OSStatus
EAPSecKeychainPasswordItemCreate(SecKeychainRef keychain,
				 CFStringRef unique_id_str,
				 CFDictionaryRef attrs)
{
    CFBooleanRef		allow_root;
    SecAccessRef		access = NULL;
    mySecKeychainAttributeList	attr_list;
    CFDataRef 			password;
    void *			password_data;
    UInt32			password_length;
    OSStatus			status;
    CFArrayRef			trusted_apps;
    CFDataRef			unique_id = NULL;

    password = CFDictionaryGetValue(attrs, kEAPSecKeychainPropPassword);
    if (password != NULL) {
	/* set it */
	password_length = CFDataGetLength(password);
	password_data = (void *)CFDataGetBytePtr(password);
    }
    else {
	/* don't set it */
	password_length = 0;
	password_data = NULL;
    }
    trusted_apps = CFDictionaryGetValue(attrs,
					kEAPSecKeychainPropTrustedApplications);
    allow_root = CFDictionaryGetValue(attrs,
				      kEAPSecKeychainPropAllowRootAccess);
    if (trusted_apps != NULL) {
	CFDataRef	label_data;

	label_data = CFDictionaryGetValue(attrs,
					  kEAPSecKeychainPropLabel);
	status = EAPSecAccessCreateWithTrustedApplications(trusted_apps,
							   label_data,
							   &access);
	if (status != noErr) {
	    fprintf(stderr, "EAPSecKeychainPasswordItemCreate "
		    "failed to get SecAccess for Trusted Apps, %d\n",
		    (int)status);
	    goto done;
	}
    }
    else if (allow_root != NULL && CFBooleanGetValue(allow_root)) {
	status = EAPSecAccessCreateWithUid(0, &access);
	if (status != noErr) {
	    fprintf(stderr, "EAPSecKeychainPasswordItemCreate "
		    "failed to get SecAccess for UID 0, %d\n",
		    (int)status);
	    goto done;
	}
    }
    mySecKeychainAttributeListInit(&attr_list);
    unique_id = CFStringCreateExternalRepresentation(NULL,
						     unique_id_str,
						     kCFStringEncodingUTF8,
						     0);
    mySecKeychainAttributeListAdd(&attr_list, kSecServiceItemAttr, unique_id);
    if (mySecKeychainAttributeListAddFromDict(&attr_list, attrs) == FALSE) {
	status = errSecBufferTooSmall;
    }
    else {
	status = SecKeychainItemCreateFromContent(kSecGenericPasswordItemClass,
						  &attr_list.list,
						  password_length,
						  password_data,
						  keychain,
						  access,
						  NULL);
    }

 done:
    my_CFRelease(&unique_id);
    return (status);
}

OSStatus
EAPSecKeychainPasswordItemCreateUnique(SecKeychainRef keychain,
				       CFDictionaryRef attrs,
				       CFStringRef * ret_unique_id)
{
    OSStatus		status;
    CFStringRef		unique_id_str;
    
    unique_id_str = my_CFUUIDStringCreate(NULL);
    status = EAPSecKeychainPasswordItemCreate(keychain,
					      unique_id_str,
					      attrs);
    if (status == noErr	&& ret_unique_id != NULL) {
	*ret_unique_id = unique_id_str;
    }
    else {
	if (ret_unique_id != NULL) {
	    *ret_unique_id = NULL;
	}
	CFRelease(unique_id_str);
    }
    return (status);
}

OSStatus
EAPSecKeychainPasswordItemSet(SecKeychainRef keychain,
			      CFStringRef unique_id_str,
			      CFDataRef password)
{
    SecKeychainItemRef		item;
    OSStatus			status;

    status = KeychainPasswordItemCopy(keychain, unique_id_str, &item);
    if (status != noErr) {
	return (status);
    }
    status = SecKeychainItemModifyAttributesAndData(item,
						    NULL, 
						    CFDataGetLength(password),
						    CFDataGetBytePtr(password));
    CFRelease(item);
    return (status);
}

OSStatus
EAPSecKeychainPasswordItemSet2(SecKeychainRef keychain,
			       CFStringRef unique_id_str,
			       CFDictionaryRef attrs)
{
    mySecKeychainAttributeList	attr_list;
    SecKeychainItemRef		item;
    CFDataRef 			password;
    void *			password_data;
    UInt32			password_length;
    OSStatus			status;

    status = KeychainPasswordItemCopy(keychain, unique_id_str, &item);
    if (status != noErr) {
	return (status);
    }
    mySecKeychainAttributeListInit(&attr_list);
    if (mySecKeychainAttributeListAddFromDict(&attr_list, attrs) == FALSE) {
	status = errSecBufferTooSmall;
	goto done;
    }
    password = CFDictionaryGetValue(attrs, kEAPSecKeychainPropPassword);
    if (password != NULL) {
	/* set it */
	password_length = CFDataGetLength(password);
	password_data = (void *)CFDataGetBytePtr(password);
    }
    else {
	/* don't set it */
	password_length = 0;
	password_data = NULL;
    }
    status = SecKeychainItemModifyAttributesAndData(item,
						    &attr_list.list,
						    password_length,
						    password_data);
 done:
    CFRelease(item);
    return (status);
}

#endif /* TARGET_OS_EMBEDDED */

OSStatus
EAPSecKeychainPasswordItemCreateUniqueWithAccess(SecKeychainRef keychain,
						 SecAccessRef access,
						 CFDataRef label,
						 CFDataRef description,
						 CFDataRef user,
						 CFDataRef password,
						 CFStringRef * ret_unique_id)
{
    OSStatus		status;
    CFStringRef		unique_id_str;

    unique_id_str = my_CFUUIDStringCreate(NULL);
    status = EAPSecKeychainPasswordItemCreateWithAccess(keychain,
							access,
							unique_id_str,
							label,
							description,
							user,
							password);
    if (status == noErr	&& ret_unique_id != NULL) {
	*ret_unique_id = unique_id_str;
    }
    else {
	if (ret_unique_id != NULL) {
	    *ret_unique_id = NULL;
	}
	CFRelease(unique_id_str);
    }
    return (status);
}

#ifdef TEST_EAPKEYCHAINUTIL

#if ! TARGET_OS_EMBEDDED
/*
 * Create a SecAccessRef with a custom form.
 * Both the owner and the ACL set allow free access to root,
 * but nothing to anyone else.
 * NOTE: This is not the easiest way to build up CSSM data structures.
 * But it's a way that does not depend on any outside software layers
 * (other than CSSM and Security's Sec* layer, of course).
 */
static OSStatus
SecKeychainCopySystemKeychain(SecKeychainRef * ret_keychain)
{
    SecKeychainRef		keychain = NULL;
    OSStatus			status;

    status = SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem,
					  &keychain);
    if (status != noErr && keychain != NULL) {
	CFRelease(keychain);
	keychain = NULL;
    }
    *ret_keychain = keychain;
    return (status);
}

#endif /* ! TARGET_OS_EMBEDDED */

#if TARGET_OS_EMBEDDED
#define SYS_OPT			" "
#else /* TARGET_OS_EMBEDDED */
#define SYS_OPT			" [system] "
#define HAS_KEYCHAINS
#endif /* TARGET_OS_EMBEDDED */

static void 
usage(const char * progname)
{
    fprintf(stderr, "%s" SYS_OPT 
	    "create <label> <description> <username> <password>\n",
	    progname);
    fprintf(stderr, 
	    "%s" SYS_OPT "set <unique_id> <username> <password>\n",
	    progname);
    fprintf(stderr, "%s" SYS_OPT "remove <unique_id>\n", progname);
    fprintf(stderr, "%s" SYS_OPT "get <unique_id>\n", progname);
    exit(1);
}

typedef enum {
    kCommandUnknown,
    kCommandCreate,
    kCommandRemove,
    kCommandSet,
    kCommandGet
} Command;

#define keapolclientPath		"/System/Library/SystemConfiguration/EAPOLController.bundle/Contents/Resources/eapolclient"
#define kAirPortApplicationGroup	"AirPort"
#define kSystemUIServerPath 		"/System/Library/CoreServices/SystemUIServer.app"

STATIC CFArrayRef
copy_trusted_applications(void)
{
    CFMutableArrayRef		array;
    SecTrustedApplicationRef	trusted_app;
    OSStatus			status;

    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    /* this executable */
    status = SecTrustedApplicationCreateFromPath(NULL, &trusted_app);
    if (status != noErr) {
	fprintf(stderr,
		"SecTrustedApplicationCreateFromPath(NULL) failed, %d\n",
		status);
    }
    else {
	CFArrayAppendValue(array, trusted_app);
	CFRelease(trusted_app);
    }

    /* eapolclient */
    status = SecTrustedApplicationCreateFromPath(keapolclientPath,
						 &trusted_app);
    if (status != noErr) {
	fprintf(stderr,
		"SecTrustedApplicationCreateFromPath(%s) failed, %d\n",
		keapolclientPath,
		status);
    }
    else {
	CFArrayAppendValue(array, trusted_app);
	CFRelease(trusted_app);
    }

    
    /* SystemUIServer */
    status = SecTrustedApplicationCreateFromPath(kSystemUIServerPath, 
						 &trusted_app);
    if (status != noErr) {
	fprintf(stderr,
		"SecTrustedApplicationCreateFromPath(%s) failed, %d\n",
		kSystemUIServerPath,
		status);
    }
    else {
	CFArrayAppendValue(array, trusted_app);
	CFRelease(trusted_app);
    }

    /* AirPort Application Group */
    status 
	= SecTrustedApplicationCreateApplicationGroup(kAirPortApplicationGroup,
						      NULL, &trusted_app);
    if (status != noErr) {
	fprintf(stderr,
		"SecTrustedApplicationCreateApplicationGroup("
		kAirPortApplicationGroup ") failed, %d\n",
		status);
    }
    else {
	CFArrayAppendValue(array, trusted_app);
	CFRelease(trusted_app);
    }
    if (CFArrayGetCount(array) == 0) {
	my_CFRelease(&array);
    }
    return (array);
}

int
main(int argc, const char *argv[])
{
    Command		cmd = kCommandUnknown;
    SecKeychainRef	keychain = NULL;
    const char *	progname = argv[0];
    OSStatus		status;
#ifdef HAS_KEYCHAINS
    bool		use_system = FALSE;
#endif

    if (argc < 2) {
	usage(argv[0]);
    }
#ifdef HAS_KEYCHAINS
    if (strcmp(argv[1], "system") == 0) {
	if (argc < 3) {
	    usage(progname);
	}
	use_system = TRUE;
	status = SecKeychainCopySystemKeychain(&keychain);
	if (status != noErr) {
	    fprintf(stderr, "SecKeychainCopySystemKeychain failed, %s (%d)\n",
		    EAPSecurityErrorString(status), (int)status);
	    exit(1);
	}
	argc--;
	argv++;
    }
#endif /* HAS_KEYCHAINS */
    
    if (strcmp(argv[1], "create") == 0) {
	if (argc < 6) {
	    usage(progname);
	}
	cmd = kCommandCreate;
    }
    else if (strcmp(argv[1], "set") == 0) {
	if (argc < 5) {
	    usage(progname);
	}
	cmd = kCommandSet;
    }
    else if (strcmp(argv[1], "get") == 0) {
	if (argc < 3) {
	    usage(progname);
	}
	cmd = kCommandGet;
    }
    else if (strcmp(argv[1], "remove") == 0) {
	if (argc < 3) {
	    usage(progname);
	}
	cmd = kCommandRemove;
    }
    else {
	usage(progname);
    }
    switch (cmd) {
      case kCommandSet: {
	  CFMutableDictionaryRef 	attrs = NULL;
	  CFDataRef			password;
	  CFArrayRef			trusted_apps;
	  CFStringRef			unique_id_str;
	  CFDataRef			username;

	  unique_id_str  
	      = CFStringCreateWithCStringNoCopy(NULL, 
						argv[2],
						kCFStringEncodingUTF8,
						kCFAllocatorNull);
	  username
	      = CFDataCreateWithBytesNoCopy(NULL,
					    (const UInt8 *)argv[3],
					    strlen(argv[3]),
					    kCFAllocatorNull);
	  password
	      = CFDataCreateWithBytesNoCopy(NULL,
					    (const UInt8 *)argv[4],
					    strlen(argv[4]),
					    kCFAllocatorNull);
	  attrs = CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	  CFDictionarySetValue(attrs, kEAPSecKeychainPropAccount,
			       username);
	  CFDictionarySetValue(attrs, kEAPSecKeychainPropPassword,
			       password);
	  trusted_apps = copy_trusted_applications();
	  CFDictionarySetValue(attrs, kEAPSecKeychainPropTrustedApplications,
			       trusted_apps);
	  CFRelease(trusted_apps);
	  status = EAPSecKeychainPasswordItemSet2(keychain,
						  unique_id_str,
						  attrs);
	  if (status != noErr) {
	      fprintf(stderr, "EAPSecKeychainItemSet2 failed %s (%d)\n",
		      EAPSecurityErrorString(status), (int)status);
	      exit(2);
	  }
	  my_CFRelease(&password);
	  break;
      }
      case kCommandCreate: {
	  SecAccessRef			access = NULL;
	  CFMutableDictionaryRef	attrs = NULL;
	  CFDataRef			data;
	  CFArrayRef			trusted_apps;
	  CFDataRef			unique_id;
	  CFStringRef			unique_id_str = NULL;

#ifdef HAS_KEYCHAINS
	  if (use_system) {
	      status = EAPSecAccessCreateWithUid(0, &access);
	      if (status != noErr) {
		  fprintf(stderr,
			  "EAPSecAccessCreateWithUid failed, %s (%d)\n",
			  EAPSecurityErrorString(status), (int)status);
		  exit(2);
	      }
	  }
	  else {
	      status = SecAccessCreate(CFSTR("keychain"), NULL, &access);
	      if (status != noErr) {
		  fprintf(stderr, "SecAccessCreate failed, %s (%d)\n",
			  EAPSecurityErrorString(status), (int)status);
		  exit(2);
	      }
	  }
#endif /* HAS_KEYCHAINS */

	  attrs = CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);

	  /* label */
	  data = CFDataCreateWithBytesNoCopy(NULL, 
					     (const UInt8 *)argv[2],
					     strlen(argv[2]),
					     kCFAllocatorNull);
	  CFDictionarySetValue(attrs, kEAPSecKeychainPropLabel, data);
	  CFRelease(data);
	  
	  /* description */
	  data = CFDataCreateWithBytesNoCopy(NULL, 
					     (const UInt8 *)argv[3],
					     strlen(argv[3]),
					     kCFAllocatorNull);
	  CFDictionarySetValue(attrs, kEAPSecKeychainPropDescription,
			       data);
	  CFRelease(data);

	  /* name */
	  data = CFDataCreateWithBytesNoCopy(NULL, 
					     (const UInt8 *)argv[4],
					     strlen(argv[4]),
					     kCFAllocatorNull);
	  CFDictionarySetValue(attrs, kEAPSecKeychainPropAccount,
			       data);
	  CFRelease(data);

	  /* password */
	  data = CFDataCreateWithBytesNoCopy(NULL,
					     (const UInt8 *)argv[5],
					     strlen(argv[5]),
					     kCFAllocatorNull);
	  CFDictionarySetValue(attrs, kEAPSecKeychainPropPassword,
			       data);
	  CFRelease(data);

	  /* trusted apps */
	  trusted_apps = copy_trusted_applications();
	  CFDictionarySetValue(attrs, kEAPSecKeychainPropTrustedApplications,
			       trusted_apps);
	  CFRelease(trusted_apps);

	  if (argc > 6) {
	      unique_id_str
		  = CFStringCreateWithCStringNoCopy(NULL, 
						    argv[6],
						    kCFStringEncodingUTF8,
						    kCFAllocatorNull);
	      status 
		  = EAPSecKeychainPasswordItemCreate(keychain,
						     unique_id_str,
						     attrs);
	      if (status != noErr) {
		  fprintf(stderr, "EAPSecKeychainItemCreate failed,"
			  " %s (%d)\n",
			  EAPSecurityErrorString(status), (int)status);
		  exit(1);
	      }
	  }
	  else {
	      status 
		  = EAPSecKeychainPasswordItemCreateUnique(keychain,
							   attrs,
							   &unique_id_str);

	      if (status != noErr) {
		  fprintf(stderr,
			  "EAPSecKeychainItemCreateUniqueWithAccessfailed,"
			  " %s (%d)\n",
			  EAPSecurityErrorString(status), (int)status);
		  exit(1);
	      }
	  }
	  unique_id 
	      = CFStringCreateExternalRepresentation(NULL,
						     unique_id_str,
						     kCFStringEncodingUTF8,
						     0);
	  fwrite(CFDataGetBytePtr(unique_id),
		 CFDataGetLength(unique_id), 1, stdout);
	  printf("\n");
	  CFRelease(attrs);
	  break;
      }
      case kCommandGet:
      case kCommandRemove: {
	  CFStringRef	unique_id_str;

	  unique_id_str  
	      = CFStringCreateWithCStringNoCopy(NULL, 
						argv[2],
						kCFStringEncodingUTF8,
						kCFAllocatorNull);
	  if (cmd == kCommandRemove) {
	      status
		  = EAPSecKeychainPasswordItemRemove(keychain, unique_id_str);
	      if (status != noErr) {
		  fprintf(stderr,
			  "EAPSecKeychainItemRemove failed, %s (%d)\n",
			  EAPSecurityErrorString(status), (int)status);
		  exit(2);
	      }
	  }
	  else {
	      CFDictionaryRef	attrs;
	      const void *	keys[] = {
		  kEAPSecKeychainPropPassword,
		  kEAPSecKeychainPropAccount,
		  kEAPSecKeychainPropLabel,
		  kEAPSecKeychainPropDescription
	      };
	      int		keys_count = sizeof(keys) / sizeof(keys[0]);
	      CFDataRef		password = NULL;
	      CFArrayRef	req_props;
	      OSStatus		status;
	      CFDataRef		username = NULL;

	      req_props = CFArrayCreate(NULL,
					keys, keys_count, 
					&kCFTypeArrayCallBacks);
	      status = EAPSecKeychainPasswordItemCopy2(keychain, 
						       unique_id_str, 
						       req_props,
						       &attrs);
	      CFRelease(req_props);
	      if (status != noErr) {
		  fprintf(stderr,
			  "EAPSecKeychainPasswordItemCopy2 failed, %s (%d)\n",
			  EAPSecurityErrorString(status), (int)status);
		  exit(2);
	      }
	      CFShow(attrs);
	      password = CFDictionaryGetValue(attrs,
					      kEAPSecKeychainPropPassword);
	      username = CFDictionaryGetValue(attrs,
					      kEAPSecKeychainPropAccount);
	      if (password != NULL) {
		  printf("Password = '");
		  fwrite(CFDataGetBytePtr(password),
			 CFDataGetLength(password), 1, stdout);
		  printf("'\n");
	      }
	      if (username != NULL) {
		  printf("Name = '");
		  fwrite(CFDataGetBytePtr(username),
			 CFDataGetLength(username), 1, stdout);
		  printf("'\n");
	      }
	      CFRelease(attrs);
	  }
	  CFRelease(unique_id_str);
      }
      default:
	  break;
    }
    if (keychain != NULL) {
	CFRelease(keychain);
    }
    exit(0);
    return (0);
}

#endif /* TEST_EAPKEYCHAINUTIL */
