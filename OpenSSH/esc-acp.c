//
//  esc-acp.c
//  Copyright © 2021 Apple Inc. All rights reserved.
//
// AuthorizedPrincipalsCommand for ESC

#include "includes.h"

#include <MobileGestalt.h>

#include <ctype.h>
#include <inttypes.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "xmalloc.h"
#include "ssh.h"
#include "log.h"
#include "sshkey.h"
#include "sshbuf.h"
#include "ssherr.h"
#include "match.h"
#include "misc.h"

extern char *__progname;
extern char **environ;

const char *appleconnect_pattern = "*@APPLECONNECT.APPLE.COM";

const char *connection_id_extnames[] = {
	"connection-id",
	"connection-id@corp.apple.com",
	NULL
};
const char *device_udid_extnames[] = {
	"device-udid",
	"device-udid@corp.apple.com",
	"device-identifiers",
	"device-identifiers@corp.apple.com",
	"host-binding",
	"host-binding@corp.apple.com",
	NULL
};
const char *dsid_extnames[] = {
	"dsid",
	"dsid@corp.apple.com",
	NULL
};
const char *groups_extnames[] = {
	"groups",
	"groups@corp.apple.com",
	NULL
};

static int debug_flag = 0;

struct extensions {
	char  *connection_id;
	char  *device_udid;
	char  *dsid;
	char  *groups;
};

static void
free_extensions(struct extensions *e) {
	if (e->connection_id != NULL) {
		free(e->connection_id);
	}
	if (e->device_udid != NULL) {
		free(e->device_udid);
	}
	if (e->dsid != NULL) {
		free(e->dsid);
	}
	if (e->groups != NULL) {
		free(e->groups);
	}
}

static void
usage() {
	fprintf(stderr, "usage: %s <cert> <principals path> <group path>\n", __progname);
	fprintf(stderr, "\t<cert>              "
	    "Base64 encoded SSH certificate.\n");
	fprintf(stderr, "\t<principals path>   "
	    "Path to authorized principals file for target user.\n");
	fprintf(stderr, "\t<group path>        "
	    "Path to authorized groups file for target user.\n");
	exit(2);
}

static const char **
readlines(const char *path) {
	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		return NULL;
	}
	size_t n = 0;
	size_t nmemb = 1;
	char *line = NULL;
	size_t linecap = 0;
	const char **result = xcalloc(nmemb+1, sizeof(result[0]));
	while (getline(&line, &linecap, fp) != -1) {
		if (n == nmemb) {
			result = xrecallocarray(result, nmemb, nmemb*2+1,
			    sizeof(result[0]));
			nmemb *= 2;
		}
		size_t i = strspn(line, " \t\n");
		if (line[i] == '\0' || line[i] == '#') {
			continue;
		}
		for (char *p = strchr(&line[i], '\0')-1; 
		    p >= &line[i] && isspace(*p);
		    *p-- = '\0')
			/* empty loop body */;
		result[n++] = xstrdup(&line[i]);
	}
	if (line != NULL) {
		free(line);
	}
	fclose(fp);
	return result;
}

const char *
match_name(const char *name, const char **names) {
	for (const char **p = names; *p != NULL; p++) {
		if (strcmp(name, *p) == 0) {
			return *p;
		}
	}
	return NULL;
}

