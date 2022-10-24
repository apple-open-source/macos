/*
 * Copyright (c) 2015 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This ds contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this ds except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * ds.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifdef MUSER_AVAILABLE

#include <dispatch/dispatch.h>
#include <libinfo_muser.h>
#include <mach/mach_host.h>
#include <machine/cpu_capabilities.h>
#include <si_data.h>
#include <si_module.h>
#include <stdio.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#include <os/log.h>

static int _si_muser_disabled = 0;
static xpc_pipe_t __muser_pipe;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define kHasSecondaryUsers      0x40000000      // this device has Secondary Users


XPC_RETURNS_RETAINED
static xpc_pipe_t _muser_xpc_pipe(bool reset);

XPC_RETURNS_RETAINED
static xpc_object_t
_muser_call(const char *procname, xpc_object_t payload)
{
	xpc_object_t result = NULL;
	xpc_object_t reply;
	xpc_pipe_t pipe;
	int rv;
	bool retried = false; /* try again incase of usermanager jetsam */
	bool retryagain = false;
	if (!payload) { return NULL; }

	pipe = _muser_xpc_pipe(false);
	if (!pipe) { return NULL; }

	xpc_dictionary_set_string(payload, kLIMMessageRPCName, procname);
	xpc_dictionary_set_uint64(payload, kLIMMessageVersion, 1);
	
	/* if EPIPE, EAGAIN retry once */
	do  {
		retryagain = false;	/* if set from previous pass, reset now */
		rv = xpc_pipe_routine(pipe, payload, &reply);
		switch (rv) {
			case 0:
				result = reply;
				break;
			case EAGAIN:
			case EPIPE:
				{
					if (retried) {
						/* retried already, so bail out */
						os_log_error(OS_LOG_DEFAULT, "_muser_call: Failure (%d) with retry, bailing", rv);
						break;
					}
					/* lets retry one more time as jetsam of usermanager results in EPIPE */
					retried = true;
					retryagain = true;
					os_log_debug(OS_LOG_DEFAULT, "_muser_call: Error from xpc pipe (%d), retrying", rv);
					/* reestablish connetion */
					xpc_release(pipe);
					/* reestablish muser pipe */
					pipe = _muser_xpc_pipe(true);
					if (!pipe) { return NULL; }
				}
				break;
			default:
				break;
		}
	} while (retryagain);

	xpc_release(pipe);
	return result;
}

static int
_muser_xpc_pipe_disabled(xpc_pipe_t pipe)
{
	if (pipe != NULL) {
		xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
		xpc_dictionary_set_string(dict, kLIMMessageReqtype, kLIMMessageReplyAvailable);
		xpc_dictionary_set_int64(dict, kLIMMessageVersion, 1);

		xpc_object_t reply = NULL;
		int rv = xpc_pipe_routine(pipe, dict, &reply);

		xpc_release(dict);
		dict = NULL;

		if (rv == 0) {
			int disabled = !xpc_dictionary_get_bool(reply, kLIMMessageReplyAvailable);
			xpc_release(reply);
			return disabled;
		}
	}

	if (xpc_user_sessions_enabled()) {
		/* retry re-connecting after a user switch */
		return 0;
	}
	return 1;
}

static void
_muser_fork_child(void)
{
	if (__muser_pipe != NULL) {
		xpc_pipe_invalidate(__muser_pipe);
		/* disable release due to 10649340, it will cause a minor leak for each fork without exec */
		// xpc_release(__muser_pipe);
		__muser_pipe = NULL;
	}
	pthread_mutex_unlock(&mutex);
}

static void
_muser_fork_prepare(void)
{
	pthread_mutex_lock(&mutex);
}

static void
_muser_fork_parent(void)
{
	pthread_mutex_unlock(&mutex);
}

XPC_RETURNS_RETAINED
static xpc_pipe_t
_muser_xpc_pipe_create(void)
{
	uint64_t pipe_create_flags = XPC_PIPE_USE_SYNC_IPC_OVERRIDE | XPC_PIPE_PROPAGATE_QOS;

	if (xpc_user_sessions_enabled()) {
		errno_t error = 0;
		uid_t foreground_uid = xpc_user_sessions_get_foreground_uid(&error);
		if (error != 0) {
			os_log_error(OS_LOG_DEFAULT, "xpc_user_sessions_get_foreground_uid() failed with error %d - %s", error, xpc_strerror(error));
			return NULL;
		}
		return xpc_pipe_create_with_user_session_uid(kLibinfoMultiuserPortName, foreground_uid, pipe_create_flags);
	}

	return xpc_pipe_create(kLibinfoMultiuserPortName, pipe_create_flags);
}

