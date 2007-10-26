/* All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Apple nor the names of any contributors
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
 * Portions Copyright 2006, Apple Computer, Inc.
 * Christopher Ryan <ryanc@apple.com>
*/

#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include "config.h"
#ifndef HAVE_ASPRINTF
#include "asprintf.h"
#endif
#include "macho.h"
#include "util.h"
#include "data.h"
#include "xar.h"

#define BIT64 0x01000000
#define PPC   0x00000012
#define I386  0x00000007

struct arches {
	int32_t size;
	int32_t offset;
};

struct _macho_context{
	struct arches *inflight;
	int32_t numarches;
	int32_t curroffset;
};

#define MACHO_CONTEXT(x) ((struct _macho_context *)(*x))

static int32_t parse_arch(xar_file_t f, struct mach_header *mh);


int32_t xar_macho_in(xar_t x, xar_file_t f, const char *attr, void **in, size_t *inlen, void **context) {
	struct mach_header *mh = *in;
	struct fat_header *fh = *in;
	uint32_t magic;
	int i;

	if( strcmp(attr, "data") != 0 )
		return 0;

	if(!MACHO_CONTEXT(context))
		*context = calloc(1,sizeof(struct _macho_context));
	else
		return 0; /*We only run for the first part of the data stream*/
	
	/* First, check for fat */
	magic = htonl(fh->magic);
	if( magic == 0xcafebabe ) {
		struct fat_arch *fa = (struct fat_arch *)((unsigned char *)*in + sizeof(struct fat_header));
		MACHO_CONTEXT(context)->numarches = htonl(fh->nfat_arch);

		/* sanity check, arbitrary number */
		if( MACHO_CONTEXT(context)->numarches > 7 )
			return 0;

		xar_prop_set(f, "contents/type", "Mach-O Fat File");

		MACHO_CONTEXT(context)->inflight = malloc( MACHO_CONTEXT(context)->numarches * sizeof(struct arches) );
		if( !MACHO_CONTEXT(context)->inflight ){
			free(*context);
			return -1;
		}
		
		for( i = 0; i < MACHO_CONTEXT(context)->numarches; ++i ) {
			int32_t sz = htonl(fa[i].size);
			int32_t off = htonl(fa[i].offset);

			MACHO_CONTEXT(context)->inflight[i].size = sz;
			MACHO_CONTEXT(context)->inflight[i].offset = off;
		}
		MACHO_CONTEXT(context)->curroffset += *inlen;
		return 0;
	}

	if( MACHO_CONTEXT(context)->inflight ) {
		for(i = 0; i < MACHO_CONTEXT(context)->numarches; ++i) {
			if( (MACHO_CONTEXT(context)->inflight[i].offset >= MACHO_CONTEXT(context)->curroffset) &&
				(MACHO_CONTEXT(context)->inflight[i].offset < (MACHO_CONTEXT(context)->curroffset+*inlen)) ) {

				mh = (struct mach_header *)((char *)*in + 
											(MACHO_CONTEXT(context)->inflight[i].offset - MACHO_CONTEXT(context)->curroffset));
				parse_arch(f, mh);
			}
		}
		MACHO_CONTEXT(context)->curroffset += *inlen;
		return 0;
	}

	parse_arch(f, mh);

	MACHO_CONTEXT(context)->curroffset += *inlen;

	return 0;
}

int32_t xar_macho_done(xar_t x, xar_file_t f, const char *attr, void **context) {

	if( MACHO_CONTEXT(context) ){
		if( MACHO_CONTEXT(context)->inflight )
			free(MACHO_CONTEXT(context)->inflight);
		free(*context);
	}
	
	return 0;
}

static int32_t parse_arch(xar_file_t f, struct mach_header *mh) {
	const char *cpustr, *typestr;
	char *typestr2;
	struct lc *lc;
	int n, byteflip = 0;;
	int32_t magic, cpu, type, ncmds, sizeofcmds;
	int32_t count = 0;

	magic = mh->magic;
	cpu = mh->cputype;
	type = mh->filetype;
	ncmds = mh->ncmds;
	sizeofcmds = mh->sizeofcmds;
	if( (magic == 0xcefaedfe) || (magic == 0xcffaedfe) ) {
		magic = xar_swap32(magic);
		cpu = xar_swap32(cpu);
		type = xar_swap32(type);
		ncmds = xar_swap32(ncmds);
		sizeofcmds = xar_swap32(sizeofcmds);
		byteflip = 1;
	}
	if( (magic != 0xfeedface) && (magic != 0xfeedfacf) ) {
		return 1;
	}
	lc = (struct lc *)((unsigned char *)mh + sizeof(struct mach_header));
	if( magic == 0xfeedfacf ) {
		lc = (struct lc *)((unsigned char *)lc + 4);
	}
	switch(cpu) {
	case PPC: cpustr = "ppc"; break;
	case I386: cpustr = "i386"; break;
	case PPC|BIT64: cpustr = "ppc64"; break;
	default: cpustr = "unknown"; break;
	};

	switch(type) {
	case 0x01: typestr = "Mach-O Object"; break;
	case 0x02: typestr = "Mach-O Executable"; break;
	case 0x03: typestr = "Mach-O Fixed VM Library"; break;
	case 0x04: typestr = "Mach-O core"; break;
	case 0x05: typestr = "Mach-O Preloaded Executable"; break;
	case 0x06: typestr = "Mach-O Dylib"; break;
	case 0x07: typestr = "Mach-O Dylinker"; break;
	case 0x08: typestr = "Mach-O Bundle"; break;
	case 0x09: typestr = "Mach-O Stub"; break;
	default: typestr = "Unknown"; break;
	};

	if( xar_prop_get(f, "contents/type", (const char **)&typestr2) ) {
		xar_prop_set(f, "contents/type", typestr);
	}
	asprintf(&typestr2, "contents/%s/type", cpustr);
	xar_prop_set(f, typestr2, typestr);
	free(typestr2);

	int32_t prevsz = 0;
	for(n = 0; n < ncmds; ++n) {
		int32_t cmd, cmdsize, stroff = 0;
		char *tmpstr = NULL;
		char *propstr = NULL;
		cmd = lc->cmd;
		cmdsize = lc->cmdsize;
		if( byteflip ) {
			cmd = xar_swap32(cmd);
			cmdsize = xar_swap32(cmdsize);
		}
		switch(cmd) {
		case 0xc:
		case 0xd:
			stroff = *(int32_t *)((unsigned char *)lc+8);
			if(byteflip)
				stroff = xar_swap32(stroff);
			tmpstr = (char *)((unsigned char *)lc+stroff);
			asprintf(&propstr, "contents/%s/library",cpustr);
			xar_prop_create(f, propstr, tmpstr);
			free(propstr);
			break;
		};
		lc = (struct lc *)((unsigned char *)lc + cmdsize);
		count += cmdsize;
		prevsz = cmdsize;

		/* saftey measure */
		if( count > sizeofcmds )
			break;
		if( count < 0 )
			break;
	}

	return 0;
}
