/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __DSENGINE_H__
#define __DSENGINE_H__

#include <NetInfo/dsrecord.h>
#include <NetInfo/dsstore.h>
#include <NetInfo/dsfilter.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define NameTypeNetInfo			0x0000
#define NameTypeX500			0x0010
#define NameTypeUserName		0x0020
#define NameTypePrincipalName	0x0030
#define NameTypeDirectoryID		0x0040

/* Naming policies. This is deliberately a subset of NameTypes. */
#define DSENGINE_FLAGS_NAMING_MASK    0x00f0
#define DSENGINE_FLAGS_NETINFO_NAMING 0x0000
#define DSENGINE_FLAGS_X500_NAMING    0x0010
#define DSENGINE_FLAGS_POSIX_NAMING   0x0020

#define DSENGINE_FLAGS_DEREFERENCE_IDS  0x0200
#define DSENGINE_FLAGS_NATIVE_AUTHORIZATION 0x0400

typedef struct
{
	dsstore *store;
	Logic3 (*delegate)(dsfilter *, dsrecord *, void *);
	void *private;
} dsengine;

/*
 * Creates and opens a new data store with the given pathname.
 */
dsstatus dsengine_new(dsengine **s, char *name, u_int32_t flags);

/*
 * Opens a server for the data store with the given pathname.
 */
dsstatus dsengine_open(dsengine **s, char *name, u_int32_t flags);

/*
 * Closes a server.
 */
dsstatus dsengine_close(dsengine *s);

dsstatus dsengine_authenticate(dsengine *s, dsdata *user, dsdata *password);

/*
 * Adds a new record as a child of another record (specified by its ID).
 */
dsstatus dsengine_create(dsengine *s, dsrecord *r, u_int32_t dsid);

/*
 * Fetch a record specified by its ID.
 */
dsstatus dsengine_fetch(dsengine *s, u_int32_t dsid, dsrecord **r);

/*
 * Fetch a list of records specified by ID.
 */
dsstatus dsengine_fetch_list(dsengine *s, u_int32_t count, u_int32_t *dsid,
	dsrecord ***l);

/*
 * Save a record.
 */
dsstatus dsengine_save(dsengine *s, dsrecord *r);
dsstatus dsengine_save_fast(dsengine *s, dsrecord *r);

/*
 * Modify a record attribute. 
 */
dsstatus dsengine_save_attribute(dsengine *s, dsrecord *r, dsattribute *a, u_int32_t asel);

/*
 * Remove a record.
 */
dsstatus dsengine_remove(dsengine *s, u_int32_t dsid);

/*
 * Move a record to a new parent.
 */
dsstatus dsengine_move(dsengine *s, u_int32_t dsid, u_int32_t pdsid);

/*
 * Copy a record to a new parent.
 */
dsstatus dsengine_copy(dsengine *s, u_int32_t dsid, u_int32_t pdsid);

/*
 * Search starting at a given record (specified by ID) for all
 * records matching the attributes of a "pattern" record.
 * Scope minimum and maximum specify how far below the given node
 * the seach will be active.  
 * Search with null pattern or null attributes matchs all.
 * Search with null pattern or attributes and minimum scope 1,
 * maximum scope 1 returns all child IDs.
 */
dsstatus dsengine_search_pattern(dsengine *s, u_int32_t dsid, dsrecord *pattern, u_int32_t scopemin, u_int32_t scopemax, u_int32_t **match, u_int32_t *count);

/*
 * Same as search_pattern, but returns ID of first matching record.
 */
dsstatus dsengine_find_pattern(dsengine *s, u_int32_t dsid, dsrecord *pattern, u_int32_t scopemin, u_int32_t scopemax, u_int32_t *match);

/*
 * Search starting at a given record (specified by ID) for all
 * records testing true agains a filter.  Scoping is the same as for 
 * dsengine_search_pattern.
 */
dsstatus dsengine_search_filter(dsengine *s, u_int32_t dsid, dsfilter *f, u_int32_t scopemin, u_int32_t scopemax, u_int32_t **match, u_int32_t *count);

/*
 * Same as search_filter, but returns ID of first matching record.
 */
dsstatus dsengine_find_filter(dsengine *s, u_int32_t dsid, dsfilter *f, u_int32_t scopemin, u_int32_t scopemax, u_int32_t *match);

/*
 * Find the first child record of a given record that has "key=val".
 * If val is NULL, returns the first child with this key.
 * If key is NULL, returns the first child with the given value for any key.
 * If key and val are both NULL, returns first child.
 */
dsstatus dsengine_match(dsengine *s, u_int32_t dsid, dsdata *key, dsdata *val, u_int32_t *match);

/*
 * Returns a list of dsids, representing the path the given record to root.
 * The final dsid in the list will always be 0 (root).
 */
dsstatus dsengine_path(dsengine *s, u_int32_t dsid, u_int32_t **list);

/*
 * Find a record following a list of key=value pairs, which are given as
 * the attributes of a "path" record.
 */
dsstatus dsengine_pathmatch(dsengine *s, u_int32_t dsid, dsrecord *path, u_int32_t *match);

/*
 * Create a path following a list of key=value pairs, which are given as
 * the attributes of a "path" record.  Returns dsid of last directory in the
 * chain of created directories.  Follows existing directories if they exist.
 */
dsstatus dsengine_pathcreate(dsengine *s, u_int32_t dsid, dsrecord *path, u_int32_t *match);

/*
 * Returns a list of dsids and values for a given attribute key.
 * Results are returned in a dsrecord.  Keys are dsids encoded using
 * uint32_to_dsdata().  Values are attribute values for the
 * corresponding record.
 */
