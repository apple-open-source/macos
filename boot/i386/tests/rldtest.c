/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* test of standalone rld
 * usage: rldtest <sarld> <kernel> <driver>
 *
 */

#include <stdio.h>
#include <streams/streams.h>
#include <mach-o/loader.h>

extern int errno;

typedef unsigned int entry_t;
struct mach_header head;

#define zalloc(a) malloc(a)
#define zfree(a) free(a)
#define xread read

#define min(a,b) ((a) < (b) ? (a) : (b))

extern void *malloc(int size);
loadmacho(
    struct mach_header *head,
    int dev,
    int io,
    entry_t *rentry,
    char **raddr,
    int *rsize,
    int file_offset
)
{
	int ncmds;
	unsigned  cmds, cp;
	struct xxx_thread_command {
		unsigned long	cmd;
		unsigned long	cmdsize;
		unsigned long	flavor;
		unsigned long	count;
		i386_thread_state_t state;
	} *th;
	unsigned int	entry;
	int size, vmsize = 0;
	unsigned int vmaddr = ~0;

	// XXX should check cputype
	cmds = (unsigned int) zalloc(head->sizeofcmds);
	b_lseek(io, sizeof (struct mach_header) + file_offset, 0);

	if (read(io, cmds, head->sizeofcmds) != head->sizeofcmds)
{printf("error reading commands\n");
	    goto shread;}
    
	for (ncmds = head->ncmds, cp = cmds; ncmds > 0; ncmds--)
	{
		unsigned int	addr;

//		putchar('.');

#define lcp	((struct load_command *)cp)		
		switch (lcp->cmd)
		{
	    
		case LC_SEGMENT:
#define scp	((struct segment_command *)cp)

			addr = (scp->vmaddr & 0x3fffffff) + (int)*raddr;
			if (scp->filesize) {
			    // Is this an OK assumption?
			    // if the filesize is zero, it doesn't
			    // take up any virtual space...
			    // (Hopefully this only excludes PAGEZERO.)
			    // Also, ignore linkedit segment when
			    // computing size, because we will erase
			    // the linkedit segment later.
			    if(strncmp(scp->segname, SEG_LINKEDIT,
					    sizeof(scp->segname)) != 0)
				vmsize += scp->vmsize;
			    vmaddr = min(vmaddr, addr);
			    
			    // Zero any space at the end of the segment.
			    bzero((char *)(addr + scp->filesize),
				scp->vmsize - scp->filesize);
				
			    // FIXME:  check to see if we overflow
			    // the available space (should be passed in
			    // as the size argument).
			    
			    b_lseek(io, scp->fileoff + file_offset, 0);
			    if (xread(io, (char *)addr, scp->filesize)
							!= scp->filesize) {
					printf("Error loading section\n");
					printf("File size =%x; fileoff_set=%x; addr=%x\n",
						scp->filesize, scp->fileoff, addr);
					goto shread;
			    }
			}
			break;
		    
		case LC_THREAD:
		case LC_UNIXTHREAD:
			th = (struct xxx_thread_command *)cp;
			entry = th->state.eip;
			break;
		}
	    
		cp += lcp->cmdsize;
	}

//	kernBootStruct->rootdev = (dev & 0xffffff00) | devMajor[Dev(dev)];

	zfree((char *)cmds);
	*rentry = (entry_t)( (int) entry & 0x3fffffff );
	*rsize = vmsize;
	*raddr = (char *)vmaddr;
	return 0;

shread:
	zfree((char *)cmds);
	printf("Read error\n");
	return -1;
}

loadprog(
	int		dev,
	int		fd,
	entry_t		*entry,		// entry point
	char		**addr,		// load address
	int		*size		// size of loaded program
)
{
    /* get file header */
    read(fd, &head, sizeof(head));

    if (head.magic == MH_MAGIC) {
	return loadmacho((struct mach_header *) &head,
		dev, fd, entry, addr, size,0);
    }

    else if (head.magic == 0xbebafeca)
    {
	printf("no fat binary support yet\n");
	return -1;
    }

    printf("unrecognized binary format\n");
    return -1;
}

#define DRIVER "/usr/Devices/EtherExpress16.config/EtherExpress16_reloc"

void usage(void)
{
    fprintf(stderr,"usage: rldtest <sarld> <kernel> <driver>\n");
    exit(1);
}

main(int argc, char **argv)
{
    int fd;
    char *mem, *workmem, *ebuf;
    char *laddr, *kaddr, *daddr;
    NXStream *str;
    struct mach_header *mh;
    int ret, size, ksize, dsize, count;
    entry_t entry;
    int (*fn)();
    struct section *sp;
    char *kernel, *sarld, *driver;
    
    if (argc != 4)
	usage();
    sarld = argv[1];
    kernel = argv[2];
    driver = argv[3];
    mem = malloc(1024 * 1024 * 16);
    printf("mem = %x\n",mem);
    laddr = (char *)0x0;
    fd = open(sarld,0);
    if (fd < 0) {
	fprintf(stderr, "couldn't open sarld %s, error %d\n",sarld,errno);
	exit(1);
    }
    printf("fd = %d\n",fd);
    loadprog(0, fd, &entry, &laddr, &size);
    close(fd);
    printf("entry = %x, laddr = %x, size = %d\n",entry, laddr, size);
    fn = (int (*)())entry;
    fd = open(kernel,0);
    if (fd < 0) {
	fprintf(stderr, "couldn't open kernel %s, error %d\n",kernel,errno);
	exit(1);
    }
    kaddr = 0;
    loadprog(0, fd, &entry, &kaddr, &ksize);
    printf("entry = %x, kaddr = %x, ksize = %d\n",entry, kaddr, ksize);
    close(fd);
    daddr = (char *)0x70000;
    fd = open(driver,0);
    if (fd < 0) {
	fprintf(stderr, "couldn't open driver %s, error %d\n",driver,errno);
	exit(1);
    }
    size = 0;
    do {
	count = read(fd, daddr, 65536);
	daddr += count;
	size += count;
    } while (count > 0);
    daddr = (char *)0x70000;
    printf("entry = %x, daddr = %x, size = %d\n",entry, daddr, size);
    close(fd);
    workmem = malloc(300 * 1024);
    ebuf = malloc(64 * 1024);
    ebuf[0] = 0;
    dsize = 16 * 1024 * 1024 - (int)kaddr - (int)ksize;
    printf("about to call %x\n",fn);
    ret = (*fn)( "mach_kernel", kaddr,
	"driver", daddr, size,
	kaddr + ksize, &dsize,
	ebuf, 64 * 1024,
	workmem, 300 * 1024
    );
    printf("rld return: %d '%s'\n",ret, ebuf);
    
#define SEG_OBJC "__OBJC"
    sp = getsectbyname ("__TEXT", "__text");
    printf("text section: %s\n",sp->sectname);
    sp = getsectbynamefromheader (kaddr+ksize, "__OBJC", "__module_info");
    printf("objc section: %s\n",sp->sectname);
    sp = getsectdatafromheader (kaddr+ksize,
		    SEG_OBJC, "__module_info", &size);
    printf("objc section: %s\n",sp->sectname);

    printf("getsectdata ret = %d\n",sp);
    free(mem);
}
