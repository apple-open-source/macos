#ifdef OPEN_DIRECTORY
#include "open_directory.h"
#include "chpass.h"
#include <err.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <OpenDirectory/OpenDirectory.h>
#include <OpenDirectory/OpenDirectoryPriv.h>

/*---------------------------------------------------------------------------
 * PUBLIC setrestricted - sets the restricted flag
 *---------------------------------------------------------------------------*/
void
setrestricted(CFDictionaryRef attrs)
{
	const char* user_allowed[] = { "shell", "full name", "office location", "office phone", "home phone", "picture", NULL };
	const char* root_restricted[] = { "password", "change", "expire", "class", NULL };
	ENTRY* ep;
	const char** pp;
	int restrict_by_default = !master_mode;

	// for ordinary users, everything is restricted except for the values
	// expressly permitted above
	// for root, everything is permitted except for the values expressly
	// restricted above
		
	for (ep = list; ep->prompt; ep++) {
		ep->restricted = restrict_by_default;
		pp = restrict_by_default ? user_allowed : root_restricted;
		for (; *pp; pp++) {
			if (strncasecmp(ep->prompt, *pp, ep->len) == 0) {
				ep->restricted = !restrict_by_default;
				break;
			}
		}
		
		// If not root, then it is only permitted to change the shell
		// when the original value is one of the approved shells.
		// Otherwise, the assumption is that root has given this user
		// a restricted shell which they must not change away from.
		if (restrict_by_default && strcmp(ep->prompt, "shell") == 0) {
			ep->restricted = 1;
			CFArrayRef values = CFDictionaryGetValue(attrs, kODAttributeTypeUserShell);
			CFTypeRef value = values && CFArrayGetCount(values) > 0 ? CFArrayGetValueAtIndex(values, 0) : NULL;
			if (value && CFGetTypeID(value) == CFStringGetTypeID()) {
				size_t size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8)+1;
				char* shell = malloc(size);
				if (CFStringGetCString(value, shell, size, kCFStringEncodingUTF8)) {
					if (ok_shell(shell)) {
						ep->restricted = 0;
					}
				}
			}
		}
	}
}

static CFStringRef
prompt_passwd(CFStringRef user)
{
	CFStringRef result = NULL;
	CFStringRef prompt = CFStringCreateWithFormat(NULL, NULL, CFSTR("Password for %@: "), user);
	size_t prompt_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(prompt), kCFStringEncodingUTF8);
	char* buf = malloc(prompt_size);
	CFStringGetCString(prompt, buf, prompt_size, kCFStringEncodingUTF8);
	char* pass = getpass(buf);
	result = CFStringCreateWithCString(NULL, pass, kCFStringEncodingUTF8);
	memset(pass, 0, strlen(pass));
	free(buf);
	CFRelease(prompt);
	return result;
}

static void
show_error(CFErrorRef error) {
	if (error) {
		CFStringRef desc = CFErrorCopyDescription(error);
		if (desc) {
			cfprintf(stderr, "%s: %@", progname, desc);
			CFRelease(desc);
		}
		desc = CFErrorCopyFailureReason(error);
		if (desc) cfprintf(stderr, "  %@", desc);
		
		desc = CFErrorCopyRecoverySuggestion(error);
		if (desc) cfprintf(stderr, "  %@", desc);
		
		fprintf(stderr, "\n");
	}
}

ODRecordRef
odGetUser(CFStringRef location, CFStringRef authname, CFStringRef user, CFDictionaryRef* attrs)
{
	ODNodeRef node = NULL;
	ODRecordRef rec = NULL;
	CFErrorRef error = NULL;

	assert(attrs);

	/*
	 * Open the specified node, or perform a search.
	 * Copy the record and put the record's location into DSPath.
	 */
	if (location) {
		node = ODNodeCreateWithName(NULL, kODSessionDefault, location, &error);
	} else {
		node = ODNodeCreateWithNodeType(NULL, kODSessionDefault, kODNodeTypeAuthentication, &error);
	}
	if (node) {
		CFTypeRef	vals[] = { kODAttributeTypeStandardOnly };
		CFArrayRef desiredAttrs = CFArrayCreate(NULL, vals, 1, &kCFTypeArrayCallBacks);
		rec = ODNodeCopyRecord(node, kODRecordTypeUsers, user, desiredAttrs, &error);
		if (desiredAttrs) CFRelease(desiredAttrs);
		CFRelease(node);
	}
	if (rec) {
		*attrs = ODRecordCopyDetails(rec, NULL, &error);
		if (*attrs) {
			CFArrayRef values = CFDictionaryGetValue(*attrs, kODAttributeTypeMetaNodeLocation);
			DSPath = (values && CFArrayGetCount(values) > 0) ? CFArrayGetValueAtIndex(values, 0) : NULL;
		}

		/*
		 * Prompt for a password if -u was specified,
		 * or if we are not root,
		 * or if we are updating something not on the
		 * local node.
		 */
		if (authname || !master_mode ||
			(DSPath && CFStringCompareWithOptions(DSPath, CFSTR("/Local/"), CFRangeMake(0, 7), 0) != kCFCompareEqualTo)) {
			
			CFStringRef password = NULL;
			
			if (!authname) authname = user;
			
			password = prompt_passwd(authname);
			if (!ODRecordSetNodeCredentials(rec, authname, password, &error)) {
				CFRelease(rec);
				rec = NULL;
			}
		}
	}

	if (error) show_error(error);
	return rec;
}

void
odUpdateUser(ODRecordRef rec, CFDictionaryRef attrs_orig, CFDictionaryRef attrs)
{
	CFErrorRef error = NULL;
	int updated = 0;
	ENTRY* ep;
	
	for (ep = list; ep->prompt; ep++) {
	
		// Nothing to update
		if (!rec || !attrs_orig || !attrs) break;

		// No need to update if entry is restricted
		if (ep->restricted) continue;

		CFArrayRef values_orig = CFDictionaryGetValue(attrs_orig, *ep->attrName);
		CFTypeRef value_orig = values_orig && CFArrayGetCount(values_orig) ? CFArrayGetValueAtIndex(values_orig, 0) : NULL;
		CFTypeRef value = CFDictionaryGetValue(attrs, *ep->attrName);
		
		// No need to update if both values are the same
		if (value == value_orig) continue;

		// No need to update if strings are equal
		if (value && value_orig) {
			if (CFGetTypeID(value_orig) == CFStringGetTypeID() &&
				CFStringCompare(value_orig, value, 0) == kCFCompareEqualTo) continue;
		}
		
		// No need to update if empty string replaces NULL
		if (!value_orig && value) {
			if (CFStringGetLength(value) == 0) continue;
		}
		
		// Needs update
		if (value) {
			// if new value is an empty string, send an empty dictionary which will delete the property.
			CFIndex count = CFEqual(value, CFSTR("")) ? 0 : 1;
			CFTypeRef	vals[] = { value };
			CFArrayRef	values = CFArrayCreate(NULL, vals, count, &kCFTypeArrayCallBacks);
			if (values && ODRecordSetValue(rec, *ep->attrName, values, &error)) {
				updated = 1;
			}
			if (values) CFRelease(values);
			if (error) show_error(error);
		}
	}

	if (updated) {
		updated = ODRecordSynchronize(rec, &error);
		if (error) show_error(error);
	}
	if (!updated) {
		fprintf(stderr, "%s: no changes made\n", progname);
	}
}
#endif /* OPEN_DIRECTORY */
