/*
 * Copyright (c) 2003-2006 Apple Inc. All rights reserved.
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
/*
 * AFPUsers.c
 * - create/maintain AFP logins
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <mach/boolean.h>
#include <sys/errno.h>
#include <limits.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCPrivate.h>	// for _SC_cfstring_to_cstring
#include <OpenDirectory/OpenDirectory.h>

#include "netinfo.h"
#include "NICache.h"
#include "NICachePrivate.h"
#include "AFPUsers.h"
#include "NetBootServer.h"
#include "cfutil.h"

extern void
my_log(int priority, const char *message, ...);

#define kAFPUserODRecord		CFSTR("record")
#define kAFPUserUID			CFSTR("uid")
#define kAFPUserPassword		CFSTR("passwd")
#define kAFPUserDatePasswordLastSet	CFSTR("setdate")

#define	BSDPD_CREATOR		"bsdpd"
#define MAX_RETRY		5

static uid_t
uid_from_odrecord(ODRecordRef record)
{
    uid_t		uid = -2;
    CFArrayRef		values	= NULL;

    values = ODRecordCopyValues(record, CFSTR(kDS1AttrUniqueID), NULL);
    if ((values != NULL) && (CFArrayGetCount(values) > 0)) {
	char		buf[64];
	char *		end;
	CFStringRef	uidStr;
	unsigned long	val;

	uidStr = CFArrayGetValueAtIndex(values, 0);
	(void) _SC_cfstring_to_cstring(uidStr, buf, sizeof(buf),
				       kCFStringEncodingASCII);
	errno = 0;
	val = strtoul(buf, &end, 0);
	if ((buf[0] != '\0') && (*end == '\0') && (errno == 0)) {
	    uid = (uid_t)val;
	}
    }
    my_CFRelease(&values);
    return (uid);
}

static AFPUserRef
AFPUser_create(ODRecordRef record)
{
    AFPUserRef		user;
    uid_t		uid;
    CFNumberRef 	uid_cf;

    user = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(user, kAFPUserODRecord, record);
    uid = uid_from_odrecord(record);
    uid_cf = CFNumberCreate(NULL, kCFNumberSInt32Type, &uid);
    CFDictionarySetValue(user, kAFPUserUID, uid_cf);
    CFRelease(uid_cf);
    return (user);
}

void
AFPUserList_free(AFPUserListRef users)
{
    my_CFRelease(&users->node);
    my_CFRelease(&users->list);
    bzero(users, sizeof(*users));
}

Boolean
AFPUserList_init(AFPUserListRef users)
{
    CFErrorRef	error;
    int		i;
    int		n;
    CFArrayRef	results;
    ODQueryRef	query;

    bzero(users, sizeof(*users));

    users->node = ODNodeCreateWithNodeType(NULL, kODSessionDefault, 
					   kODNodeTypeLocalNodes, &error);
    if (users->node == NULL) {
	my_log(LOG_NOTICE,
	       "AFPUserList_init: ODNodeCreateWithNodeType() failed");
	goto failed;
    }

    query = ODQueryCreateWithNode(NULL,
				  users->node,			// inNode
				  CFSTR(kDSStdRecordTypeUsers),	// inRecordTypeOrList
				  CFSTR(NIPROP__CREATOR),	// inAttribute
				  kODMatchEqualTo,		// inMatchType
				  CFSTR(BSDPD_CREATOR),		// inQueryValueOrList
				  CFSTR(kDSAttributesAll),	// inReturnAttributeOrList
				  0,				// inMaxResults
				  &error);
    if (query == NULL) {
	my_log(LOG_NOTICE, "AFPUserList_init: ODQueryCreateWithNode() failed");
	my_CFRelease(&error);
	goto failed;
    }

    results = ODQueryCopyResults(query, FALSE, &error);
    CFRelease(query);
    if (results == NULL) {
	my_log(LOG_NOTICE, "AFPUserList_init: ODQueryCopyResults() failed");
	my_CFRelease(&error);
	goto failed;
    }

    users->list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    n = CFArrayGetCount(results);
    for (i = 0; i < n; i++) {
	ODRecordRef		record;
	AFPUserRef		user;

	record = (ODRecordRef)CFArrayGetValueAtIndex(results, i);
	user = AFPUser_create(record);
	CFArrayAppendValue(users->list, user);
	CFRelease(user);
    }
    CFRelease(results);
    return (TRUE);

 failed:
    AFPUserList_free(users);
    return (FALSE);
}

static __inline__ Boolean
S_uid_taken(AFPUserListRef users, CFStringRef uid)
{
    CFErrorRef	error;
    Boolean	taken	= FALSE;
    ODQueryRef	query;
    CFArrayRef	results;

    query = ODQueryCreateWithNode(NULL,
				  users->node,			// inNode
				  CFSTR(kDSStdRecordTypeUsers),	// inRecordTypeOrList
				  CFSTR(kDS1AttrUniqueID),	// inAttribute
				  kODMatchEqualTo,		// inMatchType
				  uid,				// inQueryValueOrList
				  NULL,				// inReturnAttributeOrList
				  0,				// inMaxResults
				  &error);
    if (query == NULL) {
	my_log(LOG_NOTICE, "S_uid_taken: ODQueryCreateWithNode() failed");
	my_CFRelease(&error);
	goto failed;
    }

    results = ODQueryCopyResults(query, FALSE, &error);
    CFRelease(query);
    if (results == NULL) {
	my_log(LOG_NOTICE, "S_uid_taken: ODQueryCopyResults() failed");
	my_CFRelease(&error);
	goto failed;
    }

    if (CFArrayGetCount(results) > 0) {
	taken = TRUE;
    }
    CFRelease(results);

 failed:
    return (taken);
}

static void
_myCFDictionarySetStringValueAsArray(CFMutableDictionaryRef dict,
				     CFStringRef key,  CFStringRef str)
{
    CFArrayRef			array;

    array = CFArrayCreate(NULL, (const void **)&str,
			  1, &kCFTypeArrayCallBacks);
    CFDictionarySetValue(dict, key, array);
    CFRelease(array);
    return;
}

Boolean
AFPUserList_create(AFPUserListRef users, gid_t gid,
		uid_t start, int count)
{
    CFMutableDictionaryRef	attributes;
    char			buf[256];
    CFStringRef			gidStr;
    int				need;
    Boolean			ret = FALSE;
    uid_t			scan;

    need = count - CFArrayGetCount(users->list);
    if (need <= 0) {
	return (TRUE);
    }

    attributes = CFDictionaryCreateMutable(NULL, 0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
    _myCFDictionarySetStringValueAsArray(attributes,
					 CFSTR(kDS1AttrUserShell),
					 CFSTR("/bin/false"));

    snprintf(buf, sizeof(buf), "%d", gid);
    gidStr = CFStringCreateWithCString(NULL, buf, kCFStringEncodingASCII);
    _myCFDictionarySetStringValueAsArray(attributes,
					 CFSTR(kDS1AttrPrimaryGroupID),
					 gidStr);
    CFRelease(gidStr);
    _myCFDictionarySetStringValueAsArray(attributes,
					 CFSTR(kDS1AttrPassword),
					 CFSTR("*"));
    _myCFDictionarySetStringValueAsArray(attributes,
					 CFSTR(NIPROP__CREATOR),
					 CFSTR(BSDPD_CREATOR));

    for (scan = start; need > 0; scan++) {
	CFErrorRef	error	= NULL;
	ODRecordRef	record	= NULL;
	CFStringRef	uidStr;
	CFDictionaryRef	user;
	CFStringRef	userStr	= NULL;

	snprintf(buf, sizeof(buf), "%d", scan);
	uidStr = CFStringCreateWithCString(NULL, buf, kCFStringEncodingASCII);
	if (S_uid_taken(users, uidStr)) {
	    goto nextUid;
	}
	_myCFDictionarySetStringValueAsArray(attributes,
					     CFSTR(kDS1AttrUniqueID),
					     uidStr);
	snprintf(buf, sizeof(buf), NETBOOT_USER_PREFIX "%03d", scan);
	userStr = CFStringCreateWithCString(NULL, buf, kCFStringEncodingASCII);

	_myCFDictionarySetStringValueAsArray(attributes,
					     CFSTR(kDS1AttrDistinguishedName),
					     userStr);
	record = ODNodeCreateRecord(users->node,
				    CFSTR(kDSStdRecordTypeUsers),
				    userStr,
				    attributes,
				    &error);
	if (record == NULL) {
	    my_log(LOG_NOTICE,
		   "AFPUserList_create: ODNodeCreateRecord() failed");
	    goto nextUid;
	}

	if (!ODRecordSynchronize(record, &error)) {
	    my_log(LOG_NOTICE,
		   "AFPUserList_create: ODRecordSynchronize() failed");
	    goto nextUid;
	}
	user = AFPUser_create(record);
	CFArrayAppendValue(users->list, user);
	CFRelease(user);
	need--;

     nextUid:
	my_CFRelease(&record);
	my_CFRelease(&uidStr);
	my_CFRelease(&userStr);
	if (error != NULL) {
	    my_CFRelease(&error);
	    goto done;
	}
    }

    ret = TRUE;

 done:
    my_CFRelease(&attributes);
    return (ret);
}

AFPUserRef
AFPUserList_lookup(AFPUserListRef users, CFStringRef afp_user)
{
    int		i;
    int		n;

    n = CFArrayGetCount(users->list);
    for (i = 0; i < n; i++) {
	CFStringRef	name;
	AFPUserRef	user;
	ODRecordRef	record;

	user = (AFPUserRef)CFArrayGetValueAtIndex(users->list, i);
	record = (ODRecordRef)CFDictionaryGetValue(user, kAFPUserODRecord);
	name = ODRecordGetRecordName(record);
	if (CFEqual(name, afp_user)) {
		return (user);
	}
    }

    return (NULL);
}

uid_t
AFPUser_get_uid(AFPUserRef user)
{
    uid_t	uid = -2;

    CFNumberGetValue(CFDictionaryGetValue(user, kAFPUserUID),
		     kCFNumberSInt32Type, &uid);
    return (uid);
}

char *
AFPUser_get_user(AFPUserRef user, char *buf, size_t buf_len)
{
    CFStringRef	name;
    ODRecordRef	record;

    record = (ODRecordRef)CFDictionaryGetValue(user, kAFPUserODRecord);
    name = ODRecordGetRecordName(record);
    (void) _SC_cfstring_to_cstring(name, buf, buf_len, kCFStringEncodingASCII);
    return buf;
}

#define AFPUSER_PASSWORD_CHANGE_INTERVAL	((int)8)
/*
 * Function: AFPUser_set_random_password
 * Purpose:
 *   Set a random password for the user and returns it in passwd.
 *   Do not change the password again until AFPUSER_PASSWORD_CHANGE_INTERVAL
 *   has elapsed.  This overcomes the problem where every client
 *   request packet is duplicated. In that case, the client tries to use
 *   a password that subsequently gets changed when the duplicate arrives.
 */
