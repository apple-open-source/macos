/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 *	ns_od.c
 *
 * Based on ns_ldap.c
 *
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007-2011 Apple Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

#include <OpenDirectory/OpenDirectory.h>

#include "automount.h"
#include "automount_od.h"

/*
 * Callback routines for od_process_record_attributes() and od_search().
 */
typedef enum {
	OD_CB_KEEPGOING,		/* continue the search */
	OD_CB_DONE,			/* we're done with the search */
	OD_CB_REJECTED,			/* this record had a problem - keep going */
	OD_CB_ERROR			/* error - quit and return __NSW_UNAVAIL */
} callback_ret_t;
typedef callback_ret_t (*callback_fn)(CFStringRef key, CFStringRef value,
    void *udata);

static callback_ret_t mastermap_callback(CFStringRef key, CFStringRef value,
    void *udata);
static callback_ret_t directmap_callback(CFStringRef key, CFStringRef value,
    void *udata);
static callback_ret_t match_callback(CFStringRef key, CFStringRef value,
    void *udata);
static callback_ret_t readdir_callback(CFStringRef key, CFStringRef value,
    void *udata);

struct match_cbdata {
	const char *map;
	const char *key;
	char **od_line;
	int *od_len;
};

struct loadmaster_cbdata {
	char *defopts;
	char **stack;
	char ***stkptr;
};

struct loaddirect_cbdata {
	char *opts;
	char *localmap;
	char **stack;
	char ***stkptr;
};

struct dir_cbdata {
	struct dir_entry **list;
	struct dir_entry *last;
	int error;
};

static int od_match(const char *map, const char *key, char **od_line,
    int *od_len);
static int od_search(CFStringRef attr_to_match, char *value_to_match,
    callback_fn callback, void *udata);

/*
 * Get the C-string length of a CFString.
 */
static inline CFIndex
od_cfstrlen(CFStringRef cfstr)
{
	return (CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr),
	    kCFStringEncodingUTF8));
}

/*
 * Given a CFString and its C-string length, copy it to a buffer.
 */
static inline Boolean
od_cfstrlcpy(char *string, CFStringRef cfstr, size_t size)
{
	return (CFStringGetCString(cfstr, string, (CFIndex)size,
	    kCFStringEncodingUTF8));
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

	length = od_cfstrlen(cfstr);
	string = malloc(length + 1);
	if (string == NULL)
		return (NULL);
	if (!od_cfstrlcpy(string, cfstr, length + 1)) {
		free(string);
		return (NULL);
	}
	return (string);
}


char *
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

/*ARGSUSED*/
void
init_od(__unused char **stack, __unused char ***stkptr)
{
}

/*ARGSUSED*/
int
getmapent_od(const char *key, const char *map, struct mapline *ml,
    __unused char **stack, __unused char ***stkptr,
    bool_t *iswildcard, __unused bool_t isrestricted)
{
	char *od_line = NULL;
	char *lp;
	int od_len;
	size_t len;
	int nserr;

	if (trace > 1)
		trace_prt(1, "getmapent_od called\n");

	if (trace > 1) {
		trace_prt(1, "getmapent_od: key=[ %s ]\n", key);
	}

	if (iswildcard)
		*iswildcard = FALSE;
	nserr = od_match(map, key, &od_line, &od_len);
	if (nserr) {
		if (nserr == __NSW_NOTFOUND) {
			/* Try the default entry "*" */
			if ((nserr = od_match(map, "*", &od_line,
			    &od_len)))
				goto done;
			else {
				if (iswildcard)
					*iswildcard = TRUE;
			}
		} else
			goto done;
	}

	/*
	 * at this point we are sure that od_match
	 * succeeded so massage the entry by
	 * 1. ignoring # and beyond
	 * 2. trim the trailing whitespace
	 */
	if ((lp = strchr(od_line, '#')) != NULL)
		*lp = '\0';
	len = strlen(od_line);
	if (len == 0) {
		nserr = __NSW_NOTFOUND;
		goto done;
	}
	lp = &od_line[len - 1];
	while (lp > od_line && isspace(*lp))
		*lp-- = '\0';
	if (lp == od_line) {
		nserr = __NSW_NOTFOUND;
		goto done;
	}
	(void) strncpy(ml->linebuf, od_line, LINESZ);
	unquote(ml->linebuf, ml->lineqbuf);
	nserr = __NSW_SUCCESS;
done:
	if (od_line)
		free((char *)od_line);

	if (trace > 1)
		trace_prt(1, "getmapent_od: exiting ...\n");

	return (nserr);
}

