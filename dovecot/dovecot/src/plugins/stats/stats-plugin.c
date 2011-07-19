/* APPLE - got from http://www.dovecot.org/patches/2.0/stats-plugin.c */

/* Copyright (C) 2009 Timo Sirainen, LGPLv2.1 */

/*
   eval `cat /usr/local/lib/dovecot/dovecot-config`
   gcc -fPIC -shared -DHAVE_CONFIG_H \
     `echo $DOVECOT_CFLAGS $LIBDOVECOT_INCLUDE $LIBDOVECOT_STORAGE_INCLUDE $LIBDOVECOT_IMAP_INCLUDE` \
     stats-plugin.c -o stats_plugin.so
*/

#include "imap-common.h"
#include "module-context.h"
#include "hash.h"
#include "hostpid.h"
#include "str.h"
#include "imap-commands.h"

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <dlfcn.h>						/* APPLE */

#define STATS_IMAP_CONTEXT(obj) \
	MODULE_CONTEXT(obj, stats_imap_module)

struct stats_io {
	uoff_t rchar, wchar, rbytes, wbytes, cwbytes;
	unsigned int syscr, syscw;
};

struct stats_cmd_context {
	union imap_module_context module_ctx;

	command_func_t *orig_func;
	struct timeval t1;
	struct stats_io io1;
};

const char *stats_plugin_version = PACKAGE_VERSION;

static MODULE_CONTEXT_DEFINE_INIT(stats_imap_module, &imap_module_register);
static struct hash_table *orig_cmds;
static bool io_disabled = FALSE;
static int *sk_assert_build_cancel;				/* APPLE */

static bool stats_command(struct client_command_context *cmd);

static void stats_get_io(struct stats_io *io_r)
{
#ifdef __APPLE__
	memset(io_r, 0, sizeof *io_r);
	io_disabled = TRUE;
#else
	char path[100], buf[1024];
	const char *const *tmp;
	int fd, ret;

	memset(io_r, 0, sizeof(*io_r));

	i_snprintf(path, sizeof(path), "/proc/%s/io", my_pid);
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		if (errno != ENOENT)
			i_error("open(%s) failed: %m", path);
		io_disabled = TRUE;
		return;
	}
	ret = read(fd, buf, sizeof(buf));
	if (ret <= 0) {
		if (ret == -1)
			i_error("read(%s) failed: %m", path);
		else
			i_error("read(%s) returned EOF", path);
	} else if (ret == sizeof(buf)) {
		/* just shouldn't happen.. */
		i_error("%s is larger than expected", path);
	} else {
		buf[ret] = '\0';
		tmp = t_strsplit(buf, "\n");
		for (; *tmp != NULL; tmp++) {
			if (strncmp(*tmp, "rchar: ", 7) == 0)
				io_r->rchar = strtoull(*tmp + 7, NULL, 10);
			else if (strncmp(*tmp, "wchar: ", 7) == 0)
				io_r->wchar = strtoull(*tmp + 7, NULL, 10);
			else if (strncmp(*tmp, "syscr: ", 7) == 0)
				io_r->syscr = strtoul(*tmp + 7, NULL, 10);
			else if (strncmp(*tmp, "syscw: ", 7) == 0)
				io_r->syscw = strtoul(*tmp + 7, NULL, 10);
			else if (strncmp(*tmp, "read_bytes: ", 12) == 0)
				io_r->rbytes = strtoull(*tmp + 12, NULL, 10);
			else if (strncmp(*tmp, "write_bytes: ", 13) == 0)
				io_r->wbytes = strtoull(*tmp + 13, NULL, 10);
			else if (strncmp(*tmp, "cancelled_write_bytes: ", 23) == 0)
				io_r->cwbytes = strtoull(*tmp + 23, NULL, 10);
		}
	}
	(void)close(fd);
#endif
}

static void stats_command_done(struct client_command_context *cmd,
			       struct stats_cmd_context *ctx)
{
	struct timeval t2;
	struct stats_io io2;
	string_t *str;

	if (gettimeofday(&t2, NULL) < 0)
		i_error("gettimeofday() failed: %m");

