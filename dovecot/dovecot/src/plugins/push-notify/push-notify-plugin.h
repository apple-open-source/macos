/* Copyright (c) 2008-2011 Apple, inc. */

#ifndef __NOTIFY_PLUGIN_H__
#define __NOTIFY_PLUGIN_H__

struct module;
void push_notify_plugin_init(struct module *module);
void push_notify_plugin_deinit(void);

typedef struct msg_data_s {
	unsigned long msg;
	unsigned long pid;

	char d1[128];
	char d2[512];
	char d3[512];
	char d4[512];
} msg_data_t;

#endif
