#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

#include <OpenDirectory/OpenDirectory.h>

/*
 * Return values for od_print_record().
 */
typedef enum {
	OD_CB_KEEPGOING,		/* continue the search */
	OD_CB_REJECTED,			/* this record had a problem - keep going */
	OD_CB_ERROR			/* error - quit and return an error */
} callback_ret_t;

static void pr_msg(const char *fmt, ...);
static int od_search(CFStringRef attr_to_match, char *value_to_match);

int
main(int argc, char **argv)
{
	int ret;
	char *pattern;

	if (argc < 2 || argc > 3) {
		fprintf(stderr, "Usage: dumammap <map name> [ <key> ]\n");
		return 1;
	}
	if (argc == 2) {
		/*
		 * Dump the entire map.
		 */
		ret = od_search(kODAttributeTypeMetaAutomountMap, argv[1]);
	} else {
		/*
		 * Dump an entry in the map.
		 * First, construct the string value to search for.
		 */
		if (asprintf(&pattern, "%s,automountMapName=%s", argv[2],
		    argv[1]) == -1) {
			pr_msg("malloc failed");
			return 2;
		}

		/*
		 * Now search for that entry.
		 */
		ret = od_search(kODAttributeTypeRecordName, pattern);
		free(pattern);
	}
	return ret ? 0 : 2;
}

/*
 * Get a C string from a CFStringRef.
 * The string is allocated with malloc(), and must be freed when it's
 * no longer needed.
 */
static char *
od_CFStringtoCString(CFStringRef cfstr)
{
	char *string;
	CFIndex length;

	length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr),
	    kCFStringEncodingUTF8);
	string = malloc(length + 1);
	if (string == NULL)
		return (NULL);
	if (!CFStringGetCString(cfstr, string, length + 1,
	    kCFStringEncodingUTF8)) {
		free(string);
		return (NULL);
	}
	return (string);
}


static char *
od_get_error_string(CFErrorRef err)
{
	CFStringRef errstringref;
	char *errstring;

	if (err != NULL) {
		errstringref = CFErrorCopyDescription(err);
		errstring = od_CFStringtoCString(errstringref);
		CFRelease(errstringref);
	} else
		errstring = strdup("Unknown error");
	return (errstring);
}

/*
 * Looks for kODAttributeTypeRecordName and
 * kODAttributeTypeAutomountInformation; if it finds them, it prints them.
 */
static callback_ret_t
od_print_record(ODRecordRef record)
{
	CFErrorRef error;
	char *errstring;
	CFArrayRef keys;
	CFStringRef key;
	CFArrayRef values;
	CFStringRef value;
	char *key_cstring, *value_cstring;

	/*
	 * Get kODAttributeTypeRecordName and
	 * kODAttributeTypeAutomountInformation for this record.
	 *
	 * Even though LDAP allows for multiple values per attribute, we take
	 * only the 1st value for each attribute because the automount data is
	 * organized as such (same as NIS+).
	 */
	error = NULL;
	keys = ODRecordCopyValues(record, kODAttributeTypeRecordName, &error);
	if (keys == NULL) {
		if (error != NULL) {
			errstring = od_get_error_string(error);
			pr_msg("od_print_record: can't get kODAttributeTypeRecordName attribute for record: %s",
			    errstring);
			free(errstring);
			return (OD_CB_ERROR);
		} else {
			/*
			 * We just reject records missing the attributes
			 * we need.
			 */
			pr_msg("od_print_record: record has no kODAttributeTypeRecordName attribute");
			return (OD_CB_REJECTED);
		}
	}
	if (CFArrayGetCount(keys) == 0) {
		/*
		 * We just reject records missing the attributes
		 * we need.
		 */
		CFRelease(keys);
		pr_msg("od_print_record: record has no kODAttributeTypeRecordName attribute");
		return (OD_CB_REJECTED);
	}
	key = CFArrayGetValueAtIndex(keys, 0);
	error = NULL;
	values = ODRecordCopyValues(record,
	    kODAttributeTypeAutomountInformation, &error);
	if (values == NULL) {
		CFRelease(keys);
		if (error != NULL) {
			errstring = od_get_error_string(error);
			pr_msg("od_print_record: can't get kODAttributeTypeAutomountInformation attribute for record: %s",
			    errstring);
			free(errstring);
			return (OD_CB_ERROR);
		} else {
			/*
			 * We just reject records missing the attributes
			 * we need.
			 */
			pr_msg("od_print_record: record has no kODAttributeTypeAutomountInformation attribute");
			return (OD_CB_REJECTED);
		}
	}
	if (CFArrayGetCount(values) == 0) {
		/*
		 * We just reject records missing the attributes
		 * we need.
		 */
		CFRelease(values);
		CFRelease(keys);
		pr_msg("od_print_record: record has no kODAttributeTypeRecordName attribute");
		return (OD_CB_REJECTED);
	}
	value = CFArrayGetValueAtIndex(values, 0);

	/*
	 * We have both of the attributes we need.
	 */
	key_cstring = od_CFStringtoCString(key);
	value_cstring = od_CFStringtoCString(value);
	printf("%s %s\n", key_cstring, value_cstring);
	free(key_cstring);
	free(value_cstring);
	CFRelease(values);
	CFRelease(keys);
	return (OD_CB_KEEPGOING);
}