XPC_RETURNS_RETAINED
static xpc_pipe_t
_muser_xpc_pipe(bool reset)
{
	static dispatch_once_t once;
	xpc_pipe_t pipe = NULL;

	dispatch_once(&once, ^{
		char *xbs_disable;
		kern_return_t rv;
		uint32_t multiuser_flags = 0;

		/* mirrors the ds_module build environment override */
		xbs_disable = getenv("XBS_DISABLE_LIBINFO");
		if ((issetugid() == 0) && (xbs_disable != NULL) && (strcmp(xbs_disable, "YES") == 0)) {
			_si_muser_disabled = 1;
			return;
		}

		/* check the xnu commpage bit to see if we're multiuser */
		rv = host_get_multiuser_config_flags(mach_host_self(), &multiuser_flags);
		if (rv == KERN_SUCCESS &&
			(((multiuser_flags & kIsMultiUserDevice) == 0) && ((multiuser_flags & kHasSecondaryUsers) == 0))
			) {
			_si_muser_disabled = 1;
			return;
		}

		pthread_atfork(_muser_fork_prepare, _muser_fork_parent, _muser_fork_child);
	});

	if (_si_muser_disabled == 1) {
		return NULL;
	}

	pthread_mutex_lock(&mutex);
	if (reset) {
		xpc_release(__muser_pipe);
		__muser_pipe = NULL;
	}

	if (__muser_pipe == NULL) {
		__muser_pipe = _muser_xpc_pipe_create();
		if (!_si_muser_disabled) { _si_muser_disabled = _muser_xpc_pipe_disabled(__muser_pipe); }
	}

	if (__muser_pipe) { pipe = xpc_retain(__muser_pipe); }
	pthread_mutex_unlock(&mutex);

	return pipe;
}

static bool
_muser_available(void)
{
	xpc_pipe_t pipe;

	pipe = _muser_xpc_pipe(false);
	if (pipe != NULL) {
		xpc_release(pipe);
	}

	return !_si_muser_disabled;
}

static si_item_t *
_muser_extract_user(si_mod_t *si, xpc_object_t reply) {
	__block struct passwd p;
	__block int reqs = 3;
	p.pw_name = (char *)"";
	p.pw_passwd = (char *)"*";
	p.pw_uid = (uid_t)0;
	p.pw_gid = (gid_t)0;
	p.pw_change = (time_t)0;
	p.pw_expire = (time_t)0;
	p.pw_class = (char *)"";
	p.pw_gecos = (char *)"";
	p.pw_dir = (char *)"/var/empty";
	p.pw_shell = (char *)"/usr/bin/false";

	xpc_dictionary_apply(reply, ^bool(const char *key, xpc_object_t value) {
		if (key == NULL) { return true; }
		else if (!strcmp(key, kLIMMessageReplyName)) {
			const char *nm = xpc_string_get_string_ptr(value);
			if (nm) {
				p.pw_name = (char *)nm;
				reqs--;
			}
		}
		else if (!strcmp(key, kLIMMessageReplyPasswd)) {
			const char *pw = xpc_string_get_string_ptr(value);
			if (pw) { p.pw_passwd = (char *)pw; }
		}
		else if (!strcmp(key, kLIMMessageReplyUID)) {
			if (xpc_get_type(value) == XPC_TYPE_INT64) {
				int64_t uid = xpc_int64_get_value(value);
				p.pw_uid = uid;
				reqs--;
			}
		}
		else if (!strcmp(key, kLIMMessageReplyGID)) {
			if (xpc_get_type(value) == XPC_TYPE_INT64) {
				p.pw_gid = xpc_int64_get_value(value);
				reqs--;
			}
		}
		else if (!strcmp(key, kLIMMessageReplyHome)) {
			const char *home = xpc_string_get_string_ptr(value);
			if (home) { p.pw_dir = (char *)home; };
		}
		else if (!strcmp(key, kLIMMessageReplyShell)) {
			const char *shell = xpc_string_get_string_ptr(value);
			if (shell) { p.pw_shell = (char *)shell; }
		}
		return true;
	});

	if (reqs != 0) { return NULL; }

	return (si_item_t *)LI_ils_create("L4488ss44LssssL", (unsigned long)si, CATEGORY_USER, 1, 0, 0, p.pw_name, p.pw_passwd, p.pw_uid, p.pw_gid, p.pw_change, p.pw_class, p.pw_gecos, p.pw_dir, p.pw_shell, p.pw_expire);
}

