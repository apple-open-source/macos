/*
 * Copyright (c) 2024 Klara, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#include <sys/types.h>
#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

/*
 * Does not support the rsync 3.x &include and &merge directives.
 */

#define	RSYNCD_LOG_FORMAT	"%o %h [%a] %m (%u) %f %l"
#define	RSYNCD_LOG_FILE_PREFIX	"%t [%p] "

struct daemon_cfg_module;
struct daemon_cfg_param;

#define	PARAM(nameval, cnameval)	\
	{ .name = (nameval), .cname = (cnameval), .dflt = "" }
#define	PARAM_DFLT(nameval, cnameval, dfltval)	\
	{ .name = (nameval), .cname = (cnameval), .dflt = (dfltval), .dflt_set = true }

/* XXX We may want to validate these on read. */
static struct rsync_daemon_param {
	const char	*name;
	const char	*cname;
	const char	*dflt;
	bool		 dflt_set;

	/* Global value, just for quicker lookup. */
	const struct daemon_cfg_param	*global;
} rsync_daemon_params[] = {
	/* Implemented */
	/** Global parameters **/
	PARAM("address",	NULL),
	PARAM("motd file",	"motdfile"),
	PARAM("pid file",	"pidfile"),
	PARAM_DFLT("port",	NULL,			"rsync"),
	PARAM("socket options",	"socketoptions"),

	/** Module parameters **/
	PARAM("auth users",	"authusers"),
	PARAM("comment",	NULL),
	PARAM("exclude",	NULL),
	PARAM("exclude from",	"excludefrom"),
	PARAM("filter",		NULL),
	PARAM_DFLT("gid",		NULL,		"-2"),
	PARAM("hosts allow",	"hostsallow"),
	PARAM("hosts deny",	"hostsdeny"),
	PARAM_DFLT("ignore errors",	"ignoreerrors",	"false"),
	PARAM_DFLT("ignore nonreadable",	"ignorenonreadable",	"false"),
	PARAM("include",	NULL),
	PARAM("include from",	"includefrom"),
	PARAM("incoming chmod",	"incomingchmod"),
	PARAM_DFLT("list",		NULL,		"true"),
	PARAM_DFLT("lock file",		"lockfile",	"/var/run/rsyncd.lock"),
	PARAM("log file",	"logfile"),
	PARAM_DFLT("max connections",	"maxconnections",	"0"),
	PARAM_DFLT("max verbosity",	"maxverbosity",	"1"),
	/* next two defaults are based on chroot. */
	PARAM("munge symlinks",	"mungesymlinks"),
	PARAM("numeric ids",	"numericids"),
	PARAM("outgoing chmod",	"outgoingchmod"),
	PARAM("path",		NULL),
	PARAM("post-xfer exec",	"post-xferexec"),
	PARAM("pre-xfer exec",	"pre-xferexec"),
	PARAM_DFLT("read only",		"readonly",	"true"),
	PARAM("refuse options",	"refuseoptions"),
	PARAM("secrets file",	"secretsfile"),
	PARAM_DFLT("strict modes",	"strictmodes",	"true"),
	PARAM_DFLT("syslog facility",	"syslogfacility",	"daemon"),
	PARAM_DFLT("timeout",	NULL,	"0"),
	PARAM_DFLT("uid",		NULL,		"-2"),
	PARAM_DFLT("use chroot",	"usechroot",	"true"),
	PARAM_DFLT("write only",	"writeonly",	"false"),

	/* Not implemented module options */
	PARAM_DFLT("transfer logging",	"transferlogging",	"false"),
	PARAM_DFLT("log format",	"logformat",		RSYNCD_LOG_FORMAT),

	/* Intentionally omitted options (will warn) */
	PARAM("dont compress",	"dontcompress"),
};

struct daemon_cfg_param {
	STAILQ_ENTRY(daemon_cfg_param)	entry;

	/* global overrides global, module overrides global. */
	const struct daemon_cfg_module	*module;

	const struct rsync_daemon_param	*param;
	char				*value;
};

STAILQ_HEAD(daemon_cfg_modules, daemon_cfg_module);
STAILQ_HEAD(daemon_cfg_params, daemon_cfg_param);

