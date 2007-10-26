/*
 * Copyright (c) 2004 Rob Braun
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Rob Braun nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * 24-Apr-2005
 * DRI: Rob Braun <bbraun@synack.net>
 */
/*
 * Portions Copyright 2006, Apple Computer, Inc.
 * Christopher Ryan <ryanc@apple.com>
*/

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "xar.h"
#include "arcmod.h"
#include "b64.h"
#include <errno.h>
#include <string.h>
#include "util.h"
#include "linuxattr.h"
#include "io.h"
#include "appledouble.h"
#include "stat.h"

#if defined(HAVE_SYS_XATTR_H)
#include <sys/xattr.h>
#endif

struct _darwinattr_context{
	int fd;
	char *finfo;
	char *buf;
	int   len;
	int   off;
};

#define DARWINATTR_CONTEXT(x) ((struct _darwinattr_context *)(x))
#if defined(__APPLE__)
#ifdef HAVE_GETATTRLIST
#include <sys/attr.h>
#include <sys/vnode.h>
struct fi {
    uint32_t     length;
    fsobj_type_t objtype;
    char finderinfo[32];
};

/* finfo_read
 * This is for archiving the finderinfo via the getattrlist method.
 * This function is used from the nonea_archive() function.
 */
static int32_t finfo_read(xar_t x, xar_file_t f, void *buf, size_t len, void *context) {
	if( len < 32 )
		return -1;

	if( DARWINATTR_CONTEXT(context)->finfo == NULL )
		return 0;

	memcpy(buf, DARWINATTR_CONTEXT(context)->finfo, 32);
	DARWINATTR_CONTEXT(context)->finfo = NULL;
	return 32;
}

/* finfo_write
 * This is for extracting the finderinfo via the setattrlist method.
 * This function is used from the nonea_extract() function.
 */
static int32_t finfo_write(xar_t x, xar_file_t f, void *buf, size_t len, void *context) {
	struct attrlist attrs;
	struct fi finfo;

	if( len < 32 )
		return -1;
	if( DARWINATTR_CONTEXT(context)->finfo == NULL )
		return 0;

	memset(&attrs, 0, sizeof(attrs));
	attrs.bitmapcount = ATTR_BIT_MAP_COUNT;
	attrs.commonattr = ATTR_CMN_OBJTYPE | ATTR_CMN_FNDRINFO;

	getattrlist(DARWINATTR_CONTEXT(context)->finfo, &attrs, &finfo, sizeof(finfo), 0);

	attrs.commonattr = ATTR_CMN_FNDRINFO;
	if( setattrlist(DARWINATTR_CONTEXT(context)->finfo, &attrs, buf, 32, 0) != 0 )
		return -1;

	DARWINATTR_CONTEXT(context)->finfo = NULL;
	return 32;
}
#endif /* HAVE_GETATTRLIST */

/* xar_rsrc_read
 * This is the read callback function for archiving the resource fork via
 * the ..namedfork method.  This callback is used from nonea_archive()
 */
static int32_t xar_rsrc_read(xar_t x, xar_file_t f, void *inbuf, size_t bsize, void *context) {
	int32_t r;

	while(1) {
		r = read(DARWINATTR_CONTEXT(context)->fd, inbuf, bsize);
		if( (r < 0) && (errno == EINTR) )
			continue;
		return r;
	}
}
#endif /* __APPLE__ */

/* xar_rsrc_write
 * This is the write callback function for writing the resource fork
 * back to the file via ..namedfork method.  This is the callback used
 * in nonea_extract() and underbar_extract().
 */
static int32_t xar_rsrc_write(xar_t x, xar_file_t f, void *buf, size_t len, void *context) {
	int32_t r;
	size_t off = 0;
	do {
		r = write(DARWINATTR_CONTEXT(context)->fd, buf+off, len-off);
		if( (r < 0) && (errno != EINTR) )
			return r;
		off += r;
	} while( off < len );
	return off;
}

