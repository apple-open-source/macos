
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
NBSPEntry_print(NBSPEntryRef entry)
{
    printf("%s: path %s\n", entry->name, entry->path);
    return;
}

int
NBSPList_count(NBSPListRef list)
{
    dynarray_t *	dlist = (dynarray_t *)list;
    
    return (dynarray_count(dlist));
}

NBSPEntryRef
NBSPList_element(NBSPListRef list, int i)
{
    dynarray_t *	dlist = (dynarray_t *)list;

    return (dynarray_element(dlist, i));
}


void
NBSPList_print(NBSPListRef list)
{
    dynarray_t *	dlist = (dynarray_t *)list;
    int 		i;

    for (i = 0; i < dynarray_count(dlist); i++) {
	NBSPEntryRef entry = (NBSPEntryRef)dynarray_element(dlist, i);
	NBSPEntry_print(entry);
    }
    return;
}

void
NBSPList_free(NBSPListRef * l)
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


NBSPListRef
NBSPList_init(const char * symlink_name)
{
    hfsVolList_t		vols = NULL;	
    dynarray_t *		list = NULL;			
    int				i;

    vols = hfsVolList_init();
    if (vols == NULL) {
	goto done;
    }

    for (i = 0; i < hfsVolList_count(vols); i++) {
	NBSPEntryRef	entry;
	char		sharename[MAXNAMLEN];
	int		sharename_len = 0;
	char		sharedir[PATH_MAX];
	int		sharedir_len = 0;
	char		sharelink[PATH_MAX];
	char *		root;
	struct stat	sb;
	hfsVol_t *	vol = hfsVolList_entry(vols, i);

	root = vol->mounted_on;
	if (strcmp(root, "/") == 0)
	    root = "";
	snprintf(sharelink, sizeof(sharelink), 
		 "%s" NETBOOT_DIRECTORY "/%s", root, symlink_name);
	if (lstat(sharelink, &sb) < 0) {
	    continue; /* doesn't exist */
	}
	if ((sb.st_mode & S_IFLNK) == 0) {
	    continue; /* not a symlink */
	}
	if (stat(sharelink, &sb) < 0) {
	    continue;
	}
	sharename_len = readlink(sharelink, sharename, sizeof(sharename));
	if (sharename_len <= 0) {
	    continue;
	}
	sharename[sharename_len] = '\0';
	if (list == NULL) {
	    list = (dynarray_t *)malloc(sizeof(*list));
	    if (list == NULL) {
		goto done;
	    }
	    bzero(list, sizeof(*list));
	    dynarray_init(list, free, NULL);
	}
	snprintf(sharedir, sizeof(sharedir), 
		 "%s" NETBOOT_DIRECTORY "/%s", root, sharename);
	sharedir_len = strlen(sharedir);
	entry = malloc(sizeof(*entry) + sharename_len + sharedir_len + 2);
	if (entry == NULL) {
	    continue;
	}
	bzero(entry, sizeof(*entry));
	entry->name = (char *)(entry + 1);
	strncpy(entry->name, sharename, sharename_len);
	entry->name[sharename_len] = '\0';
	entry->path = entry->name + sharename_len + 1;
	strncpy(entry->path, sharedir, sharedir_len);
	entry->path[sharedir_len] = '\0';
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
    return ((NBSPListRef)list);
}

#ifdef TEST_NBSP

int
main()
{
    NBSPListRef list = NBSPList_init();

    if (list != NULL) {
	NBSPList_print(list);
	NBSPList_free(&list);
    }
    
    exit(0);
}

#endif TEST_NBSP
