/* $Xorg: server.c,v 1.5 2000/08/17 19:54:01 cpqbld Exp $ */

/************************************************************************/
/* Copyright (c) 1993 Quarterdeck Office Systems			*/
/*									*/
/* Permission to use, copy, modify, distribute, and sell this software	*/
/* and software and its documentation for any purpose is hereby granted	*/
/* without fee, provided that the above copyright notice appear in all	*/
/* copies and that both that copyright notice and this permission	*/
/* notice appear in supporting documentation, and that the name		*/
/* Quarterdeck Office Systems, Inc. not be used in advertising or	*/
/* publicity pertaining to distribution of this software without	*/
/* specific, written prior permission.					*/
/*									*/
/* THIS SOFTWARE IS PROVIDED `AS-IS'.  QUARTERDECK OFFICE SYSTEMS,	*/
/* INC., DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,		*/
/* INCLUDING WITHOUT LIMITATION ALL IMPLIED WARRANTIES OF		*/
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR		*/
/* NONINFRINGEMENT.  IN NO EVENT SHALL QUARTERDECK OFFICE SYSTEMS,	*/
/* INC., BE LIABLE FOR ANY DAMAGES WHATSOEVER, INCLUDING SPECIAL,	*/
/* INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS OF USE, DATA, OR	*/
/* PROFITS, EVEN IF ADVISED OF THE POSSIBILITY THEREOF, AND REGARDLESS	*/
/* OF WHETHER IN AN ACTION IN CONTRACT, TORT OR NEGLIGENCE, ARISING OUT	*/
/* OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.	*/
/************************************************************************/
/* $XFree86: xc/programs/rstart/server.c,v 1.5 2001/07/25 15:05:15 dawes Exp $ */

/* Extended rsh "helper" program */
#include <stdio.h>
#include <ctype.h>
#include <X11/Xos.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef	ODT1_DISPLAY_HACK
#include <netdb.h>
#endif	/* ODT1_DISPLAY_HACK */

#define	TRUE	1
#define	FALSE	0

extern void squish_out_escapes ( char *s );
extern char * Strdup ( char *s );
extern int get_a_line ( FILE *f, int *pargc, char ***pargv );
extern void nomem ( void );
static char *Strlwr ( char *s0 );
extern void do_it ( void );
extern void process ( FILE *f, int is_the_real_thing );
extern void detach ( void );
extern void putenv_with_prefix ( char *prefix, char *name, char *value );

/* auth.c */
extern void do_auth ( void );

struct key {
	char *name;
	void (*func)(int ac, char **av);
};

extern void key_cmd(int ac, char **av);
extern void key_exec(int ac, char **av);
extern void key_context(int ac, char **av);
extern void key_misc(int ac, char **av);
extern void key_generic_cmd(int ac, char **av);
extern void key_dir(int ac, char **av);
extern void key_detach(int ac, char **av);
extern void key_nodetach(int ac, char **av);
extern void key_posix_umask(int ac, char **av);
extern void key_auth(int ac, char **av);
extern void key_internal_registries(int ac, char **av);
extern void key_internal_local_default(int ac, char **av);
extern void key_internal_global_contexts(int ac, char **av);
extern void key_internal_local_contexts(int ac, char **av);
extern void key_internal_global_commands(int ac, char **av);
extern void key_internal_local_commands(int ac, char **av);
extern void key_internal_variable_prefix(int ac, char **av);
extern void key_internal_print(int ac, char **av);
extern void key_internal_auth_program(int ac, char **av);
extern void key_internal_auth_input(int ac, char **av);


struct key keys[] = {
	{ "cmd",			key_cmd },
	{ "exec",			key_exec },
	{ "context",			key_context },
	{ "misc",			key_misc },
	{ "generic-cmd",		key_generic_cmd },
	{ "dir",			key_dir },
	{ "detach",			key_detach },
	{ "nodetach",			key_nodetach },
	{ "posix-umask",		key_posix_umask },
	{ "auth",			key_auth },
	{ "internal-registries",	key_internal_registries },
	{ "internal-local-default",	key_internal_local_default },
	{ "internal-global-contexts",	key_internal_global_contexts },
	{ "internal-local-contexts",	key_internal_local_contexts },
	{ "internal-global-commands",	key_internal_global_commands },
	{ "internal-local-commands",	key_internal_local_commands },
	{ "internal-variable-prefix",	key_internal_variable_prefix },
	{ "internal-print",		key_internal_print },
	{ "internal-auth-program",	key_internal_auth_program },
	{ "internal-auth-input",	key_internal_auth_input },
	{ NULL,				NULL }
};