Boolean
AFPUser_set_random_password(AFPUserRef user,
			    char * passwd, size_t passwd_len)
{
    CFDateRef		last_set;
    Boolean		ok = TRUE;
    CFDateRef		now;
    CFStringRef		pw;
    ODRecordRef		record;

    now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    pw = CFDictionaryGetValue(user, kAFPUserPassword);
    last_set = CFDictionaryGetValue(user, kAFPUserDatePasswordLastSet);
    if (pw != NULL && last_set != NULL
	&& (CFDateGetTimeIntervalSinceDate(now, last_set) 
	    < AFPUSER_PASSWORD_CHANGE_INTERVAL)) {
	/* return what we have */
#ifdef TEST_AFPUSERS
	printf("No need to change the password %d < %d\n",
	       (int)CFDateGetTimeIntervalSinceDate(now, last_set),
	       AFPUSER_PASSWORD_CHANGE_INTERVAL);
#endif TEST_AFPUSERS
	(void)_SC_cfstring_to_cstring(pw, passwd, passwd_len,
				      kCFStringEncodingASCII);
	CFDictionarySetValue(user, kAFPUserDatePasswordLastSet, now);
    }
    else {
	snprintf(passwd, passwd_len, "%08x", arc4random());

	record = (ODRecordRef)CFDictionaryGetValue(user, kAFPUserODRecord);
	pw = CFStringCreateWithCString(NULL, passwd, kCFStringEncodingASCII);
	ok = ODRecordChangePassword(record, NULL, pw, NULL);
	if (ok) {
	    CFDictionarySetValue(user, kAFPUserPassword, pw);
	    CFDictionarySetValue(user, kAFPUserDatePasswordLastSet, now);
	}
	else {
	    my_log(LOG_NOTICE, "AFPUser_set_random_password:"
		   " ODRecordChangePassword() failed");
	    CFDictionaryRemoveValue(user, kAFPUserPassword);
	    CFDictionaryRemoveValue(user, kAFPUserDatePasswordLastSet);
	}
	CFRelease(pw);
    }
    CFRelease(now);
    return ok;
}