struct daemon_cfg_module {
	STAILQ_ENTRY(daemon_cfg_module)	entry;

	struct daemon_cfg_params	params;
	char		*name;
};

struct daemon_cfg {
	struct daemon_cfg_modules	modules;
};

/*
 * Find a module in the current set.  Based on testing of the reference rsync,
 * module names coming from the client are case sensitive, but it's only
 * possible to use the casing of the first time the section appears in the
 * config file.
 */
static struct daemon_cfg_module *
cfg_find_module(struct daemon_cfg *dcfg, const char *module_name, bool strict)
{
	struct daemon_cfg_module *module;
	int cmp;

	/*
	 * Spaces have already been neutralized.
	 */
	STAILQ_FOREACH(module, &dcfg->modules, entry) {
		if (strict)
			cmp = strcmp(module->name, module_name);
		else
			cmp = strcasecmp(module->name, module_name);
		if (cmp == 0)
			return module;
	}

	return NULL;
}

static struct daemon_cfg_module *
cfg_module_alloc(struct daemon_cfg *dcfg, char *name)
{
	struct daemon_cfg_module *module;

	module = malloc(sizeof(*module));
	if (module == NULL) {
		ERR("malloc");
		return NULL;
	}

	module->name = name;
	STAILQ_INIT(&module->params);

	STAILQ_INSERT_TAIL(&dcfg->modules, module, entry);
	return module;
}

static struct daemon_cfg_module *
cfg_module_alloc_global(struct daemon_cfg *dcfg)
{
	struct daemon_cfg_module *module;
	char *name;

	name = strdup("global");
	if (name == NULL) {
		ERR("strdup");
		return NULL;
	}

	module = cfg_module_alloc(dcfg, name);
	if (module == NULL)
		free(name);
	return module;
}

static struct rsync_daemon_param *
cfg_param_info(const char *key)
{
	struct rsync_daemon_param *dparam;

	for (size_t i = 0; i < nitems(rsync_daemon_params); i++) {
		dparam = &rsync_daemon_params[i];

		if (strcmp(dparam->name, key) == 0)
			return dparam;
		else if (dparam->cname != NULL &&
		    strcmp(dparam->cname, key) == 0)
			return dparam;
	}

	return NULL;
}

static struct daemon_cfg_param *
cfg_module_find_param(struct daemon_cfg_module *module,
    const struct rsync_daemon_param *dparam)
{
	struct daemon_cfg_param *cparam;

	STAILQ_FOREACH(cparam, &module->params, entry) {
		if (cparam->param == dparam)
			return cparam;
	}

	return NULL;
}

/*
 * value is invalid after we add it.
 */
static int
cfg_module_add_param(struct daemon_cfg_module *module,
    struct rsync_daemon_param *dparam, char *value)
{
	struct daemon_cfg_param *param;

	param = cfg_module_find_param(module, dparam);
	if (param == NULL) {
		/* Not set yet, let's add it. */
		param = malloc(sizeof(*param));
		if (param == NULL) {
			ERR("malloc");

			/* We own value, free it. */
			free(value);
			return -1;
		}

		param->module = module;
		param->param = dparam;
		param->value = NULL;

		if (strcmp(module->name, "global") == 0)
			dparam->global = param;

		STAILQ_INSERT_TAIL(&module->params, param, entry);
	}

	free(param->value);
	param->value = value;
	return 0;
}

static void
cfg_module_free(struct daemon_cfg_module *module)
{

	/* XXX Free params */

	free(module->name);
	free(module);
}

static struct daemon_cfg_module *
cfg_parse_module_name(struct daemon_cfg *dcfg, const char *start,
    const char *end)
{
	struct daemon_cfg_module *module;
	char *section, *wr;

	/* Trim any trailing whitespace. */
	while (end > start && isspace(*(end - 1)))
		end--;

	if (end == start) {
		/* XXX Error */
		return NULL;
	}

	/* Trim any leading whitespace. */
	while (isspace(*start))
		start++;

	section = malloc((end - start) + 1);
	if (section == NULL) {
		/* XXX Error */
		return NULL;
	}

	/* Fold all spaces down to a single whitespace. */
	wr = section;
	for (const char *p = start; p != end; p++) {
		if (!isspace(*p)) {
			if (*p == '/' || *p == ']') {
				/* XXX Syntax error */
				free(section);
				return NULL;
			}

			*wr = *p;
			wr++;
			continue;
		}

		/* Even a tab character is replaced with a single space. */
		*wr = ' ';
		wr++;

		while (isspace(*(p + 1)))
			p++;
	}

	assert(wr - section <= end - start);
	*wr = '\0';

	module = cfg_find_module(dcfg, section, false);
	if (module == NULL) {
		module = cfg_module_alloc(dcfg, section);
		if (module == NULL)
			free(section);
	} else {
		/* No longer need this section name. */
		free(section);
	}

	return module;
}

