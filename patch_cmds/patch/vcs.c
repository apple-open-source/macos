/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#include <sys/param.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "util.h"

#ifndef nitems
#define	nitems(arr)	(sizeof(arr) / sizeof((arr)[0]))
#endif

enum {
	PROBE_WEAK = 0,		/* Can't really support it. */
	PROBE_ALMOST,		/* Almost there, maybe not great. */
	PROBE_PERFECT,		/* Preferred, perfect match */
};

/*
 * VCS probe functions are expected to return a probe strength (PROBE_*
 * constants) on success, or a negative errno value on failure.
 */
typedef int vcs_probe_fn(const char *);

/*
 * VCS checkout functions are expected to return 0 on success, or a non-zero
 * value on failure.  The non-zero value will typically be an errno value, but
 * it could also be a negative value to determine an unknown or non-specific
 * error.
 */
typedef int vcs_checkout_fn(const char *);

/* Probe only */
static int sccs_probe(const char *filename);
static int cc_probe(const char *filename);

/* Probe + checkout */
static int rcs_probe(const char *filename);
static int rcs_checkout(const char *filename);

static int p4_probe(const char *filename);
static int p4_checkout(const char *filename);

struct vcs_driver {
	const char		*vcs_name;
	vcs_probe_fn		*vcs_probe;
	vcs_checkout_fn		*vcs_checkout;
	bool			 vcs_grab_missing;
};

static const struct vcs_driver *current_vcs;

static const struct vcs_driver vcs_drivers[] = {
	/*
	 * These ones are not implemented because client support is non-trivial
	 * to obtain, so an implementation would not be properly tested.  We'll
	 * at least probe for the respective VCS so that we can warn the user
	 * that they're not supported.
	 */
	{
		.vcs_name = "SCCS",
		.vcs_probe = &sccs_probe,
	},
	{
		.vcs_name = "ClearCase",
		.vcs_probe = &cc_probe,
	},

	/* Clients are readily accessible for these ones. */
	{
		.vcs_name = "RCS",
		.vcs_grab_missing = true,
		.vcs_probe = &rcs_probe,
		.vcs_checkout = &rcs_checkout,
	},
	{
		/* Keep Perforce last, its probe is imprecise. */
		.vcs_name = "Perforce",
		.vcs_grab_missing = true,
		.vcs_probe = &p4_probe,
		.vcs_checkout = &p4_checkout,
	},
};

/*
 * Returns a probe strength (PROBE_* constant) on success, or a negative errno
 * on failure.
 */
static int
vcs_do_probe(const struct vcs_driver *test_vcs, const char *filename,
    bool missing)
{
	int error;

	error = (*test_vcs->vcs_probe)(filename);
	if (error < 0)
		return (-ENXIO);

	/*
	 * In all cases, we don't let vcs_do_probe() upgrade the probe status of
	 * any vcs_probe() implementation.  The best example of why this is
	 * needed is Perforce, where we don't necessarily know that it's
	 * PROBE_PERFECT.  By default, we can only tell if Perforce is
	 * *configured*, not if it's actually used *here*.
	 */
	if (test_vcs->vcs_checkout == NULL)
		return MIN(error, PROBE_WEAK);

	if (missing && !test_vcs->vcs_grab_missing)
		return MIN(error, PROBE_ALMOST);

	return (error);
}

/*
 * vcs_probe() checks all included VCS drivers to determine if the given
 * filename is under a version control system.  The missing parameter is
 * pertinent because not all VCS implementations can cope with the file being
 * missing; it could be a weird state that the user's expected to cope with.
 *
 * vcs_probe() returns 0 on success, or an errno value on error.
 *
 * Note that vcs_do_probe() and all of the VCS probe implementations return
 * either a negative errno on failure or an integer >= 0 on success, so that it
 * can return both errors and probe strength rather than needing to separate the
 * latter out into an out parameter.
 */
