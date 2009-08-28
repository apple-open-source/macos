/*
 * Kqueue file notification support.
 *
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "includes.h"

#include <sys/event.h>

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

#define KQTRACE 10

typedef void (*notify_callback)(struct sys_notify_context *,
			       void *, struct notify_event *);

#define KQ_NOTIFY_MASK ( \
	    FILE_NOTIFY_CHANGE_FILE_NAME | \
	    FILE_NOTIFY_CHANGE_DIR_NAME \
	)

struct kqueue_watch_context
{
	const char *	kq_path;	/* path we are watching */
	struct kevent	kq_event;	/* kevent */
	int		kq_fd;		/* kevent file descriptor */

	struct {
		notify_callback			callback;
		struct sys_notify_context *	notify;
		void *				data;
	} kq_observer;
};

static int global_kq = -1;

static int
kqueue_watch_dealloc(
		struct kqueue_watch_context * kq_watch)
{
	/* Just cancel the kqueue watch. */
	DEBUG(KQTRACE, ("cancelling watch for fd=%d, path=%s\n",
			kq_watch->kq_fd, kq_watch->kq_path));

	if (kq_watch->kq_fd != -1) {
		EV_SET(&kq_watch->kq_event,
			kq_watch->kq_fd /* ident */,
			EVFILT_VNODE, EV_DELETE,
			0 /* fflags */, 0, NULL);

		kevent(global_kq, &kq_watch->kq_event, 1, NULL, 0, NULL);
		close(kq_watch->kq_fd);
		kq_watch->kq_fd = -1;
	}

	return 0;
}

static struct kqueue_watch_context *
kqueue_watch_alloc(
		void * mem_ctx,
		const struct notify_entry * entry)
{
	struct kqueue_watch_context * kq_watch;

	kq_watch = TALLOC_P(mem_ctx, struct kqueue_watch_context);
	if (kq_watch == NULL) {
		return NULL;
	}

	kq_watch->kq_path = talloc_strdup(kq_watch, entry->path);
	if (kq_watch->kq_path == NULL) {
		TALLOC_FREE(kq_watch);
		return NULL;
	}

	kq_watch->kq_fd = -1;
	return kq_watch;
}

static NTSTATUS
kqueue_watch_start(
		const struct connection_struct * conn,
		struct kqueue_watch_context * kq_watch)
{
	int fd;
	int err;
	struct stat sbuf;

	DEBUG(KQTRACE, ("adding watch for path=%s\n", kq_watch->kq_path));

	if (lp_widelinks(SNUM(conn))) {
		fd = open(kq_watch->kq_path, O_EVTONLY);
	} else {
		fd = open(kq_watch->kq_path, O_EVTONLY|O_NOFOLLOW);
	}

	if (fd == -1) {
		int errsav = errno;
		DEBUG(2, ("open(%s) failed: errno=%d, %s\n",
				kq_watch->kq_path, errsav, strerror(errsav)));
		return map_nt_error_from_unix(errsav);
	}

	fcntl(fd, F_SETFD, FD_CLOEXEC);

	fstat(fd, &sbuf);
	if (!S_ISDIR(sbuf.st_mode)) {
		DEBUG(KQTRACE, ("%s is not a directory\n", kq_watch->kq_path));
		close(fd);
		return NT_STATUS_NOT_A_DIRECTORY;
	}

	/* We can tell when a directory changes, but that's about it. We have
	 * no idea when attributes of the directory entries change and we have
	 * no idea whether a file or a directory changed.
	 */

	EV_SET(&kq_watch->kq_event, fd /* ident */,
		EVFILT_VNODE,
		EV_ADD | EV_CLEAR | EV_ENABLE,
		NOTE_WRITE /* fflags */, 0, kq_watch);

	err = kevent(global_kq, &kq_watch->kq_event, 1, NULL, 0, NULL);
	if (err == -1) {
		int errsav = errno;
		close(kq_watch->kq_fd);
		DEBUG(2, ("kevent failed: errno=%d, %s\n",
			errsav, strerror(errsav)));
		return map_nt_error_from_unix(errsav);
	}

	kq_watch->kq_fd = fd;

	DEBUG(KQTRACE, ("added watch for fd=%d path=%s\n",
			kq_watch->kq_fd, kq_watch->kq_path));

	return NT_STATUS_OK;
}