void
cfg_free(struct daemon_cfg *dcfg)
{
	struct daemon_cfg_module *module;

	assert(dcfg != NULL);
	while (!STAILQ_EMPTY(&dcfg->modules)) {
		module = STAILQ_FIRST(&dcfg->modules);
		STAILQ_REMOVE_HEAD(&dcfg->modules, entry);

		cfg_module_free(module);
	}
}

static void
cfg_normalize_param_name(char *key)
{
	size_t len;

	len = strlen(key);
	for (size_t i = 0; i < len; i++) {
		size_t next;

		if (!isspace(key[i])) {
			key[i] = tolower(key[i]);
			continue;
		}

		next = i + 1;
		while (isspace(key[next]))
			next++;

		if (key[next] == '\0') {
			/* String ends at i. */
			key[i] = '\0';
			break;
		}

		/* Internal whitespace, trim it. */
		memmove(&key[i], &key[next], len - next);

		len -= next - i;
		key[len] = '\0';
	}
}

static struct rsync_daemon_param *
cfg_valid_param_name(const char *key)
{

	if (*key == '\0')
		return NULL;

	return cfg_param_info(key);
}

static char *
cfg_glue_values(char *heapval, const char *fragment, size_t *valuelen)
{
	size_t fraglen;
	char *newheap;

	/* Nothing to do. */
	if (*fragment == '\0')
		return heapval;

	fraglen = strlen(fragment);
	*valuelen += fraglen;

	newheap = realloc(heapval, *valuelen + 1);
	if (newheap == NULL) {
		/* Fatal */
		free(heapval);
		return NULL;
	}

	fraglen = strlcat(newheap, fragment, *valuelen + 1);
	/* No margin for error, we strlen'd it. */
	assert(fraglen == *valuelen);

	return newheap;
}

