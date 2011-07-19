/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __MANAGESIEVE_LOGIN_SETTINGS_H
#define __MANAGESIEVE_LOGIN_SETTINGS_H

struct managesieve_login_settings {
	const char *managesieve_implementation_string;
	const char *managesieve_sieve_capability;
	const char *managesieve_notify_capability;
};

extern const struct setting_parser_info *managesieve_login_settings_set_roots[];

#ifdef _CONFIG_PLUGIN
void managesieve_login_settings_init(void);
void managesieve_login_settings_deinit(void);
#endif

#endif /* __MANAGESIEVE_LOGIN_SETTINGS_H */