static si_item_t *
_muser_extract_group(si_mod_t *si, xpc_object_t reply)
{
	__block struct group g;
	__block int reqs = 2;
	g.gr_gid = (gid_t)0;
	g.gr_name = (char *)"";
	g.gr_passwd = (char *)"*";
	g.gr_mem = NULL;

	xpc_dictionary_apply(reply, ^bool(const char *key, xpc_object_t value) {
		if (key == NULL) { return true; }
		else if (!strcmp(key, kLIMMessageReplyGroupname)) {
			const char *nm = xpc_string_get_string_ptr(value);
			if (nm) {
				g.gr_name = (char *)nm;
				reqs--;
			}
		}
		else if (!strcmp(key, kLIMMessageReplyGroupID)) {
			if (xpc_get_type(value) == XPC_TYPE_INT64) {
				g.gr_gid = xpc_int64_get_value(value);
				reqs--;
			}
		}
		else if (!strcmp(key, kLIMMessageReplyGroupMembers)) {
			if (xpc_get_type(value) == XPC_TYPE_ARRAY) {
				size_t count = xpc_array_get_count(value);
				g.gr_mem = (char **)malloc(sizeof(char *) * (count + 1));
				g.gr_mem[count] = NULL;

				for (size_t i=0; i<count; i++) {
					g.gr_mem[i] = (char *)xpc_array_get_string(value, i);
				}
			}
		}
		return true;
	});

	if (reqs != 0) { return NULL; }

	si_item_t *item = (si_item_t *)LI_ils_create("L4488ss4*", (unsigned long)si, CATEGORY_GROUP, 1,
			0, 0, g.gr_name, g.gr_passwd, g.gr_gid, g.gr_mem);

	free(g.gr_mem);
	return item;
}

static si_item_t *
_muser_extract_grouplist(si_mod_t *si, xpc_object_t reply, const char *user)
{
	gid_t __block *gidlist = NULL;
	size_t gidcount = 0;
	si_item_t *item = NULL;

	if (user == NULL) { return NULL; }

	xpc_object_t list = xpc_dictionary_get_value(reply, kLIMMessageReplyGrouplist);
	if (list) {
		if (xpc_get_type(list) == XPC_TYPE_ARRAY) {
			gidcount = xpc_array_get_count(list);
			gidlist = (gid_t *)calloc(gidcount + 1, sizeof(gid_t));

			xpc_array_apply(list, ^bool(size_t index, xpc_object_t value) {
				if (xpc_get_type(value) == XPC_TYPE_INT64) {
					gidlist[index] = xpc_int64_get_value(value);
					return true;
				} else {
					free(gidlist);
					gidlist = NULL;
					return false;
				}
			});
		}
	}

	if (gidlist) {
		item = (si_item_t *)LI_ils_create("L4488s4@", (unsigned long)si, CATEGORY_GROUPLIST,
				1, 0, 0, user, gidcount, gidcount * sizeof(gid_t), gidlist);
	}

	free(gidlist);
	return item;
}

#pragma mark -

static int
muser_is_valid(si_mod_t *si, si_item_t *item)
{
	si_mod_t *src;

	if (si == NULL) return 0;
	if (item == NULL) return 0;
	if (si->name == NULL) return 0;
	if (item->src == NULL) return 0;

	src = (si_mod_t *)item->src;

	if (src->name == NULL) return 0;
	if (string_not_equal(si->name, src->name)) return 0;

	return 1;
}

static si_item_t *
muser_user_byname(si_mod_t *si, const char *name)
{
	xpc_object_t payload;
	si_item_t *item = NULL;

	// Bypass muser for system role account users. This allows processes like
	// logd to be able to get off the ground before usermanagerd starts, which
	// in turn can avoid deadlock scenarios.
	if (name[0] == '_') { return NULL; }

	if (!_muser_available()) { return NULL; }

	payload = xpc_dictionary_create(NULL, NULL, 0);
	if (!payload) { return NULL; }

	xpc_dictionary_set_string(payload, kLIMMessageReqtype, kLIMMessageRequestUsername);
	xpc_dictionary_set_string(payload, kLIMMessageQuery, name);

	xpc_object_t reply = _muser_call("getpwnam", payload);
	if (reply) {
		item = _muser_extract_user(si, reply);
		xpc_release(reply);
	}

	xpc_release(payload);
	return item;
}