struct daemon_cfg *
cfg_parse(const struct sess *sess, const char *cfg_file, int module)
{
	struct daemon_cfg *dcfg;
	FILE *fp;
	struct rsync_daemon_param *dparam;
	struct daemon_cfg_module *current_module = NULL;
	char *key, *tail, *value;
	char *line = NULL;
	size_t linesize = 0, valuelen;
	ssize_t linecnt, linelen;
	int error;
	bool continued;

	dcfg = malloc(sizeof(*dcfg));
	if (dcfg == NULL) {
		ERR("malloc");
		return NULL;
	}

	STAILQ_INIT(&dcfg->modules);

	/*
	 * The leading section defines global parameters, and we'll implicitly
	 * stuff all of them into a [global] section.  When we're parsing for
	 * modules, later [global] sections will get merged into this one.
	 */
	current_module = cfg_module_alloc_global(dcfg);
	if (current_module == NULL) {
		/* cfg_module_alloc reported the error. */
		cfg_free(dcfg);
		return NULL;
	}

	fp = fopen(cfg_file, "r");
	if (fp == NULL) {
		ERR("%s: open", cfg_file);
		cfg_free(dcfg);
		return NULL;
	}

	continued = false;
	error = 0;
	dparam = NULL;
	linecnt = 0;
	key = value = NULL;
	valuelen = 0;
	while ((linelen = getline(&line, &linesize, fp)) != -1) {
		char *start;

		linecnt++;
		line[--linelen] = '\0';

		/*
		 * Intentionally happens before trimming the start, apparently
		 * this whitespace is significant whether we had any non
		 * whitespace prior to the continuation or not.
		 */
		if (continued) {
			if (line[linelen - 1] == '\\') {
				line[--linelen] = '\0';
			} else {
				continued = false;
			}

			/* Glue these two together. */
			value = cfg_glue_values(value, line, &valuelen);
			goto hasval;
		}

		start = &line[0];
		while (isspace(*start))
			start++;

		/*
		 * Ignore empty lines / only comments.  The reference rsync can
		 * do line continuations, but seemingly only within values.
		 */
		if (*start == '#' || *start == '\0')
			continue;

		if (*start == '[') {
			char *end;

			/*
			 * rsyncd.conf(5) officially supports [global] sections,
			 * but it's actually not *that* global.  Notably, one
			 * cannot define global parameters that affect --daemon
			 * startup in a [global] section.  If module == NULL,
			 * then we're done after we hit a section or EOF.
			 */
			if (!module)
				break;

			start++;
			end = strrchr(start, ']');
			if (end == NULL) {
				/* XXX ERROR */
				/*
				 * Some errors don't cause a parse failure, but
				 * this seems to be one of them.
				 */
				error = -1;
				break;
			}

			current_module = cfg_parse_module_name(dcfg, start, end);
			if (current_module == NULL) {
				/* XXX Error */
				error = -1;
				break;
			}

			/*
			 * Reference rsync seems to ignore all kinds of garbage
			 * that may happen after a section heading, so we'll do
			 * the same for compatibility.
			 */
			continue;
		}

		/*
		 * Parse out a key = value pair; only the first = is significant
		 * here; leading and trailing spaces are trimmed from both key
		 * and value.  Leading spaces are already trimmed when we get to
		 * key.  An exception to the trimming rule appears to be in a
		 * line continuation, any leading space on the new line is
		 * retained, even if we haven't seen any significant character
		 * yet..  Errors in value parsing apparently shouldn't cause us
		 * to fail the file or section.
		 */
		key = start;

		/* Only the first = is significant. */
		tail = value = strchr(key, '=');
		if (value == NULL) {
			/* XXX malformed line, warn */
			continue;
		}

		while (tail > key && isspace(*(tail - 1))) {
			*--tail = '\0';
		}

		/*
		 * Won't move the start, may shrink the string to remove
		 * whitespace.
		 */
		cfg_normalize_param_name(key);

		if ((dparam = cfg_valid_param_name(key)) == NULL) {
			/*
			 * XXX Exception: this *does* throw an unrecoverable
			 * error.
			 */
			fprintf(stderr, "Invalid key in rsyncd.conf: '%s'\n", key);
			error = -1;
			break;
		}

		/* Terminate the key, we'll trim trailing whitespace later. */
		*value++ = '\0';
		while (isspace(*value))
			value++;

		if (line[linelen - 1] == '\\') {
			continued = true;
			line[--linelen] = '\0';
		}

		/*
		 * Trailing \ is trimmed, we can make a copy of the value now
		 * and do with it what we need to do.  We don't tap out a copy
		 * of the key because there's a good chance that the entry
		 * already exists and we just need to replace the value.
		 */
		valuelen = &line[linelen] - value;
		value = strdup(value);
		if (value == NULL) {
			ERR("strdup");
			free(key);
			key = NULL;

			error = -1;
			break;
		}

hasval:
		if (!continued) {
			/*
			 * cfg_module_add_param() will free our value or take
			 * possession of it.
			 */
			error = cfg_module_add_param(current_module, dparam,
			    value);
			value = NULL;
			valuelen = 0;
			if (error != 0)
				break;	/* Allocation error */
		}
	}

	if (continued) {
		/* Apparently we just fill in a blank value, I guess. */
	}

	free(line);
	if (error != 0 || ferror(fp))
		err(ERR_SYNTAX, "failed to parse file %s", cfg_file);
	if (error != 0) {
		cfg_free(dcfg);
		dcfg = NULL;
	}

	fclose(fp);

	return dcfg;
}

int
cfg_is_valid_module(struct daemon_cfg *dcfg, const char *module)
{

	assert(module != NULL);
	return cfg_find_module(dcfg, module, true) != NULL;
}

