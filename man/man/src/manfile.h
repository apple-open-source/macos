struct manpage {
     struct manpage *next;
     char *filename;
     int type;
};

#define TYPE_MAN	0x0001
#define TYPE_CAT	0x0002
#define TYPE_SCAT	0x0004
#define TYPE_HTML	0x0008
#define TYPE_XML	0x0010	/* not presently used */

#define ONLY_ONE_PERSEC	0x0020	/* do not return more pages from one section */
#define ONLY_ONE	0x0040	/* return only a single page */

/* various standards have various ideas about where the cat pages
   ought to live */
#define FSSTND		0x0080
#define	FHS		0x0100

/* HP has a peculiar way to indicate that pages are compressed */
#define DO_HP		0x0200	/* compressed file in man1.Z/ls.1 */

/* IRIX has a peculiar cat page naming */
#define DO_IRIX		0x0400	/* cat page ls.z, not ls.1.z */

/* Sun uses both man and sman, where sman contains XML */
#define DO_SUN		0x0800	/* unused today */

/* NTFS cannot handle : in filenames */
#define DO_WIN32	0x1000	/* turn :: into ? */

extern struct manpage *
manfile(const char *name, const char *section, int flags,
        char **sectionlist, char **manpath,
	const char *(*tocat)(const char *, const char *, int));
