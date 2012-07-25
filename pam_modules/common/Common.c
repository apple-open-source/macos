#include <asl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/openpam.h>


#include <CoreFoundation/CoreFoundation.h>
#include <DirectoryService/DirectoryService.h>
#include <OpenDirectory/OpenDirectory.h>
#include <OpenDirectory/OpenDirectoryPriv.h>
#include <ServerInformation/ServerInformation.h>

#include "Common.h"

#if !defined(kDSValueAuthAuthorityDisabledUser)
#define kDSValueAuthAuthorityDisabledUser ";DisabledUser;"
#endif

#define kOSInstall_mpkg "/System/Installation/Packages/OSInstall.mpkg"
#define kOSInstall_collection "/System/Installation/Packages/OSInstall.collection"

enum {
	kWaitSeconds       =  1,
	kMaxIterationCount = 30
};

int
cfboolean_get_value(CFTypeRef p)
{
	int value = 0;
	int retval = 0;
	
	if (NULL == p) {
		goto cleanup;
	}
	
	if (CFBooleanGetTypeID() == CFGetTypeID(p))
		retval = CFBooleanGetValue(p);
	else if (CFNumberGetTypeID() == CFGetTypeID(p) && CFNumberGetValue(p, kCFNumberIntType, &value))
		retval = value;
	else
		retval = 0;
	
cleanup:
	if (PAM_SUCCESS != retval)
		openpam_log(PAM_LOG_ERROR, "failed: %d", retval);
	
	return retval;
}

int
cstring_to_cfstring(const char *val, CFStringRef *buffer)
{
	int retval = PAM_BUF_ERR;
	
	if (NULL == val || NULL == buffer) {
		openpam_log(PAM_LOG_DEBUG, "NULL argument passed");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}
	
	*buffer = CFStringCreateWithCString(kCFAllocatorDefault, val, kCFStringEncodingUTF8);
	if (NULL == *buffer) {
		openpam_log(PAM_LOG_DEBUG, "CFStringCreateWithCString() failed");
		retval = PAM_BUF_ERR;
		goto cleanup;
	}
	
	retval =  PAM_SUCCESS;
	
cleanup:
	if (PAM_SUCCESS != retval)
		openpam_log(PAM_LOG_ERROR, "failed: %d", retval);
	
	return retval;
}

int
cfstring_to_cstring(const CFStringRef val, char **buffer)
{
	CFIndex maxlen = 0;
	int retval = PAM_BUF_ERR;
	
	if (NULL == val || NULL == buffer) {
		openpam_log(PAM_LOG_DEBUG, "NULL argument passed");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}
	
	maxlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(val), kCFStringEncodingUTF8);
	*buffer = calloc(maxlen + 1, sizeof(char));
	if (NULL == *buffer) {
		openpam_log(PAM_LOG_DEBUG, "malloc() failed");
		retval = PAM_BUF_ERR;
		goto cleanup;
	}
	
	if (CFStringGetCString(val, *buffer, maxlen + 1, kCFStringEncodingUTF8)) {
		retval =  PAM_SUCCESS;
	} else {
		openpam_log(PAM_LOG_DEBUG, "CFStringGetCString failed.");
		*buffer = NULL;
	}
	
cleanup:
	if (PAM_SUCCESS != retval)
		openpam_log(PAM_LOG_ERROR, "failed: %d", retval);
	
	return retval;
}

#ifdef OPENDIRECTORY_CACHE
static void
cleanup_cache(pam_handle_t *pamh, void *data, int pam_end_status)
{
    CFRelease((CFTypeRef)data);
}
#endif /* OPENDIRECTORY_CACHE */