static callback_ret_t
match_callback(CFStringRef key, CFStringRef value, void *udata)
{
	char *key_str, *value_str;
	CFIndex value_len;
	struct match_cbdata *temp = (struct match_cbdata *)udata;
	char **od_line = temp->od_line;
	int *od_len = temp->od_len;

	if (trace > 1) {
		key_str = od_CFStringtoCString(key);
		value_str = od_CFStringtoCString(value);
		if (key_str != NULL && value_str != NULL) {
			trace_prt(1, "  match_callback called: key %s, value %s\n",
			    key_str, value_str);
		}
		free(value_str);
		free(key_str);
	}

	/*
	 * value contains a list of mount options AND mount locations
	 * for a particular mount point (key).
	 * For example:
	 *
	 * key: /work
	 *	^^^^^
	 *	(mount point)
	 *
	 * value:
	 *	        -rw,intr,nosuid,noquota hosta:/export/work
	 *		^^^^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^
	 *		(    mount options    ) (remote mount location)
	 *
	 */
	value_len = od_cfstrlen(value);
	*od_len = (int)(value_len + 1);

	/*
	 * so check for the length; it should be less than
	 * LINESZ
	 */
	if (*od_len > LINESZ) {
		key_str = od_CFStringtoCString(key);
		pr_msg(
		    "Open Directory map %s, entry for %s"
		    " is too long %d chars (max %d)",
		    temp->map, key_str, *od_len, LINESZ);
		free(key_str);
		return (OD_CB_REJECTED);
	}
	*od_line = (char *)malloc(*od_len);
	if (*od_line == NULL) {
		pr_msg("match_callback: malloc failed");
		return (OD_CB_ERROR);
	}

	if (!od_cfstrlcpy(*od_line, value, *od_len)) {
		key_str = od_CFStringtoCString(key);
		pr_msg("match_callback: can't get line for %s", key_str);
		free(key_str);
		free(*od_line);
		return (OD_CB_ERROR);
	}

	if (trace > 1)
		trace_prt(1, "  match_callback: found: %s\n", *od_line);

	return (OD_CB_DONE);
}

static int
od_match(const char *map, const char *key, char **od_line, int *od_len)
{
	int ret;
	char *pattern;
	struct match_cbdata cbdata;

	if (trace > 1) {
		trace_prt(1, "od_match called\n");
		trace_prt(1, "od_match: key =[ %s ]\n", key);
	}

	/* Construct the string value to search for. */
	if (asprintf(&pattern, "%s,automountMapName=%s", key, map) == -1) {
		pr_msg("od_match: malloc failed");
		ret = __NSW_UNAVAIL;
		goto done;
	}

	if (trace > 1)
		trace_prt(1, "  od_match: Searching for %s\n", pattern);

	cbdata.map = map;
	cbdata.key = key;
	cbdata.od_line = od_line;
	cbdata.od_len = od_len;
	ret = od_search(kODAttributeTypeRecordName, pattern, match_callback,
	    (void *) &cbdata);
	free(pattern);
			
	if (trace > 1) {
		if (ret == __NSW_NOTFOUND)
			trace_prt(1, "  od_match: no entries found\n");
		else if (ret != __NSW_UNAVAIL)
			trace_prt(1,
			    "  od_match: od_search FAILED: %d\n", ret);
		else
			trace_prt(1, "  od_match: od_search OK\n");
	}

	if (verbose) {
		if (ret == __NSW_NOTFOUND)
			pr_msg("od_search failed");
	}

done:
	return (ret);
}

int
loadmaster_od(char *mapname, char *defopts, char **stack, char ***stkptr)
{
	int res;
	struct loadmaster_cbdata master_cbdata;

	if (trace > 1)
		trace_prt(1, "loadmaster_od called\n");

	master_cbdata.defopts = defopts;
	master_cbdata.stack = stack;
	master_cbdata.stkptr = stkptr;

	if (trace > 1)
		trace_prt(1, "loadmaster_od: Requesting list in %s\n",
		    mapname);

	res = od_search(kODAttributeTypeMetaAutomountMap, mapname,
	    mastermap_callback, (void *) &master_cbdata);

	if (trace > 1)
		trace_prt(1,
			"loadmaster_od: od_search just returned: %d\n",
			res);

	return (res);
}

