
/*
 * Copyright (c) 2006-2008 Apple Inc. All rights reserved.
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
#include <CoreFoundation/CFUUID.h>
#include <CoreFoundation/CFNumber.h>
#include "EAPSecurity.h"
#include "EAPKeychainUtil.h"

static CFStringRef
EAPCFUUIDStringCreate(CFAllocatorRef alloc)
{
    CFUUIDRef 	uuid;
    CFStringRef	uuid_str;

    uuid = CFUUIDCreate(alloc);
    uuid_str = CFUUIDCreateString(alloc, uuid);
    CFRelease(uuid);
    return (uuid_str);
}

#if TARGET_OS_EMBEDDED
#include "myCFUtil.h"

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

#include <Security/cssmtype.h>
#include <Security/cssmapple.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>

OSStatus
EAPSecKeychainPasswordItemRemove(SecKeychainRef keychain,
				 CFStringRef unique_id_str)
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
    if (status != noErr) {
	fprintf(stderr, "SecKeychainFindGenericPassword failed: %s (%d)\n", 
		EAPSecurityErrorString(status), (int)status);
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
EAPSecKeychainPasswordItemCopy(SecKeychainRef keychain, 
			       CFStringRef unique_id_str, 
			       CFDataRef * ret_password)
{
    void *		password;
    UInt32 		password_length;
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
					 0, NULL,
					 &password_length,
					 &password,
					 NULL);
    CFRelease(unique_id);
    if (status == noErr) {
	*ret_password = CFDataCreate(NULL, password, password_length);
	SecKeychainItemFreeContent(NULL, password);
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
    SecKeychainAttribute	attributes[4];
    SecKeychainAttributeList	attributeList = {
	0, attributes
    };
    int				i;
    OSStatus			status;
    CFDataRef			unique_id;

    unique_id = CFStringCreateExternalRepresentation(NULL,
						     unique_id_str,
						     kCFStringEncodingUTF8,
						     0);
    attributes[0].tag = kSecServiceItemAttr;
    attributes[0].length = CFDataGetLength(unique_id);
    attributes[0].data = (void *)CFDataGetBytePtr(unique_id);

    i = 1;
    if (label != NULL) {
	attributes[i].tag = kSecLabelItemAttr;
	attributes[i].length = CFDataGetLength(label);
	attributes[i].data = (void *)CFDataGetBytePtr(label);
	i++;
    }
    if (description != NULL) {
	attributes[i].tag = kSecDescriptionItemAttr;
	attributes[i].length = CFDataGetLength(description);
	attributes[i].data = (void *)CFDataGetBytePtr(description);
	i++;
    }
    if (user != NULL) {
	attributes[i].tag = kSecAccountItemAttr;
	attributes[i].length = CFDataGetLength(user);
	attributes[i].data = (void *)CFDataGetBytePtr(user);
	i++;
    }
    attributeList.count = i;
    status = SecKeychainItemCreateFromContent(kSecGenericPasswordItemClass,
					      &attributeList,
					      CFDataGetLength(password),
					      CFDataGetBytePtr(password),
					      keychain,
					      access,
					      NULL);
    CFRelease(unique_id);
    return (status);
}

OSStatus
EAPSecKeychainPasswordItemSet(SecKeychainRef keychain,
			      CFStringRef unique_id_str,
			      CFDataRef password)
{
    void *			existing_password;
    UInt32 			existing_password_length;
    SecKeychainItemRef		item;
    OSStatus			status;
    CFDataRef			unique_id;

    unique_id = CFStringCreateExternalRepresentation(NULL,
						     unique_id_str,
						     kCFStringEncodingUTF8,
						     0);
    status
	= SecKeychainFindGenericPassword(keychain,
					 CFDataGetLength(unique_id),
					 (const char *)CFDataGetBytePtr(unique_id),
					 0, NULL,
					 &existing_password_length,
					 &existing_password,
					 &item);
    CFRelease(unique_id);
    if (status != noErr) {
	return (status);
    }
    if (CFDataGetLength(password) == existing_password_length
	&& bcmp(CFDataGetBytePtr(password), existing_password,
		existing_password_length) == 0) {
	goto done;
    }
    status
	= SecKeychainItemModifyAttributesAndData(item,
						 NULL, 
						 CFDataGetLength(password),
						 CFDataGetBytePtr(password));
 done:
    SecKeychainItemFreeContent(NULL, existing_password);
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

    if (ret_unique_id != NULL) {
	*ret_unique_id = NULL;
    }
    unique_id_str = EAPCFUUIDStringCreate(NULL);
    status = EAPSecKeychainPasswordItemCreateWithAccess(keychain,
							access,
							unique_id_str,
							label,
							description,
							user,
							password);
    if (status == noErr
	&& ret_unique_id != NULL) {
	*ret_unique_id = unique_id_str;
    }
    else {
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

static OSStatus
SecKeychainCopySystemKeychain(SecKeychainRef * ret_keychain)
{
    SecKeychainRef	keychain = NULL;
    OSStatus		status;
    
    status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
    if (status != noErr) {
	goto done;
    }
    status = SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem,
					  &keychain);
 done:
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
	    "%s" SYS_OPT "set <unique_id> <password>\n",
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
	    fprintf(stderr, "SecKeychainCopySystemKeychain failed, %s (%ld)\n",
		    EAPSecurityErrorString(status), status);
	    exit(1);
	}
	argc--;
	argv++;
    }
#endif HAS_KEYCHAINS
    
    if (strcmp(argv[1], "create") == 0) {
	if (argc < 6) {
	    usage(progname);
	}
	cmd = kCommandCreate;
    }
    else if (strcmp(argv[1], "set") == 0) {
	if (argc < 4) {
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
	  CFStringRef	unique_id_str;
	  CFDataRef	password;

	  unique_id_str  
	      = CFStringCreateWithCStringNoCopy(NULL, 
						argv[2],
						kCFStringEncodingUTF8,
						kCFAllocatorNull);
	  password
	      = CFDataCreateWithBytesNoCopy(NULL,
					    (const UInt8 *)argv[3],
					    strlen(argv[3]),
					    kCFAllocatorNull);
	  status = EAPSecKeychainPasswordItemSet(keychain,
						 unique_id_str,
						 password);
	  if (status != noErr) {
	      fprintf(stderr, "EAPSecKeychainItemSet failed %s (%ld)\n",
		      EAPSecurityErrorString(status), status);
	      exit(2);
	  }
	  CFRelease(password);
	  break;
      }
      case kCommandCreate: {
	  SecAccessRef	access = NULL;
	  CFDataRef	unique_id;
	  CFStringRef	unique_id_str = NULL;
	  CFDataRef	label;
	  CFDataRef	description;
	  CFDataRef	username;
	  CFDataRef	password;

#ifdef HAS_KEYCHAINS
	  if (use_system) {
	      status = EAPSecAccessCreateWithUid(0, &access);
	      if (status != noErr) {
		  fprintf(stderr,
			  "EAPSecAccessCreateWithUid failed, %s (%ld)\n",
			  EAPSecurityErrorString(status), status);
		  exit(2);
	      }
	  }
	  else {
	      status = SecAccessCreate(CFSTR("keychain"), NULL, &access);
	      if (status != noErr) {
		  fprintf(stderr, "SecAccessCreate failed, %s (%ld)\n",
			  EAPSecurityErrorString(status), status);
		  exit(2);
	      }
	  }
#endif /* HAS_KEYCHAINS */

	  label = CFDataCreateWithBytesNoCopy(NULL, 
					      (const UInt8 *)argv[2],
					      strlen(argv[2]),
					      kCFAllocatorNull);
	  description 
	      = CFDataCreateWithBytesNoCopy(NULL, 
					    (const UInt8 *)argv[3],
					    strlen(argv[3]),
					    kCFAllocatorNull);
	  username
	      = CFDataCreateWithBytesNoCopy(NULL, 
					    (const UInt8 *)argv[4],
					    strlen(argv[4]),
					    kCFAllocatorNull);
	  password
	      = CFDataCreateWithBytesNoCopy(NULL,
					    (const UInt8 *)argv[5],
					    strlen(argv[5]),
					    kCFAllocatorNull);
	  if (argc > 6) {
	      unique_id_str
		  = CFStringCreateWithCStringNoCopy(NULL, 
						    argv[6],
						    kCFStringEncodingUTF8,
						    kCFAllocatorNull);
	      status 
		  = EAPSecKeychainPasswordItemCreateWithAccess(keychain,
							       access,
							       unique_id_str,
							       label,
							       description,
							       username,
							       password);
	      if (status != noErr) {
		  fprintf(stderr, "EAPSecKeychainItemCreateWithAccessfailed,"
			  " %s (%ld)\n",
			  EAPSecurityErrorString(status), status);
		  exit(1);
	      }
	  }
	  else {
	      status 
		  = EAPSecKeychainPasswordItemCreateUniqueWithAccess(keychain,
								     access,
								     label,
								     description,
								     username,
								     password,
								     &unique_id_str);
	      if (status != noErr) {
		  fprintf(stderr,
			  "EAPSecKeychainItemCreateUniqueWithAccessfailed,"
			  " %s (%ld)\n",
			  EAPSecurityErrorString(status), status);
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
	  CFRelease(unique_id_str);
	  CFRelease(unique_id);
	  CFRelease(label);
	  CFRelease(description);
	  CFRelease(username);
	  CFRelease(password);
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
			  "EAPSecKeychainItemRemove failed, %s (%ld)\n",
			  EAPSecurityErrorString(status), status);
		  exit(2);
	      }
	  }
	  else {
	      CFDataRef		password = NULL;
	      OSStatus		status;

	      status = EAPSecKeychainPasswordItemCopy(keychain, 
						      unique_id_str, 
						      &password);
	      if (status != noErr) {
		  fprintf(stderr,
			  "EAPSecKeychainPasswordItemCopy failed, %s (%ld)\n",
			  EAPSecurityErrorString(status), status);
		  exit(2);
	      }
	      fwrite(CFDataGetBytePtr(password),
		     CFDataGetLength(password), 1, stdout);
	      printf("\n");
	      CFRelease(password);
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
