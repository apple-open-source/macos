struct manpage {
     struct manpage *next;
     char *filename;
     int type;
};

#define TYPE_MAN	1
#define TYPE_CAT	2
#define TYPE_SCAT	4
#define TYPE_XML	8

#define ONLY_ONE_PERSEC	16	/* do not return more pages from one section */
#define ONLY_ONE	48	/* return only a single page */

/* various standards have various ideas about where the cat pages
   ought to live */
#define FSSTND		64
#define	FHS		128

/* HP has a peculiar way to indicate that pages are compressed */
#define DO_HP		256	/* compressed file in man1.Z/ls.1 */

/* IRIX has a peculiar cat page naming */
#define DO_IRIX		512	/* cat page ls.z, not ls.1.z */

/* Sun uses both man and sman, where sman contains XML */
#define DO_SUN		1024	/* unused today */

/* NTFS cannot handle : in filenames */
#define DO_WIN32	2048	/* turn :: into ? */

extern struct manpage *
manfile(const char *name, const char *section, int flags,
        char **sectionlist, char **manpath,
	const char *(*tocat)(const char *, const char *, int));