int
loaddirect_od(char *nsmap, char *localmap, char *opts,
    char **stack, char ***stkptr)
{
	struct loaddirect_cbdata direct_cbdata;

	if (trace > 1) {
		trace_prt(1, "loaddirect_od called\n");
	}

	if (trace > 1)
		trace_prt(1, "loaddirect_od: Requesting list for %s in %s\n",
		    localmap, nsmap);

	direct_cbdata.opts = opts;
	direct_cbdata.localmap = localmap;
	direct_cbdata.stack = stack;
	direct_cbdata.stkptr = stkptr;
	return (od_search(kODAttributeTypeMetaAutomountMap, nsmap,
	    directmap_callback, (void *) &direct_cbdata));
}

static callback_ret_t
mastermap_callback(CFStringRef key, CFStringRef invalue, void *udata)
{
	char *key_str, *value_str, *value;
	CFIndex key_len, value_len;
	char *pmap, *opts;
	char dir[LINESZ], map[LINESZ], qbuff[LINESZ];
	struct loadmaster_cbdata *temp = (struct loadmaster_cbdata *)udata;
	char *defopts = temp->defopts;
	char **stack = temp->stack;
	char ***stkptr = temp->stkptr;
	CFIndex i;

	if (trace > 1) {
		key_str = od_CFStringtoCString(key);
		value_str = od_CFStringtoCString(invalue);
		if (key_str != NULL && value_str != NULL) {
			trace_prt(1, "  mastermap_callback called: key %s, value %s\n",
			    key_str, value_str);
		}
		free(value_str);
		free(key_str);
	}

	key_len = od_cfstrlen(key);
	value_len = od_cfstrlen(invalue);
	if (key_len >= LINESZ || value_len >= LINESZ)
		return (OD_CB_KEEPGOING);
	if (key_len < 2 || value_len < 2)
		return (OD_CB_KEEPGOING);

	value_str = od_CFStringtoCString(invalue);
	value = value_str;
	i = value_len;
	while (i > 0 && isspace((unsigned char)*value)) {
		value++;
		i--;
	}
	if (*value == '\0') {
		free(value_str);
		return (OD_CB_KEEPGOING);
	}
	if (!od_cfstrlcpy(dir, key, key_len)) {
		free(value_str);
		return (OD_CB_KEEPGOING);
	}
	if (isspace((unsigned char)dir[0]) || dir[0] == '#') {
		free(value_str);
		return (OD_CB_KEEPGOING);
	}

	if (trace > 1)
		trace_prt(1, "mastermap_callback: dir= [ %s ]\n", dir);
	for (i = 0; i < LINESZ; i++)
		qbuff[i] = ' ';
	switch (macro_expand("", dir, qbuff, sizeof (dir))) {

	case MEXPAND_OK:
		break;

	case MEXPAND_LINE_TOO_LONG:
		pr_msg(
		    "%s in Open Directory map: entry too long (max %zu chars)",
		    dir, sizeof (dir) - 1);
		free(value_str);
		return (OD_CB_KEEPGOING);

	case MEXPAND_VARNAME_TOO_LONG:
		pr_msg(
		    "%s in Open Directory map: variable name too long",
		    dir);
		free(value_str);
		return (OD_CB_KEEPGOING);
	}
	strlcpy(map, value, sizeof (map));	/* we know this will not truncate */
	free(value_str);
	if (trace > 1)
		trace_prt(1, "mastermap_callback: map= [ %s ]\n", map);
	switch (macro_expand("", map, qbuff, sizeof (map))) {

	case MEXPAND_OK:
		break;

	case MEXPAND_LINE_TOO_LONG:
		pr_msg(
		    "%s in Open Directory map: entry too long (max %zu chars)",
		    map, sizeof (map) - 1);
		return (OD_CB_KEEPGOING);

	case MEXPAND_VARNAME_TOO_LONG:
		pr_msg(
		    "%s in Open Directory map: variable name too long",
		    map);
		return (OD_CB_KEEPGOING);
	}
	pmap = map;
	while (*pmap && isspace(*pmap))
		pmap++;		/* skip blanks in front of map */
	opts = pmap;
	while (*opts && !isspace(*opts))
		opts++;
	if (*opts) {
		*opts++ = '\0';
		while (*opts && isspace(*opts))
			opts++;
		if (*opts == '-')
			opts++;
			else
			opts = defopts;
	}
	/*
	 * Check for no embedded blanks.
	 */
	if (strcspn(opts, " \t") == strlen(opts)) {
		if (trace > 1)
			trace_prt(1,
			"mastermap_callback: dir=[ %s ], pmap=[ %s ]\n",
			    dir, pmap);
		dirinit(dir, pmap, opts, 0, stack, stkptr);
	} else {
		/* XXX - this was "dn=" for LDAP; is that the server name? */
		pr_msg(
	"Warning: invalid entry for %s in Open Directory ignored.\n",
		    dir);
	}
	if (trace > 1)
		trace_prt(1, "mastermap_callback exiting...\n");
	return (OD_CB_KEEPGOING);
}

