#include <config.h>
#include "ntp_workimpl.h"

#ifdef WORK_DISPATCH
#include <dispatch/dispatch.h>

#include "ntp_stdlib.h"
#include "ntp_malloc.h"
#include "ntp_syslog.h"
#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_worker.h"

addremove_io_fd_func	addremove_io_fd;

static dispatch_semaphore_t worker_memlock = NULL;

/* --------------------------------------------------------------------
 * locking the global worker state table (and other global stuff)
 */
void
worker_global_lock(
        int inOrOut)
{
	if (worker_memlock) {
		if (inOrOut)
			dispatch_semaphore_wait(worker_memlock, DISPATCH_TIME_FOREVER);
		else
			dispatch_semaphore_signal(worker_memlock);
	}
}

static void
cleanup_after_child(
	blocking_child *	c
	)
{
	if (-1 != c->resp_read_pipe) {
		(*addremove_io_fd)(c->resp_read_pipe, c->ispipe, TRUE);
		close(c->resp_read_pipe);
		c->resp_read_pipe = -1;
		c->resp_read_ctx = NULL;
	}
	if (-1 != c->resp_write_pipe) {
		close(c->resp_write_pipe);
		c->resp_write_pipe = -1;
	}
	if (-1 != c->req_read_pipe) {
		close(c->req_read_pipe);
		c->req_read_pipe = -1;
	}
	if (-1 != c->req_write_pipe) {
		close(c->req_write_pipe);
		c->req_write_pipe = -1;
	}
	c->resp_read_ctx = NULL;
	c->reusable = TRUE;
}

int
send_blocking_req_internal(
	blocking_child *	c,
	blocking_pipe_header *	hdr,
	void *			data
	)
{
	int rc;
	int blocking_pipes[4] = { -1, -1, -1, -1 };
	int was_pipe;
	int is_pipe = 0;
	int saved_errno = 0;
	int octets;

	if (-1 == c->req_write_pipe) {
		rc = pipe_socketpair(&blocking_pipes[0], &was_pipe);
		if (0 != rc) {
			saved_errno = errno;
		} else {
			rc = pipe_socketpair(&blocking_pipes[2], &is_pipe);
			if (0 != rc) {
				saved_errno = errno;
				close(blocking_pipes[0]);
				close(blocking_pipes[1]);
			} else {
				INSIST(was_pipe == is_pipe);
			}
		}
		if (0 != rc) {
			errno = saved_errno;
			msyslog(LOG_ERR, "unable to create worker pipes: %m");
			exit(1);
		}

		/*
		 * Move the descriptors the parent will keep open out of the
		 * low descriptors preferred by C runtime buffered FILE *.
		 */
		c->req_read_pipe = move_fd(blocking_pipes[0]);
		c->req_write_pipe = move_fd(blocking_pipes[1]);
		c->resp_read_pipe = move_fd(blocking_pipes[2]);
		c->resp_write_pipe = move_fd(blocking_pipes[3]);
	}

	c->ispipe = is_pipe;
	memset(blocking_pipes, -1, sizeof(blocking_pipes));

	(*addremove_io_fd)(c->resp_read_pipe, c->ispipe, FALSE);

	if (!c->queue) {
		c->queue = dispatch_queue_create("org.ntp.ntpd.worker_q", NULL);
		c->sleep_sem = dispatch_semaphore_create(0);
	}
	if (!worker_memlock) {
		worker_memlock = dispatch_semaphore_create(1);
	}
	dispatch_async(c->queue, ^(){ blocking_child_common(c); });

	octets = sizeof(*hdr);
	rc = write(c->req_write_pipe, hdr, octets);

	if (rc == octets) {
		octets = hdr->octets - sizeof(*hdr);
		rc = write(c->req_write_pipe, data, octets);

		if (rc == octets)
			return 0;
	}

	if (rc < 0)
		msyslog(LOG_ERR, "send_blocking_req_internal: pipe write: %m");
	else
		msyslog(LOG_ERR, "send_blocking_req_internal: short write %d of %d", rc, octets);
	
	/* Fatal error.  Clean up the child process.  */
	req_child_exit(c);

	return -1;
}

