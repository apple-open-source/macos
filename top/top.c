/*
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "top.h"

/* Program options. */
char		top_opt_c = 'n';
boolean_t	top_opt_f = TRUE;
unsigned	top_opt_i = 10;
boolean_t	top_opt_L = FALSE;
boolean_t	top_opt_l = FALSE;
unsigned	top_opt_l_samples = 0;
unsigned	top_opt_n = TOP_MAX_NPROCS;
top_sort_key_t	top_opt_O = TOP_SORT_pid;
boolean_t	top_opt_O_ascend = FALSE;
top_sort_key_t	top_opt_o = TOP_SORT_pid;
boolean_t	top_opt_o_ascend = FALSE;
char *          top_opt_p_format=NULL;
char *          top_opt_P_legend=NULL;
boolean_t	top_opt_r = TRUE;
boolean_t	top_opt_S = FALSE;
unsigned	top_opt_s = 1;
boolean_t	top_opt_t = FALSE;
boolean_t	top_opt_U = FALSE;
boolean_t	top_opt_U_uid = FALSE;
boolean_t	top_opt_w = FALSE;
boolean_t	top_opt_x = TRUE;

/* Prototypes. */
static void
top_p_opts_parse(int a_argc, char **a_argv);
static void
top_p_usage(void);

int
main(int argc, char **argv)
{
	/* Parse arguments. */
	top_p_opts_parse(argc, argv);

	/*
	 * Make sure the user doesn't mind logging mode, if not running on a
	 * tty.
	 */
	if (isatty(0)) {
	  if (top_opt_l) return log_run();        /* Run in logging mode. */
	  else           return disp_run();       /* Run interactively. */
	} else { // not a tty
	  if (top_opt_L) {
	    fprintf(stderr, "top: Not running on a tty\n");
	    exit(1);
	  } else return log_run();
	}
}

const char *
top_sort_key_str(top_sort_key_t a_key, boolean_t a_ascend)
{
	const char	*strings[] = {
		"-command", "+command",
		"-cpu", "+cpu",
		"-pid", "+pid",
		"-prt", "+prt",
		"-reg", "+reg",
		"-rprvt", "+rprvt",
		"-rshrd", "+rshrd",
		"-rsize", "+rsize",
		"-th", "+th",
		"-time", "+time",
		"-uid", "+uid",
		"-username", "+username",
		"-vprvt", "+vprvt",
		"-vsize", "+vsize"
	};

	return a_ascend ? strings[a_key * 2] : strings[a_key * 2 + 1];
}