void
extract_extensions(struct sshbuf *buf, struct extensions *ext) {
	while (sshbuf_len(buf) != 0) {
		int r = 0;
		char *name = NULL, *val = NULL;
		char **dst = NULL;
		struct sshbuf *valbuf = NULL;

		if ((r = sshbuf_get_cstring(buf, &name, NULL)) != 0 ||
		    (r = sshbuf_froms(buf, &valbuf)) != 0) {
			fatal("malformed extension: %s", ssh_err(r));
		}
		if (match_name(name, connection_id_extnames) != NULL) {
			dst = &ext->connection_id;
		} else if (match_name(name, device_udid_extnames) != NULL) {
			dst = &ext->device_udid;
		} else if (match_name(name, dsid_extnames) != NULL) {
			dst = &ext->dsid;
		} else if (match_name(name, groups_extnames) != NULL) {
			dst = &ext->groups;
		} else {
			free(name);
			sshbuf_free(valbuf);
			continue;
		}
		debug3("extension named \"%s\"", name);
		if ((r = sshbuf_get_cstring(valbuf, &val, NULL)) != 0) {
			fatal("malformed extension: %s", ssh_err(r));
		}
		if (sshbuf_len(valbuf) > 0) {
			fatal("extra data at end of extension value");
		}
		*dst = val;
		free(name);
		sshbuf_free(valbuf);
	}
}

static char *
getudid() {
	char buf[128];
	const char *mock = getenv("ESC_ACP_MOCK_UDID");
	const char *result = "<none>";
	CFStringRef udid = NULL;

	if (mock != NULL) {
		result = mock;
	} else if ((udid = MGCopyAnswer(kMGOUniqueDeviceID, NULL)) == NULL) {
		verbose("Failed to obtain UDID: "
		    "MGCopyAnswer kMGOUniqueDeviceID");
	} else if (!CFStringGetCString(udid, buf, sizeof(buf),
	    kCFStringEncodingUTF8)) {
		verbose("Failed to UTF-8 decode UDID");
	} else {
		result = &buf[0];
	}
	if (udid != NULL) {
		CFRelease(udid);
	}
	return xstrdup(result);
}

char *
display(const char *string) {
	if (string == NULL) {
		return xstrdup("<none>");
	}
	size_t buflen = strlen(string) * 4 + 1;
	char *buf = xcalloc(1, buflen);
	if (strnvis(buf, string, buflen, VIS_DQ) < 0) {
		fatal("strnvis failed");
	}
	return buf;
}

static int
authorize(struct sshkey *key, const char **principal_patterns,
    const char **group_patterns) {
	const char *default_principals[] = { appleconnect_pattern, NULL };
	char *udid = getudid();
	struct sshkey_cert *cert = key->cert;
	struct extensions exts = { NULL };
	int authorized = 0;

	extract_extensions(cert->extensions, &exts);

	debug3("our udid=%s", udid);
	if (exts.device_udid != NULL) {
		const char *p = udid, *q = exts.device_udid;
		while (*p && *q) {
			while (!isxdigit(*p)) {
				p++;
			}
			while (!isxdigit(*q)) {
				q++;
			}
			if (tolower(*p) != tolower(*q)) {
				break;
			}
			p++;
			q++;
		}
		if (*q != '\0') {
			debug3("certificate udid mismatch %s",
			    exts.device_udid);
			goto fin;
		}
	}

	if (principal_patterns == NULL && group_patterns == NULL) {
		principal_patterns = &default_principals[0];
	} else if (principal_patterns == NULL) {
		principal_patterns = &default_principals[1];
	}
	for (int i = 0; i < cert->nprincipals; i++) {
		const char *pr = cert->principals[i];
		debug3("principal %s", pr);
		for (const char **p = principal_patterns; *p != NULL; p++) {
			if (match_pattern(pr, *p)) {
				debug3("pattern %s ~ %s: matched", pr, *p);
				printf("%s\n", pr);
				authorized = 1;
				break;
			}
			debug3("pattern %s ~ %s: matched", pr, *p);
		}
	}
	if (authorized || group_patterns == NULL || exts.groups == NULL) {
		goto fin;
	}
	debug3("groups %s", exts.groups);
	char *grfree = xstrdup(exts.groups);
	char *gr = grfree, *grnext = grfree;
	while ((gr = strsep(&grnext, ",")) != NULL) {
		for (const char **p = group_patterns; *p != NULL; p++) {
			if (match_pattern(gr, *p)) {
				debug3("pattern %s ~ %s: matched", gr, *p);
				for (int i = 0; i < cert->nprincipals; i++) {
					printf("%s\n", cert->principals[i]);
					authorized = 1;
				}
				goto grfin;
			}
			debug3("pattern %s ~ %s", gr, *p);
		}
	}
grfin:
	free(grfree);

fin:    /* report the result */;
	struct extensions e = {
		.connection_id = display(exts.connection_id),
		.device_udid = display(exts.device_udid),
		.dsid = display(exts.dsid),
		.groups = display(exts.groups),
	};
	char *key_id = display(cert->key_id);
	free_extensions(&exts);
	verbose("%s authorization result: %s "
	    "serial=%" PRIu64 " "
	    "key-id=\"%s\" "
	    "connection-id=\"%s\" "
	    "dsid=\"%s\" "
	    "groups=\"%s\" "
	    "device-udid=\"%s\" ",
	    __progname, authorized ? "true" : "false",
	    cert->serial, key_id,
	    e.connection_id, e.dsid, e.groups, e.device_udid);
	free(udid);
	free(key_id);
	free_extensions(&e);
	return authorized;
}