int
od_record_create(pam_handle_t *pamh, ODRecordRef *record, CFStringRef cfUser)
{
	int retval = PAM_SERVICE_ERR;
	const int attr_num = 5;

	ODNodeRef cfNode = NULL;
	CFErrorRef cferror = NULL;
	CFArrayRef attrs = NULL;
	CFTypeRef cfVals[attr_num];

	if (NULL == record || NULL == cfUser) {
		openpam_log(PAM_LOG_DEBUG, "NULL argument passed");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

#ifdef OPENDIRECTORY_CACHE
#define CFRECORDNAME_CACHE "CFRecordName"
#define CFRECORDNAME_NAME CFSTR("name")
#define CFRECORDNAME_RECORD CFSTR("record")

	CFDictionaryRef cfdict;
	CFStringRef cachedUser;

	if (pam_get_data(pamh, CFRECORDNAME_CACHE, (void *)&cfdict) == PAM_SUCCESS &&
	    (CFGetTypeID(cfdict) == CFDictionaryGetTypeID()) &&
	    (cachedUser = CFDictionaryGetValue(cfdict, CFRECORDNAME_NAME)) != NULL &&
	    CFGetTypeID(cachedUser) == CFStringGetTypeID() &&
	    CFStringCompare(cfUser, cachedUser, 0) == kCFCompareEqualTo &&
	    (*record = (ODRecordRef)CFDictionaryGetValue(cfdict, CFRECORDNAME_RECORD)) != NULL)
	{
		CFRetain(*record);
		return PAM_SUCCESS;
	}
#endif /* OPENDIRECTORY_CACHE */

	int current_iterations = 0;

	cfNode = ODNodeCreateWithNodeType(kCFAllocatorDefault,
					  kODSessionDefault,
					  eDSAuthenticationSearchNodeName,
					  &cferror);
	if (NULL == cfNode || NULL != cferror) {
		openpam_log(PAM_LOG_ERROR, "ODNodeCreateWithNodeType failed.");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

	cfVals[0] = kODAttributeTypeAuthenticationAuthority;
	cfVals[1] = kODAttributeTypeHomeDirectory;
	cfVals[2] = kODAttributeTypeNFSHomeDirectory;
	cfVals[3] = kODAttributeTypeUserShell;
	cfVals[4] = kODAttributeTypeUniqueID;
	attrs = CFArrayCreate(kCFAllocatorDefault, cfVals, (CFIndex)attr_num, &kCFTypeArrayCallBacks);
	if (NULL == attrs) {
		openpam_log(PAM_LOG_DEBUG, "CFArrayCreate() failed");
		retval = PAM_BUF_ERR;
		goto cleanup;
	}

	retval = PAM_SERVICE_ERR;
	while (current_iterations <= kMaxIterationCount) {
		CFIndex unreachable_count = 0;
		CFArrayRef unreachable_nodes = ODNodeCopyUnreachableSubnodeNames(cfNode, NULL);
		if (unreachable_nodes) {
			unreachable_count = CFArrayGetCount(unreachable_nodes);
			CFRelease(unreachable_nodes);
			openpam_log(PAM_LOG_DEBUG, "%lu OD nodes unreachable.", unreachable_count);
		}

		*record = ODNodeCopyRecord(cfNode, kODRecordTypeUsers, cfUser, attrs, &cferror);
		if (*record)
			break;
		if (0 == unreachable_count)
			break;

		openpam_log(PAM_LOG_DEBUG, "Waiting %d seconds for nodes to become reachable", kWaitSeconds);
		sleep(kWaitSeconds);
		++current_iterations;
	}

	if (*record) {
#ifdef OPENDIRECTORY_CACHE
		const void *keys[] = { CFRECORDNAME_NAME, CFRECORDNAME_RECORD };
		const void *values[] = { cfUser, *record };
		CFDictionaryRef dict;
		
		dict = CFDictionaryCreate(NULL, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (dict)
			pam_set_data(pamh, CFRECORDNAME_CACHE, (void *)dict, cleanup_cache);
#endif /* OPENDIRECTORY_CACHE */
		retval = PAM_SUCCESS;
	} else {
		retval = PAM_USER_UNKNOWN;
	}

	if (current_iterations > 0) {
		char *wt = NULL, *found = NULL;

		if (*record)
			found = "failure";
		else
			found = "success";

		retval = asprintf(&wt, "%d", kWaitSeconds * current_iterations);
		if (-1 == retval) {
			openpam_log(PAM_LOG_DEBUG, "Failed to convert current wait time to string.");
			retval = PAM_BUF_ERR;
			goto cleanup;
		}


		aslmsg m = asl_new(ASL_TYPE_MSG);
		asl_set(m, "com.apple.message.domain", "com.apple.pam_modules.odAvailableWaitTime" );
		asl_set(m, "com.apple.message.signature", "wait_time");
		asl_set(m, "com.apple.message.value", wt);
		asl_set(m, "com.apple.message.result", found);
		asl_log(NULL, m, ASL_LEVEL_NOTICE, "OD nodes online delay: %ss. User record lookup: %s.", wt, found);
		asl_free(m);
		free(wt);
	}

cleanup:
	if (NULL != attrs) {
		CFRelease(attrs);
	}

	if (NULL != cferror) {
		CFRelease(cferror);
	}

	if (NULL != cfNode) {
		CFRelease(cfNode);
	}

	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_ERROR, "failed: %d", retval);
		if (NULL != *record) {
			CFRelease(*record);
			*record = NULL;
		}
	}

	return retval;
}

int
od_record_create_cstring(pam_handle_t *pamh, ODRecordRef *record, const char *user)
{
	int retval = PAM_SUCCESS;
	CFStringRef cfUser = NULL;

	if (NULL == record || NULL == user) {
		openpam_log(PAM_LOG_DEBUG, "NULL argument passed");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

	if (PAM_SUCCESS != (retval = cstring_to_cfstring(user, &cfUser)) ||
	    PAM_SUCCESS != (retval = od_record_create(pamh, record, cfUser))) {
		openpam_log(PAM_LOG_DEBUG, "od_record_create() failed");
		goto cleanup;
	}

cleanup:
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_ERROR, "failed: %d", retval);
		if (NULL != *record) {
			CFRelease(*record);
		}
	}

	if (NULL != cfUser) {
		CFRelease(cfUser);
	}

	return retval;
}

