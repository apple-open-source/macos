#include "ni_shared.h"
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinfo/ni.h>

typedef struct ni_private
{
		int naddrs;
		struct in_addr *addrs;
		int whichwrite;
		ni_name *tags;
		int pid;
		int tsock;
		int tport;
		CLIENT *tc;
		long tv_sec;
		long rtv_sec;
		long wtv_sec;
		int abort;
		int needwrite;
		int uid;
		ni_name passwd;
} ni_private;

static unsigned long ni_connect_timeout = 30;
static unsigned long ni_connect_abort = 1;

static unsigned long _shared_handle_count_ = 0;
static ni_shared_handle_t **_shared_handle_ = NULL;
static ni_shared_handle_t *_shared_local_ = NULL;

unsigned long
get_ni_connect_timeout(void)
{
	return ni_connect_timeout;
}

void
set_ni_connect_timeout(unsigned long t)
{
	ni_connect_timeout = t;
}

unsigned long
get_ni_connect_abort(void)
{
	return ni_connect_abort;
}

void
ni_shared_set_flags(unsigned long mask)
{
	int i;

	if (_shared_local_ != NULL)
	{
		_shared_local_->flags |= mask;
	}

	for (i = 0; i < _shared_handle_count_; i++)
	{
		_shared_handle_[i]->flags |= mask;
	}
}

void
ni_shared_clear_flags(unsigned long mask)
{
	int i;

	if (_shared_local_ != NULL)
	{
		_shared_local_->flags &= ~mask;
	}

	for (i = 0; i < _shared_handle_count_; i++)
	{
		_shared_handle_[i]->flags &= ~mask;
	}
}

void
set_ni_connect_abort(unsigned long a)
{
	ni_connect_abort = a;
}

static ni_shared_handle_t *
ni_shared_handle(struct in_addr *addr, char *tag)
{
	struct sockaddr_in sa;
	void *domain, *d2;
	ni_status status;
	ni_id root;
	ni_shared_handle_t *h;

	if (addr == NULL) return NULL;
	if (tag == NULL) return NULL;

	memset(&sa, 0, sizeof(struct in_addr));
	sa.sin_family = AF_INET;
	sa.sin_addr = *addr;

	domain = ni_connect(&sa, tag);

	if (domain == NULL) return NULL;
	if (strcmp(tag, "local") != 0)
	{
		ni_setreadtimeout(domain, ni_connect_timeout);
		ni_setabort(domain, ni_connect_abort);

		d2 = ni_new(domain, ".");
		ni_free(domain);
		domain = d2;
		if (domain == NULL) return NULL;
	}

	root.nii_object = 0;
	root.nii_instance = 0;

	status = ni_self(domain, &root);

	if (status != NI_OK) 
	{
		ni_free(domain);
		return NULL;
	}

	h = (ni_shared_handle_t *)calloc(1, sizeof(ni_shared_handle_t));
	if (h == NULL) return NULL;

	h->refcount = 1;
	h->flags = 0;
	h->ni = domain;
	h->parent = NULL;

	return h;
}
	
ni_shared_handle_t *
ni_shared_local(void)
{
	struct in_addr loop;

	if (_shared_local_ != NULL)
	{
		_shared_local_->refcount++;
		return _shared_local_;
	}

	memset(&loop, 0, sizeof(struct in_addr));
	loop.s_addr = htonl(INADDR_LOOPBACK);

	_shared_local_ = ni_shared_handle(&loop, "local");

	return _shared_local_;
}

static int
ni_shared_match(ni_shared_handle_t *h, struct in_addr *a, char *t)
{
	ni_private *ni;
	unsigned long i;

	if (h == NULL) return 0;
	if (h->ni == NULL) return 0;
	if (a == NULL) return 0;
	if (t == NULL) return 0;

	ni = (ni_private *)h->ni;
	if (ni == NULL) return 0;

	for (i = 0; i < ni->naddrs; i++)
	{
		if ((ni->addrs[i].s_addr == a->s_addr) && (strcmp(ni->tags[i], t) ==  0)) return 1;
	}

	return 0;
}