#ifdef __APPLE__
#if defined(HAVE_GETXATTR)

static int32_t xar_ea_read(xar_t x, xar_file_t f, void *buf, size_t len, void *context) {
	if( DARWINATTR_CONTEXT(context)->buf == NULL )
		return 0;

	if( ((DARWINATTR_CONTEXT(context)->len)-(DARWINATTR_CONTEXT(context)->off)) <= len ) {
		int siz = (DARWINATTR_CONTEXT(context)->len)-(DARWINATTR_CONTEXT(context)->off);
		memcpy(buf, DARWINATTR_CONTEXT(context)->buf+DARWINATTR_CONTEXT(context)->off, siz);
		free(DARWINATTR_CONTEXT(context)->buf);
		DARWINATTR_CONTEXT(context)->buf = NULL;
		DARWINATTR_CONTEXT(context)->off = 0;
		DARWINATTR_CONTEXT(context)->len = 0;
		return siz;
	}

	memcpy(buf, DARWINATTR_CONTEXT(context)->buf+DARWINATTR_CONTEXT(context)->off, len);
	DARWINATTR_CONTEXT(context)->off += len;

	if( DARWINATTR_CONTEXT(context)->off == DARWINATTR_CONTEXT(context)->len ) {
		free(DARWINATTR_CONTEXT(context)->buf);
		DARWINATTR_CONTEXT(context)->buf = NULL;
		DARWINATTR_CONTEXT(context)->off = 0;
		DARWINATTR_CONTEXT(context)->len = 0;
	}

	return len;
}

static int32_t xar_ea_write(xar_t x, xar_file_t f, void *buf, size_t len, void *context) {
	if( DARWINATTR_CONTEXT(context)->buf == NULL )
		return 0;

	if( DARWINATTR_CONTEXT(context)->off == DARWINATTR_CONTEXT(context)->len )
		return 0;

	if( ((DARWINATTR_CONTEXT(context)->len)-(DARWINATTR_CONTEXT(context)->off)) <= len ) {
		int siz = (DARWINATTR_CONTEXT(context)->len)-(DARWINATTR_CONTEXT(context)->off);
		memcpy((DARWINATTR_CONTEXT(context)->buf)+(DARWINATTR_CONTEXT(context)->off), buf, siz);
		return siz;
	}

	memcpy((DARWINATTR_CONTEXT(context)->buf)+(DARWINATTR_CONTEXT(context)->off), buf, len);
	DARWINATTR_CONTEXT(context)->off += len;

	return len;
}

static int32_t ea_archive(xar_t x, xar_file_t f, const char* file, void *context) {
	char *buf, *i;
	int ret, bufsz;
	int32_t retval = 0;

	ret = listxattr(file, NULL, 0, XATTR_NOFOLLOW);
	if( ret < 0 )
		return -1;
	if( ret == 0 )
		return 0;
	bufsz = ret;

TRYAGAIN:
	buf = malloc(bufsz);
	if( !buf )
		goto TRYAGAIN;
	ret = listxattr(file, buf, bufsz, XATTR_NOFOLLOW);
	if( ret < 0 ) {
		switch(errno) {
		case ERANGE: bufsz = bufsz*2; free(buf); goto TRYAGAIN;
		case ENOTSUP: retval = 0; goto BAIL;
		default: retval = -1; goto BAIL;
		};
	}
	if( ret == 0 ) {
		retval = 0;
		goto BAIL;
	}

	for( i = buf; (i-buf) < ret; i += strlen(i)+1 ) {
		char tempnam[1024];
		ret = getxattr(file, i, NULL, 0, 0, XATTR_NOFOLLOW);
		if( ret < 0 )
			continue;
		DARWINATTR_CONTEXT(context)->len = ret;
		DARWINATTR_CONTEXT(context)->buf = malloc(DARWINATTR_CONTEXT(context)->len);
		if( !DARWINATTR_CONTEXT(context)->buf )
			goto BAIL;

		ret = getxattr(file, i, DARWINATTR_CONTEXT(context)->buf, DARWINATTR_CONTEXT(context)->len, 0, XATTR_NOFOLLOW);
		if( ret < 0 ) {
			free(DARWINATTR_CONTEXT(context)->buf);
			DARWINATTR_CONTEXT(context)->buf = NULL;
			DARWINATTR_CONTEXT(context)->len = 0;
			continue;
		}

		memset(tempnam, 0, sizeof(tempnam));
		snprintf(tempnam, sizeof(tempnam)-1, "ea/%s", i);
		xar_attrcopy_to_heap(x, f, tempnam, xar_ea_read, context);
	}
BAIL:
	free(buf);
	return retval;
}