/* Can return NULL */
int
od_record_attribute_create_cfarray(ODRecordRef record, CFStringRef attrib,  CFArrayRef *out)
{
	int retval = PAM_SUCCESS;

	if (NULL == record || NULL == attrib || NULL == out) {
		openpam_log(PAM_LOG_DEBUG, "NULL argument passed");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

	*out = ODRecordCopyValues(record, attrib, NULL);

cleanup:
	if (PAM_SUCCESS != retval) {
		if (NULL != out) {
			CFRelease(out);
		}
	}
	return retval;
}

/* Can return NULL */
int
od_record_attribute_create_cfstring(ODRecordRef record, CFStringRef attrib,  CFStringRef *out)
{
	int retval = PAM_SERVICE_ERR;
	CFTypeRef cval = NULL;
	CFArrayRef vals = NULL;
	CFIndex i = 0, count = 0;

	if (NULL == record || NULL == attrib || NULL == out) {
		openpam_log(PAM_LOG_DEBUG, "NULL argument passed");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

	*out = NULL;
	retval = od_record_attribute_create_cfarray(record, attrib, &vals);
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_DEBUG, "od_record_attribute_create_cfarray() failed");
		goto cleanup;
	}
	if (NULL == vals) {
		retval = PAM_SUCCESS;
		goto cleanup;
	}

	count = CFArrayGetCount(vals);
	if (1 != count) {
		char *attr_cstr = NULL;
		cfstring_to_cstring(attrib, &attr_cstr);
		openpam_log(PAM_LOG_DEBUG, "returned %lx attributes for %s", count, attr_cstr);
		free(attr_cstr);
	}

	for (i = 0; i < count; ++i) {
		cval = CFArrayGetValueAtIndex(vals, i);
		if (NULL == cval) {
			continue;
		}
		if (CFGetTypeID(cval) == CFStringGetTypeID()) {
			*out = CFStringCreateCopy(kCFAllocatorDefault, cval);
			if (NULL == *out) {
				openpam_log(PAM_LOG_DEBUG, "CFStringCreateCopy() failed");
				retval = PAM_BUF_ERR;
				goto cleanup;
			}
			break;
		} else {
			openpam_log(PAM_LOG_DEBUG, "attribute is not a cfstring");
			retval = PAM_PERM_DENIED;
			goto cleanup;
		}
	}
	retval = PAM_SUCCESS;

cleanup:
	if (PAM_SUCCESS != retval) {
		if (NULL != out) {
			CFRelease(out);
		}
	}
	if (NULL != vals) {
		CFRelease(vals);
	}

	return retval;
}