ni_shared_handle_t *
ni_shared_connection(struct in_addr *addr, char *tag)
{
	unsigned long i;
	ni_shared_handle_t *h;

	if (addr == NULL) return NULL;
	if (tag == NULL) return NULL;

	if (!strcmp(tag, "local") && (addr->s_addr == htonl(INADDR_LOOPBACK)))
	{
		return ni_shared_local();
	}

	for (i = 0; i < _shared_handle_count_; i++)
	{
		if (ni_shared_match(_shared_handle_[i], addr, tag))
		{
			_shared_handle_[i]->refcount++;
			return _shared_handle_[i];
		}
	}
	
	h = ni_shared_handle(addr, tag);
	if (h == NULL) return NULL;

	if (_shared_handle_count_ == 0)
	{
		_shared_handle_ = (ni_shared_handle_t **)malloc(sizeof(ni_shared_handle_t *));
	}
	else
	{
		_shared_handle_ = (ni_shared_handle_t **)realloc(_shared_handle_, (_shared_handle_count_ + 1) * sizeof(ni_shared_handle_t *));
	}

	_shared_handle_[_shared_handle_count_] = h;
	_shared_handle_count_++;

	return h;
}

void
ni_shared_release(ni_shared_handle_t *h)
{
	unsigned long i, j;

	if (h == NULL) return;

	if (h->refcount > 0) h->refcount--;
	if (h->refcount > 0) return;
	
	ni_free(h->ni);
	h->parent = NULL;

	if (h == _shared_local_)
	{
		free(_shared_local_);
		_shared_local_ = NULL;
		return;
	}

	for (i = 0; i < _shared_handle_count_; i++)
	{
		if (_shared_handle_[i] == h)
		{
			free(_shared_handle_[i]);
			for (j = i + 1; j < _shared_handle_count_; j++, i++)
			{
				_shared_handle_[i] = _shared_handle_[j];
			}
			_shared_handle_count_--;
			if (_shared_handle_count_ == 0)
			{
				free(_shared_handle_);
				_shared_handle_ = NULL;
				return;
			}
			_shared_handle_ = (ni_shared_handle_t **)realloc(_shared_handle_, _shared_handle_count_ * sizeof(ni_shared_handle_t *));
			return;
		}
	}
}

ni_shared_handle_t *
ni_shared_retain(ni_shared_handle_t *h)
{
	if (h == NULL) return NULL;

	h->refcount++;
	return h;
}

ni_shared_handle_t *
ni_shared_parent(ni_shared_handle_t *h)
{
	ni_rparent_res rpres;
	ni_private *ni;
	struct in_addr addr;
	ni_shared_handle_t *p;
	struct timeval now, tnew, tcurr;
	enum clnt_stat rpc_status;
	ni_status status;

	if (h == NULL) return NULL;
	if (h->ni == NULL) return NULL;
	if (h->parent != NULL)
	{
		ni_shared_retain((ni_shared_handle_t *)h->parent);
		return (ni_shared_handle_t *)h->parent;
	}

	now.tv_sec = 0;

	if (h->flags & NI_SHARED_ISROOT)
	{
		gettimeofday(&now, NULL);
		if (now.tv_sec <= (h->isroot_time + NI_SHARED_ISROOT_TIMEOUT)) return NULL;
		h->flags &= ~NI_SHARED_ISROOT;
	}

	ni = (ni_private *)h->ni;

	status = NI_FAILED;

	if (ni->tc != NULL)
	{
		memset(&rpres, 0, sizeof(ni_rparent_res));
		tnew.tv_sec = 60;
		tnew.tv_usec = 0;

		clnt_control(ni->tc, CLGET_TIMEOUT, &tcurr);
		clnt_control(ni->tc, CLSET_TIMEOUT, &tnew);

		rpc_status = clnt_call(ni->tc, _NI_RPARENT, xdr_void, NULL, xdr_ni_rparent_res, &rpres, tnew);

		clnt_control(ni->tc, CLSET_TIMEOUT, &tcurr);

		if (rpc_status == RPC_SUCCESS) status = rpres.status;
	}

	if (status == NI_NETROOT)
	{
		if (now.tv_sec == 0) gettimeofday(&now, NULL);
		h->isroot_time = now.tv_sec;
		h->flags |= NI_SHARED_ISROOT;
		return NULL;
	}

	if (status != NI_OK) return NULL;

	addr.s_addr = htonl(rpres.ni_rparent_res_u.binding.addr);
	p = ni_shared_connection(&addr, rpres.ni_rparent_res_u.binding.tag);
	free(rpres.ni_rparent_res_u.binding.tag);

	h->parent = p;
	return p;
}

ni_shared_handle_t *
ni_shared_open(void *x, char *rel)
{
	void *d;
	ni_private *ni;
	ni_status status;
	ni_shared_handle_t *h;

	if (rel == NULL) return NULL;

	status = ni_open(x, rel, &d);
	if (status != NI_OK) return NULL;

	ni = (ni_private *)d;
	h = ni_shared_connection(&(ni->addrs[0]), ni->tags[0]);
	ni_free(d);
	return h;
}