void
disable_autoerase()
{
	const char *tool = "/usr/local/bin/autoerasetool";
	const char *args[] = {
		tool,
		"disable",
		NULL
	};
	int rc = posix_spawn(NULL, tool, NULL, NULL, args, environ);
	if (rc != 0) {
		verbose("exec \"%s disable\" failed: %s", tool, strerror(rc));
	}
}

int
main(int ac, char *av[])
{
        extern char *optarg;
        extern int optind;

	int ch;
	int on_stderr = 0;
	int debug_flag = 0;
	SyslogFacility log_facility = SYSLOG_FACILITY_AUTH;
        LogLevel log_level = SYSLOG_LEVEL_INFO;

	/* Ensure that fds 0, 1 and 2 are open or directed to /dev/null */
        sanitise_stdfd();

        __progname = ssh_get_progname(av[0]);
        setvbuf(stdout, NULL, _IOLBF, 0);

	while ((ch = getopt(ac, av, "dD")) != -1) {
		switch (ch) {
		case 'D':
			on_stderr = 1;
			break;
		case 'd':
			if (debug_flag == 0) {
				debug_flag = 1;
				log_level = SYSLOG_LEVEL_DEBUG1;
			} else if (log_level < SYSLOG_LEVEL_DEBUG3)
				log_level++;
			break;
		default:
			usage();
		}
	}
	ac -= optind;
	av += optind;
	if (ac != 3) {
		usage();
	}

	log_init(__progname, log_level, log_facility, on_stderr);

	struct sshbuf *blob = sshbuf_new();
	if (blob == NULL) {
		fatal("out of memory");
	}
	int r = sshbuf_b64tod(blob, av[0]);
	if (r != SSH_ERR_SUCCESS) {
		sshbuf_free(blob);
		fatal("Failed decoding base64: %s", ssh_err(r));
	}
	struct sshkey *key = NULL;
	r = sshkey_fromb(blob, &key);
	sshbuf_free(blob);
	if (r != SSH_ERR_SUCCESS) {
		fatal("Failed decoding key: %s", ssh_err(r));
	}
	if (!sshkey_is_cert(key)) {
		sshkey_free(key);
		fatal("SSH key is not a certificate.");
	}
	const char **principal_patterns = readlines(av[1]);
	const char **group_patterns = readlines(av[2]);
	if (principal_patterns != NULL) {
		debug2("using principals file \"%s\"", av[1]);
	}
	if (group_patterns != NULL) {
		debug2("using groups file \"%s\"", av[2]);
	}

	if (authorize(key, principal_patterns, group_patterns)) {
		disable_autoerase();
	}

	if (principal_patterns != NULL) {
		free(principal_patterns);
	}
	if (group_patterns != NULL) {
		free(group_patterns);
	}
	sshkey_free(key);
	return 0;
}