/* Can return NULL */
int
od_record_attribute_create_cstring(ODRecordRef record, CFStringRef attrib,  char **out)
{
	int retval = PAM_SERVICE_ERR;
	CFStringRef val = NULL;

	if (NULL == record || NULL == attrib || NULL == out) {
		openpam_log(PAM_LOG_DEBUG, "NULL argument passed");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

	retval = od_record_attribute_create_cfstring(record, attrib, &val);
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_DEBUG, "od_record_attribute_create_cfstring() failed");
		goto cleanup;
	}

	if (NULL != val) {
		retval = cfstring_to_cstring(val, out);
		if (PAM_SUCCESS != retval) {
			openpam_log(PAM_LOG_DEBUG, "cfstring_to_cstring() failed");
			goto cleanup;
		}
	}

cleanup:
	if (PAM_SUCCESS != retval) {
		free(out);
	}

	if (NULL != val) {
		CFRelease(val);
	}

	return retval;
}

int
od_record_check_pwpolicy(ODRecordRef record)
{
	CFDictionaryRef policy = NULL;
	const void *isDisabled;
	const void *newPasswordRequired;
	int retval = PAM_SERVICE_ERR;

	if (NULL == record) {
		openpam_log(PAM_LOG_DEBUG, "NULL argument passed");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

	if (NULL == (policy = ODRecordCopyPasswordPolicy(kCFAllocatorDefault, record, NULL)) ||
	    NULL == (isDisabled = CFDictionaryGetValue(policy, CFSTR("isDisabled"))) ||
	    !cfboolean_get_value(isDisabled))
		retval = PAM_SUCCESS;
	else
		retval = PAM_PERM_DENIED;
	if (NULL != policy &&
		NULL != (newPasswordRequired = CFDictionaryGetValue(policy, CFSTR("newPasswordRequired"))) &&
	    cfboolean_get_value(newPasswordRequired))
		retval = PAM_NEW_AUTHTOK_REQD;

	if (NULL != policy) {
		CFRelease(policy);
	}

cleanup:
	openpam_log(PAM_LOG_DEBUG, "retval: %d", retval);
	return retval;
}

int
od_record_check_authauthority(ODRecordRef record)
{
	int retval = PAM_PERM_DENIED;
	CFStringRef authauth = NULL;

	if (NULL == record) {
		openpam_log(PAM_LOG_DEBUG, "NULL argument passed");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

	retval = od_record_attribute_create_cfstring(record, kODAttributeTypeAuthenticationAuthority, &authauth);
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_DEBUG, "od_record_attribute_create_cfstring() failed");
		goto cleanup;
	}
	if (NULL == authauth) {
		retval = PAM_SUCCESS;
		goto cleanup;
	}
	if (!CFStringHasPrefix(authauth, CFSTR(kDSValueAuthAuthorityDisabledUser))) {
		retval = PAM_SUCCESS;
	}

cleanup:
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_ERROR, "failed: %d", retval);
	}

	if (authauth) {
		CFRelease(authauth);
	}

	return retval;
}

int
od_record_check_homedir(ODRecordRef record)
{
	int retval = PAM_SERVICE_ERR;
	CFStringRef tmp = NULL;

	if (NULL == record) {
		openpam_log(PAM_LOG_DEBUG, "NULL argument passed");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

	retval = od_record_attribute_create_cfstring(record, kODAttributeTypeNFSHomeDirectory, &tmp);
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_DEBUG, "od_record_attribute_create_cfstring() failed");
		goto cleanup;
	}

	/* Allow NULL home directories */
	if (NULL == tmp) {
		retval = PAM_SUCCESS;
		goto cleanup;
	}

	/* Do not allow login with '/dev/null' home */
	if (kCFCompareEqualTo == CFStringCompare(tmp, CFSTR("/dev/null"), 0)) {
		openpam_log(PAM_LOG_DEBUG, "home directory is /dev/null");
		retval = PAM_PERM_DENIED;
		goto cleanup;
	}

	if (kCFCompareEqualTo == CFStringCompare(tmp, CFSTR("99"), 0)) {
		openpam_log(PAM_LOG_DEBUG, "home directory is 99");
		retval = PAM_PERM_DENIED;
		goto cleanup;
	}

	retval = PAM_SUCCESS;