/*
 * Fetch all the map records in Open Directory that have a certain attribute
 * that matches a certain value and pass those records to od_print_record().
 */
static int
od_search(CFStringRef attr_to_match, char *value_to_match)
{
	int ret;
	CFErrorRef error;
	char *errstring;
	ODNodeRef node_ref;
	CFArrayRef attrs;
	CFStringRef value_to_match_cfstr;
	ODQueryRef query_ref;
	CFArrayRef results;
	CFIndex num_results;
	CFIndex i;
	ODRecordRef record;
	callback_ret_t callback_ret;

	/*
	 * Create the search node.
	 */
	error = NULL;
	node_ref = ODNodeCreateWithNodeType(kCFAllocatorDefault, kODSessionDefault, 
	     kODNodeTypeAuthentication, &error);
	if (node_ref == NULL) {
		errstring = od_get_error_string(error);
		pr_msg("od_search: can't create search node for /Search: %s",
		    errstring);
		free(errstring);
		return (0);
	}

	/*
	 * Create the query.
	 */
	value_to_match_cfstr = CFStringCreateWithCString(kCFAllocatorDefault,
	    value_to_match, kCFStringEncodingUTF8);
	if (value_to_match_cfstr == NULL) {
		CFRelease(node_ref);
		pr_msg("od_search: can't make CFString from %s",
		    value_to_match);
		return (0);
	}
	attrs = CFArrayCreate(kCFAllocatorDefault,
	    (const void *[2]){kODAttributeTypeRecordName,
	                      kODAttributeTypeAutomountInformation}, 2,
	    &kCFTypeArrayCallBacks);
	if (attrs == NULL) {
		CFRelease(value_to_match_cfstr);
		CFRelease(node_ref);
		pr_msg("od_search: can't make array of attribute types");
		return (0);
	}
	error = NULL;
	query_ref = ODQueryCreateWithNode(kCFAllocatorDefault, node_ref,
	    kODRecordTypeAutomount, attr_to_match, kODMatchEqualTo,
	    value_to_match_cfstr, attrs, 0, &error);
	CFRelease(attrs);
	CFRelease(value_to_match_cfstr);
	if (query_ref == NULL) {
		CFRelease(node_ref);
		errstring = od_get_error_string(error);
		pr_msg("od_search: can't create query: %s",
		    errstring);
		free(errstring);
		return (0);
	}

	/*
	 * Wait for the query to get all the results, and then copy them.
	 */
	error = NULL;
	results = ODQueryCopyResults(query_ref, false, &error);
	if (results == NULL) {
		CFRelease(query_ref);
		CFRelease(node_ref);
		errstring = od_get_error_string(error);
		pr_msg("od_search: query failed: %s", errstring);
		free(errstring);
		return (0);
	}

	ret = 0;	/* we haven't found any records yet */
	num_results = CFArrayGetCount(results);
	for (i = 0; i < num_results; i++) {
		/*
		 * We've found a record.
		 */
		record = (ODRecordRef)CFArrayGetValueAtIndex(results, i);
		callback_ret = od_print_record(record);
		if (callback_ret == OD_CB_KEEPGOING) {
			/*
			 * We processed one record, but we want
			 * to keep processing records.
			 */
			ret = 1;
		} else if (callback_ret == OD_CB_ERROR) {
			/*
			 * Fatal error - give up.
			 */
			break;
		}

		/*
		 * Otherwise it's OD_CB_REJECTED, which is a non-fatal
		 * error.  We haven't found a record, so we shouldn't
		 * return __NSW_SUCCESS yet, but if we do find a
		 * record, we shouldn't fail.
		 */
	}
	CFRelease(results);
	CFRelease(query_ref);
	CFRelease(node_ref);
	return (ret);
}

/*
 * Print an error.
 */
static void
pr_msg(const char *fmt, ...)
{
	va_list ap;

	(void) fprintf(stderr, "dumpammap: ");
	va_start(ap, fmt);
	(void) vfprintf(stderr, fmt, ap);
	putc('\n', stderr);
	va_end(ap);
}

