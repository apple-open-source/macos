#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *rindex (const char *, int);	/* not always in <string.h> */

#include "defs.h"
#include "manfile.h"
#include "man-config.h"
#include "to_cat.h"
#include "util.h"

/*
 * Given PATH/man1/name.1, return a pointer to the '/' following PATH.
 */
static char *
mantail_of(char *name) {
     char *s0, *s1, *s;

     s0 = s1 = 0;
     for (s = name; *s; s++) {
	     if (*s == '/') {
		     s0 = s1;
		     s1 = s;
	     }
     }
     return s0;
}

/*
 * Given PATH/man1/name.1, return PATH, newly allocated.
 * The argument must be writable, not a constant string.
 */
const char *
mandir_of(const char *name) {
     char *p, *q;

     q = my_strdup(name);
     p = mantail_of(q);
     if (p) {
	  *p = 0;
	  return q;
     }
     free(q);
     return NULL;
}

/*
 * Change a name of the form PATH/man1/name.1[.Z]
 *                      into PATH/cat1/name.1.EXT
 * or (FSSTND) change /usr/PA/man/PB/man1/name.1
 *               into /var/catman/PA/PB/cat1/name.1.EXT
 * or (FHS) change /usr/PATH/share/man/LOC/man1/name.1
 *            into /var/cache/man/PATH/LOC/cat1/name.1.EXT
 * (here the /LOC part is absent or a single [locale] dir).
 *
 * Returns 0 on failure.
 */

const char *
convert_to_cat (const char *name0, const char *ext, int standards) {
     char *name, *freename, *cat_name = 0;
     char *t0, *t2, *t3, *t4;
     struct dirs *dlp;
     int len;

     freename = name = my_strdup (name0);

     t0 = rindex (name, '.');
     if (t0 && get_expander(t0)) /* remove compressee extension */
	  *t0 = 0;

     t2 = mantail_of (name);
     if (t2 == NULL)
	  return 0;
     *t2 = 0;			/* remove man1/name.1 part */

     if (strncmp(t2+1, "man", 3) != 0)
	  return 0;
     t2[1] = 'c';
     t2[3] = 't';

     len = (ext ? strlen(ext) : 0);

     /* Explicitly given cat file? */
     for (dlp = cfdirlist.nxt; dlp; dlp = dlp->nxt) {
	  if (!strcmp (name, dlp->mandir)) {
	       if (!dlp->catdir[0])
		    break;
	       *t2 = '/';
	       len += strlen (dlp->catdir) + strlen (t2) + 1;
	       cat_name = (char *) my_malloc (len);
	       strcpy (cat_name, dlp->catdir);
	       strcat (cat_name, t2);
	       goto gotit;
	  }
     }

     if (standards & FHS) {
	  if (*name != '/')
	       return 0;

	  /* possibly strip locale part */
	  t3 = t2;
	  if ((t4 = rindex(name,'/')) != NULL && strcmp(t4, "/man")) {
	       *t3 = '/';
	       t3 = t4;
	       *t3 = 0;
	  }

	  if(t3 - name >= 4 && !strcmp(t3 - 4, "/man")) {
	       /* fhs is applicable; strip leading /usr and trailing share */
	       if(!strncmp(name, "/usr/", 5))
	            name += 4;
	       t4 = t3 - 4;
	       *t4 = 0;
	       if(t4 - name >= 6 && !strcmp(t4 - 6, "/share"))
		    t4[-6] = 0;
	       *t3 = '/';

	       len += strlen("/var/cache/man") + strlen(name) + strlen(t3) + 1;
	       cat_name = (char *) my_malloc (len);
	       strcpy (cat_name, "/var/cache/man");
	       strcat (cat_name, name);
	       strcat (cat_name, t3);
	       goto gotit;
	  }

	  return 0;
     }

     if ((standards & FSSTND) && !strncmp(name, "/usr/", 5)) {
	  /* search, starting at the end, for a part `man' to delete */
	  t3 = t2;
	  while ((t4 = rindex(name, '/')) != NULL && strcmp(t4, "/man")) {
	       *t3 = '/';
	       t3 = t4;
	       *t3 = 0;
	  }
	  *t3 = '/';
	  if (t4) {
	       *t4 = 0;
	       len += strlen("/var/catman") + strlen (name+4) + strlen (t3) + 1;
	       cat_name = (char *) my_malloc (len);
	       strcpy (cat_name, "/var/catman");
	       strcat (cat_name, name+4);
	       strcat (cat_name, t3);
	       goto gotit;
	  }
     } else
	  *t2 = '/';

     if (ext) {			/* allocate room for extension */
	  len += strlen(name) + 1;
	  cat_name = (char *) my_malloc (len);
	  strcpy (cat_name, name);
     } else
	  cat_name = name;

gotit:

     if ((standards & DO_HP) && get_expander(cat_name)) {
	  /* nothing - we have cat1.Z/file.1 */
     } else if (ext)
	  strcat (cat_name, ext);

     if (name != cat_name)
	  free (freename);

     return cat_name;
}