char **parm_cmd = NULL;
char **parm_exec = NULL;
char **parm_generic_cmd = NULL;
char *parm_dir = NULL;
char *parm_context = NULL;
char **parm_internal_registries = NULL;
char *parm_internal_local_default = NULL;
char *parm_internal_global_contexts = NULL;
char *parm_internal_local_contexts = NULL;
char *parm_internal_global_commands = NULL;
char *parm_internal_local_commands = NULL;
char *parm_internal_variable_prefix = NULL;
int parm_detach = FALSE;

char *parm_global_default = DEFAULT_CONFIG;
char	myname[]=SERVERNAME;

int
main(int argc, char *argv[])
{
	FILE *f;

	if(argc == 3 && !strcmp(argv[1], "-c")) parm_global_default = argv[2];

	setbuf(stdin, NULL);

	printf(
	    "%s: Ready: version 1.0, May 02 1994 X11R6.3\n",
	    myname);
	fflush(stdout);

	f = fopen(parm_global_default, "r");
	if(f) process(f, FALSE);

	if(parm_internal_local_default) {
		/* We start in $HOME */
		f = fopen(parm_internal_local_default, "r");
		if(f) process(f, FALSE);
	}

	process(stdin, TRUE);

	do_it();
	exit(0);
}

void
squish_out_escapes(s)
char *s;
{
	char *p;
	char *o;

	o = s;
	for(p = s; *p; p++) {
		if(*p == '\\') {
			if(p[1] >= '0' && p[1] <= '3'
			&& p[2] >= '0' && p[2] <= '7'
			&& p[3] >= '0' && p[3] <= '7') {
				*o++ = ((p[1]-'0') << 6)
					+ ((p[2]-'0') << 3)
					+ (p[3]-'0');
				p += 3;
			} else {
				printf(
			"%s: Failure: Improper \\ escape\n",myname);
				exit(255);
			}
		} else *o++ = *p;
	}
	*o = '\0';
}

char *
Strdup(s)
    char *s;
{
    char *cs;

    if (!s)
	return s;
    cs = malloc(strlen(s)+1);
    strcpy(cs, s);
    return cs;
}

int
get_a_line(f, pargc, pargv)
FILE *f;
int *pargc;
char ***pargv;
{
	char buf[2048];
	char *p;
	int c;
	char **pa;
	int was_space;
	char *saved;

	while(1) {
		p = buf;
		while((c = getc(f)) != '\n') {
			switch(c) {
			    case '\0':
			    case '\r':
				/* Ignored, per spec */
				break;
			    case EOF:
				return FALSE;
			    default:
				*p++ = c;
				break;
			}
		}
		*p = '\0';

		saved = Strdup(buf);
		if(!saved) nomem();

		*pargc = 0;
		was_space = TRUE;
		for(p = saved; *p; p++) {
			if(was_space && !isspace(*p)) (*pargc)++;
			was_space = isspace(*p);
		}

		*pargv = (char **)malloc((*pargc+1)*sizeof(char *));
		if(!*pargv) nomem();

		pa = *pargv;
		was_space = TRUE;
		for(p = saved; *p; p++) {
			if(was_space && !isspace(*p)) *pa++ = p;
			was_space = isspace(*p);
			if(isspace(*p)) *p = '\0';
		}
		*pa = NULL;

		if(*pargc > 0 && (*pargv)[0][0] == '#') {
			/* How embarrassing.  All that work for a comment. */
			free(saved);
			free(*pargv);
			continue;
		}

		break;
	}

	for(pa = *pargv; *pa; pa++) {
		squish_out_escapes(*pa);
	}

	return TRUE;
}

void
nomem()
{
	printf("%s: Failure: Out of memory\n",myname);
	exit(255);
}

void
key_internal_registries(ac, av)
int ac;
char **av;
{
	parm_internal_registries = av+1;
}

void
key_internal_variable_prefix(ac, av)
int ac;
char **av;
{
	if(ac != 2) {
		printf(
		    "%s: Failure: Malformed INTERNAL-VARIABLE-PREFIX\n",myname);
		exit(255);
	}
	parm_internal_variable_prefix = av[1];
}

void
key_internal_local_default(ac, av)
int ac;
char **av;
{
	if(ac != 2) {
		printf("%s: Failure: Malformed INTERNAL-LOCAL-DEFAULT\n",myname);
		exit(255);
	}
	parm_internal_local_default = av[1];
}

