
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <string.h>
#include <sys/syslimits.h>
#include <dirent.h>

#include "dynarray.h"
#include "hfsvols.h"
#include "NetBootServer.h"
#include "nbsp.h"

static void
nbspEntry_print(nbspEntry_t * entry)
{
    printf("%s: path %s\n", entry->name, entry->path);
    return;
}

static void
nbspEntry_free(void * arg)
{
    nbspEntry_t * entry = (nbspEntry_t *)arg;
    if (entry->name)
	free(entry->name);
    if (entry->path)
	free(entry->path);
    bzero(entry, sizeof(*entry));
    free(entry);
    return;
}

int
nbspList_count(nbspList_t list)
{
    dynarray_t *	dlist = (dynarray_t *)list;
    
    return (dynarray_count(dlist));
}

nbspEntry_t *
nbspList_element(nbspList_t list, int i)
{
    dynarray_t *	dlist = (dynarray_t *)list;

    return (dynarray_element(dlist, i));
}


void
nbspList_print(nbspList_t list)
{
    dynarray_t *	dlist = (dynarray_t *)list;
    int 		i;

    printf("There are %d NetBoot sharepoints defined\n", 
	   dynarray_count(dlist));
    for (i = 0; i < dynarray_count(dlist); i++) {
	nbspEntry_t * entry = (nbspEntry_t *)dynarray_element(dlist, i);
	nbspEntry_print(entry);
    }
    return;
}

void
nbspList_free(nbspList_t * l)
{
    dynarray_t * list;
    if (l == NULL)
	return;
    list = *((dynarray_t * *)l);
    if (list == NULL)
	return;
    dynarray_free(list);
    free(list);
    *l = NULL;
    return;
}


nbspList_t
nbspList_init()
{
    hfsVolList_t		vols = NULL;	
    dynarray_t *		list = NULL;			
    int				i;


    vols = hfsVolList_init();
    if (vols == NULL) {
	goto done;
    }

    for (i = 0; i < hfsVolList_count(vols); i++) {
	nbspEntry_t *	entry;
	int		n = 0;
	char		sharename[MAXNAMLEN];
	char		sharedir[PATH_MAX];
	char		sharelink[PATH_MAX];
	char *		root;
	struct stat	sb;
	hfsVol_t *	vol = hfsVolList_entry(vols, i);

	root = vol->mounted_on;
	if (strcmp(root, "/") == 0)
	    root = "";
	snprintf(sharelink, sizeof(sharelink), 
		 "%s" NETBOOT_DIRECTORY "/" NETBOOT_SHAREPOINT_LINK, root);
	if (lstat(sharelink, &sb) < 0) {
	    continue; /* doesn't exist */
	}
	if ((sb.st_mode & S_IFLNK) == 0) {
	    continue; /* not a symlink */
	}
	if (stat(sharelink, &sb) < 0) {
	    continue;
	}
	n = readlink(sharelink, sharename, sizeof(sharename));
	if (n <= 0) {
	    continue;
	}
	sharename[n] = '\0';
	if (list == NULL) {
	    list = (dynarray_t *)malloc(sizeof(*list));
	    if (list == NULL) {
		goto done;
	    }
	    bzero(list, sizeof(*list));
	    dynarray_init(list, nbspEntry_free, NULL);
	}
	entry = malloc(sizeof(*entry));
	if (entry == NULL) {
	    continue;
	}
	snprintf(sharedir, sizeof(sharedir), 
		 "%s" NETBOOT_DIRECTORY "/%s", root, sharename);
	bzero(entry, sizeof(*entry));
	entry->name = strdup(sharename);
	entry->path = strdup(sharedir);
	dynarray_add((dynarray_t *)list, entry);
    }
 done:
    if (list) {
	if (dynarray_count((dynarray_t *)list) == 0) {
	    free(list);
	    list = NULL;
	}
    }
    if (vols)
	hfsVolList_free(&vols);
    return ((nbspList_t)list);
}

#ifdef TEST_NBSP

int
main()
{
    nbspList_t list = nbspList_init();

    if (list != NULL) {
	nbspList_print(list);
	nbspList_free(&list);
    }
    
    exit(0);
}

#endif TEST_NBSP