static int32_t ea_extract(xar_t x, xar_file_t f, const char* file, void *context) {
	const char *prop;
	xar_iter_t iter;
	
	iter = xar_iter_new();
	for(prop = xar_prop_first(f, iter); prop; prop = xar_prop_next(iter)) {
		const char *opt;
		char sz[1024];
		int len;

		if( strncmp(prop, XAR_EA_FORK, strlen(XAR_EA_FORK)) )
			continue;
		if( strlen(prop) <= strlen(XAR_EA_FORK) )
			continue;

		memset(sz, 0, sizeof(sz));
		snprintf(sz, sizeof(sz)-1, "%s/size", prop);
		xar_prop_get(f, sz, &opt);
		if( !opt )
			continue;

		len = strtol(opt, NULL, 10);
		DARWINATTR_CONTEXT(context)->buf = malloc(len);
		if( !DARWINATTR_CONTEXT(context)->buf )
			return -1;
		DARWINATTR_CONTEXT(context)->len = len;

		xar_attrcopy_from_heap(x, f, prop, xar_ea_write, context);

		setxattr(file, prop+strlen(XAR_EA_FORK)+1, DARWINATTR_CONTEXT(context)->buf, DARWINATTR_CONTEXT(context)->len, 0, XATTR_NOFOLLOW);
		free(DARWINATTR_CONTEXT(context)->buf);
		DARWINATTR_CONTEXT(context)->buf = NULL;
		DARWINATTR_CONTEXT(context)->len = 0;
		DARWINATTR_CONTEXT(context)->off = 0;
	}

	return 0;
}
#endif /* HAVE_GETXATTR */

/* nonea_archive
 * Archive the finderinfo and resource fork through getattrlist and
 * ..namedfork methods rather than via EAs.  This is mainly for 10.3
 * and earlier support
 */
static int32_t nonea_archive(xar_t x, xar_file_t f, const char* file, void *context) {
	char rsrcname[4096];
	struct stat sb;
#ifdef HAVE_GETATTRLIST
	struct attrlist attrs;
	struct fi finfo;
	int ret;
	char z[32];
	
	memset(&attrs, 0, sizeof(attrs));
	attrs.bitmapcount = ATTR_BIT_MAP_COUNT;
	attrs.commonattr = ATTR_CMN_OBJTYPE | ATTR_CMN_FNDRINFO;

	ret = getattrlist(file, &attrs, &finfo, sizeof(finfo), 0);
	if( ret != 0 )
		return -1;

	memset(z, 0, sizeof(z));
	if( memcmp(finfo.finderinfo, z, sizeof(finfo.finderinfo)) != 0 ) {
		DARWINATTR_CONTEXT(context)->finfo = finfo.finderinfo;
		xar_attrcopy_to_heap(x, f, "ea/com.apple.FinderInfo", finfo_read, context);
	}
#endif /* HAVE_GETATTRLIST */


	memset(rsrcname, 0, sizeof(rsrcname));
	snprintf(rsrcname, sizeof(rsrcname)-1, "%s/..namedfork/rsrc", file);
	if( lstat(rsrcname, &sb) != 0 )
		return 0;

	if( sb.st_size == 0 )
		return 0;

	DARWINATTR_CONTEXT(context)->fd = open(rsrcname, O_RDONLY, 0);
	if( DARWINATTR_CONTEXT(context)->fd < 0 )
		return -1;

	xar_attrcopy_to_heap(x, f, "ea/com.apple.ResourceFork", xar_rsrc_read, context);
	close(DARWINATTR_CONTEXT(context)->fd);
	return 0;
}

