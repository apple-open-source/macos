/*
 * super-dns client.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <sys/time.h>
#include <NetInfo/sdns.h>

#define DEFAULT_RESOLVER_CONF "/etc/resolv.conf"
#define RESOLVER_DIR "/etc/resolver"

#define DEFAULT_STAT_LATENCY 10

/*
 * Open a named client using dns_open.
 */
static sdns_client_t *
_sdns_client_open(sdns_handle_t *sdns, char *name)
{
	sdns_client_t *c;
	dns_handle_t *dns;
	int status;
	struct stat sb;
	char *path;
	struct timeval now;

	memset(&sb, 0, sizeof(struct stat));
	dns = dns_open(name);
	if (dns == NULL) return NULL;

	c = (sdns_client_t *)calloc(1, sizeof(sdns_client_t));

	c->name = NULL;
	if ((name == NULL) && (dns->domain != NULL)) c->name = strdup(dns->domain);
	else if (name != NULL) c->name = strdup(name);

	c->dns = dns;

	dns_open_log(dns, sdns->log_title, sdns->log_dest, sdns->log_file, sdns->log_flags, sdns->log_facility, sdns->log_callback);

	gettimeofday(&now, NULL);
	if (name == NULL)
	{
		status = stat(DEFAULT_RESOLVER_CONF, &sb);
	}
	else
	{
		path = malloc(strlen(RESOLVER_DIR) + strlen(name) + 2);
		sprintf(path, "%s/%s", RESOLVER_DIR, name);
		status = stat(path, &sb);
		free(path);
	}

	c->modtime = sb.st_mtimespec.tv_sec;
	c->stattime = now.tv_sec;

	return c;
}

static void
_sdns_client_free(sdns_client_t *c)
{
	if (c == NULL) return;

	if (c->name != NULL) free(c->name);
	if (c->dns != NULL) dns_free(c->dns);
	free(c);
}

/*
 * Fetches a client, possibly from cache.
 */
static sdns_client_t *
_sdns_client_for_name(sdns_handle_t *sdns, char *name, unsigned int validate)
{
	struct stat sb;
	char *p, *path;
	int i, status, use_default, trailing_dot;
	sdns_client_t *c;
	DIR *dp;
	struct direct *d;
	struct timeval now;

	if (sdns == NULL) return NULL;

	memset(&sb, 0, sizeof(struct stat));

	use_default = 0;
	if (name == NULL) use_default = 1;
	else if (name[0] == '\0') use_default = 1;

	if (use_default == 1)
	{
		gettimeofday(&now, NULL);
		if (((sdns->dns_default != NULL) && (validate == 1) && (now.tv_sec > (sdns->dns_default->stattime + sdns->stat_latency))))
		{
			/* Check modtime */
			status = stat(DEFAULT_RESOLVER_CONF, &sb);
			if ((status != 0) || (sb.st_mtimespec.tv_sec > sdns->dns_default->modtime))
			{
				_sdns_client_free(sdns->dns_default);
				sdns->dns_default = NULL;
			}
			else if (status == 0)
			{
				sdns->dns_default->stattime = now.tv_sec;
			}
		}

		if (sdns->dns_default == NULL) sdns->dns_default = _sdns_client_open(sdns, NULL);

		return sdns->dns_default;
	}

	gettimeofday(&now, NULL);
	if ((validate == 1) && (now.tv_sec > (sdns->stattime + sdns->stat_latency)))
	{
		/* If directory changed, we invalidate cache */
		status = stat(RESOLVER_DIR, &sb);
		if ((status != 0) || (sb.st_mtimespec.tv_sec > sdns->modtime))
		{
			for (i = 0; i < sdns->client_count; i++)
			{
				_sdns_client_free(sdns->client[i]);
			}

			sdns->client_count = 0;
			if (sdns->client != NULL) free(sdns->client);
			sdns->client = NULL;
		}

		if ((status == 0) && (sb.st_mtimespec.tv_sec > sdns->modtime))
		{
			dp = opendir(RESOLVER_DIR);
			if (dp != NULL)
			{
				while (NULL != (d = readdir(dp)))
				{
					if (d->d_name[0] == '.') continue;

					c = _sdns_client_open(sdns, d->d_name);
					if (c == NULL) continue;

					if (sdns->client_count == 0)
					{
						sdns->client = (sdns_client_t **)malloc(sizeof(sdns_client_t *));
					}
					else
					{
						sdns->client = (sdns_client_t **)realloc(sdns->client, (sdns->client_count + 1) * sizeof(sdns_client_t *));
					}
			
					sdns->client[sdns->client_count] = c;
					sdns->client_count++;
				}
				closedir(dp);
			}
			
			sdns->modtime = sb.st_mtimespec.tv_sec;
			sdns->stattime = now.tv_sec;
		}
		else if (status == 0)
		{
			sdns->stattime  = now.tv_sec;
		}
	}

	trailing_dot = -1;
	i = strlen(name) - 1;
	if ((i >= 0) && (name[i] == '.'))
	{
		name[i] = '\0';
		trailing_dot = i;
	}

	p = name;
	while (p != NULL)
	{
		for (i = 0; i < sdns->client_count; i++)
		{
			if (!strcasecmp(sdns->client[i]->name, p))
			{
				gettimeofday(&now, NULL);
				if (now.tv_sec > (sdns->client[i]->stattime + sdns->stat_latency))
				{
					path = malloc(strlen(RESOLVER_DIR) + strlen(p) + 2);
					sprintf(path, "%s/%s", RESOLVER_DIR, p);
					status = stat(path, &sb);
					free(path);
					if (status != 0)
					{
						/* XXX Something bad happened */
						if (trailing_dot >= 0) name[trailing_dot] = '.';
						return NULL;
					}
					else if (sb.st_mtimespec.tv_sec > sdns->client[i]->modtime)
					{
						_sdns_client_free(sdns->client[i]);
						sdns->client[i] = _sdns_client_open(sdns, p);
					}
					else
					{
						sdns->client[i]->stattime = now.tv_sec;
					}
				}

				if (trailing_dot >= 0) name[trailing_dot] = '.';
				return sdns->client[i];
			}
		}

		p = strchr(p, '.');
		if (p != NULL) p++;
	}

	if (trailing_dot >= 0) name[trailing_dot] = '.';

	if (sdns->dns_default == NULL) sdns->dns_default = _sdns_client_open(sdns, NULL);

	return sdns->dns_default;
}

