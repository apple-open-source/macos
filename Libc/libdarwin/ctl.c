#include "internal.h"

#pragma mark Definitions
#define CTL_OUTPUT_WIDTH (80)
#define CTL_OUTPUT_OPTARG_PAD (28)
#define CTL_OUTPUT_LIST_PAD (4)
#define SUBCOMMAND_LINKER_SET "__subcommands"

#pragma mark Module Globals
static const os_subcommand_t _help_cmd;

#pragma mark Module Routines
static char *
_os_subcommand_copy_optarg_usage(const os_subcommand_t *osc,
		const struct option *opt, os_subcommand_optarg_format_t format,
		os_subcommand_option_t *scopt)
{
	char optbuff[64] = "";
	char argbuff[64] = "";
	char *final = NULL;
	int ret = -1;

	snprintf(optbuff, sizeof(optbuff), "--%s", opt->name);

	if (osc->osc_info) {
		osc->osc_info(osc, format, opt, scopt);
	}

	switch (opt->has_arg) {
	case no_argument:
		break;
	case optional_argument:
		snprintf(argbuff, sizeof(argbuff), "[=%s]", scopt->osco_argdesc);
		break;
	case required_argument:
		snprintf(argbuff, sizeof(argbuff), "=<%s>", scopt->osco_argdesc);
		break;
	default:
		__builtin_unreachable();
	}

	ret = asprintf(&final, "%s%s", optbuff, argbuff);
	if (ret < 0) {
		os_assert_zero(ret);
	}

	return final;
}

static void
_os_subcommand_print_optarg_usage(const os_subcommand_t *osc,
		const struct option *opt, FILE *f)
{
	os_subcommand_option_t scopt = {
		.osco_flags = 0,
		.osco_argdesc = opt->name,
	};
	char *__os_free usage = NULL;
	char *braces[2] = {
		"",
		"",
	};

	usage = _os_subcommand_copy_optarg_usage(osc, opt,
			OS_SUBCOMMAND_OPTARG_USAGE, &scopt);
	if (scopt.osco_flags & OS_SUBCOMMAND_OPTION_FLAG_OPTIONAL) {
		braces[0] = "[";
		braces[1] = "]";
	}

	fprintf(f, " %s%s%s", braces[0], usage, braces[1]);
}

static void
_os_subcommand_print_usage(const os_subcommand_t *osc, FILE *f)
{
	const struct option *opts = osc->osc_options;
	const struct option *curopt = NULL;
	size_t i = 0;

	fprintf(f, "usage: %s %s", getprogname(), osc->osc_name);

	while ((curopt = &opts[i]) && curopt->name) {
		_os_subcommand_print_optarg_usage(osc, curopt, f);
		i++;
	}

	fprintf(f, "\n");
}

static void
_os_subcommand_print_optarg_human(const os_subcommand_t *osc,
		const struct option *opt, FILE *f)
{
	os_subcommand_option_t scopt = {
		.osco_flags = 0,
		.osco_argdesc = opt->name,
	};
	char *__os_free usage = NULL;
	char *__os_free human = NULL;

	usage = _os_subcommand_copy_optarg_usage(osc, opt,
			OS_SUBCOMMAND_OPTARG_USAGE, &scopt);
	fprintf(f, "    %-24s", usage);

	human = _os_subcommand_copy_optarg_usage(osc, opt,
			OS_SUBCOMMAND_OPTARG_HUMAN, &scopt);
	wfprintf_np(f, -CTL_OUTPUT_OPTARG_PAD, CTL_OUTPUT_OPTARG_PAD,
			CTL_OUTPUT_WIDTH, "%s", scopt.osco_argdesc);
}

static void
_os_subcommand_print_human(const os_subcommand_t *osc, FILE *f)
{
	const struct option *opts = osc->osc_options;
	const struct option *curopt = NULL;
	size_t i = 0;

	_os_subcommand_print_usage(osc, f);

	while ((curopt = &opts[i]) && curopt->name) {
		_os_subcommand_print_optarg_human(osc, curopt, f);
		i++;
	}
}

static void
_os_subcommand_print_list(const os_subcommand_t *osc, FILE *f)
{
	wfprintf_np(f, CTL_OUTPUT_LIST_PAD, 0, 0, "%-24s %s",
			osc->osc_name, osc->osc_desc);
}