#ifdef TEST_AFPUSERS

#include "afp.h"

#define USECS_PER_SEC	1000000
/*
 * Function: timeval_subtract
 *
 * Purpose:
 *   Computes result = tv1 - tv2.
 */
void
timeval_subtract(struct timeval tv1, struct timeval tv2, 
		 struct timeval * result)
{
    result->tv_sec = tv1.tv_sec - tv2.tv_sec;
    result->tv_usec = tv1.tv_usec - tv2.tv_usec;
    if (result->tv_usec < 0) {
	result->tv_usec += USECS_PER_SEC;
	result->tv_sec--;
    }
    return;
}

void
timestamp_printf(char * msg)
{
    static struct timeval	tvp = {0,0};
    struct timeval		tv;

    gettimeofday(&tv, 0);
    if (tvp.tv_sec) {
	struct timeval result;
	
	timeval_subtract(tv, tvp, &result);
	printf("%d.%06d (%d.%06d): %s\n", 
	       (int)tv.tv_sec, 
	       (int)tv.tv_usec,
	       (int)result.tv_sec,
	       (int)result.tv_usec, msg);
    }
    else 
	printf("%d.%06d (%d.%06d): %s\n", 
	       (int)tv.tv_sec, (int)tv.tv_usec, 0, 0, msg);
    tvp = tv;
}