	t2.tv_sec -= ctx->t1.tv_sec;
	t2.tv_usec -= ctx->t1.tv_usec;
	if (t2.tv_usec < 0) {
		t2.tv_sec++;
		t2.tv_usec += 1000000;
	}
	str = t_str_new(256);
	str_printfa(str, "Command: secs=%u.%03u",
		    (int)t2.tv_sec, (int)t2.tv_usec/1000);

	stats_get_io(&io2);
	if (!io_disabled) {
		io2.rchar -= ctx->io1.rchar;
		io2.wchar -= ctx->io1.wchar;
		io2.syscr -= ctx->io1.syscr;
		io2.syscw -= ctx->io1.syscw;
		io2.rbytes -= ctx->io1.rbytes;
		io2.wbytes -= ctx->io1.wbytes;
		io2.cwbytes -= ctx->io1.cwbytes;
		str_printfa(str, " rchar=%"PRIuUOFF_T" wchar=%"PRIuUOFF_T" "
			    "syscr=%u syscw=%u rbytes=%"PRIuUOFF_T" "
			    "wbytes=%"PRIuUOFF_T" cwbytes=%"PRIuUOFF_T,
			    io2.rchar, io2.wchar, io2.syscr, io2.syscw,
			    io2.rbytes, io2.wbytes, io2.cwbytes);
	}

	str_printfa(str, ": %s %s", cmd->tag, cmd->name);
	if (cmd->args != NULL)
		str_printfa(str, " %s", cmd->args);
	i_info("%s", str_c(str));
}

static bool stats_command_continue(struct client_command_context *cmd)
{
	struct stats_cmd_context *ctx = STATS_IMAP_CONTEXT(cmd);
	bool ret;

	cmd->func = ctx->orig_func;
	ret = cmd->func(cmd);
	i_assert(cmd->func != stats_command_continue);
	i_assert(cmd->func != stats_command);

	ctx->orig_func = cmd->func;
	cmd->func = stats_command_continue;

	if (ret || cmd->state >= CLIENT_COMMAND_STATE_WAIT_SYNC) {
		stats_command_done(cmd, ctx);
		return ret;
	}
	return FALSE;
}

static bool stats_command(struct client_command_context *cmd)
{
	command_func_t *func;
	struct stats_cmd_context *ctx;
	bool ret;

	ctx = p_new(cmd->pool, struct stats_cmd_context, 1);

	func = hash_table_lookup(orig_cmds, cmd->name);
	if (func == NULL)
		i_panic("Lost command %s", cmd->name);

	stats_get_io(&ctx->io1);
	if (gettimeofday(&ctx->t1, NULL) < 0)
		i_error("gettimeofday() failed: %m");
	ret = func(cmd);

	if (ret || cmd->state >= CLIENT_COMMAND_STATE_WAIT_SYNC) {
		stats_command_done(cmd, ctx);
		return ret;
	}

	MODULE_CONTEXT_SET(cmd, stats_imap_module, ctx);

	ctx->orig_func = cmd->func == stats_command ? func : cmd->func;
	cmd->func = stats_command_continue;
	return FALSE;
}

void stats_plugin_init(void);
void stats_plugin_deinit(void);

void stats_plugin_init(void)
{
	struct command *commands;
	unsigned int i, count;

	orig_cmds = hash_table_create(default_pool, default_pool, 0,
				      strcase_hash,
				      (hash_cmp_callback_t *)strcasecmp);

	commands = array_get_modifiable(&imap_commands, &count);
	for (i = 0; i < count; i++) {
		/* APPLE */
		if (strcasecmp(commands[i].name, "UID") == 0)
			continue;

		hash_table_insert(orig_cmds, (char *)commands[i].name,
				  commands[i].func);
		commands[i].func = stats_command;
	}

	/* APPLE - stop the fts-sk plugin from assert-crashing when we
	   redirect continuation functions.  Could just comment out the
	   assert there, but it's valuable.  Could make the fts-sk
	   plugin check for the presence of the stats plugin, but
	   RTLD_DEFAULT is expensive, fts-sk is common, and stats is
	   rarely used.  So put that costly op here. */
	sk_assert_build_cancel = dlsym(RTLD_DEFAULT, "sk_assert_build_cancel");
	if (sk_assert_build_cancel != NULL)
		*sk_assert_build_cancel = 0;
}

void stats_plugin_deinit(void)
{
	/* APPLE */
	if (sk_assert_build_cancel != NULL)
		*sk_assert_build_cancel = 1;
}
