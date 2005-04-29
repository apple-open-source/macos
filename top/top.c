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
char		top_opt_c;
boolean_t	top_opt_f;
boolean_t	top_opt_L;
boolean_t	top_opt_l;
unsigned	top_opt_l_samples;
unsigned	top_opt_n;
top_sort_key_t	top_opt_O;
boolean_t	top_opt_O_ascend;
top_sort_key_t	top_opt_o;
boolean_t	top_opt_o_ascend;
boolean_t	top_opt_r;
boolean_t	top_opt_S;
unsigned	top_opt_s;
boolean_t	top_opt_t;
boolean_t	top_opt_U;
boolean_t	top_opt_U_uid;
boolean_t	top_opt_w;
#ifdef TOP_DEPRECATED
boolean_t	top_opt_x;
#endif

/* Prototypes. */
static void
top_p_opts_parse(int a_argc, char **a_argv);
static void
top_p_usage(void);

int
main(int argc, char **argv)
{
	int		retval;
	boolean_t	tty;

	/* Parse arguments. */
	top_p_opts_parse(argc, argv);

	tty = isatty(0);

	/*
	 * Make sure the user doesn't mind logging mode, if not running on a
	 * tty.
	 */
	if (tty == FALSE && top_opt_L) {
		fprintf(stderr, "top: Not running on a tty\n");
		exit(1);
	}

	/* Determine whether to run interactively or in logging mode. */
	if (tty && top_opt_l == FALSE) {
		/* Run interactively. */
		if (disp_run()) {
			retval = 1;
			goto RETURN;
		}
	} else {
		/* Run in logging mode. */
		if (log_run()) {
			retval = 1;
			goto RETURN;
		}
	}

	retval = 0;
	RETURN:
	return retval;
}

const char *
top_sort_key_str(top_sort_key_t a_key, boolean_t a_ascend)
{
	const char	*retval;
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

	if (a_ascend == FALSE) {
		retval = strings[a_key * 2];
	} else {
		retval = strings[a_key * 2 + 1];
	}

	return retval;
}