/* nonea_extract
 * Extract the finderinfo and resource fork through setattrlist and
 * ..namedfork methods rather than via EAs.  This is mainly for 10.3
 * and earlier support
 */
static int32_t nonea_extract(xar_t x, xar_file_t f, const char* file, void *context) {
	char rsrcname[4096];
#ifdef HAVE_SETATTRLIST
	struct attrlist attrs;
	struct fi finfo;
	int ret;
	
	memset(&attrs, 0, sizeof(attrs));
	attrs.bitmapcount = ATTR_BIT_MAP_COUNT;
	attrs.commonattr = ATTR_CMN_OBJTYPE | ATTR_CMN_FNDRINFO;

	ret = getattrlist(file, &attrs, &finfo, sizeof(finfo), 0);
	if( ret != 0 )
		return -1;

	DARWINATTR_CONTEXT(context)->finfo = (char *)file;

	xar_attrcopy_from_heap(x, f, "ea/com.apple.FinderInfo", finfo_write, context);
#endif /* HAVE_SETATTRLIST */
	
	memset(rsrcname, 0, sizeof(rsrcname));
	snprintf(rsrcname, sizeof(rsrcname)-1, "%s/..namedfork/rsrc", file);
	DARWINATTR_CONTEXT(context)->fd = open(rsrcname, O_RDWR|O_TRUNC);
	if( DARWINATTR_CONTEXT(context)->fd < 0 )
		return 0;

	xar_attrcopy_from_heap(x, f, "ea/com.apple.ResourceFork", xar_rsrc_write, context);
	close(DARWINATTR_CONTEXT(context)->fd);
	return 0;
}
#endif /* __APPLE__ */

/* xar_underbar_check
 * Check to see if the file we're archiving is a ._ file.  If so,
 * stop the archival process.
 */
int32_t xar_underbar_check(xar_t x, xar_file_t f, const char* file, void *context) {
	char *bname, *tmp;

	tmp = strdup(file);
	bname = basename(tmp);

	if(bname && (bname[0] == '.') && (bname[1] == '_')) {
		char *nonunderbar, *nupath, *tmp2, *dname;
		struct stat sb;
		
		nonunderbar = bname+2;
		tmp2 = strdup(file);
		dname = dirname(tmp2);
		asprintf(&nupath, "%s/%s", dname, nonunderbar);
		free(tmp2);

		/* if there is no file that the ._ corresponds to, archive
		 * it like a normal file.
		 */
		if( stat(nupath, &sb) ) {
			free(tmp);
			free(nupath);
			return 0;
		}

		asprintf(&tmp2, "%s/..namedfork/rsrc", nupath);
		free(nupath);

		/* If there is a file that the ._ file corresponds to, and
		 * there is no resource fork, assume the ._ file contains
		 * the file's resource fork, and skip it (to be picked up
		 * when the file is archived.
		 */
		if( stat(tmp2, &sb) ) {
			free(tmp2);
			free(tmp);
			return 1;
		}

		/* otherwise, we have a corresponding file and it supports
		 * resource forks, so we assume this is a detached ._ file
		 * and archive it as a real file.
		 */
		free(tmp2);
		free(tmp);
		return 0;
	}

	free(tmp);
	return 0;
}

