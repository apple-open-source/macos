/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are 
 * subject to the Apple Public Source License Version 1.2 (the 'License'). You 
 * may not use this file except in compliance with the License. Please obtain a 
 * copy of the License at http://www.apple.com/publicsource and read it before 
 * using this file.
 * This Original Code and all software distributed under the License are 
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS 
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT 
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, 
 * QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the specific 
 * language governing rights and limitations under the License.
 */

/* 
 * mod_hfs_apple Apache module (enforce casing in URLs which need it)
 *
 * When a <Directory> statement is found in the configuration file (this
 * discussion does not apply if .htaccess files are used instead) then
 * its directory path is supposed to apply to any URL which URI uses
 * that directory. In other words, a <Directory> statement usually
 * defines some restrictions and any URL that goes to the targeted
 * directory (or its sub-directories) should "follow" those restrictions.
 *
 * On UFS volumes, since the file system is case-sensitive, a URI must
 * always match the actual path, in order for the file to be fetched. Any
 * <Directory> statement will consequently be enforced. Because if there
 * is a case-mismatch a file-not-found error will be returned and if 
 * there is no case-mismatch then relevant <Directory> statements will 
 * be walked through while parsing the URI.
 *
 * On HFS volumes, since the file system is case-insensitive, a URI may
 * not always case-match the actual path to the file that needs to be 
 * fetched. That means that <Directory> statements may not be walked
 * through if a case-mismatch appears in the URI (or in the statement)
 * in regards to the actual path stored on disk. Consequently, some
 * restrictive statements may be missed but the target file may still be 
 * returned as response. In this situation we have a problem: to solve
 * it we should refuse such URL that case-mismatches part of the path
 * which, if not miscased, would actually make a <Directory> statement
 * currently configured applies.
 *
 * That is what this modules does. Consequently, when this module is
 * installed, some "pseudo-case-sensitivity" is enforced when Apache 
 * deals with HFS volumes.
 *
 * 13-JUN-2001	[JFA, Apple Computer, Inc.]
 *		Initial version for Mac OS X Server 10.0.
 */

#ifdef DARWIN

#define CORE_PRIVATE
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_request.h"
#include "http_protocol.h"
#include "http_log.h"
#include "http_main.h"
#include "util_script.h"
#include "http_conf_globals.h"

#define __MACHINEEXCEPTIONS__
#define __DRIVERSERVICES__
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#include <sys/param.h>
#include <sys/mount.h>


module MODULE_VAR_EXPORT hfs_apple_module;

/*
 *	Support routine: check_file_system
 *	Check if the file system is case-sensitive assuming they all are 
 *	case-sensitive except HFS volumes. Return a Mac error. 
 *	The input path may not necessarily exist but its volume MUST. 
 *	This routine walks the path up if the specified file or directory 
 *	is not found. 
 */
static int check_file_system(char *path, int *case_sensitive)
{
	int					len;
	/*OSStatus			err;*/
	int 				err;
	char				*unixPath;
	struct statfs		fsInfo;
	#define VOLUME		"/Volumes"

	*case_sensitive = 1;
	len = (int) strlen(path);
	unixPath = malloc(len + 1);
	if (unixPath == NULL) return (int) memFullErr;
	strcpy(unixPath, path);
// removing so I can change the way this is done	
#if 0	
	do {
		FSRef			fsRef;
		Boolean			isDirectory;
		FSSpec			fsSpec;
		FSVolumeRefNum	actualVRefNum;
		FSVolumeInfo 	vInfo;
		
		/* Get rid of trailing slash unless that's all what's there */
		if (len > 1 && unixPath[len - 1] == '/') unixPath[--len] = 0;
		if (*unixPath == 0 || strcasecmp(unixPath, VOLUME) == 0) {
			err = (OSStatus) fnfErr;
			break;
		}
		
		/* Try to convert path to a 'FSRef' */
		err = FSPathMakeRef((UInt8*) unixPath, &fsRef, &isDirectory);
		if (err != (OSStatus) noErr) {
		
			/* Walk the path up: remove last item until slash */
			len--;
			while (len > 0 && unixPath[len] != '/') len--;
			if (len == 0 && unixPath[0] == '/') len++;
			unixPath[len] = 0;
			continue;
		}
		
		/* Convert the 'FSRef' (an existing path) to a 'FSSpec' */
		err = (OSStatus) FSGetCatalogInfo(
			&fsRef, kFSCatInfoNone, NULL, NULL, &fsSpec, NULL);
		if (err != (OSStatus) noErr) break;

		/* Query volume information */
		err = (OSStatus) FSGetVolumeInfo(
			(FSVolumeRefNum) fsSpec.vRefNum, (ItemCount) 0,
			(FSVolumeRefNum*) &actualVRefNum, kFSVolInfoFSInfo,
			&vInfo, NULL, NULL);
		if (err != (OSStatus) noErr) break;
		
		/* Parse result */
		if (vInfo.signature == kHFSSigWord || 
			vInfo.signature == kHFSPlusSigWord) {
			*case_sensitive = 0;
		}
		free(unixPath);
		return (int) noErr;
		
	} while (1);
#endif
	
	err = statfs(unixPath, &fsInfo);
	
	/* Now check the type... */
	if (-1 == err)
	{
		#if DEBUG
		perror("statfs failed because");
		#endif
	}
	else
	{
		*case_sensitive = strcmp(fsInfo.f_fstypename, "hfs");
	}

	free(unixPath);
	return (int) err;
}

