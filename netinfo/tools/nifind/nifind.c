/*
 * nifind: NetInfo domain heirarchy search
 * Written by Marc Majka
 *
 * Copyright 1994, NeXT Computer Inc.
 */
#include <NetInfo/config.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <netinfo/ni.h>
#include <string.h>

#define MAX_DEPTH 100
#define RTIMEOUT 2
int local_ok, poke_timeout, verbose;
char *rootname;

typedef struct domain_node_s
{
	void *domain;					// handle
	char *master;					// master's hostname (may be "localhost")
	char *mastertag;				// master's tag
	char *masterhostname;			// master's real hostname
	char *name;						// domain name
	int nchildren;					// number of child domains
	int depth;						// depth in heirarchy (root is 0)
	struct domain_node_s *parent;	// parent domain
	struct domain_node_s **child; 	// list of children

} domain_node;

char *domain_path(domain_node *d)
{
	static char path[8192];
	int len, l, i;
	domain_node *t;

	if (d == NULL) return(NULL);

	if (d->parent == NULL)
	{
		strcpy(path, "/");
		return(path);
	}

	len = 1;
	for (t = d; t->parent != NULL; t = t->parent)
		len += strlen(t->name) + 1;

	i = len - 1;
	path[i] = '\0';

	for (t = d; t->parent != NULL; t = t->parent)
	{
		l = strlen(t->name);
		i -= l + 1;
		path[i] = '/';
		bcopy(t->name, path + i + 1, l);
	}

	return path;
}

domain_node *init_node(domain_node *relative, char *name)
{
	domain_node *node;
	ni_id dir;
	ni_status status;
	ni_namelist nl;
	void *d;
	char master[1024], tag[1024], str[1024];
	int i, j;

	if (relative == NULL) 
		status = ni_open(NULL, name, &d);
	else
		status = ni_open(relative->domain, name, &d);

	if (status != NI_OK) return(NULL);

	/* need to poke the domain to see if it's really alive */
	ni_setreadtimeout(d, poke_timeout);
	ni_setabort(d, 1);
	dir.nii_object = 0;
	status = ni_self(d, &dir);
	if (status != NI_OK)
	{
		if (verbose) fprintf(stderr, "No response from %s\n", name);
		return(NULL);
	}

	NI_INIT(&nl);
	status = ni_lookupprop(d, &dir, "master", &nl);
	if ((status != NI_OK) || (nl.ni_namelist_len == 0))
	{
		if (relative == NULL) 
			fprintf(stderr, "Domain %s has no master property!\n", name);
		else
			fprintf(stderr, "Domain %s/%s has no master property!\n",
				domain_path(relative), name);
		return(NULL);
	}

	node = (domain_node *)malloc(sizeof(domain_node));
	node->domain = d;

	strcpy(str, nl.ni_namelist_val[0]);
	ni_namelist_free(&nl);

	for (i = 0; str[i] != '/'; i++) master[i] = str[i];
	master[i] = '\0';
	node->master = malloc(strlen(master) + 1);
	strcpy(node->master, master);

	for (i++, j = 0; str[i] != '\0'; i++, j++) tag[j] = str[i];
	tag[j] = '\0';
	node->mastertag = malloc(strlen(tag) + 1);
	strcpy(node->mastertag, tag);

	node->parent = NULL;
	node->depth = 0;
	node->name = malloc(2);
	strcpy(node->name, "/");
	node->nchildren = 0;
	node->child = (struct domain_node_s **)malloc(1);

	node->masterhostname = NULL;

	return(node);
}

int domain_not_equal(domain_node *a, domain_node *b)
{
	if (a == NULL && b == NULL) return(0);
	if (a == NULL) return(1);
	if (b == NULL) return(1);
	return(strcmp(a->master, b->master) && strcmp(a->mastertag, b->mastertag));
}

void node_parent(domain_node *a, domain_node *b)
{
	char str[1024], name[1024], tag[1024];
	ni_id dir;
	ni_namelist nl;
	int i, j, k;
	ni_status status;

	if (a == NULL) return;

	a->parent = b;
	a->depth = b->depth + 1;
	if (a->depth > MAX_DEPTH)
	{
		fprintf(stderr, "\n\nError! domain %s exceeded max depth limit of NetInfo heirarchy\n", domain_path(a));
		exit(1);
	}

	if (!strcmp(a->master, "localhost"))
		sprintf(str,"/machines/%s", a->masterhostname);
	else
		sprintf(str, "/machines/%s", a->master);

	status = ni_pathsearch(b->domain, &dir, str);
	if (status != NI_OK)
	{
		fprintf(stderr, "Can't find master %s in parent domain %s\n", str, domain_path(b));
		return;
	}
	
	NI_INIT(&nl);
	status = ni_lookupprop(b->domain, &dir, "serves", &nl);
	if (status != NI_OK)
	{
		fprintf(stderr, "No serves property for master %s in parent domain %s\n",
			str, domain_path(b));
		ni_namelist_free(&nl);
		return;
	}

	for (i = 0; i < nl.ni_namelist_len; i++)
	{
		strcpy(str, nl.ni_namelist_val[i]);
		for (j = 0; str[j] != '/'; j++) name[j] = str[j];
		name[j] = '\0';
		for (j++, k = 0; str[j] != '\0'; j++, k++) tag[k] = str[j];		
		tag[k] = '\0';
		if (!strcmp(tag, a->mastertag))
		{
			free(a->name);
			a->name = malloc(strlen(name) + 1);
			strcpy(a->name, name);
		}
	}

	ni_namelist_free(&nl);

	b->child = (domain_node **)realloc(b->child, (b->nchildren+1) * sizeof(domain_node *));
	b->child[b->nchildren++] = a;
}