#ifdef __APPLE__
/* This only really makes sense on OSX */
static int32_t underbar_archive(xar_t x, xar_file_t f, const char* file, void *context) {
	struct stat sb;
	char underbarname[4096], z[32];
	char *dname, *bname, *tmp, *tmp2;
	struct AppleSingleHeader ash;
	struct AppleSingleEntry  ase;
	int num_entries = 0, i, r;
	off_t off;

	if( !file )
		return 0;
	
	tmp = strdup(file);
	tmp2 = strdup(file);
	dname = dirname(tmp2);
	bname = basename(tmp);

	memset(underbarname, 0, sizeof(underbarname));
	snprintf(underbarname, sizeof(underbarname)-1, "%s/._%s", dname, bname);
	free(tmp);
	free(tmp2);
	
	if( stat(underbarname, &sb) != 0 )
		return 0;

	DARWINATTR_CONTEXT(context)->fd = open(underbarname, O_RDONLY);
	if( DARWINATTR_CONTEXT(context)->fd < 0 )
		return -1;

	memset(&ash, 0, sizeof(ash));
	memset(&ase, 0, sizeof(ase));
	r = read(DARWINATTR_CONTEXT(context)->fd, &ash, XAR_ASH_SIZE);
	if( r < XAR_ASH_SIZE )
		return -1;

	if( ntohl(ash.magic) != APPLEDOUBLE_MAGIC )
		return -1;
	if( ntohl(ash.version) != APPLEDOUBLE_VERSION )
		return -1;

	off = XAR_ASH_SIZE;
	num_entries = ntohs(ash.entries);

	for(i = 0; i < num_entries; i++) {
		off_t entoff;
		r = read(DARWINATTR_CONTEXT(context)->fd, &ase, sizeof(ase));
		if( r < sizeof(ase) )
			return -1;
		off+=r;

		if( ntohl(ase.entry_id) == AS_ID_FINDER ) {
			entoff = (off_t)ntohl(ase.offset);
			if( lseek(DARWINATTR_CONTEXT(context)->fd, entoff, SEEK_SET) == -1 )
				return -1;
			r = read(DARWINATTR_CONTEXT(context)->fd, z, sizeof(z));
			if( r < sizeof(z) )
				return -1;
			
			DARWINATTR_CONTEXT(context)->finfo = z;
			xar_attrcopy_to_heap(x, f, "ea/com.apple.FinderInfo", finfo_read, context);
			if( lseek(DARWINATTR_CONTEXT(context)->fd, (off_t)off, SEEK_SET) == -1 )
				return -1;
		}
		if( ntohl(ase.entry_id) == AS_ID_RESOURCE ) {
			entoff = (off_t)ntohl(ase.offset);
			if( lseek(DARWINATTR_CONTEXT(context)->fd, entoff, SEEK_SET) == -1 )
				return -1;

			xar_attrcopy_to_heap(x, f, "ea/com.apple.ResourceFork", xar_rsrc_read, context);

			if( lseek(DARWINATTR_CONTEXT(context)->fd, (off_t)off, SEEK_SET) == -1 )
				return -1;
		}
	}

	close(DARWINATTR_CONTEXT(context)->fd);
	DARWINATTR_CONTEXT(context)->fd = 0;
	return 0;
}
#endif

/* underbar_extract
 * Extract finderinfo and resource fork information to an appledouble
 * ._ file.
 */
