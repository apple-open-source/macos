#include "ni_shared.h"
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinfo/ni.h>
#include <NetInfo/dsstore.h>

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

#define NETINFO_DB_DIR "/var/db/netinfo"
ni_shared_handle_t *
ni_shared_local(void)
{
	char *path;
	uint32_t flags;
	dsstatus status;
	dsstore *s;

	if (_shared_local_ != NULL)
	{
		_shared_local_->refcount++;
		return _shared_local_;
	}
	asprintf(&path, "%s/local.nidb", NETINFO_DB_DIR);

	flags = 0;
	flags |= DSSTORE_FLAGS_SERVER_MASTER;
	flags |= DSSTORE_FLAGS_ACCESS_READWRITE;
	flags |= DSSTORE_FLAGS_NOTIFY_CHANGES;

	status = dsstore_open(&s, path, flags);
	free(path);

	if (status != DSStatusOK) return NULL;

	_shared_local_ = (ni_shared_handle_t *)calloc(1, sizeof(ni_shared_handle_t));
	if (_shared_local_ == NULL) return NULL;

	_shared_local_->refcount = 1;
	_shared_local_->flags = NI_SHARED_LOCALRAW;
	_shared_local_->ni = s;
	_shared_local_->parent = NULL;

	return _shared_local_;
}