static const os_subcommand_t *
_os_subcommand_find(const char *name)
{
	const os_subcommand_t **oscip = NULL;

	if (strcmp(_help_cmd.osc_name, name) == 0) {
		return &_help_cmd;
	}

	LINKER_SET_FOREACH(oscip, const os_subcommand_t **, SUBCOMMAND_LINKER_SET) {
		const os_subcommand_t *osci = *oscip;

		if (strcmp(osci->osc_name, name) == 0) {
			return osci;
		}
	}

	return NULL;
}

#pragma mark Default Usage
static void
_usage_default(FILE *f)
{
	const os_subcommand_t **oscip = NULL;

	crfprintf_np(f, "usage: %s <subcommand> [...] | help [subcommand]",
			getprogname());
	crfprintf_np(f, "");

	crfprintf_np(f, "subcommands:");
	LINKER_SET_FOREACH(oscip, const os_subcommand_t **, SUBCOMMAND_LINKER_SET) {
		const os_subcommand_t *osci = *oscip;
		_os_subcommand_print_list(osci, f);
	}

	_os_subcommand_print_list(&_help_cmd, f);
}

static int
_usage(FILE *f)
{
	_usage_default(f);
	return EX_USAGE;
}

#pragma mark Help Subcommand
static int _help_invoke(const os_subcommand_t *osc,
	int argc,
	const char *argv[]
);

static const os_subcommand_t _help_cmd = {
	.osc_version = OS_SUBCOMMAND_VERSION,
	.osc_flags = 0,
	.osc_name = "help",
	.osc_desc = "prints helpful information",
	.osc_optstring = NULL,
	.osc_options = NULL,
	.osc_info = NULL,
	.osc_invoke = &_help_invoke,
};

static void
_help_print_subcommand(const os_subcommand_t *osc, FILE *f)
{
	wfprintf_np(f, 4, 4, 76, "%-16s%s", osc->osc_name, osc->osc_desc);
}

static void
_help_print_all(FILE *f)
{
	const os_subcommand_t **oscip = NULL;

	_usage_default(f);
	crfprintf_np(f, "");

	crfprintf_np(f, "subcommands:");
	LINKER_SET_FOREACH(oscip, const os_subcommand_t **, SUBCOMMAND_LINKER_SET) {
		const os_subcommand_t *osci = *oscip;
		if (osci->osc_flags & OS_SUBCOMMAND_FLAG_HIDDEN) {
			continue;
		}
		_help_print_subcommand(osci, f);
	}
}

static int
_help_invoke(const os_subcommand_t *osc, int argc, const char *argv[])
{
	const os_subcommand_t *target = NULL;

	if (argc == 1) {
		_help_print_all(stdout);
	} else {
		target = _os_subcommand_find(argv[1]);
		if (!target) {
			crfprintf_np(stderr, "unrecognized subcommand: %s", argv[1]);
			_usage_default(stderr);
			return EX_USAGE;
		}

		_os_subcommand_print_human(target, stdout);
	}

	return 0;
}

#pragma mark API
int
os_subcommand_main(int argc, const char *argv[])
{
	int exitcode = -1;
	const char *cmdname = NULL;
	const os_subcommand_t *osci = NULL;

	if (argc < 2) {
		exitcode = _usage(stderr);
		goto __out;
	}

	// Advance argument pointer and make the subcommand argv[0].
	argc -= 1;
	argv += 1;
	cmdname = argv[0];

	osci = _os_subcommand_find(cmdname);
	if (osci) {
		if (osci->osc_flags & OS_SUBCOMMAND_FLAG_REQUIRE_ROOT) {
			if (geteuid()) {
				crfprintf_np(stderr, "subcommand requires root: %s", cmdname);
				exitcode = EX_NOPERM;
				goto __out;
			}
		}

		if (osci->osc_flags & OS_SUBCOMMAND_FLAG_TTYONLY) {
			if (!isatty(STDOUT_FILENO) || !isatty(STDIN_FILENO)) {
				crfprintf_np(stderr, "subcommand requires a tty: %s", cmdname);
				exitcode = EX_UNAVAILABLE;
				goto __out;
			}
		}

		exitcode = osci->osc_invoke(osci, argc, argv);
		if (exitcode == EX_USAGE) {
			_os_subcommand_print_usage(osci, stderr);
		}
	} else {
		crfprintf_np(stderr, "unrecognized subcommand: %s", cmdname);
		exitcode = _usage(stderr);
	}

__out:
	return exitcode;
}
