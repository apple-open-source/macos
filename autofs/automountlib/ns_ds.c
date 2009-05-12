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
 *	ns_ds.c
 *
 * Based on ns_ldap.c
 *
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007-2009 Apple Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

#include <DirectoryService/DirectoryService.h>

#include "automount.h"
#include "automount_ds.h"

/*
 * Callback routines for ds_process_record_attributes().
 */
typedef int (*attr_callback_fn)(char *attrval, unsigned long attrval_len,
    void *udata);

/*
 * Callback routines for ds_search().
 */
typedef enum {
	DS_CB_KEEPGOING,		/* continue the search */
	DS_CB_DONE,			/* we're done with the search */
	DS_CB_REJECTED,			/* this record had a problem - keep going */
	DS_CB_ERROR			/* error - quit and return __NSW_UNAVAIL */
} callback_ret_t;
typedef callback_ret_t (*callback_fn)(char *key, unsigned long key_len,
    char *contents, unsigned long contents_len, void *udata);

static callback_ret_t mastermap_callback(char *key, unsigned long key_len,
    char *contents, unsigned long contents_len, void *udata);
static callback_ret_t directmap_callback(char *key, unsigned long key_len,
    char *contents, unsigned long contents_len, void *udata);
static callback_ret_t match_callback(char *key, unsigned long key_len,
    char *contents, unsigned long contents_len, void *udata);
static callback_ret_t readdir_callback(char *key, unsigned long key_len,
    char *contents, unsigned long contents_len, void *udata);