static int32_t underbar_extract(xar_t x, xar_file_t f, const char* file, void *context) {
	char underbarname[4096];
	char *dname, *bname, *tmp, *tmp2;
	const char *rsrclenstr;
	struct AppleSingleHeader ash;
	struct AppleSingleEntry  ase;
	int num_entries = 0, rsrclen = 0, have_rsrc = 0, have_fi = 0;

	if( xar_prop_get(f, "ea/com.apple.FinderInfo", NULL) == 0 ) {
		have_fi = 1;
		num_entries++;
	}

	if( xar_prop_get(f, "ea/com.apple.ResourceFork", NULL) == 0 ) {
		have_rsrc = 1;
		num_entries++;
	}

	if( num_entries == 0 )
		return 0;

	tmp = strdup(file);
	tmp2 = strdup(file);
	dname = dirname(tmp2);
	bname = basename(tmp);

	memset(underbarname, 0, sizeof(underbarname));
	snprintf(underbarname, sizeof(underbarname)-1, "%s/._%s", dname, bname);
	free(tmp);
	free(tmp2);

	DARWINATTR_CONTEXT(context)->fd = open(underbarname, O_RDWR | O_CREAT | O_TRUNC, 0);
	if( DARWINATTR_CONTEXT(context)->fd < 0 )
		return -1;

	xar_prop_get(f, "ea/com.apple.ResourceFork/size", &rsrclenstr);
	if( rsrclenstr ) 
		rsrclen = strtol(rsrclenstr, NULL, 10);

	memset(&ash, 0, sizeof(ash));
	memset(&ase, 0, sizeof(ase));
	ash.magic = htonl(APPLEDOUBLE_MAGIC);
	ash.version = htonl(APPLEDOUBLE_VERSION);
	ash.entries = htons(num_entries);

	write(DARWINATTR_CONTEXT(context)->fd, &ash, XAR_ASH_SIZE);

	ase.offset = htonl(XAR_ASH_SIZE + ntohs(ash.entries)*12);
	if( have_fi ) {
		ase.entry_id = htonl(AS_ID_FINDER);
		ase.length = htonl(32);
		write(DARWINATTR_CONTEXT(context)->fd, &ase, 12);
	}

	if( have_rsrc ) {
		ase.entry_id = htonl(AS_ID_RESOURCE);
		ase.offset = htonl(ntohl(ase.offset) + ntohl(ase.length));
		ase.length = htonl(rsrclen);
		write(DARWINATTR_CONTEXT(context)->fd, &ase, 12);
	}
	
	if( have_fi )
		xar_attrcopy_from_heap(x, f, "ea/com.apple.FinderInfo", xar_rsrc_write, context);
	if( have_rsrc )
		xar_attrcopy_from_heap(x, f, "ea/com.apple.ResourceFork", xar_rsrc_write, context);
	close(DARWINATTR_CONTEXT(context)->fd);

	DARWINATTR_CONTEXT(context)->fd = 0;

	xar_set_perm(x, f, underbarname, NULL, 0 );
	
	return 0;
}


int32_t xar_darwinattr_archive(xar_t x, xar_file_t f, const char* file, const char *buffer, size_t len)
{
	struct _darwinattr_context context;
	
	memset(&context,0,sizeof(struct _darwinattr_context));
	
#if defined(__APPLE__)
	if( len )
		return 0;
/*
 * This is currently turned off at Apple because of problems with xml ea
 *
#if defined(HAVE_GETXATTR)
	if( ea_archive(x, f, file, (void *)&context) == 0 )
		return 0;
#endif
*/
	if( nonea_archive(x, f, file, (void *)&context) == 0 )
		return 0;
	return underbar_archive(x, f, file, (void *)&context);
#endif /* __APPLE__ */
	return 0;
}

int32_t xar_darwinattr_extract(xar_t x, xar_file_t f, const char* file, char *buffer, size_t len)
{
	struct _darwinattr_context context;
	
	memset(&context,0,sizeof(struct _darwinattr_context));
	
#if defined(__APPLE__)
	if( len )
		return 0;
/*
 * This is currently turned off at Apple because of problems with xml ea
 *
#if defined(HAVE_GETXATTR)
	if( ea_extract(x, f, file, (void *)&context) == 0 )
		return 0;
#endif
*/
	if( nonea_extract(x, f, file, (void *)&context) == 0 )
		return 0;
#endif /* __APPLE__ */
	return underbar_extract(x, f, file, (void *)&context);
}