static callback_ret_t
directmap_callback(CFStringRef key, __unused CFStringRef value, void *udata)
{
	char *str;
	CFIndex key_len;
	char dir[MAXFILENAMELEN+1];
	struct loaddirect_cbdata *temp = (struct loaddirect_cbdata *)udata;
	char *opts = temp->opts;
	char *localmap = temp->localmap;
	char **stack = temp->stack;
	char ***stkptr = temp->stkptr;

	if (trace > 1) {
		str = od_CFStringtoCString(key);
		if (str != NULL) {
			trace_prt(1, "  directmap_callback called: key %s\n",
			    str);
			free(str);
		}
	}

	key_len = od_cfstrlen(key);
	if (key_len > (CFIndex)MAXFILENAMELEN || key_len < 2)
		return (OD_CB_KEEPGOING);

	if (!od_cfstrlcpy(dir, key, key_len))
		return (OD_CB_KEEPGOING);
	if (isspace((unsigned char)dir[0]) || dir[0] == '#')
		return (OD_CB_KEEPGOING);	/* ignore blank lines and comments */

	dirinit(dir, localmap, opts, 1, stack, stkptr);

	return (OD_CB_KEEPGOING);
}

int
getmapkeys_od(char *nsmap, struct dir_entry **list, int *error,
    int *cache_time, __unused char **stack, __unused char ***stkptr)
{
	int res;
	struct dir_cbdata readdir_cbdata;

	if (trace > 1)
		trace_prt(1, "getmapkeys_od called\n");

	*cache_time = RDDIR_CACHE_TIME;
	*error = 0;

	if (trace > 1)
		trace_prt(1, "getmapkeys_od: Requesting list in %s\n",
		    nsmap);

	readdir_cbdata.list = list;
	readdir_cbdata.last = NULL;
	readdir_cbdata.error = 0;
	res = od_search(kODAttributeTypeMetaAutomountMap, nsmap,
	    readdir_callback, (void *) &readdir_cbdata);

	if (trace > 1)
		trace_prt(1, "  getmapkeys_od: od_search returned %d\n",
			res);

	if (readdir_cbdata.error)
		*error = readdir_cbdata.error;

	if (res != __NSW_SUCCESS) {
		if (*error == 0)
			*error = EIO;
	}

	return (res);
}

static callback_ret_t
readdir_callback(CFStringRef inkey, CFStringRef value, void *udata)
{
	char *str;
	CFIndex inkeylen, value_len;
	struct dir_cbdata *temp = (struct dir_cbdata *)udata;
	struct dir_entry **list = temp->list;
	struct dir_entry *last = temp->last;
	char key[MAXFILENAMELEN+1];
	char linebuf[LINESZ], lineqbuf[LINESZ];
	int error;

	if (trace > 1) {
		str = od_CFStringtoCString(inkey);
		if (str != NULL) {
			trace_prt(1, "  readdir_callback called: key %s\n",
			    str);
			free(str);
		}
	}

	inkeylen = od_cfstrlen(inkey);
	if (inkeylen > (CFIndex)MAXFILENAMELEN)
		return (OD_CB_KEEPGOING);

	if (inkeylen == 0)
		return (OD_CB_KEEPGOING);	/* ignore empty lines */
	if (!od_cfstrlcpy(key, inkey, inkeylen))
		return (OD_CB_KEEPGOING);
	if (isspace((unsigned char)key[0]) || key[0] == '#')
		return (OD_CB_KEEPGOING);	/* ignore blank lines and comments */

	/*
	 * Wildcard entry should be ignored - following entries should continue
	 * to be read to corroborate with the way we search for entries in
	 * LDAP, i.e., first for an exact key match and then a wildcard
	 * if there's no exact key match.
	 */
	if (key[0] == '*' && key[1] == '\0')
		return (OD_CB_KEEPGOING);

	value_len = od_cfstrlen(value);
	if (value_len >= LINESZ)
		return (OD_CB_KEEPGOING);
	if (value_len < 2)
		return (OD_CB_KEEPGOING);

	if (!od_cfstrlcpy(linebuf, value, value_len))
		return (OD_CB_KEEPGOING);
	unquote(linebuf, lineqbuf);
	error = add_dir_entry(key, linebuf, lineqbuf, list, &last);
	if (error != -1) {
		if (error != 0) {
			temp->error = error;
			return (OD_CB_ERROR);
		}
		temp->last = last;
	}

	if (trace > 1)
		trace_prt(1, "readdir_callback returning OD_CB_KEEPGOING...\n");

	return (OD_CB_KEEPGOING);
}