static int
cfg_param_resolve(struct daemon_cfg *dcfg, const char *which_mod,
    const char *key, const struct rsync_daemon_param **odparam,
    const struct daemon_cfg_param **ocparam)
{
	const struct rsync_daemon_param *dparam;
	const struct daemon_cfg_param *cparam;
	struct daemon_cfg_module *module;

	if (which_mod == NULL)
		which_mod = "global";

	/* This should resolve, we don't accept config keys from clients. */
	dparam = cfg_param_info(key);
	assert(dparam != NULL);

	module = cfg_find_module(dcfg, which_mod, true);
	if (module == NULL) {
		errno = ENOENT;
		return -1;
	}

	if (strcmp(which_mod, "global") == 0) {
		/* Avoid the lookup. */
		cparam = dparam->global;
	} else {
		cparam = cfg_module_find_param(module, dparam);
		if (cparam == NULL && dparam->global != NULL)
			cparam = dparam->global;
	}

	if (odparam != NULL)
		*odparam = dparam;
	*ocparam = cparam;
	return 0;
}

int
cfg_foreach_module(struct daemon_cfg *dcfg, cfg_module_iter *moditer,
    void *cookie)
{
	struct daemon_cfg_module *module;
	int rc;

	rc = 1;
	STAILQ_FOREACH(module, &dcfg->modules, entry) {
		if (strcmp(module->name, "global") == 0)
			continue;

		rc = moditer(dcfg, module->name, cookie);
		if (!rc)
			break;
	}

	return rc;
}

int
cfg_has_param(struct daemon_cfg *dcfg, const char *which_mod, const char *key)
{
	const struct rsync_daemon_param *dparam;
	const struct daemon_cfg_param *cparam;

	if (cfg_param_resolve(dcfg, which_mod, key, &dparam, &cparam) != 0)
		return 0;

	return cparam != NULL;
}

static int
cfg_param_fetch(struct daemon_cfg *dcfg, const char *which_mod, const char *key,
    const char **value)
{
	const struct rsync_daemon_param *dparam;
	const struct daemon_cfg_param *cparam;

	if (cfg_param_resolve(dcfg, which_mod, key, &dparam, &cparam) != 0)
		return -1;

	/*
	 * cfg_param_resolve() will return any cparam it can; if it returns
	 * NULL, then that means that we neither had a value supplied nor did
	 * we have a default provided in the table above.
	 */
	if (cparam == NULL && !dparam->dflt_set)
		return -1;

	if (cparam == NULL)
		*value = dparam->dflt;
	else
		*value = cparam->value;

	return 0;
}

int
cfg_param_bool(struct daemon_cfg *dcfg, const char *which_mod, const char *key,
    int *val)
{
	const char *valuestr;

	if (cfg_param_fetch(dcfg, which_mod, key, &valuestr) != 0)
		return -1;

	if (strcasecmp(valuestr, "yes") == 0 ||
	    strcasecmp(valuestr, "true") == 0 ||
	    strcasecmp(valuestr, "1") == 0) {
		*val = 1;
	} else if (strcasecmp(valuestr, "no") == 0 ||
	    strcasecmp(valuestr, "false") == 0 ||
	    strcasecmp(valuestr, "0") == 0) {
		*val = 0;
	} else {
		errno = EINVAL;
		return -1;
	}

	return 0;
}

int
cfg_param_long(struct daemon_cfg *dcfg, const char *which_mod, const char *key,
    long *val)
{
	char *endp;
	const char *valuestr;
	long lval;

	if (cfg_param_fetch(dcfg, which_mod, key, &valuestr) != 0)
		return -1;

	errno = 0;
	lval = strtol(valuestr, &endp, 10);
	if (errno != 0 || *endp != '\0') {
		errno = EINVAL;
		return -1;
	}

	*val = lval;
	return 0;
}

int
cfg_param_int(struct daemon_cfg *dcfg, const char *which_mod, const char *key,
    int *val)
{
	long lval;

	if (cfg_param_long(dcfg, which_mod, key, &lval) == -1)
		return -1;
	if (lval > INT_MAX || lval < INT_MIN) {
		errno = ERANGE;
		return -1;
	}

	*val = (int)lval;
	return 0;
}

int
cfg_param_str(struct daemon_cfg *dcfg, const char *which_mod, const char *key,
    const char **val)
{

	return cfg_param_fetch(dcfg, which_mod, key, val);
}