dsstatus dsengine_list(dsengine *s, u_int32_t dsid, dsdata *key, u_int32_t scopemin, u_int32_t scopemax, dsrecord **list);

/*
 * Detach a child (specified by its ID) from a record.
 * This just edits the record's list of children, and does not remove the
 * child record from the data store.  You should never do this!  This
 * routine is provided to allow emergency repairs to a corrupt data store.
 */
dsstatus dsengine_detach(dsengine *s, dsrecord *r, u_int32_t dsid);

/*
 * Attach a child (specified by its ID) to a record.
 * This just edits the record's list of children, and does not create the
 * child record in the data store.  You should never do this!  This
 * routine is provided to allow emergency repairs to a corrupt data store.
 */
dsstatus dsengine_attach(dsengine *s, dsrecord *r, u_int32_t dsid);

/*
 * Get a record's parent dsid.
 */
dsstatus dsengine_record_super(dsengine *s, u_int32_t dsid, u_int32_t *super);

/*
 * Get a record's version number.
 */
dsstatus dsengine_record_version(dsengine *s, u_int32_t dsid, u_int32_t *version);

/*
 * Get a record's serial number.
 */
dsstatus dsengine_record_serial(dsengine *s, u_int32_t dsid, u_int32_t *serial);

/*
 * Get the dsid of the record with a given version number.
 */
dsstatus dsengine_version_record(dsengine *s, u_int32_t version, u_int32_t *dsid);

/*
 * Get data store version number.
 */
dsstatus dsengine_version(dsengine *s, u_int32_t *version);

/*
 * Get a record's version number, serial number, and parent's dsid.
 */
dsstatus dsengine_vital_statistics(dsengine *s, u_int32_t dsid, u_int32_t *version, u_int32_t *serial, u_int32_t *super);

/*
 * Sets a record's parent.
 * This just edits the record's parent id, and does not update the
 * parent record in the data store.  You should never do this!  This
 * routine is provided to allow emergency repairs to a corrupt data store.
 */
dsstatus dsengine_set_parent(dsengine *s, dsrecord *, u_int32_t);

/*
 * Returns a string representation of the absolute path of the given
 * record.  Path components are "/" separated.  Path components are
 * the first value of the record's "name" attribute.  If a record has no
 * "name" attribute, or if the "name" attribute has no values, the path
 * component will be "dir:nnn", where nnn is the directory ID.
 */
char *dsengine_netinfo_string_path(dsengine *s, u_int32_t dsid);

/*
 * Locates a directory with the given path.  The path may be a directory ID,
 * or an absolute path.  Path components are either strings of the form
 * "key=val" or "val". In the former case the component will select the
 * first directory with the given value for the given key.  In the latter 
 * the component will match the first directory with the given value for its
 * "name" attribute.  The component "." matches the current directory, and
 * ".." matches the super-directory of the current directory.
 */
dsstatus dsengine_netinfo_string_pathmatch(dsengine *s, u_int32_t dsid, char *path, u_int32_t *match);

dsstatus dsengine_netinfo_string_pathcreate(dsengine *s, u_int32_t dsid, char *path, u_int32_t *match);

/*
 * Returns the distinguished name of the given record. The
 * "rdn" meta-attribute may be set on each traversed
 * record to determine its RDN attribute type.
 */
char *dsengine_x500_string_path(dsengine *s, u_int32_t dsid);

/*
 * Flush cache.
 */
void dsengine_flush_cache(dsengine *);

/*
 * Locates a directory with the given DN. The path may be a
 * directory ID, eg:
 *
 *	DSID=<dsid>,<arbitary DN suffix>
 *
 * or an ordinary X.500 distinguished name. The names
 *
 *	key1=val1,key2=val2,key3=val3
 *	key1=val1;key2=val2;key3=val3
 *
 * are equivalent to the NetInfo path:
 *
 *	/key3=val3/key2=val2/key1=val1
 */
dsstatus dsengine_x500_string_pathmatch(dsengine *s, u_int32_t dsid, char *path, u_int32_t *match);

dsstatus dsengine_x500_string_pathcreate(dsengine *s, u_int32_t dsid, char *path, u_int32_t *match);

/*
 * Convert a name from one type to another _relative to the datastore_.
 * You may need to do more work in order to turn the name into
 * an absoulte name relative to a directory server (for example,
 * add a DN suffix).
 *
 *	NameTypeNetInfo
 *		NetInfo string path, like /users/lukeh
 *	NameTypeX500
 *		X.500 DN, like cn=Luke howard,ou=People
 *	NameTypeUserName
 *		Name attribute of user directory ("lukeh").
 *	NameTypePrincipalName
 *		Principal attribute of user directory ("lukeh@DARWIN.APPLE.COM")
 *		For use with Trusted NetInfo.
 *	NameTypeDirectoryID
 *		Directory ID on the local data store. Data type must
 *		be DataTypeDirectoryID.
 */
dsdata *dsengine_convert_name(dsengine *e, dsdata *name, u_int32_t intype, u_int32_t desiredtype);

/*
 * Same as dsengine_convert_name(), except desiredtype is
 * set to the engine's flags & DSENGINE_FLAGS_NAMING_MASK.
 */
dsdata *dsengine_map_name(dsengine *e, dsdata *name, u_int32_t intype);

/*
 * Sets a delegate for testing filters, to allow the
 * client to override filter matching.
 */
void dsengine_set_filter_test_delegate(dsengine *, Logic3 (*)(dsfilter *, dsrecord *, void *), void *);

#endif __DSENGINE_H__
