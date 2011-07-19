/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "buffer.h"
#include "settings-parser.h"
#include "service-settings.h"
#include "mail-storage-settings.h"

#include "pigeonhole-config.h"

#include "managesieve-settings.h"

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

static bool managesieve_settings_verify(void *_set, pool_t pool,
				 const char **error_r);

/* <settings checks> */
static struct file_listener_settings managesieve_unix_listeners_array[] = {
	{ "login/sieve", 0666, "", "" }
};
static struct file_listener_settings *managesieve_unix_listeners[] = {
	&managesieve_unix_listeners_array[0]
};
static buffer_t managesieve_unix_listeners_buf = {
	managesieve_unix_listeners, sizeof(managesieve_unix_listeners), { 0, }
};
/* </settings checks> */

struct service_settings managesieve_settings_service_settings = {
	.name = "managesieve",
	.protocol = "sieve",
	.type = "",
	.executable = "managesieve",
	.user = "",
	.group = "",
	.privileged_group = "",
	.extra_groups = "",
	.chroot = "",

	.drop_priv_before_exec = FALSE,

	.process_min_avail = 0,
	.process_limit = 0,
	.client_limit = 1,
	.service_count = 1,
	.idle_kill = 0,
	.vsz_limit = (uoff_t)-1,

	.unix_listeners = { { &managesieve_unix_listeners_buf,
				   sizeof(managesieve_unix_listeners[0]) } },
	.fifo_listeners = ARRAY_INIT,
	.inet_listeners = ARRAY_INIT
};

#undef DEF
#undef DEFLIST
#define DEF(type, name) \
	{ type, #name, offsetof(struct managesieve_settings, name), NULL }
#define DEFLIST(field, name, defines) \
	{ SET_DEFLIST, name, offsetof(struct managesieve_settings, field), defines }

static struct setting_define managesieve_setting_defines[] = {
	DEF(SET_BOOL, mail_debug),
	DEF(SET_BOOL, verbose_proctitle),

	DEF(SET_UINT, managesieve_max_line_length),
	DEF(SET_STR, managesieve_implementation_string),
	DEF(SET_STR, managesieve_client_workarounds),
	DEF(SET_STR, managesieve_logout_format),
	DEF(SET_UINT, managesieve_max_compile_errors),


	SETTING_DEFINE_LIST_END
};

static struct managesieve_settings managesieve_default_settings = {
	.mail_debug = FALSE,
	.verbose_proctitle = FALSE,

	/* RFC-2683 recommends at least 8000 bytes. Some clients however don't
	   break large message sets to multiple commands, so we're pretty
	   liberal by default. */
	.managesieve_max_line_length = 65536,
	.managesieve_implementation_string = DOVECOT_NAME " " PIGEONHOLE_NAME,
	.managesieve_client_workarounds = "",
	.managesieve_logout_format = "bytes=%i/%o",
	.managesieve_max_compile_errors = 5
};

static const struct setting_parser_info *managesieve_setting_dependencies[] = {
	&mail_user_setting_parser_info,
	NULL
};

const struct setting_parser_info managesieve_setting_parser_info = {
	.module_name = "managesieve",
	.defines = managesieve_setting_defines,
	.defaults = &managesieve_default_settings,

	.type_offset = (size_t)-1,
	.struct_size = sizeof(struct managesieve_settings),

	.parent_offset = (size_t)-1,
	.parent = NULL,

	.check_func = managesieve_settings_verify,
	.dependencies = managesieve_setting_dependencies
};

const struct setting_parser_info *managesieve_settings_set_roots[] = {
	&managesieve_setting_parser_info,
	NULL
};

/* <settings checks> */
struct managesieve_client_workaround_list {
	const char *name;
	enum managesieve_client_workarounds num;
};

static const struct managesieve_client_workaround_list 
	managesieve_client_workaround_list[] = {
	{ NULL, 0 }
};

static int
managesieve_settings_parse_workarounds(struct managesieve_settings *set,
				const char **error_r)
{
	enum managesieve_client_workarounds client_workarounds = 0;
	const struct managesieve_client_workaround_list *list;
	const char *const *str;

	str = t_strsplit_spaces(set->managesieve_client_workarounds, " ,");
	for (; *str != NULL; str++) {
		list = managesieve_client_workaround_list;
		for (; list->name != NULL; list++) {
			if (strcasecmp(*str, list->name) == 0) {
				client_workarounds |= list->num;
				break;
			}
		}
		if (list->name == NULL) {
			*error_r = t_strdup_printf("managesieve_client_workarounds: "
				"Unknown workaround: %s", *str);
			return -1;
		}
	}
	set->parsed_workarounds = client_workarounds;
	return 0;
}


static bool managesieve_settings_verify
(void *_set, pool_t pool ATTR_UNUSED, const char **error_r)
{
	struct managesieve_settings *set = _set;

	if (managesieve_settings_parse_workarounds(set, error_r) < 0)
		return FALSE;
	return TRUE;
}

/* </settings checks> */

const char *managesieve_settings_version = DOVECOT_VERSION;
