
/* 
 * Copyright 1999 Apple Computer, Inc.
 *
 * ufslabel.c
 * - library routines to read/write the UFS disk label
 */

/*
 * Modification History:
 * 
 * Dieter Siegmund (dieter@apple.com)	Fri Nov  5 12:48:55 PST 1999
 * - created
 */

#include "dkopen.h"
#include <sys/types.h>
#include <sys/wait.h>
#ifndef linux
#include <dev/disk.h>
#endif
#include <fcntl.h>
#include <sys/errno.h>


#include <stdio.h>
#include <unistd.h> 
#include <string.h> 
#include <stdlib.h> 
#include <sys/stat.h>
#include <sys/time.h> 
#include <sys/mount.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <sys/param.h>
#include <ufs/ffs/fs.h>

#include "ufslabel.h"

char ufs_label_magic[4] = UFS_LABEL_MAGIC;

typedef union {
    char	c[2];
    u_short	s;
} short_union_t;

typedef union {
    u_short 	s[2];
    long	l;
} long_union_t;

/*
 * Local Functions:
 */
static __inline__ void
reduce(int * sum)
{
    long_union_t l_util;

    l_util.l = *sum;
    *sum = l_util.s[0] + l_util.s[1];
    if (*sum > 65535)
	*sum -= 65535;
    return;
}

static unsigned short
in_cksum(void * data, int len)
{
	u_short * w;
	int sum = 0;

	w = (u_short *)data;
	while ((len -= 32) >= 0) {
	    sum += w[0]; sum += w[1]; 
	    sum += w[2]; sum += w[3];
	    sum += w[4]; sum += w[5]; 
	    sum += w[6]; sum += w[7];
	    sum += w[8]; sum += w[9]; 
	    sum += w[10]; sum += w[11];
	    sum += w[12]; sum += w[13]; 
	    sum += w[14]; sum += w[15];
	    w += 16;
	}
	len += 32;
	while ((len -= 8) >= 0) {
	    sum += w[0]; sum += w[1]; 
	    sum += w[2]; sum += w[3];
	    w += 4;
	}
	len += 8;
	if (len) {
	    reduce(&sum);
	    while ((len -= 2) >= 0) {
		sum += *w++;
	    }
	}
	if (len == -1) { /* odd-length data */
	    short_union_t s_util;

	    s_util.s = 0;
	    s_util.c[0] = *((char *)w);
	    s_util.c[1] = 0;
	    sum += s_util.s;
	}
	reduce(&sum);
	return (~sum & 0xffff);
}

static boolean_t
ufslabel_check(struct ufslabel * ul_p)
{
    if (bcmp(&ul_p->ul_magic, ufs_label_magic, 
	     sizeof(ul_p->ul_magic))) {
#ifdef DEBUG
	fprintf(stderr, "check_label: label has bad magic number\n");
#endif DEBUG
	return (FALSE);
    }
    if (ntohl(ul_p->ul_version) != UFS_LABEL_VERSION) {
#ifdef DEBUG
	fprintf(stderr, 
		"check_label: label has incorect version %d (should be %d)\n",
		ntohl(ul_p->ul_version), UFS_LABEL_VERSION);
#endif DEBUG
	return (FALSE);
    }
    if (ntohs(ul_p->ul_namelen) > UFS_MAX_LABEL_NAME) {
#ifdef DEBUG
	fprintf(stderr, "check_label: name length %d is too big (> %d)\n",
		ntohs(ul_p->ul_namelen), UFS_MAX_LABEL_NAME);
#endif DEBUG
	return (FALSE);
    }
    {
	u_int16_t	calc;	
	u_int16_t 	checksum = ul_p->ul_checksum;
	
	ul_p->ul_checksum = 0;
	calc = in_cksum(ul_p, sizeof(*ul_p));
	if (calc != checksum) {
#ifdef DEBUG
	    fprintf(stderr, "check_label: label checksum %x (should be %x)\n",
		    checksum, calc);
#endif DEBUG
	    return (FALSE);
	}
    }
    return (TRUE);
}

static void *
ufslabel_read_blocks(int fd, off_t * offset_p, ssize_t * size_p, 
		     struct ufslabel * * label_p)
{
    int			blocksize;
    off_t 		offset;
    ssize_t 		size;
    void *		ptr = NULL;
    struct ufslabel *	ul_p;

    *label_p = NULL;

#ifndef linux
    if (ioctl(fd, DKIOCBLKSIZE, &blocksize) < 0) {
	fprintf(stderr, "DKIOCBLKSIZE failed, %s\n", strerror(errno));
	return (NULL);
    }
#else
    blocksize = DEV_BSIZE;
#endif