int
vcs_probe(const char *filename, bool missing, bool check_only)
{
	const struct vcs_driver *best_vcs, *test_vcs;
	int best_value, error;

	best_vcs = NULL;
	best_value = -1;
	if (current_vcs != NULL) {
		error = vcs_do_probe(current_vcs, filename, missing);
		if (error >= 0) {
			if (error == PROBE_PERFECT)
				return (0);

			/* Give others a shot at doing better. */
			best_value = error;
			best_vcs = current_vcs;
		}
	}

	for (size_t i = 0; i < nitems(vcs_drivers); i++) {
		test_vcs = &vcs_drivers[i];

		if (test_vcs == current_vcs)
			continue;

		error = vcs_do_probe(test_vcs, filename, missing);
		if (error < 0 || error < best_value)
			continue;

		best_value = error;
		best_vcs = test_vcs;
		if (best_value == PROBE_PERFECT)
			break;
	}

	/*
	 * If we're only checking (i.e., trying to determine a name), then we
	 * only accept PROBE_PERFECT results.  Anything less and we don't
	 * actually know for sure that the file will be recoverable with that
	 * method.
	 */
	if (check_only)
		return (best_value == PROBE_PERFECT ? 0 : ENXIO);

	current_vcs = best_vcs;
	return (best_vcs == NULL ? ENXIO : 0);
}

/*
 * vcs_prompt() returns true if we should proceed to checkout the file from the
 * probed VCS, or false if we should not.
 */
bool
vcs_prompt(const char *filename)
{

	assert(current_vcs != NULL);
	ask("Get file %s from %s? [y]", filename, current_vcs->vcs_name);
	return (*buf != 'n');
}

/*
 * vcs_supported() copes with the fact that not all of the included drivers are
 * actually supported.  Some of the systems supported by GNU patch have clients
 * that are difficult to locate for macOS, thus support would be untested.  We
 * left them with a probe-only implementation so that we can warn about the
 * situation, but it likely won't come up.
 *
 * vcs_supported() returns true if we actually support the probed VCS, or false
 * if the support is probe-only.
 */
bool
vcs_supported(void)
{

	assert(current_vcs != NULL);
	return (current_vcs->vcs_checkout != NULL);
}

/*
 * vcs_name() returns the driver's name.
 */
const char *
vcs_name(void)
{

	assert(current_vcs != NULL);
	return (current_vcs->vcs_name);
}

/*
 * vcs_checkout() attempts to check the file out from the probed VCS.  It shells
 * out to the appropriate binary, and returns 0 on success or a non-zero value
 * on failure.  Positive values should be interpreted as an errno, but negative
 * values are non-descriptive and represent a general failure.  A general
 * failure would be from system(3) failing -- we don't really know why.
 *
 * Note that we don't really do anything to process the output and provide any
 * sort of insight gleaned from it.  The user is left to derive the cause of any
 * failure.
 */
int
vcs_checkout(const char *filename, bool missing)
{

	assert(current_vcs != NULL);

	/*
	 * Caller should have checked vcs_implemented() so that it can warn the
	 * user that a checkout won't happen and act appropriately.
	 */
	assert(current_vcs->vcs_checkout != NULL);

	if (missing && !current_vcs->vcs_grab_missing)
		return (ENOENT);

	return ((*current_vcs->vcs_checkout)(filename));
}

/* vcs_*() private interfaces. */
static int
vcs_run_cmd(const char *cmd)
{
	int error;

	error = system(cmd);
	if (error != 0)
		return (-ENXIO);	/* Unknown error */
	return (0);
}

static const char *
vcs_quoted_file(const char *filename)
{
	static char quotedpath[MAXPATHLEN];
	size_t i, fsz, pos;
	char ch;

	pos = 0;
	fsz = strlen(filename);
	quotedpath[pos++] = '\'';
	for (i = 0; i < fsz && pos + 2 < sizeof(quotedpath) - 1; i++) {
		ch = filename[i];
		if (ch == '\'') {
			/*
			 * Leave space for ' + NUL.  Just truncate it now if
			 * we're
			 * too far off into the weeds.
			 */
			if (pos + 4 >= sizeof(quotedpath) - 1)
				break;
			quotedpath[pos++] = '\'';
			quotedpath[pos++] = '\\';
			quotedpath[pos++] = '\'';
			quotedpath[pos++] = '\'';
		} else {
			quotedpath[pos++] = ch;
		}
	}

	if (i != fsz)
		say("WARNING: Quoted filename '%s' truncated, results probably incorrect!\n",
		    filename);
	quotedpath[pos++] = '\'';
	quotedpath[pos++] = '\0';

	return (quotedpath);
}


/* VCS Implementations */