cleanup:
	if (PAM_SUCCESS != retval)
		openpam_log(PAM_LOG_ERROR, "failed: %d", retval);

	if (NULL != tmp) {
		CFRelease(tmp);
	}

	return retval;
}

int
od_record_check_shell(ODRecordRef record)
{
	int retval = PAM_PERM_DENIED;
	CFStringRef cfstr = NULL;

	if (NULL == record) {
		openpam_log(PAM_LOG_DEBUG, "NULL argument passed");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

	retval = od_record_attribute_create_cfstring(record, kODAttributeTypeUserShell, &cfstr);
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_DEBUG, "od_record_attribute_create_cfstring() failed");
		goto cleanup;
	}

	if (NULL == cfstr) {
		retval = PAM_SUCCESS;
		goto cleanup;
	}

	if (CFStringCompare(cfstr, CFSTR("/usr/bin/false"), 0) == kCFCompareEqualTo) {
		openpam_log(PAM_LOG_DEBUG, "user shell is /bin/false");
		retval = PAM_PERM_DENIED;
	}

cleanup:
	if (PAM_SUCCESS != retval)
		openpam_log(PAM_LOG_ERROR, "failed: %d", retval);

	if (NULL != cfstr) {
		CFRelease(cfstr);
	}

	return retval;
}

int
od_string_from_record(ODRecordRef record, CFStringRef attrib,  char **out)
{
	int retval = PAM_SERVICE_ERR;
	CFStringRef val = NULL;
	
	if (NULL == record) {
		openpam_log(PAM_LOG_DEBUG, "%s - NULL ODRecord passed.", __func__);
		goto cleanup;
	}
	
	retval = od_record_attribute_create_cfstring(record, attrib, &val);
	if (PAM_SUCCESS != retval) {
		goto cleanup;
	}
	
	if (val)
		retval = cfstring_to_cstring(val, out);
	
cleanup:
	if (val)
		CFRelease(val);
	
	return retval;
}

int
extract_homemount(char *in, char **out_url, char **out_path)
{
	// Directory Services people have assured me that this won't change
	static const char URL_OPEN[] = "<url>";
	static const char URL_CLOSE[] = "</url>";
	static const char PATH_OPEN[] = "<path>";
	static const char PATH_CLOSE[] = "</path>";
	
	char *server_URL = NULL;
	char *path = NULL;
	char *record_start = NULL;
	char *record_end = NULL;
	
	int retval = PAM_SERVICE_ERR;
	
	if (NULL == in)
		goto fin;
	
	record_start = in;
	server_URL = strstr(record_start, URL_OPEN);
	if (NULL == server_URL)
		goto fin;
	server_URL += sizeof(URL_OPEN)-1;
	while ('\0' != *server_URL && isspace(*server_URL))
		server_URL++;
	record_end = strstr(server_URL, URL_CLOSE);
	if (NULL == record_end)
		goto fin;
	while (record_end >= server_URL && '\0' != *record_end && isspace(*(record_end-1)))
		record_end--;
	if (NULL == record_end)
		goto fin;
	*record_end = '\0';
	if (NULL == (*out_url = strdup(server_URL)))
		goto fin;
	
	record_start = record_end+1;
	path = strstr(record_start, PATH_OPEN);
	if (NULL == path)
		goto ok;
	path += sizeof(PATH_OPEN)-1;
	while ('\0' != *path && isspace(*path))
		path++;
	record_end = strstr(path, PATH_CLOSE);
	if (NULL == record_end)
		goto fin;
	while (record_end >= path && '\0' != *record_end && isspace(*(record_end-1)))
		record_end--;
	if (NULL == record_end)
		goto fin;
	*record_end = '\0';
	if (NULL == (*out_path = strdup(path)))
		goto fin;
	
ok:
	retval = PAM_SUCCESS;
fin:
	return retval;
}