static si_item_t *
muser_user_byuid(si_mod_t *si, uid_t uid)
{
	xpc_object_t payload;
	si_item_t *item = NULL;

	// Bypass muser for system role account users. This allows processes like
	// logd to be able to get off the ground before usermanagerd starts, which
	// in turn can avoid deadlock scenarios.
	if (uid < 501) { return NULL; }

	if (!_muser_available()) { return NULL; }

	payload = xpc_dictionary_create(NULL, NULL, 0);
	if (!payload) { return NULL; }

	xpc_dictionary_set_string(payload, kLIMMessageReqtype, kLIMMessageRequestUID);
	xpc_dictionary_set_int64(payload, kLIMMessageQuery, uid);

	xpc_object_t reply = _muser_call("getpwuid", payload);
	if (reply) {
		item = _muser_extract_user(si, reply);
		xpc_release(reply);
	}

	xpc_release(payload);
	return item;
}

static si_item_t *
muser_group_byname(struct si_mod_s *si, const char *name)
{
	xpc_object_t payload;
	si_item_t *item = NULL;

	// Bypass muser for system role account groups. This allows processes like
	// logd to be able to get off the ground before usermanagerd starts, which
	// in turn can avoid deadlock scenarios.
	if (name[0] == '_' &&
			strcmp(name, "_analyticsusers") != 0 &&
			strcmp(name, "_accessory_mobile_share") != 0) {
		return NULL;
	}

	if (!_muser_available()) { return NULL; }

	payload = xpc_dictionary_create(NULL, NULL, 0);
	if (!payload) { return NULL; }

	xpc_dictionary_set_string(payload, kLIMMessageReqtype, kLIMMessageRequestGroupname);
	xpc_dictionary_set_string(payload, kLIMMessageQuery, name);

	xpc_object_t reply = _muser_call("getgrnam", payload);
	if (reply) {
		item = _muser_extract_group(si, reply);
		xpc_release(reply);
	}

	xpc_release(payload);
	return item;
}

static si_item_t *
muser_group_bygid(struct si_mod_s *si, gid_t gid)
{
	xpc_object_t payload;
	si_item_t *item = NULL;

	// Bypass muser for system role account groups. This allows processes like
	// logd to be able to get off the ground before usermanagerd starts, which
	// in turn can avoid deadlock scenarios.
	if (gid < 501 &&
			gid != 250 && // _analyticsusers
			gid != 286 && // _accessory_mobile_share
			gid != 299) { // systemusers
		return NULL;
	}

	if (!_muser_available()) { return NULL; }

	payload = xpc_dictionary_create(NULL, NULL, 0);
	if (!payload) { return NULL; }

	xpc_dictionary_set_string(payload, kLIMMessageReqtype, kLIMMessageRequestGID);
	xpc_dictionary_set_int64(payload, kLIMMessageQuery, gid);

	xpc_object_t reply = _muser_call("getgrgid", payload);
	if (reply) {
		item = _muser_extract_group(si, reply);
		xpc_release(reply);
	}

	xpc_release(payload);
	return item;
}

static si_item_t *
muser_grouplist(struct si_mod_s *si, const char *name, uint32_t count)
{
	xpc_object_t payload;
	si_item_t *item = NULL;

	// Bypass muser for system role account users. This allows processes like
	// logd to be able to get off the ground before usermanagerd starts, which
	// in turn can avoid deadlock scenarios.
	if (name[0] == '_') { return NULL; }

	if (!_muser_available()) { return NULL; }

	payload = xpc_dictionary_create(NULL, NULL, 0);
	if (!payload) { return NULL; }

	xpc_dictionary_set_string(payload, kLIMMessageReqtype, kLIMMessageRequestGrouplist);
	xpc_dictionary_set_string(payload, kLIMMessageQuery, name);

	xpc_object_t reply = _muser_call("getgrouplist", payload);
	if (reply) {
		item = _muser_extract_grouplist(si, reply, name);
		xpc_release(reply);
	}

	xpc_release(payload);
	return item;
}

#pragma mark -

si_mod_t *
si_module_static_muser(void)
{
	static struct si_mod_vtable_s muser_vtable =
	{
		.sim_user_byname = &muser_user_byname,
		.sim_user_byuid = &muser_user_byuid,

		.sim_group_byname = &muser_group_byname,
		.sim_group_bygid = &muser_group_bygid,
		.sim_grouplist = &muser_grouplist,
	};

	static si_mod_t si =
	{
		.vers = 1,
		.refcount = 1,
		.flags = SI_MOD_FLAG_STATIC,

		.private = NULL,
		.vtable = &muser_vtable,
	};

	static dispatch_once_t once;
	dispatch_once(&once, ^{
		si.name = strdup("muser");

		/* cache results everywhere except for the System session */
		bool system_session = xpc_user_sessions_enabled() && xpc_user_sessions_get_session_uid() == 0;
		if (!system_session) {
			muser_vtable.sim_is_valid = &muser_is_valid;
		}
	});

	return &si;
}

#endif // MUSER_AVAILABLE