struct match_cbdata {
	char *map;
	char *key;
	char **ds_line;
	int *ds_len;
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

static int ds_match(char *map, char *key, char **ds_line, int *ds_len);
static int ds_search(char *attr_to_match, char *value_to_match,
    callback_fn callback, void *udata);

/*ARGSUSED*/
void
init_ds(__unused char **stack, __unused char ***stkptr)
{
}

/*ARGSUSED*/
int
getmapent_ds(char *key, char *map, struct mapline *ml,
    __unused char **stack, __unused char ***stkptr,
    bool_t *iswildcard, __unused bool_t isrestricted)
{
	char *ds_line = NULL;
	char *lp;
	int ds_len, len;
	int nserr;

	if (trace > 1)
		trace_prt(1, "getmapent_ds called\n");

	if (trace > 1) {
		trace_prt(1, "getmapent_ds: key=[ %s ]\n", key);
	}

	if (iswildcard)
		*iswildcard = FALSE;
	nserr = ds_match(map, key, &ds_line, &ds_len);
	if (nserr) {
		if (nserr == __NSW_NOTFOUND) {
			/* Try the default entry "*" */
			if ((nserr = ds_match(map, "*", &ds_line,
			    &ds_len)))
				goto done;
			else {
				if (iswildcard)
					*iswildcard = TRUE;
			}
		} else
			goto done;
	}

	/*
	 * at this point we are sure that ds_match
	 * succeeded so massage the entry by
	 * 1. ignoring # and beyond
	 * 2. trim the trailing whitespace
	 */
	if ((lp = strchr(ds_line, '#')) != NULL)
		*lp = '\0';
	len = strlen(ds_line);
	if (len == 0) {
		nserr = __NSW_NOTFOUND;
		goto done;
	}
	lp = &ds_line[len - 1];
	while (lp > ds_line && isspace(*lp))
		*lp-- = '\0';
	if (lp == ds_line) {
		nserr = __NSW_NOTFOUND;
		goto done;
	}
	(void) strncpy(ml->linebuf, ds_line, LINESZ);
	unquote(ml->linebuf, ml->lineqbuf);
	nserr = __NSW_SUCCESS;
done:
	if (ds_line)
		free((char *)ds_line);

	if (trace > 1)
		trace_prt(1, "getmapent_ds: exiting ...\n");

	return (nserr);
}

static callback_ret_t
match_callback(char *key, unsigned long key_len, char *contents,
    unsigned long contents_len, void *udata)
{
	struct match_cbdata *temp = (struct match_cbdata *)udata;
	char **ds_line = temp->ds_line;
	int *ds_len = temp->ds_len;

	if (trace > 1)
		trace_prt(1, "  match_callback called: key %.*s, contents %.*s\n",
		    (int)key_len, key, (int)contents_len, contents);

	/*
	 * contents contains a list of mount options AND mount locations
	 * for a particular mount point (key).
	 * For example:
	 *
	 * key: /work
	 *	^^^^^
	 *	(mount point)
	 *
	 * contents:
	 *	        -rw,intr,nosuid,noquota hosta:/export/work
	 *		^^^^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^
	 *		(    mount options    ) (remote mount location)
	 *
	 */
	*ds_len = contents_len + 1;

	/*
	 * so check for the length; it should be less than
	 * LINESZ
	 */
	if (*ds_len > LINESZ) {
		pr_msg(
		    "DS map %s, entry for %.*s"
		    " is too long %d chars (max %d)",
		    temp->map, (int)key_len, key, *ds_len, LINESZ);
		return (DS_CB_REJECTED);
	}
	*ds_line = (char *)malloc(*ds_len);
	if (*ds_line == NULL) {
		pr_msg("match_callback: malloc failed");
		return (DS_CB_ERROR);
	}

	(void) memcpy(*ds_line, contents, contents_len);
	(*ds_line)[contents_len] = '\0';

	if (trace > 1)
		trace_prt(1, "  match_callback: found: %s\n", *ds_line);

	return (DS_CB_DONE);
}

static int
ds_match(char *map, char *key, char **ds_line, int *ds_len)
{
	int ret;
	char *pattern;
	struct match_cbdata cbdata;

	if (trace > 1) {
		trace_prt(1, "ds_match called\n");
		trace_prt(1, "ds_match: key =[ %s ]\n", key);
	}

	/* Construct the string value to search for. */
	if (asprintf(&pattern, "%s,automountMapName=%s", key, map) == -1) {
		pr_msg("ds_match: malloc failed");
		ret = __NSW_UNAVAIL;
		goto done;
	}

	if (trace > 1)
		trace_prt(1, "  ds_match: Searching for %s\n", pattern);

	cbdata.map = map;
	cbdata.key = key;
	cbdata.ds_line = ds_line;
	cbdata.ds_len = ds_len;
	ret = ds_search(kDSNAttrRecordName, pattern, match_callback,
	    (void *) &cbdata);
	free(pattern);
			
	if (trace > 1) {
		if (ret == __NSW_NOTFOUND)
			trace_prt(1, "  ds_match: no entries found\n");
		else if (ret != __NSW_UNAVAIL)
			trace_prt(1,
			    "  ds_match: ds_search FAILED\n", ret);
		else
			trace_prt(1, "  ds_match: ds_search OK\n");
	}

	if (verbose) {
		if (ret == __NSW_NOTFOUND)
			pr_msg("ds_search failed");
	}

done:
	return (ret);
}

int
loadmaster_ds(char *mapname, char *defopts, char **stack, char ***stkptr)
{
	int res;
	struct loadmaster_cbdata master_cbdata;

	if (trace > 1)
		trace_prt(1, "loadmaster_ds called\n");

	master_cbdata.defopts = defopts;
	master_cbdata.stack = stack;
	master_cbdata.stkptr = stkptr;

	if (trace > 1)
		trace_prt(1, "loadmaster_ds: Requesting list in %s\n",
		    mapname);

	res = ds_search(kDS1AttrMetaAutomountMap, mapname,
	    mastermap_callback, (void *) &master_cbdata);

	if (trace > 1)
		trace_prt(1,
			"loadmaster_ds: ds_search just returned: %d\n",
			res);

	return (res);
}

int
loaddirect_ds(char *nsmap, char *localmap, char *opts,
    char **stack, char ***stkptr)
{
	struct loaddirect_cbdata direct_cbdata;

	if (trace > 1) {
		trace_prt(1, "loaddirect_ds called\n");
	}

	if (trace > 1)
		trace_prt(1, "loaddirect_ds: Requesting list for %s in %s\n",
		    localmap, nsmap);

	direct_cbdata.opts = opts;
	direct_cbdata.localmap = localmap;
	direct_cbdata.stack = stack;
	direct_cbdata.stkptr = stkptr;
	return (ds_search(kDS1AttrMetaAutomountMap, nsmap,
	    directmap_callback, (void *) &direct_cbdata));
}

static callback_ret_t
mastermap_callback(char *key, unsigned long key_len, char *contents,
    unsigned long contents_len, void *udata)
{
	char *pmap, *opts;
	char dir[LINESZ], map[LINESZ], qbuff[LINESZ];
	struct loadmaster_cbdata *temp = (struct loadmaster_cbdata *)udata;
	char *defopts = temp->defopts;
	char **stack = temp->stack;
	char ***stkptr = temp->stkptr;
	int i;

	if (trace > 1)
		trace_prt(1, "  mastermap_callback called: key %.*s, contents %.*s\n",
		    (int)key_len, key, (int)contents_len, contents);

	if (key_len >= LINESZ || contents_len >= LINESZ)
		return (DS_CB_KEEPGOING);
	if (key_len < 2 || contents_len < 2)
		return (DS_CB_KEEPGOING);

	i = contents_len;
	while (i > 0 && isspace((unsigned char)*contents)) {
		contents++;
		i--;
	}
	if (*contents == '\0')
		return (DS_CB_KEEPGOING);
	if (isspace((unsigned char)*key) || *key == '#')
		return (DS_CB_KEEPGOING);

	(void) strncpy(dir, key, key_len);
	dir[key_len] = '\0';
	if (trace > 1)
		trace_prt(1, "mastermap_callback: dir= [ %s ]\n", dir);
	for (i = 0; i < LINESZ; i++)
		qbuff[i] = ' ';
	if (macro_expand("", dir, qbuff, sizeof (dir))) {
		pr_msg(
		    "%s in Directory Services map: entry too long (max %d chars)",
		    dir, sizeof (dir) - 1);
		return (DS_CB_KEEPGOING);
	}
	(void) strncpy(map, contents, contents_len);
	map[contents_len] = '\0';
	if (trace > 1)
		trace_prt(1, "mastermap_callback: map= [ %s ]\n", map);
	if (macro_expand("", map, qbuff, sizeof (map))) {
		pr_msg(
		    "%s in Directory Services map: entry too long (max %d chars)",
		    map, sizeof (map) - 1);
		return (DS_CB_KEEPGOING);
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
	"Warning: invalid entry for %s in Directory Services ignored.\n",
		    dir);
	}
	if (trace > 1)
		trace_prt(1, "mastermap_callback exiting...\n");
	return (DS_CB_KEEPGOING);
}