blocking_pipe_header *
receive_blocking_req_internal(
	blocking_child *	c
	)
{
	blocking_pipe_header	hdr;
	blocking_pipe_header *	req;
	int			rc;
	long			octets;

	req = NULL;
	rc = read(c->req_read_pipe, &hdr, sizeof(hdr));
	if (rc < 0) {
		msyslog(LOG_ERR, "receive_blocking_req_internal: pipe read %m");
	} else if (0 == rc) {
		TRACE(4, ("parent closed request pipe, child terminating\n"));
	} else if (rc != sizeof(hdr)) {
		msyslog(LOG_ERR, "receive_blocking_req_internal: short header read %d of %lu", rc, (u_long)sizeof(hdr));
	} else {
		INSIST(sizeof(hdr) < hdr.octets && hdr.octets < 4 * 1024);
		req = emalloc(hdr.octets);
		memcpy(req, &hdr, sizeof(*req));
		octets = hdr.octets - sizeof(hdr);
		rc = read(c->req_read_pipe, (char *)req + sizeof(*req), octets);
		if (rc < 0)
			msyslog(LOG_ERR, "receive_blocking_req_internal: pipe data read %m");
		else if (rc != octets)
			msyslog(LOG_ERR, "receive_blocking_req_internal: short read %d of %ld", rc, octets);
		else if (BLOCKING_REQ_MAGIC != req->magic_sig)
			msyslog(LOG_ERR, "receive_blocking_req_internal: packet header mismatch (0x%x)", req->magic_sig);
		else
			return req;
	}

	cleanup_after_child(c);

	if (req != NULL)
		free(req);
	return NULL;
}

int
send_blocking_resp_internal(
	blocking_child *	c,
	blocking_pipe_header *	resp
	)
{
	long	octets, off = 0;
	int	rc;

	octets = resp->octets;
	do {
		rc = write(c->resp_write_pipe, resp + off, octets - off);
		if (rc < 0) {
			if (errno != EINTR) {
				TRACE(1, ("send_blocking_resp_internal: pipe write %m\n"));
				return -1;
			}
		} else if (rc == 0 && off < octets) {
			TRACE(1, ("send_blocking_resp_internal: short write %ld of %ld\n", off, octets));
			return -1;
		} else if (rc > 0) {
			off += rc;
		}
	} while (off < octets);
	free(resp);
        
	return 0;
}

blocking_pipe_header *
receive_blocking_resp_internal(
	blocking_child *	c
	)
{
	blocking_pipe_header 	hdr;
	blocking_pipe_header *	resp;
	int			rc;
	long			octets;

	resp = NULL;
	rc = read(c->resp_read_pipe, &hdr, sizeof(hdr));
	if (rc < 0) {
		TRACE(1, ("receive_blocking_resp_internal: pipe read %m\n"));
	} else if (0 == rc) {
	} else if (rc != sizeof(hdr)) {
		TRACE(1, ("receive_blocking_resp_internal: short header read %d of %lu\n", rc, (u_long)sizeof(hdr)));
	} else if (BLOCKING_RESP_MAGIC != hdr.magic_sig) {
		TRACE(1, ("receive_blocking_resp_internal: header mismatch (0x%x)\n", hdr.magic_sig));
	} else {
		long off = 0;  // keep type in sync with octets
		// Removed upper bound check per rdar://problem/27904221
		INSIST(sizeof(hdr) < hdr.octets);
		resp = emalloc(hdr.octets);
		memcpy(resp, &hdr, sizeof(*resp));
		octets = hdr.octets - sizeof(hdr);
		do {
			rc = read(c->resp_read_pipe, (char *)resp + sizeof(*resp) + off, octets - off);
			if (rc < 0) {
				if (errno != EINTR) {
					TRACE(1, ("receive_blocking_resp_internal: pipe data read %m\n"));
					free(resp);
					cleanup_after_child(c);
					return NULL;
				}
			} else if (rc == 0 && off < octets) {
				TRACE(1, ("receive_blocking_resp_internal: short read %d of %ld\n", rc, octets));
				free(resp);
				cleanup_after_child(c);
				return NULL;
			} else if (rc > 0) {
				off += rc;
			}
		} while (off < octets);

		return resp;
	}

	cleanup_after_child(c);

	if (resp != NULL)
		free(resp);

	return NULL;
}

int
req_child_exit(
	blocking_child *c
	)
{
	if (-1 != c->req_write_pipe) {
		close(c->req_write_pipe);
		c->req_write_pipe = -1;
		return 0;
	}

	return -1;
}

int
worker_sleep(
	blocking_child *	c,
	time_t			seconds
	)
{
	if (dispatch_semaphore_wait(c->sleep_sem, dispatch_time(DISPATCH_TIME_NOW, seconds * NSEC_PER_SEC)) == 0) {
		/* semaphore woke us up */
		return -1;
	}

	/* slept the whole time */
	return 0;
}

void
interrupt_worker_sleep(void)
{
	u_int			idx;
	blocking_child *	c;

	for (idx = 0; idx < blocking_children_alloc; idx++) {
		c = blocking_children[idx];
		if ((NULL == c) || (NULL == c->sleep_sem))
			continue;
		dispatch_semaphore_signal(c->sleep_sem);
	}
}

#else /* !WORK_DISPATCH follows */
char work_dispatch_nonempty_compilation_unit;
#endif