/*
 * Looks for kODAttributeTypeRecordName and
 * kODAttributeTypeAutomountInformation; if it finds them, it calls
 * the specified callback with that information.
 */
static callback_ret_t
od_process_record_attributes(ODRecordRef record, callback_fn callback,
    void *udata)
{
	CFErrorRef error;
	char *errstring;
	CFArrayRef keys;
	CFStringRef key;
	CFArrayRef values;
	CFStringRef value;
	callback_ret_t ret;

	if (trace > 1) {
		trace_prt(1,
		"od_process_record_attributes entered\n");
	}

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
			pr_msg("od_process_record_attributes: can't get kODAttributeTypeRecordName attribute for record: %s",
			    errstring);
			free(errstring);
			return (OD_CB_ERROR);
		} else {
			/*
			 * We just reject records missing the attributes
			 * we need.
			 */
			pr_msg("od_process_record_attributes: record has no kODAttributeTypeRecordName attribute");
			return (OD_CB_REJECTED);
		}
	}
	if (CFArrayGetCount(keys) == 0) {
		/*
		 * We just reject records missing the attributes
		 * we need.
		 */
		CFRelease(keys);
		pr_msg("od_process_record_attributes: record has no kODAttributeTypeRecordName attribute");
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
			pr_msg("od_process_record_attributes: can't get kODAttributeTypeAutomountInformation attribute for record: %s",
			    errstring);
			free(errstring);
			return (OD_CB_ERROR);
		} else {
			/*
			 * We just reject records missing the attributes
			 * we need.
			 */
			pr_msg("od_process_record_attributes: record has no kODAttributeTypeAutomountInformation attribute");
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
		pr_msg("od_process_record_attributes: record has no kODAttributeTypeRecordName attribute");
		return (OD_CB_REJECTED);
	}
	value = CFArrayGetValueAtIndex(values, 0);

	/*
	 * We have both of the attributes we need.
	 */
	ret = (*callback)(key, value, udata);
	CFRelease(values);
	CFRelease(keys);
	return (ret);
}

/*
 * Fetch all the map records in Open Directory that have a certain attribute
 * that matches a certain value and pass those records to a callback function.
 */
static int
od_search(CFStringRef attr_to_match, char *value_to_match, callback_fn callback,
    void *udata)
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
		return (__NSW_UNAVAIL);
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
		return (__NSW_UNAVAIL);
	}
	attrs = CFArrayCreate(kCFAllocatorDefault,
	    (const void *[2]){kODAttributeTypeRecordName,
	                      kODAttributeTypeAutomountInformation}, 2,
	    &kCFTypeArrayCallBacks);
	if (attrs == NULL) {
		CFRelease(value_to_match_cfstr);
		CFRelease(node_ref);
		pr_msg("od_search: can't make array of attribute types");
		return (__NSW_UNAVAIL);
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
		return (__NSW_UNAVAIL);
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
		return (__NSW_UNAVAIL);
	}

	ret = __NSW_NOTFOUND;	/* we haven't found any records yet */
	num_results = CFArrayGetCount(results);
	for (i = 0; i < num_results; i++) {
		/*
		 * We've found a record.
		 */
		record = (ODRecordRef)CFArrayGetValueAtIndex(results, i);
		callback_ret = od_process_record_attributes(record,
		    callback, udata);
		if (callback_ret == OD_CB_KEEPGOING) {
			/*
			 * We processed one record, but we want
			 * to keep processing records.
			 */
			ret = __NSW_SUCCESS;
		} else if (callback_ret == OD_CB_DONE) {
			/*
			 * We processed one record, and we don't
			 * want to see any more records.
			 */
			ret = __NSW_SUCCESS;
			break;
		} else if (callback_ret == OD_CB_ERROR) {
			/*
			 * Fatal error - give up.
			 */
			ret = __NSW_UNAVAIL;
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