/*
 *	Our core data structure: each entry in the table is composed
 *	of a key (the path of a <Directory> statement, no matter what
 *	server it applies to) and a value that tells whether its
 *	volume is HFS or not (case-sensitive=0 or 1). Unfortunately
 *	the work required to fill this table will be repeated for 
 *	each Apache child process (but there is nothing new here!)
 */
static pool *g_pool = NULL;
static array_header *directories = NULL;

typedef struct dir_rec {
	char	*dir_path;
	int		case_sens;	
} dir_rec;

/*
 *	Support routine that populates our table of directories
 *	to be considered. We ignore what server configuration is 
 *	attached to the directory because it does not matter.
 */
static void add_directory_entry(request_rec *r, char *path) {
	char *dir_path;
	int i,case_sens = 0;
	dir_rec **elt;

	/* Make sure input path has a trailing slash */
	if (path[strlen(path) - 1] != '/') {
		dir_path = ap_pstrcat(g_pool, path, "/", NULL);
	} else {
		dir_path = ap_pstrdup(g_pool, path);
	}
	
	/* If the entry already exists then get out */
	for (i = 0; i < directories->nelts; i++) {
		dir_rec *entry = ((dir_rec**) directories->elts)[i];
		if (strcmp(dir_path, entry->dir_path) == 0) return;
	}
	
	/* Figure whether the targeted volume is case-sensitive */
	if (check_file_system(path, &case_sens) != 0) {
		case_sens = 0;
	}
	
	/* Add new entry to the table (ignore errors) */
	elt = ap_push_array(directories);
	*elt = (dir_rec*) ap_pcalloc(g_pool, sizeof(dir_rec));
	if (*elt == NULL) return;
	(*elt)->dir_path = ap_pstrdup(g_pool, dir_path); 
	(*elt)->case_sens = case_sens;

	/* Print a debug notice */
	#if DEBUG & 0
	ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, r,
		"%ld: %s is %s", (long) getpid(),
		(*elt)->dir_path, (*elt)->case_sens ? "UFS" : "HFS");
	#endif
};
	
/*
 *	Support routine that updates our table of directory entries,
 *	should be called whenever a request is received.
 */	
static void update_directory_entries(request_rec *r) {
	core_server_config *sconf = (core_server_config*)
		ap_get_module_config(r->server->module_config, &core_module);
	void **sec = (void**) sconf->sec->elts;
	int i,num_sec = sconf->sec->nelts;
	
	/* Parse all "<Directory>" statements for 'r->server' */
	for (i = 0; i < num_sec; ++i) {
		core_dir_config *entry_core = (core_dir_config*)
			ap_get_module_config(sec[i], &core_module);
		if (entry_core == NULL || entry_core->d == NULL) continue;
		add_directory_entry(r, entry_core->d);
	}
};

/*
 *	Support routine that does a string compare of two paths (do not
 *	care if trailing slashes are present). Return the number of
 *	characters matched (or 0 else) if both paths are equal or if
 *	'child' is a sub-directory of 'parent'. In that very case also 
 *	returns 'related'=1.
 */
