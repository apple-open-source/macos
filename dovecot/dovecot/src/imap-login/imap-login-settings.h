#ifndef IMAP_LOGIN_SETTINGS_H
#define IMAP_LOGIN_SETTINGS_H

struct imap_login_settings {
	const char *imap_capability;
#ifdef APPLE_OS_X_SERVER
	const char *aps_topic;
#endif
};

extern const struct setting_parser_info *imap_login_setting_roots[];

#endif