static void
top_p_opts_parse(int a_argc, char **a_argv)
{
	int	c;

	/* Turn off automatic error reporting. */
	opterr = 0;

	/* Iteratively process command line arguments. */
	while ((c = getopt(a_argc, a_argv, "ac:deFfhi:kLl:n:O:o:p:P:RrSs:TtU:uWwXx")) != -1) {
		switch (c) {
		case 'a': case 'd': case 'e':  top_opt_c = c; break;
		case 'c':
			top_opt_c = optarg[0];
			switch (top_opt_c) {
			case 'a': case 'd': case 'e': case 'n':
				break;
			default:
				fprintf(stderr,
				    "top: Invalid argument: -c %s\n", optarg);
				top_p_usage();
				exit(1);
			}
			break;
		case 'F': top_opt_f = FALSE; break;
		case 'f': top_opt_f = TRUE;  break;
		case 'i': {
			char	*p;

			errno = 0;
			top_opt_i = strtoul(optarg, &p, 0);
			if ((errno == EINVAL && top_opt_i == 0)
			    || (errno == ERANGE
				&& top_opt_i == ULONG_MAX)
			    || top_opt_i < 1 || top_opt_i > TOP_MAX_INTERVAL
			    || *p != '\0') {
				fprintf(stderr,
				    "top: Invalid argument: -i %s\n", optarg);
				top_p_usage();
				exit(1);
			}
			break;
		}
		case 'h': top_p_usage(); exit(0);
		case 'k': /* Ignore. */	break;
		case 'L': top_opt_L = TRUE; break;
		case 'l': {
			char	*p;

			top_opt_l = TRUE;
			errno = 0;
			top_opt_l_samples = strtoul(optarg, &p, 0);
			if ((errno == EINVAL && top_opt_l_samples == 0)
			    || (errno == ERANGE
				&& top_opt_l_samples == ULONG_MAX)
			    || *p != '\0') {
				fprintf(stderr,
				    "top: Invalid argument: -l %s\n", optarg);
				top_p_usage();
				exit(1);
			}
			break;
		}
		case 'n': {
			char	*p;

			errno = 0;
			top_opt_n = strtoul(optarg, &p, 0);
			if ((errno == EINVAL && top_opt_n == 0)
			    || (errno == ERANGE && top_opt_n == ULONG_MAX)
			    || *p != '\0'
			    || top_opt_n > TOP_MAX_NPROCS) {
				fprintf(stderr,
				    "top: Invalid argument: -n %s\n", optarg);
				top_p_usage();
				exit(1);
			}
			break;
		}
		case 'O': /* Check for + or - prefix. */
			if (optarg[0] == '+' || optarg[0] == '-') {
			  top_opt_O_ascend = (optarg[0] == '+');
			  optarg++;
			}

			if (strcasecmp(optarg, "command") == 0)       top_opt_O = TOP_SORT_command;
			else if (strcasecmp(optarg, "cpu") == 0)      top_opt_O = TOP_SORT_cpu;
			else if (strcasecmp(optarg, "pid") == 0)      top_opt_O = TOP_SORT_pid;
			else if (strcasecmp(optarg, "prt") == 0)      top_opt_O = TOP_SORT_prt;
			else if (strcasecmp(optarg, "reg") == 0)      top_opt_O = TOP_SORT_reg;
			else if (strcasecmp(optarg, "rprvt") == 0)    top_opt_O = TOP_SORT_rprvt;
			else if (strcasecmp(optarg, "rshrd") == 0)    top_opt_O = TOP_SORT_rshrd;
			else if (strcasecmp(optarg, "rsize") == 0)    top_opt_O = TOP_SORT_rsize;
			else if (strcasecmp(optarg, "th") == 0)       top_opt_O = TOP_SORT_th;
			else if (strcasecmp(optarg, "time") == 0)     top_opt_O = TOP_SORT_time;
			else if (strcasecmp(optarg, "uid") == 0)      top_opt_O = TOP_SORT_uid;
			else if (strcasecmp(optarg, "username") == 0) top_opt_O = TOP_SORT_username;
			else if (strcasecmp(optarg, "vprvt") == 0)    top_opt_O = TOP_SORT_vprvt;
			else if (strcasecmp(optarg, "vsize") == 0)    top_opt_O = TOP_SORT_vsize;
			else {
				fprintf(stderr, "top: Invalid argument: -O %s\n", optarg);
				top_p_usage();
				exit(1);
			}

			break;
		case 'o': /* Check for + or - prefix. */
			if (optarg[0] == '+' || optarg[0] == '-') {
			  top_opt_o_ascend = (optarg[0] == '+');
			  optarg++;
			}

			if (strcasecmp(optarg, "command") == 0)       top_opt_o = TOP_SORT_command;
			else if (strcasecmp(optarg, "cpu") == 0)      top_opt_o = TOP_SORT_cpu;
			else if (strcasecmp(optarg, "pid") == 0)      top_opt_o = TOP_SORT_pid;
			else if (strcasecmp(optarg, "prt") == 0)      top_opt_o = TOP_SORT_prt;
			else if (strcasecmp(optarg, "reg") == 0)      top_opt_o = TOP_SORT_reg;
			else if (strcasecmp(optarg, "rprvt") == 0)    top_opt_o = TOP_SORT_rprvt;
			else if (strcasecmp(optarg, "rshrd") == 0)    top_opt_o = TOP_SORT_rshrd;
			else if (strcasecmp(optarg, "rsize") == 0)    top_opt_o = TOP_SORT_rsize;
			else if (strcasecmp(optarg, "th") == 0)       top_opt_o = TOP_SORT_th;
			else if (strcasecmp(optarg, "time") == 0)     top_opt_o = TOP_SORT_time;
			else if (strcasecmp(optarg, "uid") == 0)      top_opt_o = TOP_SORT_uid;
			else if (strcasecmp(optarg, "username") == 0) top_opt_o = TOP_SORT_username;
			else if (strcasecmp(optarg, "vprvt") == 0)    top_opt_o = TOP_SORT_vprvt;
			else if (strcasecmp(optarg, "vsize") == 0)    top_opt_o = TOP_SORT_vsize;
			else {
				fprintf(stderr, "top: Invalid argument: -o %s\n", optarg);
				top_p_usage();
				exit(1);
			}

			break;
		case 'P':
		        top_opt_P_legend = (char *)malloc(strlen(optarg));
		        strcpy(top_opt_P_legend, optarg);
		        break;
		case 'p':
		        top_opt_p_format = (char *)malloc(strlen(optarg));
		        strcpy(top_opt_p_format, optarg);
		        break;
		case 'R': top_opt_r = FALSE; break;
		case 'r': top_opt_r = TRUE; break;
		case 'S': top_opt_S = TRUE; break;
		case 's': {
			char	*p;

			errno = 0;
			top_opt_s = strtoul(optarg, &p, 0);
			if ((errno == EINVAL && top_opt_s == 0)
			    || (errno == ERANGE	&& top_opt_s == ULONG_MAX)
			    || *p != '\0') {
				fprintf(stderr, "top: Invalid argument: -s %s\n", optarg);
				top_p_usage();
				exit(1);
			}
			break;
		}
		case 'T': top_opt_t = FALSE; break;
		case 't': top_opt_t = TRUE; break;
		case 'U': {
			char		*p;
			struct passwd	*pwd;

			top_opt_U = TRUE;
			errno = 0;
			top_opt_U_uid = strtoul(optarg, &p, 0);
			if ((errno == EINVAL && top_opt_U_uid == 0)
			    || (errno == ERANGE	&& top_opt_U_uid == ULONG_MAX)
			    || *p != '\0') {
				/*
				 * The argument isn't a number. Try it as a
				 * username.
				 */
				pwd = getpwnam(optarg);

				if (pwd != NULL) top_opt_U_uid = pwd->pw_uid;
			} else {
				/* Verify that the number is a valid uid. */
				pwd = getpwuid((uid_t)top_opt_U_uid);
			}

			endpwent();
			if (pwd == NULL) {
				fprintf(stderr, "top: Invalid argument: -U %s\n", optarg);
				top_p_usage();
				exit(1);
			}

			break;
		}
		case 'u':
			top_opt_o = TOP_SORT_cpu;
			top_opt_O = TOP_SORT_time;
			break;
		case 'W': top_opt_w = FALSE; break;
		case 'w': top_opt_w = TRUE; break;
		case 'X': top_opt_x = FALSE; break;
		case 'x':
			top_opt_x = TRUE;
			top_opt_s = 1;
			top_opt_S = FALSE;
			break;
		case '?':
		default:
		  fprintf(stderr, "top: Unrecognized or missing option %c\n",optopt);
			top_p_usage();
			exit(1);
		}
	}

	/* Check for <nprocs> without preceding -n. */
	if (optind < a_argc) {
		char	*p;
		errno = 0;
		top_opt_n = strtoul(a_argv[optind], &p, 0);
		if ((errno == EINVAL && top_opt_n == 0)
		    || (errno == ERANGE && top_opt_n == ULONG_MAX)
		    || *p != '\0'
		    || top_opt_n > TOP_MAX_NPROCS) {
			fprintf(stderr, "top: Invalid argument: %s\n", a_argv[optind]);
			top_p_usage();
			exit(1);
		}
		optind++;
	}

	/* Check for trailing command line garbage. */
	if (optind < a_argc) {
		fprintf(stderr, "top: Unrecognized trailing arguments:");
		for (; optind < a_argc; optind++) fprintf(stderr, " %s", a_argv[optind]);
		fprintf(stderr, "\n");

		top_p_usage();
		exit(1);
	}
}

static void
top_p_usage(void)
{
	fprintf(stderr, "\
top usage: top [-a | -d | -e | -c <mode>]\n\
               [-F | -f]\n\
               [-h]\n\
               [-i <interval>]\n\
               [-k]\n\
               [-L | -l <samples>]\n\
               [-o <key>] [-O <skey>]\n\
               [-p <format>] [-P <legend>]\n\
               [-R | -r]\n\
               [-s <delay>]\n\
               [-T | -t]\n\
               [-U <user>]\n\
               [-u]\n\
               [-W | -w]\n\
               [-X | x]\n\
               [[-n] <nprocs>]\n");
}