static int compare_paths(const char *parent, const char *child, 
	int *related) {
	size_t		pl,cl,i;
	const char	*p,*c;
	size_t		n = 0;

	*related = 0;

	/* Strip out trailing slashes */
	pl = (size_t) strlen(parent);
	if (pl == 0) return 0;
	if (parent[pl - 1] == '/') pl--;
	cl = (size_t) strlen(child);
	if (cl == 0) return 0;
	if (child[cl - 1] == '/') cl--;
	if (cl < pl) return 0;
	
	/* Compare both paths */
	for (p = parent,c = child,i = pl; i > 0; i--) {
		if (tolower(*p++) != tolower(*c++)) break;
		n++;
	}
	if (i > 0 || (cl > pl && *c != '/')) return 0;
	*related = cl >= pl;
	return n;
};

#pragma mark-
/*
 *	Pre-run fixups: refuse a URL that is mis-cased if it happens 
 *	there is at least one <Directory> statement that should have 
 *	applied. As input, this routine is passed a valid 'filename'
 *	that can be a path to a directory or to a file.
 */
static int hfs_apple_module_fixups(request_rec *r) {
	int i,found;
	size_t max_n_matches;
	char *url_path;
	
	/* First update table of directory entries if necessary */
	update_directory_entries(r);
	
	/*
	 * Then compare our path to each <Directory> statement we 
	 * found (case-insensitive compare) in order to find which
	 * one applies, example (the second one would apply here):
	 * 'filename'=
	 * 	/Library/WebServer/Documents/MyFolder/printenv.cgi
	 * 'directories' table=
	 * 	/Library/WebServer/Documents/
	 * 	/Library/WebServer/Documents/MyFolder/
	 * 	/Library/WebServer/Documents/MyFolder/Zero/
	 * 	/Library/WebServer/Documents/MyFolder/Zero/One/	 
	 */
	max_n_matches = 0;
	found = -1;
	if (r->filename[strlen(r->filename) - 1] != '/') {
		url_path = ap_pstrcat(g_pool, r->filename, "/", NULL);
	} else {
		url_path = ap_pstrdup(g_pool, r->filename);
	}
	for (i = 0; i < directories->nelts; i++) {
		int	related;
		size_t n_matches;
		dir_rec *entry = ((dir_rec**) directories->elts)[i];
		if (entry->case_sens == 1) continue;
		n_matches = compare_paths(
			entry->dir_path, url_path, &related);
	 	if (n_matches > 0 
	 		&& n_matches > max_n_matches && related == 1) {
	 		max_n_matches = n_matches;
	 		found = i;
	 	}
	}
	if (found < 0) return OK;
	
	/*
	 * We found at least one <Directory> statement that defines
	 * the most immediate parent of 'filename'. Do a regular 
	 * case-sensitive compare on the directory portion of it. If
	 * not-equal then return an error.
	 */
	if (strncmp(((dir_rec**) directories->elts)[found]->dir_path,
		url_path, max_n_matches) != 0) {
		#if DEBUG
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, r,
			"Mis-cased URI: %s, wants: %s",
			r->filename,
			((dir_rec**) directories->elts)[found]->dir_path);
		#endif
		return FORBIDDEN;
	}
	
	return OK;
}

/*
 *	Initialization (called only once by Apache parent process).
 *	We will be using the main pool not the request's one!
 */
static void hfs_apple_module_init(server_rec *s, pool *p) {
	g_pool = p;
	directories = ap_make_array(g_pool, 4, sizeof(dir_rec*));
};

#pragma mark DispatchTable
/*
 *	Module dispatch table.
 */
module MODULE_VAR_EXPORT hfs_apple_module = {
	STANDARD_MODULE_STUFF,
	hfs_apple_module_init,		/* initializer */
	NULL,						/* dir config creater */
	NULL,						/* dir merger --- default is to override */
	NULL,						/* server config */
	NULL,						/* merge server config */
	NULL,						/* command table */
	NULL,						/* handlers */
	NULL,						/* filename translation */
	NULL,						/* check user_id */
	NULL,						/* check auth */
	NULL,						/* check access */
	NULL,						/* type_checker */
	hfs_apple_module_fixups,	/* fixups */
	NULL,						/* logger */
	NULL,						/* header parser */
	NULL,						/* child_init */
	NULL,						/* child_exit */
	NULL						/* post read-request */
};

#endif
