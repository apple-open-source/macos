/* $Xorg: auth.c,v 1.4 2000/08/17 19:54:01 cpqbld Exp $ */

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
/* $XFree86: xc/programs/rstart/auth.c,v 1.5 2001/07/25 15:05:15 dawes Exp $ */

#include <stdio.h>
#include <X11/Xos.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>


static char * Strupr ( char *s0 );
struct auth_info * find_or_create_auth ( char *s );
void key_auth ( int ac, char **av );
void key_internal_auth_program ( int ac, char **av );
void key_internal_auth_input ( int ac, char **av );
void do_auth ( void );
char * expand ( char *s, int ac, char **av );

/* server.c */
extern void nomem ( void );

extern char myname[];

struct list_of_argv {
	struct list_of_argv *next;
	int	argc;
	char	**argv;
};

struct auth_info {
	struct auth_info	*next;
	char			*name;
	struct list_of_argv	*data;
	char			**program;
	char			**input;
};

struct auth_info *auth_schemes = NULL;

static char *
Strupr(s0)
char *s0;
{
	char *s;

	for(s = s0; *s; s++) {
		if(islower(*s)) *s = toupper(*s);
	}
	return s0;
}

/* Argument "s" is overwritten and the memory used, so it must not be	*/
/* deallocated or subsequently used by the caller.			*/
struct auth_info *
find_or_create_auth(s)
char *s;
{
	struct auth_info *auth;

	Strupr(s);

	for(auth = auth_schemes; auth; auth=auth->next) {
		if(!strcmp(s, auth->name)) return auth;
	}

	auth = (struct auth_info *)malloc(sizeof(*auth));
	if(!auth) nomem();

	auth->next = auth_schemes;
	auth->name = s;
	auth->data = NULL;
	auth->program = NULL;
	auth->input = NULL;
	auth_schemes = auth;

	return auth;
}

void
key_auth(ac, av)
int ac;
char **av;
{
	struct list_of_argv *lav;
	struct auth_info *auth;

	if(ac < 2) {
		printf(
		    "%s: Failure: Malformed AUTH\n",myname);
		exit(255);
	}

	auth = find_or_create_auth(av[1]);

	lav = (struct list_of_argv *)malloc(sizeof(*lav));
	if(!lav) nomem();

	lav->next = auth->data;
	lav->argc = ac-2;
	lav->argv = av+2;
	auth->data = lav;
}

void
key_internal_auth_program(ac, av)
int ac;
char **av;
{
	struct auth_info *auth;

	if(ac < 4) {
		printf(
		    "%s: Failure: Malformed INTERNAL-AUTH-PROGRAM\n",myname);
		exit(255);
	}

	auth = find_or_create_auth(av[1]);
	auth->program = av + 2;
}

void
key_internal_auth_input(ac, av)
int ac;
char **av;
{
	struct auth_info *auth;

	if(ac < 2) {
		printf(
		    "%s: Failure: Malformed INTERNAL-AUTH-INPUT\n",myname);
		exit(255);
	}

	auth = find_or_create_auth(av[1]);
	auth->input = av + 2;
}

void
do_auth(void)
{
	struct auth_info *auth;
	int p[2];
	char **pp;
	struct list_of_argv *lav;
	char *s;
	int pid;
	int status;

	for(auth = auth_schemes; auth; auth = auth->next) {
		if(!auth->data) continue;
		if(!auth->program) {
			printf(
"%s: Warning: no %s authorization program specified in this context\n",myname,
				auth->name);
			continue;
		}

		if(pipe(p)) {
			printf("%s: Error: pipe - %s\n",myname, strerror(errno));
			exit(255);
		}

		fflush(stdout);		/* Can't hurt. */

		switch(pid = fork()) {
		    case -1:
			printf("%s: Error: fork - %s\n",myname, strerror(errno));
			exit(255);
		    case 0:	/* kid */
			close(0);
			dup(p[0]);
			close(p[0]);
			close(p[1]);
			execvp(auth->program[0], auth->program+1);
			printf("%s: Error: %s - %s\n",myname, auth->program[0],
				strerror(errno));
			exit(255);
			break;
		    default:	/* parent */
			close(p[0]);
			for(lav = auth->data; lav; lav=lav->next) {
				for(pp = auth->input; *pp; pp++) {
					s = expand(*pp, lav->argc, lav->argv);
					write(p[1], s, strlen(s));
					write(p[1], "\n", 1);
				}
			}
			close(p[1]);
			while(wait(&status) != pid) /* LOOP */;
			if(status) {
				printf(
				"%s: Warning: %s authorization setup failed\n",myname, auth->name);
			}
			break;
		}
	}
}

char *
expand(s, ac, av)
char *s;
int ac;
char **av;
{
	static char buf[BUFSIZ];
	char *p;
	int i;

	p = buf;
	while(*s) {
		if(*s == '$') {
			s++;
			if(*s == '$') {
				*p++ = *s++;
				continue;
			}
			if(!isdigit(*s)) {
				printf(
	"%s: Failure: bad $ in configuration: non-digit after $\n",myname);
				exit(255);
			}
			i = (int)strtol(s, &s, 10);
			if(i > ac) {
				printf(
		"%s: Failure: not enough arguments to AUTH\n",myname);
				exit(255);
			}
			strcpy(p, av[i-1]);
			p += strlen(p);
		} else *p++ = *s++;
	}
	*p = '\0';

	return buf;
}