sdns_handle_t *
sdns_open()
{
	sdns_handle_t *s;

	s = (sdns_handle_t *)calloc(1, sizeof(sdns_handle_t));
	s->stat_latency = DEFAULT_STAT_LATENCY;

	return s;
}

void
sdns_free(sdns_handle_t *sdns)
{
	int i;

	if (sdns == NULL) return;

	_sdns_client_free(sdns->dns_default);

	for (i = 0; i < sdns->client_count; i++)
	{
		_sdns_client_free(sdns->client[i]);
	}

	sdns->client_count = 0;
	if (sdns->client != NULL) free(sdns->client);
	if (sdns->log_title != NULL) free(sdns->log_title);

	free(sdns);
}

static dns_reply_t *
_sdns_send_query(sdns_handle_t *sdns, dns_question_t *q, unsigned int validate, unsigned int fqdn)
{
	dns_question_t qq;
	sdns_client_t *c;
	dns_reply_t *r;

	c = _sdns_client_for_name(sdns, q->name, validate);
	if (c == NULL) return NULL;

	qq.type = q->type;
	qq.class = q->class;

	qq.name = calloc(1, strlen(q->name) + 4);
	if (qq.name == NULL) return NULL;

	sprintf(qq.name, "%s%s", q->name, (fqdn == 0) ? "." : "");

	r = dns_query(c->dns, &qq);

	free(qq.name);

	if (r == NULL) return NULL;
	
	if (r->status != DNS_STATUS_OK)
	{
		dns_free_reply(r);
		return NULL;
	}
	
	if ((r->header->flags & DNS_FLAGS_RCODE_MASK) != DNS_FLAGS_RCODE_NO_ERROR)
	{
		dns_free_reply(r);
		return NULL;
	}

	return r;
}

static dns_reply_t *
_sdns_query_validate(sdns_handle_t *sdns, dns_question_t *q, unsigned int validate, unsigned int recurse)
{
	sdns_client_t *c;
	dns_question_t qq;
	int i, fqdn;
	dns_reply_t *r;
	char *dot;

	/*
	 * If q->name is qualified:
	 *    If we have a client for the specified domain, use it.
	 *    Else use default client.
	 * If q->name is unqualified:
	 *    If there is a search list:
	 *        for each domain in default search list,
	 *            call sdns_query with the name qualified with that domain.
	 *    Else
	 *         call sdns_query with the name qualified with default domain name.
	 */

	if (sdns == NULL) return NULL;
	if (q == NULL) return NULL;
	if (q->name == NULL) return NULL;

	qq.type = q->type;
	qq.class = q->class;

	fqdn = 0;
	dot = strrchr(q->name, '.');

	if (dot != NULL)
	{
		if (*(dot + 1) == '\0') fqdn = 1;
		r = _sdns_send_query(sdns, q, validate, fqdn);
		if (r != NULL) return r;
		if (fqdn == 1) return NULL;
	}

	if (recurse == 0) return NULL;

	c = _sdns_client_for_name(sdns, NULL, validate);
	if (c == NULL) return NULL;

	if (c->dns->search_count > 0)
	{
		for (i = 0; i < c->dns->search_count	; i++)
		{
			qq.name = malloc(strlen(q->name) + strlen(c->dns->search[i]) + 2);
			sprintf(qq.name, "%s.%s", q->name, c->dns->search[i]);
			r = _sdns_query_validate(sdns, &qq, validate, 0);
			validate = 0;
			free(qq.name);
			if (r != NULL) return r;
		}
		return NULL;
	}
	
	qq.name = malloc(strlen(q->name) + strlen(c->dns->domain) + 2);
	sprintf(qq.name, "%s.%s", q->name, c->dns->domain);
	r = _sdns_query_validate(sdns, &qq, validate, 0);
	free(qq.name);
	return r;
}

dns_reply_t *
sdns_query(sdns_handle_t *sdns, dns_question_t *q)
{
	return _sdns_query_validate(sdns, q, 1, 1);
}

void
sdns_open_log(sdns_handle_t *sdns, char *title, int dest, FILE *file, int flags, int facility, int (*callback)(int, char *))
{
	if (sdns == NULL) return;

	if (title != NULL)
	{
		if (sdns->log_title != NULL) free(sdns->log_title);
		sdns->log_title = strdup(title);
	}

	sdns->log_dest = dest;
	sdns->log_file = file;
	sdns->log_flags = flags;
	sdns->log_facility = facility;
	sdns->log_callback = callback;
}