    offset = UFS_LABEL_OFFSET / blocksize * blocksize;
    size = (UFS_LABEL_SIZE + blocksize - 1) / blocksize * blocksize;

    if (dklseek(fd, offset, SEEK_SET) != offset) {
	fprintf(stderr, "ufslabel_read_blocks: lseek failed, %s\n",
		strerror(errno));
	return (NULL);
    }

    ptr = malloc(size);
    if (ptr == NULL) {
	fprintf(stderr, "ufslabel_read_blocks: malloc() failed\n");
	return (NULL);
    }
    ul_p = (struct ufslabel *)(ptr + (UFS_LABEL_OFFSET - offset));
    
    if (read(fd, ptr, size) != size) {
	fprintf(stderr, "ufslabel_read_blocks: read failed, %s\n",
		strerror(errno));
	goto fail;
    }
    if (label_p)
	*label_p = ul_p;
    if (offset_p)
	*offset_p = offset;
    if (size_p)
	*size_p = size;
    return (ptr);
 fail:
    if (ptr)
	free(ptr);
    return (NULL);
}

static boolean_t
ufslabel_read(int fd, struct ufslabel * label_p)
{
    struct ufslabel * 	ul_p;
    void * 		ptr;
    boolean_t		ret = TRUE;

    ptr = ufslabel_read_blocks(fd, NULL, NULL, &ul_p);
    if (ptr == NULL)
	return (FALSE);

    if (ufslabel_check(ul_p) == FALSE) {
	ret = FALSE;
	goto fail;
    }
    *label_p = *ul_p;

 fail:
    free(ptr);
    return (ret);
}

static boolean_t
ufslabel_write(int fd, struct ufslabel * label_p)
{
    off_t 		offset;
    ssize_t		size;
    void *		ptr = NULL;
    struct ufslabel *	ul_p;

    /* get blocks that will contain the new label */
    ptr = ufslabel_read_blocks(fd, &offset, &size, &ul_p);
    if (ptr == NULL)
	return (FALSE);

    /* copy the label into the raw blocks */
    *ul_p = *label_p;

    /* make sure the checksum is updated */
    ul_p->ul_checksum = 0;
    ul_p->ul_checksum = in_cksum(ul_p, sizeof(*ul_p));

    /* write the new label */
    if (dklseek(fd, offset, SEEK_SET) != offset) {
	fprintf(stderr, "ufslabel_write: lseek failed, %s\n",
		strerror(errno));
	goto fail;
    }
    if (write(fd, ptr, size) != (ssize_t)size) {
	fprintf(stderr, "ufslabel_write: write failed, %s\n",
		strerror(errno));
	goto fail;
    }
    if (ptr)
	free(ptr);
    return (TRUE);

 fail:
    if (ptr)
	free(ptr);
    return (FALSE);
}

static void
ufslabel_get_name(struct ufslabel * ul_p, char * name, int * len)
{
    if (ntohs(ul_p->ul_namelen) < *len)
	*len = ntohs(ul_p->ul_namelen);
    bcopy(ul_p->ul_name, name, *len);
}

static boolean_t
ufslabel_set_name(struct ufslabel * ul_p, char * name, int len)
{
    if (len > UFS_MAX_LABEL_NAME) {
	fprintf(stderr, "ufslabel_set_name: name length %d too long (>%d)\n", 
		len, UFS_MAX_LABEL_NAME);
	return (FALSE);
    }
    ul_p->ul_namelen = htons(len);
    bcopy(name, ul_p->ul_name, len);
    return (TRUE);
}

static void
ufslabel_init(struct ufslabel * ul_p)
{
    struct timeval tv;

    bzero(ul_p, sizeof(*ul_p));
    ul_p->ul_version = htonl(UFS_LABEL_VERSION);
    bcopy(ufs_label_magic, &ul_p->ul_magic, sizeof(ul_p->ul_magic));
    gettimeofday(&tv, 0);
    ul_p->ul_time = htonl(tv.tv_sec);
}


/*
 * Exported Functions:
 */
boolean_t
ufslabel_get(int fd, void * name, int * len)
{
    struct ufslabel	label;

    if (ufslabel_read(fd, &label) == FALSE)
	return (FALSE);
    ufslabel_get_name(&label, name, len);
    return (TRUE);
}


boolean_t
ufslabel_set(int fd, void * name, int len)
{
    struct ufslabel 	ul;

    if (ufslabel_read(fd, &ul) == FALSE)
	ufslabel_init(&ul);

    if (ufslabel_set_name(&ul, name, len) == FALSE)
	return (FALSE);

    if (ufslabel_write(fd, &ul) == FALSE)
	return (FALSE);
    return (TRUE);
}
