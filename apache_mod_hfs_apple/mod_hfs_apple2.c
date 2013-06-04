/*
Copyright (c) 2000-2013 Apple Inc. All Rights Reserved.

This file contains Original Code and/or Modifications of Original Code
as defined in and that are subject to the Apple Public Source License
Version 2.0 (the 'License'). You may not use this file except in
compliance with the License. Please obtain a copy of the License at
http://www.opensource.apple.com/apsl/ and read it before using this
file.

The Original Code and all software distributed under the License are
distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
Please see the License for the specific language governing rights and
limitations under the License.
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
 * On case-sensitive volumes, a URI must
 * always match the actual path, in order for the file to be fetched. Any
 * <Directory> statement will consequently be enforced. Because if there
 * is a case-mismatch a file-not-found error will be returned and if 
 * there is no case-mismatch then relevant <Directory> statements will 
 * be walked through while parsing the URI.
 *
 * On case-insensitive HFS volumes, a URI may
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
 * That is what this module does. Consequently, when this module is
 * installed, some "pseudo-case-sensitivity" is enforced when Apache 
 * deals with case-insensitive HFS volumes.
 *
 * 13-JUN-2001	[JFA, Apple Computer, Inc.]
 *		Initial version for Mac OS X Server 10.0.
 */


#define CORE_PRIVATE
#include "apr.h"
#include "apr_strings.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_request.h"
#include "http_protocol.h"
#include "http_log.h"
#include "http_main.h"
#include "util_script.h"
#include <ctype.h>

#define __MACHINEEXCEPTIONS__
#define __DRIVERSERVICES__
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <CoreFoundation/CFString.h>

#include <unistd.h>


module AP_MODULE_DECLARE_DATA hfs_apple_module;


/*
 *	Our core data structure: each entry in the table is composed
 *	of a key (the path of a <Directory> statement, no matter what
 *	server it applies to) and a value that tells whether its
 *	volume is HFS or not (case-sensitive=0 or 1). Unfortunately
 *	the work required to fill this table will be repeated for 
 *	each Apache child process (but there is nothing new here!)
 */
static apr_pool_t *g_pool = NULL;
static apr_array_header_t *directories = NULL;

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
	size_t len = strlen(path) + 2;

	/* malloc dir_path so we can explicitly free it if the path
	 * already exists in the cache, rather than leaving it in 
	 * apache's main pool.
	 */
	dir_path = malloc(len);
	if( dir_path == NULL ) return;
	strlcpy(dir_path, path, len);

	/* Make sure input path has a trailing slash */
	if (path[strlen(path) - 1] != '/') 
		strlcat(dir_path, "/", len);
	
	/* If the entry already exists then get out */
	for (i = 0; i < directories->nelts; i++) {
		dir_rec *entry = ((dir_rec**) directories->elts)[i];
		if (strcmp(dir_path, entry->dir_path) == 0) {
			free(dir_path);
			return;
		}
	}
	
	/* Figure whether the targeted volume is case-sensitive */
	case_sens = pathconf(path, _PC_CASE_SENSITIVE);
	//Non-existent paths may be considered case-sensitive
	
	/* Add new entry to the table (ignore errors) */
	elt = apr_array_push(directories);
	*elt = (dir_rec*) apr_palloc(g_pool, sizeof(dir_rec));
	if (*elt == NULL) return;
	/* Duplicate the path into apache's main pool (along with the rest
	 * of the structure) so everything stays together.  Then free what
	 * we've malloc'd. To do: Consider normalizing dir_path here.
	 */
	(*elt)->dir_path = apr_pstrdup(g_pool, dir_path); 
	free(dir_path);
	(*elt)->case_sens = case_sens;

	ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r,
		"mod_hfs_apple: %s is %s",
		(*elt)->dir_path, (*elt)->case_sens ? "case-sensitive" : "case-insensitive");
}
	
/*
 *	Support routine that updates our table of directory entries,
 *	should be called whenever a request is received.
 */	
static void update_directory_entries(request_rec *r) {
	core_server_config *sconf = (core_server_config*)
		ap_get_module_config(r->server->module_config, &core_module);
	void **sec = (void**) sconf->sec_dir->elts;
	int i,num_sec = sconf->sec_dir->nelts;
	
	/* Parse all "<Directory>" statements for 'r->server' */
	for (i = 0; i < num_sec; ++i) {
		core_dir_config *entry_core = (core_dir_config*)
			ap_get_module_config(sec[i], &core_module);
		if (entry_core == NULL || entry_core->d == NULL) continue;
		add_directory_entry(r, entry_core->d);
	}
}