void
key_internal_global_commands(ac, av)
int ac;
char **av;
{
	if(ac != 2) {
		printf("%s: Failure: Malformed INTERNAL-GLOBAL-COMMANDS\n",myname);
		exit(255);
	}
	parm_internal_global_commands = av[1];
}

void
key_internal_local_commands(ac, av)
int ac;
char **av;
{
	if(ac != 2) {
		printf("%s: Failure: Malformed INTERNAL-LOCAL-COMMANDS\n",myname);
		exit(255);
	}
	parm_internal_local_commands = av[1];
}

void
key_internal_global_contexts(ac, av)
int ac;
char **av;
{
	if(ac != 2) {
		printf("%s: Failure: Malformed INTERNAL-GLOBAL-CONTEXTS\n",myname);
		exit(255);
	}
	parm_internal_global_contexts = av[1];
}

void
key_internal_local_contexts(ac, av)
int ac;
char **av;
{
	if(ac != 2) {
		printf("%s: Failure: Malformed INTERNAL-LOCAL-CONTEXTS\n",myname);
		exit(255);
	}
	parm_internal_local_contexts = av[1];
}

void
key_cmd(ac, av)
int ac;
char **av;
{
	if(parm_cmd || parm_exec || parm_generic_cmd) {
		printf(
	"%s: Failure: more than one of CMD, EXEC, GENERIC-CMD\n",myname);
		exit(255);
	}
	parm_cmd = av;
}

void
key_exec(ac, av)
int ac;
char **av;
{
	if(parm_cmd || parm_exec || parm_generic_cmd) {
		printf(
	"%s: Failure: more than one of CMD, EXEC, GENERIC-CMD\n",myname);
		exit(255);
	}
	parm_exec = av;
}

static char *
Strlwr(s0)
char *s0;
{
	char *s;

	for(s = s0; *s; s++) {
		if(isupper(*s)) *s = tolower(*s);
	}
	return s0;
}


void
key_context(ac, av)
int ac;
char **av;
{
	char buf[1024];
	int ok;
	FILE *f;

	if(ac != 2) {
		printf("%s: Failure: Malformed CONTEXT\n",myname);
		exit(255);
	}
	Strlwr(av[1]);
	parm_context = av[1];

	ok = FALSE;

	if(parm_internal_global_contexts) {
		strcpy(buf, parm_internal_global_contexts);
		strcat(buf, "/");
		strcat(buf, parm_context);
		if((f = fopen(buf, "r"))) {
			process(f, FALSE);
			ok = TRUE;
		}
	}

	if(parm_internal_local_contexts) {
		strcpy(buf, parm_internal_local_contexts);
		strcat(buf, "/");
		strcat(buf, parm_context);
		if((f = fopen(buf, "r"))) {
			process(f, FALSE);
			ok = TRUE;
		}
	}

	if(!ok) {
		printf("%s: Error: Unknown context '%s'\n",myname, parm_context);
		exit(255);
	}
}

void
key_dir(ac, av)
int ac;
char **av;
{
	if(ac != 2) {
		printf("%s: Failure: malformed DIR line\n",myname);
		exit(255);
	}
	parm_dir = av[1];
}

void
key_misc(ac, av)
int ac;
char **av;
{
	char **pp;

	if(ac != 3) {
		printf("%s: Failure: malformed MISC line\n",myname);
		exit(255);
	}

	Strlwr(av[1]);

	if(parm_internal_registries) {
		for(pp = parm_internal_registries; *pp; pp++) {
			if(!strcmp(av[1], *pp)) goto ok;
		}
	}
	printf("%s: Warning: registry %s not recognized\n",myname, av[1]);
	fflush(stdout);

ok:
	;

#ifdef	ODT1_DISPLAY_HACK
	/* The X apps in ODT version 1 don't know how to look up */
	/* host names using DNS.  Do it for them. */
	if(!strcmp(av[1], "x")
	&& !strncmp(av[2], "DISPLAY=", 8)
	&& odt1_display_hack(av[2]+8)) return;
#endif	/* ODT1_DISPLAY_HACK */

	putenv(av[2]);
}