#define LOCAL_PORT 1033
ni_shared_handle_t *
ni_local_parent(void)
{
	struct sockaddr_in sin;
	int sock, status;
	void *domain;
	ni_id root;
	ni_shared_handle_t *h, *p;

	memset(&sin, 0, sizeof(struct sockaddr_in));

	sin.sin_family = AF_INET;
	sin.sin_port = htons(LOCAL_PORT);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	status = connect(sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
	close(sock);
	if (status < 0) return NULL;

	domain = ni_connect(&sin, "local");

	if (domain == NULL) return NULL;
	ni_setreadtimeout(domain, 1);
	ni_setabort(domain, 1);

	root.nii_object = 0;
	root.nii_instance = 0;

	status = ni_self(domain, &root);
	ni_free(domain);
	if (status != NI_OK) return NULL;

	h = ni_shared_handle(&(sin.sin_addr), "local");
	if (h == NULL) return NULL;

	p = ni_shared_parent(h);
	ni_shared_release(h);

	return p;
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
	
	if (h->flags & NI_SHARED_LOCALRAW)
	{
		dsstore_close(h->ni);
	}
	else
	{
		ni_free(h->ni);
	}

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

	free(h);
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

	if (h->flags & NI_SHARED_LOCALRAW)
	{
		return ni_local_parent();
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

		clnt_control(ni->tc, CLGET_TIMEOUT, (void *)&tcurr);
		clnt_control(ni->tc, CLSET_TIMEOUT, (void *)&tnew);

		rpc_status = clnt_call(ni->tc, _NI_RPARENT, xdr_void, NULL, xdr_ni_rparent_res, &rpres, tnew);

		clnt_control(ni->tc, CLSET_TIMEOUT, (void *)&tcurr);

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
	if ((x == NULL) && (!strcmp(rel, "."))) return ni_shared_local();

	status = ni_open(x, rel, &d);
	if (status != NI_OK) return NULL;

	ni = (ni_private *)d;
	h = ni_shared_connection(&(ni->addrs[0]), ni->tags[0]);
	ni_free(d);
	return h;
}

static ni_status
dstonistatus(dsstatus s)
{
	switch (s)
	{
		case DSStatusOK: return NI_OK;
		case DSStatusInvalidStore: return NI_INVALIDDOMAIN;
		case DSStatusNoFile: return NI_SYSTEMERR;
		case DSStatusReadFailed: return NI_PERM;
		case DSStatusWriteFailed: return NI_PERM;
		case DSStatusInvalidUpdate: return NI_SYSTEMERR;
		case DSStatusDuplicateRecord: return NI_SYSTEMERR;
		case DSStatusNoRootRecord: return NI_SYSTEMERR;
		case DSStatusLocked: return NI_SYSTEMERR;
		case DSStatusInvalidRecord: return NI_BADID;
		case DSStatusNoData: return NI_SYSTEMERR;
		case DSStatusInvalidRecordID: return NI_BADID;
		case DSStatusInvalidPath: return NI_BADID;
		case DSStatusInvalidKey: return NI_NOPROP;
		case DSStatusStaleRecord: return NI_STALE;
		case DSStatusPathNotLocal: return NI_INVALIDDOMAIN;
		case DSStatusInvalidSessionMode: return NI_SYSTEMERR;
		case DSStatusInvalidSession: return NI_SYSTEMERR;
		case DSStatusAccessRestricted: return NI_PERM;
		case DSStatusReadRestricted: return NI_PERM;
		case DSStatusWriteRestricted: return NI_PERM;
		case DSStatusFailed: return NI_FAILED;
		default: return NI_FAILED;
	}

	return NI_FAILED;
}


static char *
dsdatatostring(dsdata *d)
{
	char *s;

	if (d == NULL) return NULL;
	s = malloc(d->length + 1);
	memmove(s, d->data, d->length);
	s[d->length] = '\0';
	return s;
}


static char *
dsmetadatatostring(dsdata *d)
{
	char *s;

	if (d == NULL) return NULL;
	s = malloc(d->length + 2);
	memmove(s + 1, d->data, d->length);
	s[0] = '_';
	s[d->length + 1] = '\0';
	return s;
}


static void
dstoni_proplist(dsrecord *r, ni_proplist *o)
{
	int i, j, a, len, ix;
	ni_property *p;

	if (r == NULL) return;

	len = r->count + r->meta_count;

	o->ni_proplist_len = len;
	o->ni_proplist_val = NULL;
	if (len > 0)
		o->ni_proplist_val =
			(ni_property *)malloc(len * sizeof(ni_property));

	i = 0;
	for (a = 0; a < r->count; a++, i++)
	{
		ix = i;

		p = &(o->ni_proplist_val[ix]);
		p->nip_name = dsdatatostring(r->attribute[a]->key);

		len = r->attribute[a]->count;
		p->nip_val.ni_namelist_len = len;
		p->nip_val.ni_namelist_val = NULL;
		if (len > 0)
			p->nip_val.ni_namelist_val =
				(ni_name *)malloc(len * sizeof(ni_name));

		for (j = 0; j < r->attribute[a]->count; j++)
		{
			p->nip_val.ni_namelist_val[j] = dsdatatostring(r->attribute[a]->value[j]);
		}
	}
	
	for (a = 0; a < r->meta_count; a++, i++)
	{
		ix = i;

		p = &(o->ni_proplist_val[ix]);
		p->nip_name = dsmetadatatostring(r->meta_attribute[a]->key);

		len = r->meta_attribute[a]->count;
		p->nip_val.ni_namelist_len = len;
		p->nip_val.ni_namelist_val = NULL;
		if (len > 0)
			p->nip_val.ni_namelist_val =
				(ni_name *)malloc(len * sizeof(ni_name));

		for (j = 0; j < r->meta_attribute[a]->count; j++)
		{
			p->nip_val.ni_namelist_val[j] = dsdatatostring(r->meta_attribute[a]->value[j]);
		}
	}
}

ni_status
sa_self(ni_shared_handle_t *d, ni_id *n)
{
	dsrecord *r;
	dsstore *s;

	if (d == NULL) return NI_INVALIDDOMAIN;
	if (n == NULL) return NI_BADID;

	if ((d->flags & NI_SHARED_LOCALRAW) == 0) return ni_self(d->ni, n);

	s = (dsstore *)d->ni;
	r = dsstore_fetch(s, n->nii_object);
	if (r == NULL) return NI_NODIR;

	n->nii_instance = r->serial;

	dsrecord_release(r);
	return NI_OK;	
}

ni_status
sa_root(ni_shared_handle_t *d, ni_id *n)
{
	if (d == NULL) return NI_INVALIDDOMAIN;
	if (n == NULL) return NI_BADID;

	n->nii_object = 0;

	return sa_self(d, n);	
}

ni_status
sa_lookup(ni_shared_handle_t *d, ni_id *n, ni_name_const pname, ni_name_const pval, ni_idlist *hits)
{
	dsstore *s;
	dsdata *k, *v;
	dsrecord *pat, *r, *c;
	dsattribute *a;
	u_int32_t i, match;

	if (d == NULL) return NI_INVALIDDOMAIN;
	if (n == NULL) return NI_BADID;
	if (pname == NULL) return NI_NONAME;
	if (hits == NULL) return NI_NOSPACE;

	if ((d->flags & NI_SHARED_LOCALRAW) == 0) return ni_lookup(d->ni, n, pname, pval, hits);

	s = (dsstore *)d->ni;
	r = dsstore_fetch(s, n->nii_object);
	if (r == NULL) return NI_NODIR;
	if (r->sub_count == 0)
	{
		dsrecord_release(r);
		return NI_NODIR;
	}

	pat = dsrecord_new();

	k = NULL;
	if (pname != NULL) k = utf8string_to_dsdata((char *)pname);

	v = NULL;
	if (pval != NULL) v = utf8string_to_dsdata((char *)pval);

	a = dsattribute_new(k);
	dsattribute_append(a, v);
	dsdata_release(k);
	dsdata_release(v);

	dsrecord_append_attribute(pat, a, SELECT_ATTRIBUTE);
	dsattribute_release(a);

	hits->ni_idlist_len = 0;
	hits->ni_idlist_val = NULL;
	match = -1;
	for (i = 0; i < r->sub_count; i++)
	{
		c = dsstore_fetch(s, r->sub[i]);
		if (c == NULL) continue;
		
		if (dsrecord_match(c, pat) == 1)
		{
			match = 1;

			if (hits->ni_idlist_len == 0)
			{
				hits->ni_idlist_val = (unsigned long *)malloc(sizeof(unsigned long));
			}
			else
			{
				hits->ni_idlist_val = (unsigned long *)realloc(hits->ni_idlist_val, (hits->ni_idlist_len + 1) * sizeof(unsigned long));
			}
			hits->ni_idlist_val[hits->ni_idlist_len] = r->sub[i];
			hits->ni_idlist_len++;
		}

		dsrecord_release(c);
	}

	dsrecord_release(r);
	dsrecord_release(pat);

	if (match == -1) return NI_NODIR;
	return NI_OK;	
}

static char *
eatslash(char *path)
{
	while (*path == '/') path++;
	return path;
}

static const char *
escindex(char *str, char ch)
{
	char *p;

	p = index(str, ch);
	if (p == NULL) return NULL;
	if (p == str) return p;
	if (p[-1] == '\\') return (escindex(p + 1, ch));
	return (p);
}

static void
unescape(char **name)
{
	char *newname, *p;
	int i, len;

	p = *name;
	len = strlen(p);
	newname = malloc(len + 1);
	for (i = 0; *p != 0; i++)
	{
		if (*p == '\\') p++;
		newname[i] = *p++;
	}

	ni_name_free(name);
	newname[i] = 0;
	*name = newname;
}

static char *
sa_name_dupn(char *start, char *stop)
{
	int len;
	char * new;

	if (stop != NULL) len = stop - start;
	else len = strlen(start);

	new = malloc(len + 1);
	memmove(new, start, len);
	new[len] = 0;
	return new;
}

static ni_status
sa_relsearch(ni_shared_handle_t *d, char *path, ni_id *n)
{
	char *slash, *equal;
	ni_name key, val;
	ni_idlist idl;
	ni_status status;

	if (d == NULL) return NI_INVALIDDOMAIN;
	if (n == NULL) return NI_BADID;

	slash = (char *)escindex(path, '/');
	equal = (char *)escindex(path, '=');

	if ((equal != NULL) && (((slash == NULL) || (equal < slash))))
	{
		key = sa_name_dupn(path, equal);
		val = sa_name_dupn(equal + 1, slash);
	}
	else if ((equal == NULL) || ((slash != NULL) && (slash < equal)))
	{
		key = strdup("name");
		val = sa_name_dupn(path, slash);
	}
	else
	{
		key = sa_name_dupn(path, equal);
		val = sa_name_dupn(equal + 1, slash);
	}

	unescape(&key);
	unescape(&val);

	NI_INIT(&idl);

	status = sa_lookup(d, n, key, val, &idl);
	if (status != NI_OK)
	{
	  	ni_name_free(&key);
		ni_name_free(&val);
		return status;
	}

	n->nii_object = idl.niil_val[0];
	ni_name_free(&key);
	ni_name_free(&val);
	ni_idlist_free(&idl);

	if (slash == NULL)
	{
		sa_self(d, n);
		return NI_OK;
	}

	path = eatslash(slash);
	return sa_relsearch(d, path, n);
}

ni_status 
sa_pathsearch(ni_shared_handle_t *d, ni_id *n, char *p)
{
	ni_status status;

	if (d == NULL) return NI_INVALIDDOMAIN;
	if (n == NULL) return NI_BADID;
	if (p == NULL) return NI_NODIR;

	if (*p == '/')
	{
		status = sa_root(d, n);
		if (status != NI_OK) return status;
	}

	p = eatslash(p);
	if (*p != 0)
	{
		status = sa_relsearch(d, p, n);
		if (status != NI_OK) return status;
	}

	return NI_OK;
}

ni_status
sa_read(ni_shared_handle_t *d, ni_id *n, ni_proplist *pl)
{
	dsrecord *r;
	dsstore *s;

	if (d == NULL) return NI_INVALIDDOMAIN;
	if (n == NULL) return NI_BADID;
	if (pl == NULL) return NI_NOSPACE;

	if ((d->flags & NI_SHARED_LOCALRAW) == 0) return ni_read(d->ni, n, pl);

	s = (dsstore *)d->ni;
	r = dsstore_fetch(s, n->nii_object);
	if (r == NULL) return NI_NODIR;

	n->nii_instance = r->serial;

	dstoni_proplist(r, pl);
	dsrecord_release(r);
	return NI_OK;	
}

void
sa_setpassword(ni_shared_handle_t *d, char *pw)
{
	if (d == NULL) return;

	if ((d->flags & NI_SHARED_LOCALRAW) == 0) ni_setpassword(d->ni, pw);
}

ni_status
sa_statistics(ni_shared_handle_t *d, ni_proplist *pl)
{
	dsstore *s;
	char *str;

	if (d == NULL) return NI_INVALIDDOMAIN;
	if (pl == NULL) return NI_NOSPACE;

	if ((d->flags & NI_SHARED_LOCALRAW) == 0) return ni_statistics(d->ni, pl);
	s = (dsstore *)d->ni;
	pl->ni_proplist_len = 1;
	pl->ni_proplist_val = (ni_property *)malloc(sizeof(ni_property));

	pl->ni_proplist_val[0].nip_name = strdup("checksum");
	pl->ni_proplist_val[0].nip_val.ni_namelist_len = 1;
	pl->ni_proplist_val[0].nip_val.ni_namelist_val = malloc(sizeof(char *));
	asprintf(&str, "%u", dsstore_nichecksum(s));
	pl->ni_proplist_val[0].nip_val.ni_namelist_val[0] = str;

	return NI_OK;
}

ni_status
sa_children(ni_shared_handle_t *d, ni_id *n, ni_idlist *children)
{
	dsrecord *r;
	dsstore *s;
	u_int32_t i;

	if (d == NULL) return NI_INVALIDDOMAIN;
	if (n == NULL) return NI_BADID;
	if (children == NULL) return NI_NOSPACE;

	if ((d->flags & NI_SHARED_LOCALRAW) == 0) return ni_children(d->ni, n, children);

	s = (dsstore *)d->ni;
	r = dsstore_fetch(s, n->nii_object);
	if (r == NULL) return NI_NODIR;

	children->ni_idlist_len = r->sub_count;
	if (r->sub_count == 0)
	{
		dsrecord_release(r);
		return NI_OK;
	}

	children->ni_idlist_val = (unsigned long *)malloc(r->sub_count * sizeof(unsigned long));
	for (i = 0; i < r->sub_count; i++) children->ni_idlist_val[i] = r->sub[i];
	dsrecord_release(r);

	return NI_OK;	
}

ni_status
sa_list(ni_shared_handle_t *d, ni_id *n, ni_name_const pname, ni_entrylist *entries)
{
	dsstore *s;
	u_int32_t i, j, dsid;
	dsrecord *l;
	dsstatus status;
	dsdata *k;

	if (d == NULL) return NI_INVALIDDOMAIN;
	if (n == NULL) return NI_BADID;
	if (entries == NULL) return NI_NOSPACE;

	if ((d->flags & NI_SHARED_LOCALRAW) == 0) return ni_list(d->ni, n, pname, entries);

	s = (dsstore *)d->ni;

	if (pname == NULL) k = utf8string_to_dsdata("name");
	else k = utf8string_to_dsdata((char *)pname);

	dsid = n->nii_object;

	status = dsstore_list(s, dsid, k, SELECT_ATTRIBUTE, &l);
	dsdata_release(k);
	if (status != DSStatusOK) return dstonistatus(status);

	if (l == NULL) entries->ni_entrylist_len = 0;
	else entries->ni_entrylist_len = l->count;

	if (entries->ni_entrylist_len > 0)
		entries->ni_entrylist_val = (ni_entry *)malloc(entries->ni_entrylist_len * sizeof(ni_entry));

	for (i = 0; i < entries->ni_entrylist_len; i++)
	{
		entries->ni_entrylist_val[i].id = dsdata_to_uint32(l->attribute[i]->key);
		entries->ni_entrylist_val[i].names = (ni_namelist *)malloc(sizeof(ni_namelist));

		entries->ni_entrylist_val[i].names->ni_namelist_len = l->attribute[i]->count;
		entries->ni_entrylist_val[i].names->ni_namelist_val = NULL;
		if (l->attribute[i]->count > 0)
			entries->ni_entrylist_val[i].names->ni_namelist_val = (ni_name *)malloc(l->attribute[i]->count * sizeof(ni_name));
		
		for (j = 0; j < l->attribute[i]->count; j++)
			entries->ni_entrylist_val[i].names->ni_namelist_val[j] = dsdatatostring(l->attribute[i]->value[j]);
	}

	dsrecord_release(l);

	return NI_OK;	
}

ni_status
sa_addrtag(ni_shared_handle_t *d, struct sockaddr_in *addr, ni_name *tag)
{
	if (d == NULL) return NI_INVALIDDOMAIN;
	if (addr == NULL) return NI_NOSPACE;
	if (tag == NULL) return NI_NOSPACE;

	if ((d->flags & NI_SHARED_LOCALRAW) == 0) return ni_addrtag(d->ni, addr, tag);

	addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	*tag = strdup("local");

	return NI_OK;
}

void
sa_setabort(ni_shared_handle_t *d, unsigned int a)
{
	if (d == NULL) return;
	if (d->flags & NI_SHARED_LOCALRAW) return;

	ni_setabort(d->ni, a);
}

void
sa_setreadtimeout(ni_shared_handle_t *d, unsigned int t)
{
	if (d == NULL) return;
	if (d->flags & NI_SHARED_LOCALRAW) return;

	ni_setreadtimeout(d->ni, t);
}

/*
 * Search from local domain to root domain to locate a path.
 */
ni_status
sa_find(ni_shared_handle_t **dom, ni_id *nid, ni_name dirname, unsigned int timeout)
{
	ni_shared_handle_t *d, *p;
	ni_id n;
	ni_status status;

	*dom = NULL;
	nid->nii_object = NI_INDEX_NULL;
	nid->nii_instance = NI_INDEX_NULL;
	
	d = ni_shared_local();
	if (d == NULL) return NI_FAILED;

	while (d != NULL)
	{
		if (timeout > 0)
		{
			sa_setreadtimeout(d, timeout);
			sa_setabort(d, 1);
		}

		status = sa_pathsearch(d, &n, dirname);
		if (status == NI_OK)
		{
			*dom = d;
			*nid = n;
			return NI_OK;
		}
	
		p = ni_shared_parent(d);
		ni_shared_release(d);
		d = p;
	}
	
	return NI_NODIR;
}