/*
 *	Determine whether child path refers to a subdirectory of parent path, with equivalance determined by
 *	comparing their file system representation. Only called for case-insensitive parents, with non-ascii
 *	characters in the argument strings, since the other cases are handled by compare_paths.
 */
static int compare_non_ascii_paths(const char *parent, const char *child, int *related, int *deny, request_rec* r) {
	CFStringRef parentRef = CFStringCreateWithCString(NULL, parent, kCFStringEncodingUTF8);
	if (!parentRef) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r,
				  "mod_hfs_apple: Cannot encode parent %s. Skipping.", parent);
		return 0;
	}

	CFStringRef childRef = CFStringCreateWithCString(NULL, child, kCFStringEncodingUTF8);
	if (!childRef) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r,
					  "mod_hfs_apple: Cannot encode child %s. Denying access.", child);
		*deny = 1;
		return 0;
	}

	int parentStrLen = strlen(parent);
	int parentLength = CFStringGetLength(parentRef);
	int childLength = CFStringGetLength(childRef);
	if (CFStringHasSuffix(parentRef, CFSTR("/"))) {
		CFRelease(parentRef);
		parentRef = CFStringCreateWithSubstring(NULL, parentRef, CFRangeMake(0, --parentLength));
		parentStrLen--;
	}
	if (CFStringHasSuffix(childRef, CFSTR("/"))) {
		CFRelease(childRef);
		childRef = CFStringCreateWithSubstring(NULL, childRef, CFRangeMake(0, --childLength));
	}
	char fsrChild[PATH_MAX];
	if (!CFStringGetFileSystemRepresentation(childRef, fsrChild, sizeof(fsrChild))) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r,
					  "mod_hfs_apple: Cannot get file system representation for child %s. Denying access.", child);
		CFRelease(childRef);
		*deny = 1;
		return 0;
	}
	CFRelease(childRef);

	char fsrParent[PATH_MAX];
	if (!CFStringGetFileSystemRepresentation(parentRef, fsrParent, sizeof(fsrParent))) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r,
				  "mod_hfs_apple: Cannot get file system representation for parent %s. Skipping.", parent);
		CFRelease(parentRef);
		return 0;
	}
	CFRelease(parentRef);

	size_t fsrLen = strlen(fsrParent);
	if (!strncasecmp(fsrParent, fsrChild, fsrLen)) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r,
					  "mod_hfs_apple: Comparing FSR: %s == %s, len = %ld", fsrParent, fsrChild, fsrLen);
		*related = 1;
		return parentStrLen;
	} else {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r,
					  "mod_hfs_apple: Comparing FSR: %s != %s, len = %ld", fsrParent, fsrChild, fsrLen);
		*related = 0;
	return 0;
	}
}


/*
 *	Support routine that does a string compare of two paths (do not
 *	care if trailing slashes are present). Return the number of
 *	characters matched (or 0 else) if both paths are equal or if
 *	'child' is a sub-directory of 'parent'. In that very case also 
 *	returns 'related'=1.
 */
static int compare_paths(const char *parent, const char *child,
	int *related, int *deny, request_rec* r) {
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
		if (!isascii(*p) || !isascii(*c))
			return (compare_non_ascii_paths(parent, child, related, deny, r));
		if (tolower(*p++) != tolower(*c++)) break;
		n++;
	}
	if (i > 0 || (cl > pl && *c != '/')) return 0;
	*related = cl >= pl;
	return n;
}

/* Return 1 if string contains ignorable Unicode sequence.
 *	From 12830770:
 *	(\xFC[\x80-\x83])|(\xF8[\x80-\x87])|(\xF0[\x80-\x8F])|(\xEF\xBB\xBF)|(\xE2\x81[\xAA-\xAF])|(\xE2\x80[\x8C-\x8F\xAA-\xAE])
 */ 
static int contains_ignorable_sequence(unsigned char* s, __attribute__((unused)) request_rec* r) {
	size_t len = strlen((char*)s);
	if (len <= 2) return 0;
	size_t i;
	for (i = 0; i <= len - 2; i++) {
		// 2-char sequences
		if (s[i] == (unsigned char)'\xFC' && (unsigned char)'\x80' <= s[i+1] && s[i+1] <= (unsigned char)'\x83') return 1;
		if (s[i] == (unsigned char)'\xF8' && (unsigned char)'\x80' <= s[i+1] && s[i+1] <= (unsigned char)'\x87') return 1;
		if (s[i] == (unsigned char)'\xF0' && (unsigned char)'\x80' <= s[i+1] && s[i+1] <= (unsigned char)'\x8F') return 1;
		if (i <= len - 3) {
			// 3-char sequences
			//ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r,
			//		"mod_hfs_apple: 3-char %x %x %x", s[i], s[i+1], s[i+2]);

			if (s[i] == (unsigned char)'\xEF' && s[i+1] == (unsigned char)'\xBB' && s[i+2] == (unsigned char)'\xBF') return 1;
			if (s[i] == (unsigned char)'\xE2' && s[i+1] == (unsigned char)'\x81' && (unsigned char)'\xAA' <= s[i+2] && s[i+2] <= (unsigned char)'\xAF') return 1;
			if (s[i] == (unsigned char)'\xE2' && s[i+1] == (unsigned char)'\x80' && (((unsigned char)'\x8C' <= s[i+2] && s[i+2] <= (unsigned char)'\x8F') || ((unsigned char)'\xAA' <= s[i+2] && s[i+2] <= (unsigned char)'\xAE'))) return 1;
		}
	}
	return 0;
}


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
	size_t len;
	
