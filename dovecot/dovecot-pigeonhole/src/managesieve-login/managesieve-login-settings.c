/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "buffer.h"
#include "env-util.h"
#include "fd-close-on-exec.h"
#include "execv-const.h"
#include "settings-parser.h"
#include "service-settings.h"
#include "login-settings.h"

#include "pigeonhole-config.h"

#include "managesieve-login-settings.h"

#include <stddef.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sysexits.h>

/* <settings checks> */

static struct inet_listener_settings managesieve_login_inet_listeners_array[] = {
    { "sieve", "", 4190, FALSE },
};
static struct inet_listener_settings *managesieve_login_inet_listeners[] = {
	&managesieve_login_inet_listeners_array[0]
};
static buffer_t managesieve_login_inet_listeners_buf = {
	managesieve_login_inet_listeners, sizeof(managesieve_login_inet_listeners), { 0, }
};
/* </settings checks> */

struct service_settings managesieve_login_settings_service_settings = {
	.name = "managesieve-login",
	.protocol = "sieve",
	.type = "login",
	.executable = "managesieve-login",
	.user = "$default_login_user",
	.group = "",
	.privileged_group = "",
	.extra_groups = "",
	.chroot = "login",

	.drop_priv_before_exec = FALSE,

	.process_min_avail = 0,
	.process_limit = 0,
	.client_limit = 0,
	.service_count = 1,
	.idle_kill = 0,
	.vsz_limit = 64*1024*1024,

	.unix_listeners = ARRAY_INIT,
	.fifo_listeners = ARRAY_INIT,
	.inet_listeners = { { &managesieve_login_inet_listeners_buf,
		sizeof(managesieve_login_inet_listeners[0]) } }
};

#undef DEF
#define DEF(type, name) \
	{ type, #name, offsetof(struct managesieve_login_settings, name), NULL }

static const struct setting_define managesieve_login_setting_defines[] = {
	DEF(SET_STR, managesieve_implementation_string),
	DEF(SET_STR, managesieve_sieve_capability),
	DEF(SET_STR, managesieve_notify_capability),

	SETTING_DEFINE_LIST_END
};

static const struct managesieve_login_settings managesieve_login_default_settings = {
	.managesieve_implementation_string = DOVECOT_NAME " " PIGEONHOLE_NAME,
	.managesieve_sieve_capability = "",
	.managesieve_notify_capability = NULL
};

static const struct setting_parser_info *managesieve_login_setting_dependencies[] = {
	&login_setting_parser_info,
	NULL
};

static const struct setting_parser_info managesieve_login_setting_parser_info = {
	.module_name = "managesieve-login",
	.defines = managesieve_login_setting_defines,
	.defaults = &managesieve_login_default_settings,
	
	.type_offset = (size_t)-1,
	.struct_size = sizeof(struct managesieve_login_settings),

	.parent_offset = (size_t)-1,
	.parent = NULL,

	.dependencies = managesieve_login_setting_dependencies
};

const struct setting_parser_info *managesieve_login_settings_set_roots[] = {
	&login_setting_parser_info,
	&managesieve_login_setting_parser_info,
	NULL
};

