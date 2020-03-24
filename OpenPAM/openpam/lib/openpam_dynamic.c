/*-
 * Copyright (c) 2002-2003 Networks Associates Technology, Inc.
 * Copyright (c) 2004-2007 Dag-Erling Smørgrav
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * Network Associates Laboratories, the Security Research Division of
 * Network Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: openpam_dynamic.c 408 2007-12-21 11:36:24Z des $
 */

#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dispatch/dispatch.h>
#include <security/pam_appl.h>
#include <System/sys/codesign.h>

#include "openpam_impl.h"

#ifndef RTLD_NOW
#define RTLD_NOW RTLD_LAZY
#endif


/*
 * OpenPAM internal
 *
 * Attempt to dynamically load a module.
 * Disable LV on the process if necessary.
 */

static void *
openpam_dlopen(const char *path, int mode)
{
	void *dlh = dlopen(path, mode);
	if (dlh != NULL)
		return dlh;

	/* The module exists and is readable, but we failed to load it: disable LV and try again. */
	int rv = csops(getpid(), CS_OPS_CLEAR_LV, NULL, 0);
	if (rv == 0)
		return dlopen(path, mode);

	openpam_log(PAM_LOG_LIBDEBUG, "csops(CS_OPS_CLEAR_LV) failed: %d", rv);
	return NULL;
}


/*
 * OpenPAM internal
 *
 * Attempt to load a specific module.
 * On success, populate the `pam_module_t` structure provided.
 */

static bool
openpam_dynamic_load_single(const char *path, pam_module_t *module)
{
	if (access(path, R_OK) != 0)
		return false;

	void *dlh = openpam_dlopen(path, RTLD_NOW);
	if (dlh != NULL) {
		openpam_log(PAM_LOG_LIBDEBUG, "%s", path);
		module->path = strdup(path);
		if (module->path == NULL)
			goto bail;
		module->dlh = dlh;
		for (int i = 0; i < PAM_NUM_PRIMITIVES; ++i) {
			module->func[i] = (pam_func_t)dlsym(dlh, _pam_sm_func_name[i]);
			if (module->func[i] == NULL)
				openpam_log(PAM_LOG_LIBDEBUG, "%s: %s(): %s", path, _pam_sm_func_name[i], dlerror());
		}
		return true;
	}

bail:
	if (dlh != NULL)
		dlclose(dlh);

	return false;
}


/*
 * OpenPAM internal
 *
 * Locate a dynamically linked module.
 * Prefer a module matching the current major version, otherwise fall back to the unversioned one.
 */

static bool
openpam_dynamic_load(const char *prefix, const char *path, pam_module_t *module)
{
	char *vpath = NULL;
	bool loaded = false;

	/* Try versioned module first. */
	if (asprintf(&vpath, "%s%s.%d", prefix, path, LIB_MAJ) < 0)
		return false;

	loaded = openpam_dynamic_load_single(vpath, module);
	if (!loaded) {
		/* Try again with unversioned module: remove LIB_MAJ. */
		*strrchr(vpath, '.') = '\0';
		loaded = openpam_dynamic_load_single(vpath, module);
	}

bail:
	FREE(vpath);
	return loaded;
}


pam_module_t *
openpam_dynamic(const char *path)
{
	pam_module_t *module;

	if ((module = calloc(1, sizeof *module)) == NULL) {
		openpam_log(PAM_LOG_ERROR, "%m");
		goto no_module;
	}

	/* Prepend the standard prefix if not an absolute pathname. */
	if (path[0] != '/') {
		// <rdar://problem/21545156> Add "/usr/local/lib/pam" to the search list
		static dispatch_once_t onceToken;
		static char *pam_modules_dirs  = NULL;
		static char **pam_search_paths = NULL;

		dispatch_once(&onceToken, ^{
			size_t len = strlen(OPENPAM_MODULES_DIR);
			char *tok, *str;
			const char *delim = ";";
			const char sep = delim[0];
			int i, n;

			str = OPENPAM_MODULES_DIR;
			assert(len > 0);
			assert(str[0]     != sep);		// OPENPAM_MODULES should not start with a ';'
			assert(str[len-1] != sep);		// no terminating ';'
			for (i = 0, n = 1; i < len; i++) n += (str[i] == sep);

			if ((pam_modules_dirs = strdup(OPENPAM_MODULES_DIR)) != NULL &&
				(pam_search_paths = (char **) malloc((n + 1) * sizeof(char *))) != NULL) {
				for (tok = str = pam_modules_dirs, i = 0; i < n; i++)
					pam_search_paths[i] = tok = strsep(&str, delim);
				pam_search_paths[n] = NULL;
			} else {
				openpam_log(PAM_LOG_ERROR, "%m - PAM module search paths won't work!");
			}
		});

		if (pam_search_paths) {
			int i;
			for (i = 0; pam_search_paths[i] != NULL; i++)
				if (openpam_dynamic_load(pam_search_paths[i], path, module))
					return module;
		}
	} else {
		if (openpam_dynamic_load("", path, module))
			return module;
	}

no_module:
	FREE(module);
	return NULL;
}

/*
 * NOPARSE
 */