/*
 * Forbid access to URIs with ignorable Unicode character sequences
*/
	if (contains_ignorable_sequence((unsigned char*)r->filename, r)) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, r,
					  "mod_hfs_apple: URI %s has ignorable character sequence. Denying access.",
					  r->filename);
		return HTTP_FORBIDDEN;
	}
	
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
	len = strlen(r->filename);
	int deny = 0;
	if (r->filename[len - 1] != '/') {
		url_path = malloc(len + 2);
		if( url_path == NULL ) return HTTP_FORBIDDEN;
		strlcpy(url_path, r->filename, len + 2);
		strlcat(url_path, "/", len + 2);
	} else {
		url_path = malloc(len + 1);
		if( url_path == NULL ) return HTTP_FORBIDDEN;
		strlcpy(url_path, r->filename, len + 1);
	}
	for (i = 0; i < directories->nelts; i++) {
		int	related;
		size_t n_matches;
		dir_rec *entry = ((dir_rec**) directories->elts)[i];
		if (entry->case_sens == 1) continue;
		n_matches = compare_paths(
			entry->dir_path, url_path, &related, &deny, r);
		if (deny) {
			free(url_path);
			ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, r,
						  "mod_hfs_apple: Cannot encode for comparison, %s vs %s; denying access.", entry->dir_path, url_path);
			return HTTP_FORBIDDEN;
		}
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r,
					  "mod_hfs_apple: compare_paths %s vs %s, related=%d", entry->dir_path, url_path, related);
	 	if (n_matches > 0
	 		&& n_matches > max_n_matches && related == 1) {
	 		max_n_matches = n_matches;
	 		found = i;
	 	}
	}
	if (found < 0) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r,
					  "mod_hfs_apple: Allowing access with no matching directory. filename = %s", r->filename);
		free(url_path);
		return OK;
	}
	
	/*
	 * We found at least one <Directory> statement that defines
	 * the most immediate parent of 'filename'. Do a regular 
	 * case-sensitive compare on the directory portion of it. If
	 * not-equal then return an error.
     */
	ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r,
				  "mod_hfs_apple: Final check compares: %s vs %s, length %ld",
				  r->filename, ((dir_rec**) directories->elts)[found]->dir_path, max_n_matches);

	if (strncmp(((dir_rec**) directories->elts)[found]->dir_path,
		url_path, max_n_matches) != 0) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, r,
			"mod_hfs_apple: Mis-cased URI or unacceptable Unicode in URI: %s, wants: %s",
			r->filename,
			((dir_rec**) directories->elts)[found]->dir_path);
		free(url_path);
		return HTTP_FORBIDDEN;
	}
	ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r,
				  "mod_hfs_apple: Allowing access with matching directory. filename = %s", r->filename);
	free(url_path);
	return OK;
}

/*
 *	Initialization (called only once by Apache parent process).
 *	We will be using the main pool not the request's one!
 */
static void hfs_apple_module_init(apr_pool_t *p, __attribute__((unused)) server_rec *s ) {
	g_pool = p;
	directories = apr_array_make(g_pool, 4, sizeof(dir_rec*));
}


static void register_hooks(__attribute__((unused)) apr_pool_t *p)
{
	ap_hook_child_init(hfs_apple_module_init, NULL, NULL, APR_HOOK_MIDDLE);
	ap_hook_fixups(hfs_apple_module_fixups, NULL, NULL, APR_HOOK_MIDDLE);
}


#pragma mark DispatchTable
/*
 *	Module dispatch table.
 */
module AP_MODULE_DECLARE_DATA hfs_apple_module = {
	STANDARD20_MODULE_STUFF,
	NULL,					/* dir config creater */
	NULL,                       /* dir merger --- default is to override */
	NULL,                       /* server config */
	NULL,                       /* merge server config */
	NULL,						/* command apr_table_t */
	register_hooks              /* register hooks */
};