static void
kqueue_event_handler(
		struct event_context * context,
		struct fd_event * fd,
		uint16_t flags,
		void *data)
{
	struct kevent ev_list[8];
	struct timespec ev_timeout = { .tv_sec = 0, .tv_nsec = 0 };

	DEBUG(KQTRACE, ("polling kqueue events\n"));

	for (;;) {
		int i;
		int err;

		ZERO_ARRAY(ev_list);

		err = kevent(global_kq, NULL, 0,
			ev_list, ARRAY_SIZE(ev_list),
			&ev_timeout /* timeout */);

		if (err == -1) {
			DEBUG(2, ("kevent failed, errno=%d, %s\n",
						errno, strerror(errno)));
			return;
		}

		if (err == 0) {
			DEBUG(KQTRACE, ("done polling\n"));
			return;
		}

		DEBUG(KQTRACE, ("handling %d kqueue events\n", err));

		for (i = 0; i < MIN(err, ARRAY_SIZE(ev_list)); ++i) {
			struct kqueue_watch_context * kq_watch;
			struct kevent * kev = &ev_list[i];
			struct notify_event nev;

			kq_watch = (struct kqueue_watch_context *)kev->udata;
			if (!kq_watch) {
				/* We got an event tht doesn't have a context
				 * set. This should never happen, but we are
				 * defensive and dutifully remove the event and
				 * the file descriptor.
				 */
				DEBUG(2, ("missing kqueue watch context for fd=%d\n",
							(int)kev->ident));
				kev->flags = EV_DELETE;
				kevent(global_kq, kev, 1, NULL, 0, NULL);
				close(kev->ident);
				continue;
			}

			DEBUG(KQTRACE, ("signalling event for fd=%d path=%s\n",
					kq_watch->kq_fd,
					kq_watch->kq_path));

			/* We have no real idea what happend. Let's just say
			 * that a file waas added. We don't know the name, but
			 * we can get away with the empty string.
			 */
			nev.action = NOTIFY_ACTION_ADDED;
			nev.path = "";
			nev.private_data = NULL;

			kq_watch->kq_observer.callback(
					kq_watch->kq_observer.notify,
					kq_watch->kq_observer.data,
					&nev);

		}
	}
}

static NTSTATUS
kqueue_init_kqueue(
		struct event_context * event)
{
	if (global_kq != -1) {
		return NT_STATUS_OK;
	}

	DEBUG(KQTRACE, ("initializing global kqueue\n"));

	global_kq = kqueue();
	if (global_kq == -1) {
		DEBUG(0, ("failed create a kqueue, %s\n", strerror(errno)));
		return NT_STATUS_INSUFFICIENT_RESOURCES;
	}

	fcntl(global_kq, F_SETFD, FD_CLOEXEC);

	if (event_add_fd(event, event, global_kq, EVENT_FD_READ,
		    kqueue_event_handler, NULL) == NULL) {
		DEBUG(0, ("failed to add kqueue monitor event\n"));
		close(global_kq);
		global_kq = -1;
		return NT_STATUS_IO_DEVICE_ERROR;
	}

	return NT_STATUS_OK;
}

static NTSTATUS kqueue_notify_watch(
		vfs_handle_struct * vfs_handle,
		struct sys_notify_context * context,
		struct notify_entry * entry,
		notify_callback callback,
		void * callback_data,
		void * watch_handle)
{
	NTSTATUS status;

	struct kqueue_watch_context * kq_watch;

	if (entry->subdir_filter != 0) {
		DEBUG(10, ("ignoring subdir_filter=%#x\n", entry->subdir_filter));
	}

	if ((entry->filter & KQ_NOTIFY_MASK) == 0) {
		DEBUG(10, ("ignoring unsupported kqueue filter=%#x\n", entry->filter));
		return NT_STATUS_OK;
	}

	status = kqueue_init_kqueue(context->ev);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	kq_watch = kqueue_watch_alloc(context, entry);
	if (kq_watch == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	kq_watch->kq_observer.callback = callback;
	kq_watch->kq_observer.notify = context;
	kq_watch->kq_observer.data = callback_data;

	status = kqueue_watch_start(context->conn, kq_watch);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(kq_watch);
		return status;
	}

	*(void **)watch_handle = kq_watch;
	talloc_set_destructor(kq_watch, kqueue_watch_dealloc);
	return NT_STATUS_OK;
}

static vfs_op_tuple notify_kqueue_ops[] =
{

	{SMB_VFS_OP(kqueue_notify_watch), SMB_VFS_OP_NOTIFY_WATCH, SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(NULL), SMB_VFS_OP_NOOP, SMB_VFS_LAYER_NOOP}
};

NTSTATUS vfs_notify_kqueue_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "notify_kqueue",
				notify_kqueue_ops);
}