int
od_extract_home(pam_handle_t *pamh, const char *username, char **server_URL, char **path, char **homedir)
{
	int retval = PAM_SERVICE_ERR;
	char *tmp = NULL;
	ODRecordRef record = NULL;
	
	retval = od_record_create_cstring(pamh, &record, username);
	if (PAM_SUCCESS != retval) {
		goto cleanup;
	}
	
	retval = od_string_from_record(record, kODAttributeTypeHomeDirectory, &tmp);
	if (retval) {
		openpam_log(PAM_LOG_DEBUG, "%s - get kODAttributeTypeHomeDirectory  : %d",
			    __func__, retval);
		goto cleanup;
	}
	extract_homemount(tmp, server_URL, path);
	openpam_log(PAM_LOG_DEBUG, "%s - Server URL   : %s", __func__, *server_URL);
	openpam_log(PAM_LOG_DEBUG, "%s - Path to mount: %s", __func__, *path);
	
	retval = od_string_from_record(record, kODAttributeTypeNFSHomeDirectory, homedir);
	openpam_log(PAM_LOG_DEBUG, "%s - Home dir     : %s", __func__, *homedir);
	if (retval)
		goto cleanup;
	
	retval = PAM_SUCCESS;
	
cleanup:
	if (tmp)
		free(tmp);
	if (record)
		CFRelease(record);
	
	return retval;
}

/* extract the principal from OpenDirectory */
int
od_principal_for_user(pam_handle_t *pamh, const char *user, char **od_principal)
{
	int retval = PAM_SERVICE_ERR;
	ODRecordRef record = NULL;
	CFStringRef principal = NULL;
	CFArrayRef authparts = NULL, vals = NULL;
	CFIndex i = 0, count = 0;

	if (NULL == user || NULL == od_principal) {
		openpam_log(PAM_LOG_DEBUG, "NULL argument passed");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

	retval = od_record_create_cstring(pamh, &record, user);
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_DEBUG, "od_record_attribute_create_cfstring() failed");
		goto cleanup;
	}

	retval = od_record_attribute_create_cfarray(record, kODAttributeTypeAuthenticationAuthority, &vals);
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_DEBUG, "od_record_attribute_create_cfarray() failed");
		goto cleanup;
	}
	if (NULL == vals) {
		openpam_log(PAM_LOG_DEBUG, "no authauth availale for user.");
		retval = PAM_PERM_DENIED;
		goto cleanup;
	}

	count = CFArrayGetCount(vals);
	for (i = 0; i < count; i++)
	{
		const void *val = CFArrayGetValueAtIndex(vals, i);
		if (NULL == val || CFGetTypeID(val) != CFStringGetTypeID())
			break;

		authparts = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, val, CFSTR(";"));
		if (NULL == authparts)
			continue;

		if ((CFArrayGetCount(authparts) < 5) ||
		    (CFStringCompare(CFArrayGetValueAtIndex(authparts, 1), CFSTR("Kerberosv5"), kCFCompareEqualTo)) ||
		    (CFStringHasPrefix(CFArrayGetValueAtIndex(authparts, 4), CFSTR("LKDC:")))) {
			if (NULL != authparts) {
				CFRelease(authparts);
				authparts = NULL;
			}
			continue;
		} else {
			break;
		}
	}

	if (NULL == authparts) {
		openpam_log(PAM_LOG_DEBUG, "No authentication authority returned");
		retval = PAM_PERM_DENIED;
		goto cleanup;
	}

	principal = CFArrayGetValueAtIndex(authparts, 3);
	if (NULL == principal) {
		openpam_log(PAM_LOG_DEBUG, "no principal found in authentication authority");
		retval = PAM_PERM_DENIED;
		goto cleanup;
	}

	retval = cfstring_to_cstring(principal, od_principal);
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_DEBUG, "cfstring_to_cstring() failed");
		goto cleanup;
	}


cleanup:
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_DEBUG, "failed: %d", retval);
	}

	if (NULL != record) {
		CFRelease(record);
	}

	if (NULL != authparts) {
		CFRelease(authparts);
	}

	if (NULL != vals) {
		CFRelease(vals);
	}

	return retval;
}

void
pam_cf_cleanup(__unused pam_handle_t *pamh, void *data, __unused int pam_end_status)
{
	if (data) {
		CFStringRef *cfstring = data;
		CFRelease(*cfstring);
	}
}

Boolean IsServerInstall(void)
{
	struct stat statBuf;

	if ((SIIsServerHardware() == true) && ((stat(kOSInstall_mpkg, &statBuf) == 0) || (stat(kOSInstall_collection, &statBuf) == 0))) {
		return true;
	}

	return false;
}