int node_free(domain_node *a)
{
	if (a == NULL) return(1);
	if (a->master != NULL) free(a->master);
	if (a->mastertag != NULL) free(a->mastertag);
	if (a->masterhostname != NULL) free(a->masterhostname);
	if (a->name != NULL) free(a->name);
	
	return(0);
}

void printnidir(domain_node *d, ni_id *dir)
{
	int i, j;
	ni_proplist p;
	ni_status status;

	/* get list of properties */
	NI_INIT(&p);
	status = ni_read(d->domain, dir, &p);
	if (status != NI_OK)
	{
		fprintf(stderr, "read failed for domain %s directory %lu\n",
			d->name, dir->nii_object);
		return;
	}

	for (i = 0; i < p.ni_proplist_len; i++)
	{
		printf("%s:", p.ni_proplist_val[i].nip_name);

		for (j = 0; j < p.ni_proplist_val[i].nip_val.ni_namelist_len; j++) 
			printf(" %s", p.ni_proplist_val[i].nip_val.ni_namelist_val[j]);

		printf("\n");
	}

	ni_proplist_free(&p);
}

void dir_find(domain_node *d, char *dirname, int verbose, int doprint)
{
	ni_status status;
	ni_id dir;
	char dom[1024];
	int i;

	if (!strcmp(rootname, "/")) strcpy(dom, domain_path(d));
	else
	{
		sprintf(dom, "%s%s", rootname, domain_path(d));
		i = strlen(dom) - 1;
		if (dom[i] == '/') dom[i] = '\0';
	}

	status = ni_pathsearch(d->domain, &dir, dirname);

	if (status == NI_OK)
	{
		printf("%s found in %s, id = %lu\n", dirname, dom, dir.nii_object);
		if (doprint)
		{
			printnidir(d, &dir);
			printf("\n");
		}
	}
	else if (status == NI_NODIR)
	{
		if (verbose) printf("%s not found in %s\n", dirname, domain_path(d));
	}
	else
	{
		printf("Search error in domain %s\n", d->name);
		perror("ni_pathsearch");
	}
}

int tree_add_children(domain_node *d)
{
	char str[1024], tmp[1024], dname[1024], host[1024];
	domain_node *child;
	ni_id machines, m;
	ni_idlist ml;
	ni_namelist nl, nl2;
	int i, j, k, known, local;
	ni_status status;

	status = ni_pathsearch(d->domain, &machines, "/machines");
	if (status != NI_OK) return(0);

	NI_INIT(&ml);
	status = ni_children(d->domain, &machines, &ml);
	if (status != NI_OK) return(0);

	strcpy(dname, domain_path(d));

	for (i = 0; i < ml.ni_idlist_len; i++)
	{
		m.nii_object = ml.ni_idlist_val[i];
		m.nii_instance = 0;

		NI_INIT(&nl);
		status = ni_lookupprop(d->domain, &m, "name", &nl);
		if ((status != NI_OK) || (nl.ni_namelist_len == 0)) return 0;

		strcpy(host, nl.ni_namelist_val[0]);
		ni_namelist_free(&nl);

		NI_INIT(&nl);
		status = ni_lookupprop(d->domain, &m, "serves", &nl);
		if (status == NI_OK)
		{
			for (j = 0; j < nl.ni_namelist_len; j++)
			{
				if (!strncmp(nl.ni_namelist_val[j], "./", 2))
				{
					/* ignore this domain */
				}
				else if (!strncmp(nl.ni_namelist_val[j], "../", 3))
				{
					/* ignore parent domain */
				}
				else
				{
					/* get child's name */
					for (k = 0; nl.ni_namelist_val[j][k] != '/' && nl.ni_namelist_val[j][k] != '\0'; k++)
						str[k] = nl.ni_namelist_val[j][k];
					str[k] = '\0';

					local = !strcmp(nl.ni_namelist_val[j]+k+1, "local");
					if (verbose) fprintf(stderr,"%s: %s serves %s - ", dname, host, nl.ni_namelist_val[j]);

					if ((local && local_ok) || (!local))
					{
						/* check if this child domain already got added */
						known = 0;
						for (k = 0; (k < d->nchildren) && (!known); k++)
							known = !strcmp(d->child[k]->name, str);

						if (!known)
						{
							if (verbose) fprintf(stderr,"new child domain\n");

							child = init_node(d, str);
							if ((child != NULL) && local)
							{
								NI_INIT(&nl2);
								status = ni_lookupprop(d->domain, &m, "name", &nl2);
								if ((status != NI_OK) || (nl2.ni_namelist_len == 0))
								{
									fprintf(stderr,"Can't get host name in directory %lu domain %s\n",
										m.nii_object, d->name);
									exit(1);
								}
								child->masterhostname = malloc(strlen(nl2.ni_namelist_val[0]) + 1);
								strcpy(child->masterhostname, nl2.ni_namelist_val[0]);
								ni_namelist_free(&nl2);
							}
							node_parent(child, d);
						}
						else if (verbose)
						{
							strcpy(tmp, domain_path(d));
							fprintf(stderr,"already have this child\n");
						}
					}
					else if (verbose) fprintf(stderr, "ignoring local domain\n");
				}
			}

			ni_namelist_free(&nl);
		}
	}

	for (i = 0; i < d->nchildren; i++)
		tree_add_children(d->child[i]);

	return(0);
}