#ifdef	ODT1_DISPLAY_HACK
odt1_display_hack(s)
char *s;
{
	char buf[80];
	struct hostent *he;
	char *p;
	int ok;

	ok = FALSE;
	for(p = s; *p; p++) {
		if(*p == ':') {
			if(!ok) break;
			*p = '\0';
			he = gethostbyname(s);
			*p = ':';
			if(!he) break;
			sprintf(buf, "DISPLAY=%u.%u.%u.%u%s",
				he->h_addr_list[0][0] & 0xff,
				he->h_addr_list[0][1] & 0xff,
				he->h_addr_list[0][2] & 0xff,
				he->h_addr_list[0][3] & 0xff,
				p);
			s = Strdup(buf);
			if(!s) nomem();
			putenv(s);
			return TRUE;
		}
		if(!isdigit(*p) && *p != '.') ok = TRUE;
	}
	return FALSE;
}
#endif	/* ODT1_DISPLAY_HACK */

void
key_generic_cmd(ac, av)
int ac;
char **av;
{
	if(parm_cmd || parm_exec || parm_generic_cmd) {
		printf(
	"%s: Failure: more than one of CMD, EXEC, GENERIC-CMD\n",myname);
		exit(255);
	}
	parm_generic_cmd = av;
}

void
do_it(void)
{
	if(parm_dir) {
		if(chdir(parm_dir)) {
			printf("%s: Error: %s - %s\n",myname,
				parm_dir, strerror(errno));
			exit(255);
		}
	}

	if(parm_internal_variable_prefix) {
		putenv_with_prefix(parm_internal_variable_prefix,
			"CONTEXT", parm_context);
		putenv_with_prefix(parm_internal_variable_prefix,
			"LOCAL_COMMANDS", parm_internal_local_commands);
		putenv_with_prefix(parm_internal_variable_prefix,
			"GLOBAL_COMMANDS", parm_internal_global_commands);
		putenv_with_prefix(parm_internal_variable_prefix,
			"LOCAL_CONTEXTS", parm_internal_local_contexts);
		putenv_with_prefix(parm_internal_variable_prefix,
			"GLOBAL_CONTEXTS", parm_internal_global_contexts);
	}

	do_auth();

	if(parm_cmd) {
		char *shell;
		char *buf;
		int len;
		char **pa;

		if(!(shell = getenv("SHELL"))) shell = "/bin/sh";

		len = 0;
		for(pa = parm_cmd+1; *pa; pa++) {
			len += strlen(*pa) + 1;
		}

		buf = malloc(len+1);
		if(!buf) nomem();

		buf[0] = '\0';
		for(pa = parm_cmd+1; *pa; pa++) {
			strcat(buf, " ");
			strcat(buf, *pa);
		}

		printf("%s: Success: about to exec %s -c %s\n",myname,
			shell, buf+1);
		fflush(stdout);

		if(parm_detach) detach();

		execl(shell, shell, "-c", buf+1, (char *)NULL);
		printf("%s: Error: %s - %s\n",myname,
			shell, strerror(errno));
		exit(255);
	}

	if(parm_exec) {
		printf("%s: Success: about to exec %s with args\n",myname,
			parm_exec[1]);
		fflush(stdout);

		if(parm_detach) detach();

		execvp(parm_exec[1], parm_exec+2);
		printf("%s: Error: %s - %s\n",myname,
			parm_exec[0], strerror(errno));
		exit(255);
	}

	if(parm_generic_cmd) {
		char buf[1024];

		if(!parm_internal_local_commands
		&& !parm_internal_global_commands) {
			printf(
	"%s: Failure: No generic command directory!\n",myname);
			exit(255);
		}

		printf("%s: Success: about to exec generic command\n",myname);
		fflush(stdout);

		if(parm_detach) detach();

		/* Try context-specific generic commands first */
		if(parm_context) {
			if(parm_internal_local_commands) {
				strcpy(buf, parm_internal_local_commands);
				strcat(buf, "/");
				strcat(buf, parm_context);
				strcat(buf, "/");
				strcat(buf, parm_generic_cmd[1]);
				execv(buf, parm_generic_cmd+1);
			}

			if(parm_internal_global_commands) {
				strcpy(buf, parm_internal_global_commands);
				strcat(buf, "/");
				strcat(buf, parm_context);
				strcat(buf, "/");
				strcat(buf, parm_generic_cmd[1]);
				execv(buf, parm_generic_cmd+1);
			}
		}

		/* Failing that, try non-context-specific */
		if(parm_internal_local_commands) {
			strcpy(buf, parm_internal_local_commands);
			strcat(buf, "/");
			strcat(buf, parm_generic_cmd[1]);
			execv(buf, parm_generic_cmd+1);
		}

		if(parm_internal_global_commands) {
			strcpy(buf, parm_internal_global_commands);
			strcat(buf, "/");
			strcat(buf, parm_generic_cmd[1]);
			execv(buf, parm_generic_cmd+1);
		}

		printf("%s: Error: %s - %s\n",myname,
			buf, strerror(errno));
		exit(255);
	}

	printf("%s: Failure: No CMD, EXEC, or GENERIC-CMD.\n",myname);
	exit(255);
}

