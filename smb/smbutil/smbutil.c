#include <sys/param.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <sysexits.h>

#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>

#include "common.h"

extern char *__progname;

static void help(void);
static void help_usage(void);
static int  cmd_crypt(int argc, char *argv[]);
static int  cmd_help(int argc, char *argv[]);

int verbose;

typedef int cmd_fn_t (int argc, char *argv[]);
typedef void cmd_usage_t (void);

#define	CMDFL_NO_KMOD	0x0001

static struct commands {
	const char *	name;
	cmd_fn_t*	fn;
	cmd_usage_t *	usage;
	int 		flags;
} commands[] = {
	{"crypt",	cmd_crypt,	NULL, CMDFL_NO_KMOD},
	{"help",	cmd_help,	help_usage, CMDFL_NO_KMOD},
	{"lookup",	cmd_lookup,	lookup_usage, CMDFL_NO_KMOD},
	{"status",	cmd_status,	status_usage, 0},
	{"view",	cmd_view,	view_usage, 0},
	{NULL, NULL, NULL, 0}
};

static struct commands *
lookupcmd(const char *name)
{
	struct commands *cmd;

	for (cmd = commands; cmd->name; cmd++) {
		if (strcmp(cmd->name, name) == 0)
			return cmd;
	}
	return NULL;
}

int
cmd_crypt(int argc, char *argv[])
{
	char *cp, *psw;
    
	if (argc < 2)
		psw = getpass("Password:");
	else
		psw = argv[1];
	/* XXX Better to embed malloc/free in smb_simplecrypt? */
	cp = malloc(4 + 2 * strlen(psw));
	if (cp == NULL)
		errx(EX_DATAERR, "out of memory");
	smb_simplecrypt(cp, psw);
	printf("%s\n", cp);
	free(cp);
	exit(0);
}

int
cmd_help(int argc, char *argv[])
{
	struct commands *cmd;
	char *cp;
    
	if (argc < 2)
		help_usage();
	cp = argv[1];
	cmd = lookupcmd(cp);
	if (cmd == NULL)
		errx(EX_DATAERR, "unknown command %s", cp);
	if (cmd->usage == NULL)
		errx(EX_DATAERR, "no specific help for command %s", cp);
	cmd->usage();
	exit(0);
}

int
main(int argc, char *argv[])
{
	struct commands *cmd;
	char *cp;
	int opt;
        extern void dropsuid();

	dropsuid();

	if (argc < 2)
		help();

	while ((opt = getopt(argc, argv, "hv")) != EOF) {
		switch (opt) {
		    case 'h':
			help();
			/*NOTREACHED */
		    case 'v':
			verbose = 1;
			break;
		    default:
			warnx("invalid option %c", opt);
			help();
			/*NOTREACHED */
		}
	}
	if (optind >= argc)
		help();

	cp = argv[optind];
	cmd = lookupcmd(cp);
	if (cmd == NULL)
		errx(EX_DATAERR, "unknown command %s", cp);

	if ((cmd->flags & CMDFL_NO_KMOD) == 0 && smb_lib_init() != 0)
		exit(1);

	argc -= optind;
	argv += optind;
	optind = optreset = 1;
	return cmd->fn(argc, argv);
}

static void
help(void) {
	printf("\n");
	printf("usage: %s [-hv] subcommand [args]\n", __progname);
	printf("where subcommands are:\n"
	" crypt		slightly obscure password\n"
	" help		display help on specified subcommand\n"
	" lc 		display active connections\n"
	" login		login to specified host\n"
	" logout 	logout from specified host\n"
	" lookup 	resolve NetBIOS name to IP address\n"
	" print		print file to the specified remote printer\n"
	" status 	resolve IP address or DNS name to NetBIOS names\n"
	" view		list resources on specified host\n"
	"\n");
	exit(1);
}

static void
help_usage(void) {
	printf("usage: smbutil help command\n");
	exit(1);
}