void tree_dir_find(domain_node *d, char *dirname, int verbose, int doprint)
{
	int i;

	dir_find(d, dirname, verbose, doprint);
	for (i = 0; i < d->nchildren; i++)
		tree_dir_find(d->child[i], dirname, verbose, doprint);
}

void tree_free(domain_node *d)
{
	int i;

	for (i = 0; i < d->nchildren; i++)
		tree_free(d->child[i]);

	node_free(d);
}

void usage(char *name)
{
	fprintf(stderr, "usage: %s [options] directory [domain]\n", name);
	fprintf(stderr, "options:\n");
	fprintf(stderr, "   -a      search entire NetInfo hierarchy\n");
	fprintf(stderr, "   -v      verbose\n");
	fprintf(stderr, "   -n      ignore local domains\n");
	fprintf(stderr, "   -p      print directory contents\n");
	fprintf(stderr, "   -T %%d  connect timeout\n");
	fprintf(stderr, "directory: name of directory to find\n");
	fprintf(stderr, "domain:    use this domain as the root domain\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int all, dirp, rootp, doprint, i, j;
	domain_node *root, *local, *d, *up;
	char *dirname, str[1024];

	all = 0;
	dirp = 0;
	rootp = 0;
	doprint = 0;
	verbose = 0;
	poke_timeout = -1;
	local_ok = 1;
	
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (j = 1; argv[i][j] != '\0'; j++)
			{
				if (argv[i][j] == 'a') all = 1;
				else if (argv[i][j] == 'v') verbose = 1;
				else if (argv[i][j] == 'p') doprint = 1;
				else if (argv[i][j] == 'n') local_ok = 0;
				else if (argv[i][j] == 'T') poke_timeout = i + 1;
				else
				{
					fprintf(stderr, "unknown option: %c\n",argv[i][j]);
					usage(argv[0]);
				}
			}
			if (poke_timeout > 0)
			{
				poke_timeout = atoi(argv[++i]);
			}
		}
		else if (!dirp) dirp = i;
		else if (!rootp) rootp = i;
	}

	if (!dirp) usage(argv[0]);
	if (poke_timeout < 0) poke_timeout = 2;

	dirname = malloc(strlen(argv[dirp]) + 1);
	strcpy(dirname, argv[dirp]);

	if (!rootp)
	{
		rootname = malloc(2);
		strcpy(rootname, "/");
	}
	else
	{
		rootname = malloc(strlen(argv[rootp]) + 1);
		strcpy(rootname, argv[rootp]);
	}

	if (all)
	{
		root = init_node(NULL, rootname);
		if (root == NULL)
		{
			fprintf(stderr, "error: can't open domain %s\n", rootname);
			exit(1);
		}
		tree_add_children(root);
		tree_dir_find(root, dirname, verbose, doprint);
		tree_free(root);
	}
	else
	{
		root = init_node(NULL, rootname);
		if (root == NULL)
		{
			fprintf(stderr, "error: can't open root domain\n");
			exit(1);
		}

		local = init_node(NULL, ".");
		if (local == NULL)
		{
			fprintf(stderr, "error: can't open local domain\n");
			exit(1);
		}

		/* get real hostname for "localhost" */
		gethostname(str, 128);
		local->masterhostname = malloc(strlen(str) + 1);
		strcpy(local->masterhostname, str);

		d = local;
		up = local;
		while (d != NULL && domain_not_equal(d, root))
		{
			up = init_node(d, "..");
			node_parent(d, up);
			d = up;
		}
		node_free(root);
		root = up;

		d = local;
		while (d != NULL)
		{
			dir_find(d, dirname, verbose, doprint);
			d = d->parent;
		}

		tree_free(root);
	}

	exit(0);
}	