static callback_ret_t
directmap_callback(char *key, unsigned long key_len, __unused char *contents,
    __unused unsigned long contents_len, void *udata)
{
	char dir[MAXFILENAMELEN+1];
	struct loaddirect_cbdata *temp = (struct loaddirect_cbdata *)udata;
	char *opts = temp->opts;
	char *localmap = temp->localmap;
	char **stack = temp->stack;
	char ***stkptr = temp->stkptr;

	if (trace > 1)
		trace_prt(1, "  directmap_callback called: key %.*s\n",
		    (int)key_len, key);

	if (key_len > MAXFILENAMELEN || key_len < 2)
		return (DS_CB_KEEPGOING);

	if (isspace((unsigned char)*key) || *key == '#')
		return (DS_CB_KEEPGOING);

	(void) strncpy(dir, key, key_len);
	dir[key_len] = '\0';

	dirinit(dir, localmap, opts, 1, stack, stkptr);

	return (DS_CB_KEEPGOING);
}

int
getmapkeys_ds(char *nsmap, struct dir_entry **list, int *error,
    int *cache_time, __unused char **stack, __unused char ***stkptr)
{
	int res;
	struct dir_cbdata readdir_cbdata;

	if (trace > 1)
		trace_prt(1, "getmapkeys_ds called\n");

	*cache_time = RDDIR_CACHE_TIME;
	*error = 0;

	if (trace > 1)
		trace_prt(1, "getmapkeys_ds: Requesting list in %s\n",
		    nsmap);

	readdir_cbdata.list = list;
	readdir_cbdata.last = NULL;
	res = ds_search(kDS1AttrMetaAutomountMap, nsmap, readdir_callback,
	    (void *) &readdir_cbdata);

	if (trace > 1)
		trace_prt(1, "  getmapkeys_ds: ds_search returned %d\n",
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
readdir_callback(char *inkey, unsigned long inkeylen, __unused char *contents,
    __unused unsigned long contents_len, void *udata)
{
	struct dir_cbdata *temp = (struct dir_cbdata *)udata;
	struct dir_entry **list = temp->list;
	struct dir_entry *last = temp->last;
	char key[MAXFILENAMELEN+1];

	if (trace > 1)
		trace_prt(1, "  readdir_callback called: key %.*s\n",
		    (int)inkeylen, key);

	if (inkeylen > MAXFILENAMELEN)
		return (DS_CB_KEEPGOING);

	if (inkeylen == 0 || isspace((unsigned char)*inkey) || *inkey == '#')
		return (DS_CB_KEEPGOING);

	strncpy(key, inkey, inkeylen);
	key[inkeylen] = '\0';

	/*
	 * Wildcard entry should be ignored - following entries should continue
	 * to be read to corroborate with the way we search for entries in
	 * LDAP, i.e., first for an exact key match and then a wildcard
	 * if there's no exact key match.
	 */
	if (key[0] == '*' && key[1] == '\0')
		return (DS_CB_KEEPGOING);

	if (add_dir_entry(key, list, &last)) {
		temp->error = ENOMEM;
		return (DS_CB_ERROR);
	}

	temp->last = last;
	temp->error = 0;

	if (trace > 1)
		trace_prt(1, "readdir_callback returning DS_CB_KEEPGOING...\n");

	return (DS_CB_KEEPGOING);
}

/*
 * Loops over all attributes in a record, looking for kDSNAttrRecordName
 * and kDSNAttrAutomountInformation; if it finds them, it calls the
 * specified callback with that information.
 */
static callback_ret_t
ds_process_record_attributes(tDirReference session, tDirNodeReference node_ref,
    tDataBufferPtr buffer, tAttributeListRef attr_list_ref,
    tRecordEntryPtr entry, callback_fn callback, void *udata)
{
	unsigned long i;
	tAttributeValueListRef key_value_list_ref = 0;
	tAttributeEntry *key_attr_entry_p = NULL;
	tAttributeValueEntry *key_value_entry_p = NULL;
	char *key;
	unsigned long key_len;
	tAttributeValueListRef contents_value_list_ref = 0;
	tAttributeEntry *contents_attr_entry_p = NULL;
	tAttributeValueEntry *contents_value_entry_p = NULL;
	char *contents;
	unsigned long contents_len;
	callback_ret_t ret;

	if (trace > 1) {
		trace_prt(1,
		"ds_process_record_attributes: entry->fRecordAttributeCount=[ %d ]\n",
		    entry->fRecordAttributeCount);
	}

	/*
	 * Iterate over all attributes in the record.
	 * Obtain the values of the kDSNAttrRecordName and the
	 * kDSNAttrAutomountInformation attributes and the length of
	 * each value (kDSNAttrRecordName=key, kDSNAttrAutomountInformation=
	 * contents).  We skip the description.
	 *
	 * Even though LDAP allows for multiple values per attribute, we take
	 * only the 1st value for each attribute because the automount data is
	 * organized as such (same as NIS+).
	 */
	for (i = 1; i <= entry->fRecordAttributeCount; i++) {
		tAttributeValueListRef value_list_ref;
		tAttributeEntry *attr_entry_p;
		char *attrname;
		tAttributeValueEntry *value_entry_p;
		tDirStatus status;

		status = dsGetAttributeEntry(node_ref, buffer, attr_list_ref,
		    i, &value_list_ref, &attr_entry_p);
		if (status != eDSNoErr) {
			pr_msg("ds_process_record_attributes: dsGetAttributeEntry failed: %s (%d)",
			    dsCopyDirStatusName(status), status);
			return (DS_CB_ERROR);
		}
		attrname = attr_entry_p->fAttributeSignature.fBufferData;

		if (trace > 1)
			trace_prt(1,
			"ds_process_record_attributes: attrname=[ %s ]\n",
			    attrname);

		if (strcmp(attrname, kDSNAttrRecordName) == 0) {
			/*
			 * We only want the first value for
			 * kDSNAttrRecordName in a record.
			 */
			status = dsGetAttributeValue(node_ref, buffer, 1,
			    value_list_ref, &value_entry_p);
			if (status != eDSNoErr) {
				pr_msg("ds_process_record_attributes: dsGetAttributeValue failed: %s (%d)",
				    dsCopyDirStatusName(status), status);
				dsDeallocAttributeEntry(session, attr_entry_p);
				dsCloseAttributeValueList(value_list_ref);
				return (DS_CB_ERROR);
			}

			key_value_entry_p = value_entry_p;
			key_attr_entry_p = attr_entry_p;
			key_value_list_ref = value_list_ref;
		} else if (strcmp(attrname, kDSNAttrAutomountInformation) == 0) {
			/*
			 * We only want the first value for
			 * kDSNAttrAutomountInformation in a record.
			 */
			status = dsGetAttributeValue(node_ref, buffer, 1,
			    value_list_ref,  &value_entry_p);
			if (status != eDSNoErr) {
				pr_msg("ds_process_record_attributes: dsGetAttributeValue failed: %s (%d)",
				    dsCopyDirStatusName(status), status);
				dsDeallocAttributeEntry(session, attr_entry_p);
				dsCloseAttributeValueList(value_list_ref);
				return (DS_CB_ERROR);
			}

			contents_value_entry_p = value_entry_p;
			contents_attr_entry_p = attr_entry_p;
			contents_value_list_ref = value_list_ref;
		} else {
			dsDeallocAttributeEntry(session, attr_entry_p);
			dsCloseAttributeValueList(value_list_ref);
		}
	}

	if (key_value_entry_p != NULL && contents_value_entry_p != NULL) {
		/*
		 * We have both of the attributes we need.
		 */
		key = key_value_entry_p->fAttributeValueData.fBufferData;
		key_len = key_value_entry_p->fAttributeValueData.fBufferLength;
		contents = contents_value_entry_p->fAttributeValueData.fBufferData;
		contents_len = contents_value_entry_p->fAttributeValueData.fBufferLength;

		ret = (*callback)(key, key_len, contents, contents_len, udata);
	} else {
		/*
		 * We don't have the attributes we need; reject this
		 * record, and keep processing.
		 */
		ret = DS_CB_REJECTED;
	}
	if (key_value_entry_p != NULL) {
		dsDeallocAttributeValueEntry(session, key_value_entry_p);
		dsDeallocAttributeEntry(session, key_attr_entry_p);
		dsCloseAttributeValueList(key_value_list_ref);
	}
	if (contents_value_entry_p != NULL) {
		dsDeallocAttributeValueEntry(session, contents_value_entry_p);
		dsDeallocAttributeEntry(session, contents_attr_entry_p);
		dsCloseAttributeValueList(contents_value_list_ref);
	}
	return (ret);
}

/*
 * Get the root-level node for a search.
 */
int
ds_get_root_level_node(tDirReference session, tDirNodeReference *node_refp)
{
	static unsigned long dir_node_bufsize = 2*1024;
	tDataBufferPtr buffer;
	tDirStatus status;
	unsigned long num_results;
	tContextData context;
	tDataListPtr node_path;

	/*
	 * Get the search node.
	 */
	for (;;) {
		/* Allocate a buffer. */
		buffer = dsDataBufferAllocate(session, dir_node_bufsize);
		if (buffer == NULL) {
			pr_msg("ds_get_search_node: malloc failed");
			return (__NSW_UNAVAIL);
		}

		/* Find the default search node. */
		num_results = 1;	/* "there can be only one" */
		context = NULL;
		status = dsFindDirNodes(session, buffer, NULL,
		    eDSSearchNodeName, &num_results, &context);
		if (status != eDSBufferTooSmall) {
			/* Well, the buffer wasn't too small */
			break;
		}

		/*
		 * The buffer was too small; free the buffer, and try one
		 * twice as big.
		 */
		dsDataBufferDeAllocate(session, buffer);
		dir_node_bufsize = 2*dir_node_bufsize;
	}
	if (status != eDSNoErr) {
		dsDataBufferDeAllocate(session, buffer);
		pr_msg(
		    "ds_get_search_node: can't find default search node: %s (%d)",
		    dsCopyDirStatusName(status), status);
		return (__NSW_UNAVAIL);
	}

	/* Get a reference to that node. */
	status = dsGetDirNodeName(session, buffer, 1, &node_path);
	dsDataBufferDeAllocate(session, buffer);
	if (status != eDSNoErr) {
		pr_msg(
		    "ds_get_search_node: can't get reference to default search node: %s (%d)",
		    dsCopyDirStatusName(status), status);
		return (__NSW_UNAVAIL);
	}

	/* Open root level node for search. */
	status = dsOpenDirNode(session, node_path, node_refp);
	dsDataListDeallocate(session, node_path);
	free(node_path);
	if (status != eDSNoErr) {
		pr_msg(
		    "ds_get_search_node: can't open root level node for search: %s (%d)",
		    dsCopyDirStatusName(status), status);
		return (__NSW_UNAVAIL);
	}
	return (__NSW_SUCCESS);
}

/*
 * Fetch all the map records in DS that have a certain attribute that
 * matches a certain value and pass those records to a callback function.
 */
static int
ds_search(char *attr_to_match, char *value_to_match, callback_fn callback,
    void *udata)
{
	int ret;
	tDirReference session;
	tDirStatus status;
	tDirNodeReference node_ref;
	int dir_node_open = 0;
	tDataNodePtr pattern_data_node = NULL;
	tDataListPtr record_type = NULL;
	tDataNodePtr match_type = NULL;
	tDataListPtr requested_attributes = NULL;
	unsigned long num_results;
	unsigned long i;
	tContextData context;
	tAttributeListRef attr_list_ref;
	tRecordEntryPtr record_entry_p;
	callback_ret_t callback_ret;
	static unsigned long attr_bufsize = 2*1024;
	tDataBufferPtr buffer = NULL;

	/* Open an Open Directory session. */
	if (dsOpenDirService(&session) != eDSNoErr)
		return (__NSW_UNAVAIL);	/* or __NSW_TRYAGAIN? */

	/*
	 * Get the search node.
	 */
	ret = ds_get_root_level_node(session, &node_ref);
	if (ret != __NSW_SUCCESS)
		goto done;
	dir_node_open = 1;

	pattern_data_node = dsDataNodeAllocateString(session, value_to_match);
	if (pattern_data_node == NULL) {
		pr_msg("ds_search: malloc failed");
		ret = __NSW_UNAVAIL;
		goto done;
	}

	/*
	 * Build the tDataList containing the record type that we are
	 * searching for.
	 */
	record_type = dsBuildListFromStrings(session,
	    kDSStdRecordTypeAutomount, NULL);
	if (record_type == NULL) {
		pr_msg(
		    "ds_search: can't build record type list: %s (%d)",
		    dsCopyDirStatusName(status), status);
		ret = __NSW_UNAVAIL;
		goto done;
	}

	/*
	 * Build the tDataNode containing the attribute we'll match
	 * in the search.
	 */
	match_type = dsDataNodeAllocateString(session, attr_to_match);
	if (match_type == NULL) {
		pr_msg("ds_search: malloc failed");
		ret = __NSW_UNAVAIL;
		goto done;
	}

	/*
	 * Build a list to contain the requested attributes of the records
	 * returned from the search.
	 */
	requested_attributes = dsBuildListFromStrings(session,
	    kDSNAttrRecordName, kDSNAttrAutomountInformation, NULL);
	if (requested_attributes == NULL) {
		pr_msg("ds_search: malloc failed");
		ret = __NSW_UNAVAIL;
		goto done;
	}

	ret = __NSW_NOTFOUND;	/* we haven't found any records yet */
	num_results = 0;	/* give me all the records that match */
	context = NULL;
	do {
		/*
		 * We don't know how big a buffer we need; we just keep
		 * growing it.
		 * We remember the last buffer size, so we keep using
		 * that size until it's too small.
		 *
		 * XXX - should we give up at some point?
		 *
		 * XXX - there could be independent references to the
		 * size variable from separate threads, but:
		 *
		 *	if somebody makes it bigger out from under us,
		 *	that's not a problem;
		 *
		 *	if two threads store into it at the same time,
		 *	at worst you lose one doubling, so somebody
		 *	might get a "buffer too small" error later;
		 *
		 * so I don't think it's worth worrying about concurrent
		 * accesses.
		 */
		for (;;) {
			/* Allocate a buffer. */
			buffer = dsDataBufferAllocate(session, attr_bufsize);
			if (buffer == NULL) {
				pr_msg("ds_match: malloc failed");
				ret = __NSW_UNAVAIL;
				goto done;
			}

			status = dsDoAttributeValueSearchWithData(node_ref,
			    buffer, record_type, match_type, eDSExact,
			    pattern_data_node, requested_attributes, FALSE,
			    &num_results, &context);
			if (status != eDSBufferTooSmall) {
				/* Well, the buffer wasn't too small */
				break;
			}

			/*
			 * The buffer was too small; free the buffer, and
			 * try one twice as big.
			 */
			dsDataBufferDeAllocate(session, buffer);
			buffer = NULL;
			attr_bufsize = 2*attr_bufsize;
		}
			
		if (status != eDSNoErr) {
			pr_msg("ds_search: can't get record list: %s (%d)",
			    dsCopyDirStatusName(status), status);
			ret = __NSW_UNAVAIL;	/* XXX - or succeed? */
			goto done;
		}
		for (i = 1; i <= num_results; i++) {
			status = dsGetRecordEntry(node_ref, buffer, i,
			    &attr_list_ref, &record_entry_p);
			if (status != eDSNoErr) {
				pr_msg("ds_search: can't get record entry: %s (%d)",
				    dsCopyDirStatusName(status), status);
				/*
				 * We don't want to see any more records.
				 */
				if (context != NULL) {
					dsReleaseContinueData(session, context);
					context = NULL;
				}
				ret = __NSW_UNAVAIL;	/* XXX - or succeed? */
				break;
			}

			/*
			 * We've found a record.
			 */
			callback_ret = ds_process_record_attributes(session,
			    node_ref, buffer, attr_list_ref, record_entry_p,
			    callback, udata);
			dsCloseAttributeList(attr_list_ref);
			dsDeallocRecordEntry(session, record_entry_p);

			if (callback_ret == DS_CB_KEEPGOING) {
				/*
				 * We processed one record, but we want
				 * to keep processing records.
				 */
				ret = __NSW_SUCCESS;
			} else if (callback_ret == DS_CB_DONE) {
				/*
				 * We processed one record, and we don't
				 * want to see any more records.
				 */
				ret = __NSW_SUCCESS;
				if (context != NULL) {
					dsReleaseContinueData(session, context);
					context = NULL;
				}
				break;
			} else if (callback_ret == DS_CB_ERROR) {
				ret = __NSW_UNAVAIL;
				if (context != NULL) {
					dsReleaseContinueData(session, context);
					context = NULL;
				}
				break;
			}
		}
	} while (context != NULL);

done:
	if (buffer != NULL)
		dsDataBufferDeAllocate(session, buffer);
	if (requested_attributes != NULL) {
		dsDataListDeallocate(session, requested_attributes);
		free(requested_attributes);
	}
	if (match_type != NULL)
		dsDataNodeDeAllocate(session, match_type);
	if (record_type != NULL) {
		dsDataListDeallocate(session, record_type);
		free(record_type);
	}
	if (pattern_data_node != NULL)
		dsDataNodeDeAllocate(session, pattern_data_node);
	if (dir_node_open)
		dsCloseDirNode(node_ref);
	dsCloseDirService(session);
	return (ret);
}