static void
top_p_opts_parse(int a_argc, char **a_argv)
{
	int	c;

	/* Set defaults. */
	top_opt_c = 'n';
	top_opt_f = TRUE;
	top_opt_L = FALSE;
	top_opt_l = FALSE;
	top_opt_n = TOP_MAX_NPROCS;
	top_opt_O = TOP_SORT_pid;
	top_opt_O_ascend = FALSE;
	top_opt_o = TOP_SORT_pid;
	top_opt_o_ascend = FALSE;
	top_opt_r = TRUE;
	top_opt_S = FALSE;
	top_opt_s = 1;
	top_opt_t = FALSE;
	top_opt_U = FALSE;
	top_opt_w = FALSE;
#ifdef TOP_DEPRECATED
	top_opt_x = TRUE;
#endif

	/* Turn off automatic error reporting. */
	opterr = 0;

	/* Iteratively process command line arguments. */
	while ((c = getopt(a_argc, a_argv,
#ifdef TOP_DEPRECATED
	    "ac:deFfhkLl:n:O:o:RrSs:TtU:uWwXx"
#else
	    "c:FfhLl:n:O:o:RrSs:TtU:WwXx"
#endif
	    )) != -1) {
		switch (c) {
#ifdef TOP_DEPRECATED
		case 'a':
			top_opt_c = 'a';
			break;
#endif
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
#ifdef TOP_DEPRECATED
		case 'd':
			top_opt_c = 'd';
			break;
		case 'e':
			top_opt_c = 'e';
			break;
#endif
		case 'F':
			top_opt_f = FALSE;
			break;
		case 'f':
			top_opt_f = TRUE;
			break;
		case 'h':
			top_p_usage();
			exit(0);
#ifdef TOP_DEPRECATED
		case 'k':
			/* Ignore. */
			break;
#endif
		case 'L':
			top_opt_L = TRUE;
			break;
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
		case 'O': {
			int	i;

			/* Check for + or - prefix. */
			if (optarg[0] == '+') {
				top_opt_O_ascend = TRUE;
				i = 1;
			} else if (optarg[0] == '-') {
				top_opt_O_ascend = FALSE;
				i = 1;
			} else {
				i = 0;
			}

			if (strcmp(&optarg[i], "command") == 0) {
				top_opt_O = TOP_SORT_command;
			} else if (strcmp(&optarg[i], "cpu") == 0) {
				top_opt_O = TOP_SORT_cpu;
			} else if (strcmp(&optarg[i], "pid") == 0) {
				top_opt_O = TOP_SORT_pid;
			} else if (strcmp(&optarg[i], "prt") == 0) {
				top_opt_O = TOP_SORT_prt;
			} else if (strcmp(&optarg[i], "reg") == 0) {
				top_opt_O = TOP_SORT_reg;
			} else if (strcmp(&optarg[i], "rprvt") == 0) {
				top_opt_O = TOP_SORT_rprvt;
			} else if (strcmp(&optarg[i], "rshrd") == 0) {
				top_opt_O = TOP_SORT_rshrd;
			} else if (strcmp(&optarg[i], "rsize") == 0) {
				top_opt_O = TOP_SORT_rsize;
			} else if (strcmp(&optarg[i], "th") == 0) {
				top_opt_O = TOP_SORT_th;
			} else if (strcmp(&optarg[i], "time") == 0) {
				top_opt_O = TOP_SORT_time;
			} else if (strcmp(&optarg[i], "uid") == 0) {
				top_opt_O = TOP_SORT_uid;
			} else if (strcmp(&optarg[i], "username") == 0) {
				top_opt_O = TOP_SORT_username;
			} else if (strcmp(&optarg[i], "vprvt") == 0) {
				top_opt_O = TOP_SORT_vprvt;
			} else if (strcmp(&optarg[i], "vsize") == 0) {
				top_opt_O = TOP_SORT_vsize;
			} else {
				fprintf(stderr,
				    "top: Invalid argument: -O %s\n", optarg);
				top_p_usage();
				exit(1);
			}

			break;
		}
		case 'o': {
			int	i;

			/* Check for + or - prefix. */
			if (optarg[0] == '+') {
				top_opt_o_ascend = TRUE;
				i = 1;
			} else if (optarg[0] == '-') {
				top_opt_o_ascend = FALSE;
				i = 1;
			} else {
				i = 0;
			}

			if (strcmp(&optarg[i], "command") == 0) {
				top_opt_o = TOP_SORT_command;
			} else if (strcmp(&optarg[i], "cpu") == 0) {
				top_opt_o = TOP_SORT_cpu;
			} else if (strcmp(&optarg[i], "pid") == 0) {
				top_opt_o = TOP_SORT_pid;
			} else if (strcmp(&optarg[i], "prt") == 0) {
				top_opt_o = TOP_SORT_prt;
			} else if (strcmp(&optarg[i], "reg") == 0) {
				top_opt_o = TOP_SORT_reg;
			} else if (strcmp(&optarg[i], "rprvt") == 0) {
				top_opt_o = TOP_SORT_rprvt;
			} else if (strcmp(&optarg[i], "rshrd") == 0) {
				top_opt_o = TOP_SORT_rshrd;
			} else if (strcmp(&optarg[i], "rsize") == 0) {
				top_opt_o = TOP_SORT_rsize;
			} else if (strcmp(&optarg[i], "th") == 0) {
				top_opt_o = TOP_SORT_th;
			} else if (strcmp(&optarg[i], "time") == 0) {
				top_opt_o = TOP_SORT_time;
			} else if (strcmp(&optarg[i], "uid") == 0) {
				top_opt_o = TOP_SORT_uid;
			} else if (strcmp(&optarg[i], "username") == 0) {
				top_opt_o = TOP_SORT_username;
			} else if (strcmp(&optarg[i], "vprvt") == 0) {
				top_opt_o = TOP_SORT_vprvt;
			} else if (strcmp(&optarg[i], "vsize") == 0) {
				top_opt_o = TOP_SORT_vsize;
			} else {
				fprintf(stderr,
				    "top: Invalid argument: -o %s\n", optarg);
				top_p_usage();
				exit(1);
			}

			break;
		}
		case 'R':
			top_opt_r = FALSE;
			break;
		case 'r':
			top_opt_r = TRUE;
			break;
		case 'S':
			top_opt_S = TRUE;
			break;
		case 's': {
			char	*p;

			errno = 0;
			top_opt_s = strtoul(optarg, &p, 0);
			if ((errno == EINVAL && top_opt_s == 0)
			    || (errno == ERANGE
				&& top_opt_s == ULONG_MAX)
			    || *p != '\0') {
				fprintf(stderr,
				    "top: Invalid argument: -s %s\n", optarg);
				top_p_usage();
				exit(1);
			}
			break;
		}
		case 'T':
			top_opt_t = FALSE;
			break;
		case 't':
			top_opt_t = TRUE;
			break;
		case 'U': {
			char		*p;
			struct passwd	*pwd;

			top_opt_U = TRUE;
			errno = 0;
			top_opt_U_uid = strtoul(optarg, &p, 0);
			if ((errno == EINVAL && top_opt_U_uid == 0)
			    || (errno == ERANGE
				&& top_opt_U_uid == ULONG_MAX)
			    || *p != '\0') {
				/*
				 * The argument isn't a number. Try it as a
				 * username.
				 */
				pwd = getpwnam(optarg);

				if (pwd != NULL) {
					top_opt_U_uid = pwd->pw_uid;
				}
			} else {
				/* Verify that the number is a valid uid. */
				pwd = getpwuid((uid_t)top_opt_U_uid);
			}

			endpwent();
			if (pwd == NULL) {
				fprintf(stderr,
				    "top: Invalid argument: -U %s\n",
				    optarg);
				top_p_usage();
				exit(1);
			}

			break;
		}
#ifdef TOP_DEPRECATED
		case 'u':
			top_opt_o = TOP_SORT_cpu;
			top_opt_O = TOP_SORT_time;
			break;
#endif
		case 'W':
			top_opt_w = FALSE;
			break;
		case 'w':
			top_opt_w = TRUE;
			break;
#ifdef TOP_DEPRECATED
		case 'X':
			top_opt_x = FALSE;
			break;
		case 'x':
			top_opt_x = TRUE;
			top_opt_s = 1;
			top_opt_S = FALSE;
			break;
#endif
		case '?':
		default:
			fprintf(stderr,
			    "top: Unrecognized or missing option\n");
			top_p_usage();
			exit(1);
		}
	}

#ifdef TOP_DEPRECATED
	/* Check for <nprocs> without preceding -n. */
	if (optind < a_argc) {
		char	*p;
		errno = 0;
		top_opt_n = strtoul(a_argv[optind], &p, 0);
		if ((errno == EINVAL && top_opt_n == 0)
		    || (errno == ERANGE && top_opt_n == ULONG_MAX)
		    || *p != '\0'
		    || top_opt_n > TOP_MAX_NPROCS) {
			fprintf(stderr,
			    "top: Invalid argument: %s\n", a_argv[optind]);
			top_p_usage();
			exit(1);
		}
		optind++;
	}
#endif

	/* Check for trailing command line garbage. */
	if (optind < a_argc) {
		fprintf(stderr, "top: Incorrect trailing arguments:");
		for (; optind < a_argc; optind++) {
			fprintf(stderr, " %s", a_argv[optind]);
		}
		fprintf(stderr, "\n");

		top_p_usage();
		exit(1);
	}
}

static void
top_p_usage(void)
{
#ifdef TOP_DEPRECATED
	fprintf(stderr, "\
top usage: top [-a | -d | -e | -c <mode>]\n\
               [-F | -f]\n\
               [-h]\n\
               [-k]\n\
               [-L | -l <samples>]\n\
               [-o <key>] [-O <skey>]\n\
               [-R | -r]\n\
               [-s <delay>]\n\
               [-T | -t]\n\
               [-U <user>]\n\
               [-u]\n\
               [-W | -w]\n\
               [-X | x]\n\
               [[-n] <nprocs>]\n");
#else
	fprintf(stderr, "\
top usage: top [-c <mode>]\n\
               [-F | -f]\n\
               [-h]\n\
               [-L | -l <samples>]\n\
               [-o <key>] [-O <skey>]\n\
               [-R | -r]\n\
               [-s <delay>]\n\
               [-T | -t]\n\
               [-U <user>]\n\
               [-W | -w]\n\
               [-n <nprocs>]\n");
#endif	
}
