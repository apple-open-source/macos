/*
 *	Copyright (c) 1988, 1989 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 */

#include <sys/unistd.h>
/* XXX static char sccsid[] = "@(#)chooser.c: 2.0, 1.9; 1/27/93; Copyright 1988-89, Apple Computer, Inc."; */

#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <string.h>

#include <netat/appletalk.h>

#include <AppleTalk/at_proto.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <fcntl.h>

static char	gError[400];	/* Error message buffer */
extern char 	*rData();
char		dataFile[100];

	/* device file names for the three printers */
#define	IMAGEWRITER_FILE	"AppleTalk ImageWriter"
#define	LQ_IMAGEWRITER_FILE	"LQ AppleTalk ImageWriter"
#define	LASERWRITER_FILE	"LaserWriter"

	/* type strings for the three printers */
#define	IMAGEWRITER_TYPE	"ImageWriter"
#define	LQ_IMAGEWRITER_TYPE	"LQ"
#define	LASERWRITER_TYPE	"LaserWriter"

	/* printer name storage */
#define DATA_FILE	"atprint.dat"
#define DATA_DIR	"/etc/atalk"	/* this dir is defined in packaging, 
									   not in the makefile */

	/* macros */
#define MASK(o,g,w) ((o&0x7)<<6|(g&0x7)<<3|w)
/*#define MSGSTR(num,str)		catgets(catd, MS_ATBIN, num,str)*/
#define MSGSTR(num,str)			str

	/* structs */
/* Resource item in resource data section */

typedef struct {
	long	len;
	char	data[512];	/* We only read up to 512 bytes of data */
} RForkData;

static RForkData data;

static	int nocase_strncmp(char *str1, char *str2, int count);

/* Gets the chooser default printer setting and returns pointer to a C string
 * of the form object:type@zone.
 */
char *
get_chooserPrinter(progname)
char *progname;
{
	FILE *fd;
	char *p, *oldp;
	at_nvestr_t myzone;

	sprintf(dataFile,"%s/%s",DATA_DIR,DATA_FILE);
	sprintf(gError, MSGSTR(M_ERR_OPEN,"error opening %s file"), 
		dataFile);
	
	if ((fd = fopen(dataFile, "r")) == NULL) {
		if (errno == ENOENT)
			sprintf (gError, MSGSTR(M_DEF_PRINTER,
				"No default printer selected. Select a printer by\n"\
				"using the at_cho_prn command.\n"));
		goto bad;
	}

	if(fread (&data.len,sizeof(data.len),1,fd) != 1)
		goto bad;
	if (fread(data.data, data.len, 1, fd) != 1)
		goto bad;
	fclose(fd);
	
	/* Format of 'PAPA' data is object,type,zone,etc as pascal 
	 * strings.  Massage to a C nve string.
	 */
	if (data.data[0] == 0)
	    goto bad;
	p = data.data + data.data[0] + 1;
	oldp = p;
	p += *p + 1;
	*oldp = ':';
	oldp = p;
	p += *p + 1;
	*oldp = '@';
	*p = '\0';
	if (zip_getmyzone(ZIP_DEF_INTERFACE, &myzone) != -1
	    && myzone.len == 1 && myzone.str[0] == '*') {
		*++oldp = '*';
		*++oldp = 0;
        }

	return (&data.data[1]);

bad:
	if (fd != NULL)
		(void) fclose(fd);
	fprintf(stderr, "%s: %s\n", progname, gError);
	return (NULL);
}



/* Sets the system default printer name to the name in "entity".  Returns 0 on 
 * successful completion, -1 otherwise.
 */