/** SCCS (Probe only) **/
static int
sccs_probe(const char *filename)
{
	char namebuf[MAXPATHLEN];
	struct stat sb;

	snprintf(namebuf, sizeof(namebuf), "SCCS/%s.v", filename);
	if (stat(namebuf, &sb) == 0 && S_ISREG(sb.st_mode))
		return (PROBE_PERFECT);

	/* Try the "out of tree" histfile support. */
	snprintf(namebuf, sizeof(namebuf), ".sccs/SCCS/%s.v", filename);
	if (stat(namebuf, &sb) == 0 && S_ISREG(sb.st_mode))
		return (PROBE_PERFECT);

	return (-ENXIO);
}

/** ClearCase Implementation (Probe only) **/
static int
cc_probe(const char *filename)
{
	char namebuf[MAXPATHLEN];
	struct stat sb;

	snprintf(namebuf, sizeof(namebuf), "%s@@", filename);
	if (stat(namebuf, &sb) == 0 && S_ISDIR(sb.st_mode))
		return (PROBE_PERFECT);

	return (-ENXIO);
}

/** RCS Implementation **/
static int
rcs_probe(const char *filename)
{
	char namebuf[MAXPATHLEN];
	struct stat sb;

	/*
	 * RCS files can either be in the dedicated RCS directory, or in-tree
	 * with the file itself.
	 */
	snprintf(namebuf, sizeof(namebuf), "RCS/%s,v", filename);
	if (stat(namebuf, &sb) == 0 && S_ISREG(sb.st_mode))
		return (PROBE_PERFECT);

	snprintf(namebuf, sizeof(namebuf), "%s,v", filename);
	if (stat(namebuf, &sb) == 0 && S_ISREG(sb.st_mode))
		return (PROBE_PERFECT);

	return (-ENXIO);
}

static int
rcs_checkout(const char *filename)
{
	char *cmd;
	int error;

	error = asprintf(&cmd, "co -l %s", vcs_quoted_file(filename));
	if (error < 0)
		return (errno);

	error = vcs_run_cmd(cmd);
	free(cmd);

	return (error);
}

/* Perforce Implementation */
static bool
p4_probe_env(void)
{

	/*
	 * The server being defined is an obvious hint that it's configured, so
	 * we need not search any further.
	 */
	if (getenv("P4PORT") != NULL)
		return (true);

	/*
	 * If the environment specifies where we can find ~/.p4environ or the
	 * same config within a workspace, then we're definitely configured for
	 * perforce.
	 */
	if (getenv("P4CONFIG") != NULL || getenv("P4ENVIRON") != NULL)
		return (true);

	/*
	 * Less obvious is that aliases can also define the environment in which
	 * a command is run, so we must check for that as well.
	 */
	if (getenv("P4ALIASES") != NULL)
		return (true);

	return (false);
}

static bool
p4_probe_home(void)
{
	char probe_path[MAXPATHLEN];
	struct stat sb;
	const char *homedir;

	homedir = getenv("HOME");
	snprintf(probe_path, sizeof(probe_path), "%s/.p4enviro", homedir);
	if (stat(probe_path, &sb) == 0 && S_ISREG(sb.st_mode))
		return (true);
	snprintf(probe_path, sizeof(probe_path), "%s/.p4aliases", homedir);
	if (stat(probe_path, &sb) == 0 && S_ISREG(sb.st_mode))
		return (true);

	return (false);

}

static int
p4_probe_cwd(void)
{
	struct stat sb;
	const char *cfg;

	/*
	 * If $P4CONFIG isn't set, we must assume that we could be inside a
	 * workspace.
	 */
	cfg = getenv("P4CONFIG");
	if (cfg == NULL)
		return (PROBE_ALMOST);

	if (stat(cfg, &sb) == 0 && S_ISREG(sb.st_mode))
		return (PROBE_PERFECT);

	return (PROBE_ALMOST);
}

static int
p4_probe(const char *filename)
{
	static bool cfg_probed, has_cfg;

	if (!cfg_probed) {
		cfg_probed = true;

		has_cfg = p4_probe_env();
		if (!has_cfg)
			has_cfg = p4_probe_home();
	}


	/*
	 * If we don't have -any- configuration present, then it's a definitive
	 * no.  If we do have something, we need to see if $CWD/$P4CONFIG exists
	 * if P4CONFIG was set.
	 */
	if (!has_cfg)
		return (-ENXIO);

	return (p4_probe_cwd());
}

static int
p4_checkout(const char *filename)
{
	char *cmd;
	int error;

	error = asprintf(&cmd, "p4 edit %s", vcs_quoted_file(filename));
	if (error < 0)
		return (errno);

	error = vcs_run_cmd(cmd);
	free(cmd);

	return (error);
}