void
process(f, is_the_real_thing)
FILE *f;
int is_the_real_thing;
{
	int line_argc;
	char **line_argv;
	struct key *pk;

	while(1) {
		if(!get_a_line(f, &line_argc, &line_argv)) {
			if(is_the_real_thing) {
				printf(
		"%s: Failure: No blank line after request\n",myname);
				exit(255);
			}
			return;
		}

		if(line_argc == 0) {
			if(is_the_real_thing) return;
			continue;
		}

		Strlwr(line_argv[0]);
		for(pk = keys; pk->name; pk++) {
			if(!strcmp(line_argv[0], pk->name)) {
				(*pk->func)(line_argc, line_argv);
				goto ok;
			}
		}
		printf("%s: Failure: %s not recognized\n",myname, line_argv[0]);
		exit(255);
ok:
		;
	}
}

void
key_internal_print(ac, av)
int ac;
char **av;
{
	printf("%s: Debug:",myname);
	while(*++av) printf(" %s", *av);
	printf("\n");
}

void
key_detach(ac,av)
int ac;
char **av;
{
	parm_detach = TRUE;
}

void
key_nodetach(ac,av)
int ac;
char **av;
{
	parm_detach = FALSE;
}

void
detach(void)
{
	/* I'm not exactly sure how you're supposed to handle stdio here */
	switch(fork()) {
	    case -1:
		printf("%s: Error: fork - %s\n",myname, strerror(errno));
		exit(255);
	    case 0:
		/* Child */
		close(0);
		close(1);
		close(2);
		dup(dup(open("/dev/null", O_RDWR)));
		return;
	    default:
		/* Parent */
		_exit(0);
	}
}

void
putenv_with_prefix(prefix, name, value)
char *prefix;
char *name;
char *value;
{
	char *s;

	if(!value) return;

	s = malloc(strlen(prefix) + strlen(name) + strlen(value) + 3);

	if(!s) nomem();

	sprintf(s, "%s_%s=%s", prefix, name, value);

	putenv(s);
}

void
key_posix_umask(ac, av)
int ac;
char **av;
{
	unsigned i;
	char *s;

	if(ac != 2) {
		printf(
	"%s: Failure: Malformed POSIX-UMASK - wrong number of args\n",myname);
		exit(255);
	}

	i = strtol(av[1], &s, 8);

	if(*s || i < 0 || i > 0777) {
		printf(
	"%s: Failure: Malformed POSIX-UMASK - bad arg\n",myname);
		exit(255);
	}

	umask(i);
}

#ifdef NOPUTENV
/*
 * define our own putenv() if the system doesn't have one.
 * putenv(s): place s (a string of the form "NAME=value") in
 * the environment; replacing any existing NAME.  s is placed in
 * environment, so if you change s, the environment changes (like
 * putenv on a sun).  Binding removed if you putenv something else
 * called NAME.
 */
int
putenv(s)
    char *s;
{
    char *v;
    int varlen, idx;
    extern char **environ;
    char **newenv;
    static int virgin = 1; /* true while "environ" is a virgin */

    v = index(s, '=');
    if(v == 0)
	return 0; /* punt if it's not of the right form */
    varlen = (v + 1) - s;

    for (idx = 0; environ[idx] != 0; idx++) {
	if (strncmp(environ[idx], s, varlen) == 0) {
	    if(v[1] != 0) { /* true if there's a value */
		environ[idx] = s;
		return 0;
	    } else {
		do {
		    environ[idx] = environ[idx+1];
		} while(environ[++idx] != 0);
		return 0;
	    }
	}
    }
    
    /* add to environment (unless no value; then just return) */
    if(v[1] == 0)
	return 0;
    if(virgin) {
	register i;

	newenv = (char **) malloc((unsigned) ((idx + 2) * sizeof(char*)));
	if(newenv == 0)
	    return -1;
	for(i = idx-1; i >= 0; --i)
	    newenv[i] = environ[i];
	virgin = 0;     /* you're not a virgin anymore, sweety */
    } else {
	newenv = (char **) realloc((char *) environ,
				   (unsigned) ((idx + 2) * sizeof(char*)));
	if (newenv == 0)
	    return -1;
    }

    environ = newenv;
    environ[idx] = s;
    environ[idx+1] = 0;
    
    return 0;
}
#endif /* NOPUTENV */