int
set_chooserPrinter(progname, entity)
char	*progname;
at_entity_t	*entity;
{
	FILE *fd;
	int	offset;
	char	devfile[30];	/* name of the device file */
	int	i;
	struct stat st;
	
	errno=0;
	if (nocase_strncmp(LASERWRITER_TYPE, entity->type.str, entity->type.len)
		== 0){
		strcpy(&devfile[1], LASERWRITER_FILE);
		devfile[0] = strlen(LASERWRITER_FILE);
	} else if (nocase_strncmp(IMAGEWRITER_TYPE, entity->type.str, 
		entity->type.len) == 0) {
		strcpy(&devfile[1], IMAGEWRITER_FILE);
		devfile[0] = strlen(IMAGEWRITER_FILE);
	} else if (nocase_strncmp(LQ_IMAGEWRITER_TYPE, entity->type.str, 
		entity->type.len) == 0) {
		strcpy(&devfile[1], LQ_IMAGEWRITER_FILE);
		devfile[0] = strlen(LQ_IMAGEWRITER_FILE);
	} else {
		/* The type name doesn't match any known device */
		sprintf(gError,MSGSTR(M_UNKNOWN, "Unknown printer type"));
		goto bad;
	}
		/* this dir should exist, but just in case */
	if (getuid() == 0) {
		if (mkdir(DATA_DIR,  MASK(7,5,5)) ) {
			if(errno != EEXIST) {
				sprintf(gError,MSGSTR(M_ERR_CREAT,"error creating %s directory"),
					DATA_DIR);
				goto bad;
			}
		}
	}
		/* we won't check parent dir access, that would have already
		   been done with access_allowed() in calling fn
		 */
	i=0;
	if (!access(dataFile,F_OK))
		i=unlink(dataFile);
	if ( i || (fd=fopen(dataFile, "w"))== NULL) {
		if (errno == EACCES) {
			sprintf(gError, MSGSTR(M_ERR_MB_ROOT,
				"Error opening %s, you must have root permission\n"), dataFile);
		     	goto bad;
		}
		else {
			sprintf(gError, MSGSTR(M_ERR_OPEN,
				"error opening %s file"), dataFile);
		     	goto bad;
		}
	}
	if (stat(DATA_DIR,&st)) {
		sprintf(gError,MSGSTR(M_ERR_STAT,"Error getting status of %s:\n"),
			dataFile);
		goto bad;
	}
		/* set access of data file to that of parent (prevent user in
		   group a from locking out user in group b from changing
		   printer if mere mortals are allowed to cho_prn)
		 */
	if (chmod(dataFile,st.st_mode&MASK(6,6,6))) {
		sprintf(gError,MSGSTR(M_ERR_CHMOD,
			"Error setting access permision of %s:\n"), dataFile);
		goto bad;
	}

	data.len = entity->object.len + entity->type.len + entity->zone.len + 3;
	offset = 0;
	data.data[offset] = entity->object.len;
	strncpy(&data.data[offset+1], entity->object.str, entity->object.len);
	offset = entity->object.len + 1;
	data.data[offset] = entity->type.len;
	strncpy(&data.data[offset+1], entity->type.str, entity->type.len);
	offset += entity->type.len + 1;
	data.data[offset] = entity->zone.len;
	strncpy(&data.data[offset+1], entity->zone.str, entity->zone.len);

	/* Put the information in the temporary file */
	if (fwrite (&data.len, sizeof(data.len), 1, fd) != 1) {
		sprintf(gError,MSGSTR(M_ERR_WRITE,"Error writing %s"),dataFile);
		goto bad;
	}
	if (fwrite (data.data, data.len, 1, fd) != 1) {
		sprintf(gError,MSGSTR(M_ERR_WRITE,"Error writing %s"),dataFile);
		goto bad;
	}
	fclose(fd);
return (0);
bad:
        if (fd != NULL)
                (void) fclose(fd);
	fprintf(stderr, "%s: %s:\n", progname, gError);
	if (errno)
		perror("");
	return (-1);
}

static	int
nocase_strncmp(str1, str2, count)
char	*str1, *str2;
int	count;
{
	int	i;
	char	ch1,ch2;

	/* case insensitive strncmp */
	for (i=0; i<count; i++) {
		ch1 = (str1[i] >= 'a' && str1[i] <= 'z') ?
			(str1[i] + 'A' - 'a') : str1[i];
		ch2 = (str2[i] >= 'a' && str2[i] <= 'z') ?
			(str2[i] + 'A' - 'a') : str2[i];
		if (ch1 != ch2)
			return(-1);
		/* if both the strings are of same length, shorter than 'count',
		 * then they're same.
		 */
		if (ch1 == '\0' && ch2 == '\0')
			return(0);
	}

	return(0);
}

int
access_allowed()
/* tests users ability to access dir where printer selection is stored
   returns 0  if user is allowed
		   -1 if not
   appropriate messages are printed here if not allowed
*/
{

	sprintf(dataFile,"%s/%s",DATA_DIR,DATA_FILE);
	if (!access(DATA_DIR, F_OK)) {			/* if dir exists */
		if (!access(DATA_DIR, W_OK)) {		/* if we can write to it */
			if (!access(dataFile, W_OK)) 
				return(0);
			else {
				if (errno == ENOENT)		/* data file missing, OK */
					return(0);
				fprintf(stderr,MSGSTR(M_ERR_FILE_PERM,
					"Write permission to %s not allowed, re-run as root\n"),
					dataFile);
				return(-1);
			}
		}
		else {
			fprintf(stderr,MSGSTR(M_ERR_DIR_PERM,
				"You do not have write permission to %s\n"\
				"(printer selection is stored there)\n"),DATA_DIR);
			return(-1);
		}
	}
	else {
		if (getuid() == 0)
			return(0);
		else {
			fprintf(stderr,MSGSTR(M_ERR_DIR_MISSING,
				"Directory %s is missing, re-run as root to create it\n"),
				DATA_DIR);
			return(-1);
		}
	}
}