void
AFPUserList_print(AFPUserListRef users)
{
    CFShow(users->list);
}

int 
main(int argc, char * argv[])
{
    CFIndex		i;
    CFIndex		n;
    AFPUserList 	users;
    struct group *	group_ent_p;
    int			count;
    int			start;

    if (argc < 3) {
	printf("usage: AFPUsers user_count start\n");
	exit(1);
    }

    group_ent_p = getgrnam(NETBOOT_GROUP);
    if (group_ent_p == NULL) {
        printf("Group '%s' missing\n", NETBOOT_GROUP);
        exit(1);
    }

    count = strtol(argv[1], NULL, 0);
    if (count < 0 || count > 100) {
	printf("invalid user_count\n");
	exit(1);
    }
    start = strtol(argv[2], NULL, 0);
    if (start <= 0) {
	printf("invalid start\n");
	exit(1);
    }
    timestamp_printf("before processing existing users");
    AFPUserList_init(&users);
    timestamp_printf("after processing existing users");
    //AFPUserList_print(&users);

    timestamp_printf("before creating new users");
    AFPUserList_create(&users, group_ent_p->gr_gid, start, count);
    timestamp_printf("after creating new users");
    //AFPUserList_print(&users);

    timestamp_printf("before setting passwords");
    n = CFArrayGetCount(users.list);
    for (i = 0; i < n; i++) {
	char 		pass_buf[AFP_PASSWORD_LEN + 1];
	AFPUserRef	user;

	user = (AFPUserRef)CFArrayGetValueAtIndex(users.list, i);
	AFPUser_set_random_password(user, pass_buf, sizeof(pass_buf));
    }
    timestamp_printf("after setting passwords");

    printf("Sleeping 1 second\n");
    sleep (1);
    timestamp_printf("before setting passwords again");
    n = CFArrayGetCount(users.list);
    for (i = 0; i < n; i++) {
	char 		pass_buf[AFP_PASSWORD_LEN + 1];
	AFPUserRef	user;

	user = (AFPUserRef)CFArrayGetValueAtIndex(users.list, i);
	AFPUser_set_random_password(user, pass_buf, sizeof(pass_buf));
    }
    timestamp_printf("after setting passwords again");

    printf("Sleeping 1 second\n");
    sleep(1);

    timestamp_printf("before setting passwords for 3rd time");
    for (i = 0; i < n; i++) {
	char 		pass_buf[AFP_PASSWORD_LEN + 1];
	AFPUserRef	user;

	user = (AFPUserRef)CFArrayGetValueAtIndex(users.list, i);
	AFPUser_set_random_password(user, pass_buf, sizeof(pass_buf));
    }
    timestamp_printf("after setting passwords for 3rd time");

    printf("sleeping %d seconds\n", AFPUSER_PASSWORD_CHANGE_INTERVAL);
    sleep(AFPUSER_PASSWORD_CHANGE_INTERVAL);

    timestamp_printf("before setting passwords for second time");
    for (i = 0; i < n; i++) {
	char 		pass_buf[AFP_PASSWORD_LEN + 1];
	AFPUserRef	user;

	user = (AFPUserRef)CFArrayGetValueAtIndex(users.list, i);
	AFPUser_set_random_password(user, pass_buf, sizeof(pass_buf));
    }
    timestamp_printf("after setting passwords for second time");

    AFPUserList_free(&users);
    printf("sleeping for 60 seconds, run leaks on %d\n", getpid());
    sleep(60);
    exit(0);
    return (0);
}

void
my_log(int priority, const char *message, ...)
{
    va_list 		ap;

    va_start(ap, message);
    vprintf(message, ap);
    return;
}

#endif TEST_AFPUSERS
