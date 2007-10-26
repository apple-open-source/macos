/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
#include <CoreFoundation/CoreFoundation.h>

#include <libc.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>
#include <mach/mach_port.h>
#include <mach-o/kld.h>
#include <mach-o/arch.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/swap.h>

#include <IOKit/kext/KXKextManager.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/IOCFSerialize.h>

__private_extern__ KXKextManagerError
readFile(const char *path, vm_offset_t * objAddr, vm_size_t * objSize);
__private_extern__ KXKextManagerError
writeFile(int fd, const void * data, size_t length);


__private_extern__ KXKextManagerError
writeFile(int fd, const void * data, size_t length)
{
    KXKextManagerError err;

    if (length != (size_t)write(fd, data, length))
        err = kKXKextManagerErrorDiskFull;
    else
        err = kKXKextManagerErrorNone;

    if (kKXKextManagerErrorNone != err)
        perror("couldn't write output");

    return( err );
}

__private_extern__ KXKextManagerError
readFile(const char *path, vm_offset_t * objAddr, vm_size_t * objSize)
{
    KXKextManagerError err = kKXKextManagerErrorFileAccess;
    int fd;
    struct stat stat_buf;

    *objAddr = 0;
    *objSize = 0;

    do
    {
        if((fd = open(path, O_RDONLY)) == -1)
	    continue;

	if(fstat(fd, &stat_buf) == -1)
	    continue;

        if (0 == (stat_buf.st_mode & S_IFREG)) 
            continue;

       /* Don't try to map an empty file, it fails now due to conformance
        * stuff (PR 4611502).
        */
        if (0 == stat_buf.st_size) {
            err = kKXKextManagerErrorNone;
            continue;
        }

	*objSize = stat_buf.st_size;

        *objAddr = (vm_offset_t)mmap(NULL /* address */, *objSize,
            PROT_READ|PROT_WRITE, MAP_FILE|MAP_PRIVATE /* flags */,
            fd, 0 /* offset */);

	if ((void *)*objAddr == MAP_FAILED) {
            *objAddr = 0;
            *objSize = 0;
	    continue;
	}

	err = kKXKextManagerErrorNone;

    } while( false );

    if (-1 != fd)
    {
        close(fd);
    }
    if (kKXKextManagerErrorNone != err)
    {
        fprintf(stderr, "couldn't read %s: %s\n", path, strerror(errno));
    }

    return( err );
}

struct symbol {
    char * name;
    unsigned int name_len;
    char * indirect;
    unsigned int indirect_len;
};

static bool issymchar( char c )
{
    return ((c > ' ') && (c <= '~') && (c != ':'));
}

static bool iswhitespace( char c )
{
    return ((c == ' ') || (c == '\t'));
}

/*
 * Function for qsort for comparing symbol list names.
 */
static int
qsort_cmp(const void * _left, const void * _right)
{
    struct symbol * left  = (struct symbol *) _left;
    struct symbol * right = (struct symbol *) _right;

    return (strcmp(left->name, right->name));
}

/*
 * Function for bsearch for finding a symbol name.
 */
static int
bsearch_cmp( const void * _key, const void * _cmp)
{
    char * key = (char *)_key;
    struct symbol * cmp = (struct symbol *) _cmp;

    return(strcmp(key, cmp->name));
}

static uint32_t
count_symbols(char * file, vm_size_t file_size)
{
    uint32_t nsyms = 0;
    char *   scan;
    char *   eol;
    char *   next;

    for (scan = file; true; scan = next) {

        eol = memchr(scan, '\n', file_size - (scan - file));
        if (eol == NULL) {
            break;
        }
        next = eol + 1;

       /* Skip empty lines.
        */
        if (eol == scan) {
            continue;
        }

       /* Skip comment lines.
        */
        if (scan[0] == '#') {
            continue;
        }

       /* Scan past any non-symbol characters at the beginning of the line. */
        while ((scan < eol) && !issymchar(*scan)) {
            scan++;
        }

       /* No symbol on line? Move along.
        */
        if (scan == eol) {
            continue;
        }

       /* Skip symbols starting with '.'.
        */
        if (scan[0] == '.') {
            continue;
        }
        nsyms++;
    }
    
    return nsyms;
}

static uint32_t
store_symbols(char * file, vm_size_t file_size, struct symbol * symbols, uint32_t idx, uint32_t max_symbols)
{
    char *   scan;
    char *   line;
    char *   eol;
    char *   next;

    uint32_t strtabsize;

    strtabsize = 0;

    for (scan = file, line = file; true; scan = next, line = next) {

        char *       name = NULL;
        char *       name_term = NULL;
        unsigned int name_len = 0;
        char *       indirect = NULL;
        char *       indirect_term = NULL;
        unsigned int indirect_len = 0;

        eol = memchr(scan, '\n', file_size - (scan - file));
        if (eol == NULL) {
            break;
        }
        next = eol + 1;

       /* Skip empty lines.
        */
        if (eol == scan) {
            continue;
        }

        *eol = '\0';

       /* Skip comment lines.
        */
        if (scan[0] == '#') {
            continue;
        }

       /* Scan past any non-symbol characters at the beginning of the line. */
        while ((scan < eol) && !issymchar(*scan)) {
            scan++;
        }

       /* No symbol on line? Move along.
        */
        if (scan == eol) {
            continue;
        }

       /* Skip symbols starting with '.'.
        */
        if (scan[0] == '.') {
            continue;
        }

        name = scan;

       /* Find the end of the symbol.
        */
        while ((*scan != '\0') && issymchar(*scan)) {
            scan++;
        }

       /* Note char past end of symbol.
        */
        name_term = scan;

       /* Stored length must include the terminating nul char.
        */
        name_len = name_term - name + 1;

       /* Now look for an indirect.
        */
        if (*scan != '\0') {
            while ((*scan != '\0') && iswhitespace(*scan)) {
                scan++;
            }
            if (*scan == ':') {
                scan++;
                while ((*scan != '\0') && iswhitespace(*scan)) {
                    scan++;
                }
                if (issymchar(*scan)) {
                    indirect = scan;

                   /* Find the end of the symbol.
                    */
                    while ((*scan != '\0') && issymchar(*scan)) {
                        scan++;
                    }

                   /* Note char past end of symbol.
                    */
                    indirect_term = scan;

                   /* Stored length must include the terminating nul char.
                    */
                    indirect_len = indirect_term - indirect + 1;

                } else if (*scan == '\0') {
		    fprintf(stderr, "bad format in symbol line: %s\n", line);
		    exit(1);
		}
            } else if (*scan != '\0') {
                fprintf(stderr, "bad format in symbol line: %s\n", line);
                exit(1);
            }
        }

        if(idx >= max_symbols) {
            fprintf(stderr, "symbol[%d/%d] overflow: %s\n", idx, max_symbols, line);
            exit(1);
        }

        *name_term = '\0';
        if (indirect_term) {
            *indirect_term = '\0';
        }

        symbols[idx].name = name;
        symbols[idx].name_len = name_len;
        symbols[idx].indirect = indirect;
        symbols[idx].indirect_len = indirect_len;

        strtabsize += symbols[idx].name_len + symbols[idx].indirect_len;
        idx++;
    }

    return strtabsize;
}

int main(int argc, char * argv[])
{
    KXKextManagerError	err;
    int			i, fd;
    const char *	output_name = NULL;
    uint32_t		zero = 0, num_files = 0;
    uint32_t		filenum;
    uint32_t		strx, strtabsize, strtabpad;
    struct symbol *	import_symbols;
    struct symbol *	export_symbols;
    uint32_t		num_import_syms, num_export_syms, num_removed_syms;
    uint32_t		import_idx, export_idx;
    const NXArchInfo *	host_arch;
    const NXArchInfo *	target_arch;
    boolean_t		require_imports = true;
    boolean_t		diff = false;

    struct {
	struct mach_header    hdr;
	struct symtab_command symcmd;
    } load_cmds;

    struct file {
        vm_offset_t  mapped;
        vm_size_t    mapped_size;
	uint32_t     nsyms;
	boolean_t    import;
	const char * path;
    };
    struct file files[64];
    
    host_arch = NXGetLocalArchInfo();
    target_arch = host_arch;

    for( i = 1; i < argc; i += 2)
    {
	boolean_t import;

        if (!strcmp("-sect", argv[i]))
        {
	    require_imports = false;
	    i--;
	    continue;
        }
        if (!strcmp("-diff", argv[i]))
        {
	    require_imports = false;
	    diff = true;
	    i--;
	    continue;
        }

	if (i == (argc - 1))
	{
	    fprintf(stderr, "bad arguments: %s\n", argv[i]);
	    exit(1);
	}

        if (!strcmp("-arch", argv[i]))
        {
            target_arch = NXGetArchInfoFromName(argv[i + 1]);
	    if (!target_arch)
	    {
		fprintf(stderr, "unknown architecture name: %s\n", argv[i+1]);
		exit(1);
	    }
            continue;
        }
        if (!strcmp("-output", argv[i]))
        {
	    output_name = argv[i+1];
            continue;
        }

        if (!strcmp("-import", argv[i]))
	    import = true;
	else if (!strcmp("-export", argv[i]))
	    import = false;
	else
	{
	    fprintf(stderr, "unknown option: %s\n", argv[i]);
	    exit(1);
	}

        err = readFile(argv[i+1], &files[num_files].mapped, &files[num_files].mapped_size);
        if (kKXKextManagerErrorNone != err)
            exit(1);

        if (files[num_files].mapped && files[num_files].mapped_size)
	{
	    files[num_files].import = import;
	    files[num_files].path   = argv[i+1];
            num_files++;
	}
    }

    if (!output_name)
    {
	fprintf(stderr, "no output file\n");
	exit(1);
    }

    num_import_syms = 0;
    num_export_syms = 0;
    for (filenum = 0; filenum < num_files; filenum++)
    {
        files[filenum].nsyms = count_symbols((char *) files[filenum].mapped, files[filenum].mapped_size);
	if (files[filenum].import)
	    num_import_syms += files[filenum].nsyms;
	else
	    num_export_syms += files[filenum].nsyms;
    }
    if (!num_export_syms)
    {
	fprintf(stderr, "no export names\n");
	exit(1);
    }

    import_symbols = calloc(num_import_syms, sizeof(struct symbol));
    export_symbols = calloc(num_export_syms, sizeof(struct symbol));

    strtabsize = 4;
    import_idx = 0;
    export_idx = 0;

    for (filenum = 0; filenum < num_files; filenum++)
    {
	if (files[filenum].import)
	{
	    store_symbols((char *) files[filenum].mapped, files[filenum].mapped_size,
					import_symbols, import_idx, num_import_syms);
	    import_idx += files[filenum].nsyms;
	}
	else
	{
	    strtabsize += store_symbols((char *) files[filenum].mapped, files[filenum].mapped_size,
					export_symbols, export_idx, num_export_syms);
	    export_idx += files[filenum].nsyms;
	}
	if (!files[filenum].nsyms)
	{
	    fprintf(stderr, "warning: file %s contains no names\n", files[filenum].path);
	}
    }


    qsort(import_symbols, num_import_syms, sizeof(struct symbol), &qsort_cmp);
    qsort(export_symbols, num_export_syms, sizeof(struct symbol), &qsort_cmp);

    num_removed_syms = 0;
    if (num_import_syms)
    {
	for (export_idx = 0; export_idx < num_export_syms; export_idx++)
	{
	    boolean_t found = true;

	    if (export_symbols[export_idx].indirect)
	    {
		if (!bsearch(export_symbols[export_idx].indirect, import_symbols, 
				num_import_syms, sizeof(struct symbol), &bsearch_cmp))
		{
		    if (require_imports)
			fprintf(stderr, "exported name not in import list: %s\n", 
				    export_symbols[export_idx].indirect);
		    found = false;
		}
	    } 
            else
            {
                if (!bsearch(export_symbols[export_idx].name, import_symbols, 
                                num_import_syms, sizeof(struct symbol), &bsearch_cmp))
                {
                    if (require_imports)
                        fprintf(stderr, "exported name not in import list: %s\n", 
                                    export_symbols[export_idx].name);
                    found = false;
                }
            }
    
	    if (found && !diff)
		continue;
	    if (!found && diff)
		continue;

	    num_removed_syms++;
	    strtabsize -= (export_symbols[export_idx].name_len + export_symbols[export_idx].indirect_len);
	    export_symbols[export_idx].name = 0;
	}
    }

    if (require_imports && num_removed_syms)
    {
	err = kKXKextManagerErrorUnspecified;
	goto finish;
    }

    fd = open(output_name, O_WRONLY|O_CREAT|O_TRUNC, 0755);
    if (-1 == fd)
    {
	perror("couldn't write output");
	err = kKXKextManagerErrorFileAccess;
	goto finish;
    }

    strtabpad = (strtabsize + 3) & ~3;

    load_cmds.hdr.magic		= MH_MAGIC;
    load_cmds.hdr.cputype	= target_arch->cputype;
    load_cmds.hdr.cpusubtype	= target_arch->cpusubtype;
    load_cmds.hdr.filetype	= MH_OBJECT;
    load_cmds.hdr.ncmds		= 1;
    load_cmds.hdr.sizeofcmds	= sizeof(load_cmds.symcmd);
    load_cmds.hdr.flags		= MH_INCRLINK;

    load_cmds.symcmd.cmd	= LC_SYMTAB;
    load_cmds.symcmd.cmdsize	= sizeof(load_cmds.symcmd);
    load_cmds.symcmd.symoff	= sizeof(load_cmds);
    load_cmds.symcmd.nsyms	= (num_export_syms - num_removed_syms);
    load_cmds.symcmd.stroff	= (num_export_syms - num_removed_syms) * sizeof(struct nlist) 
				+ load_cmds.symcmd.symoff;
    load_cmds.symcmd.strsize	= strtabpad;

    if (target_arch->byteorder != host_arch->byteorder)
    {
	swap_mach_header(&load_cmds.hdr, target_arch->byteorder);
	swap_symtab_command(&load_cmds.symcmd, target_arch->byteorder);
    }

    err = writeFile(fd, &load_cmds, sizeof(load_cmds));
    if (kKXKextManagerErrorNone != err)
	goto finish;

    strx = 4;
    for (export_idx = 0; export_idx < num_export_syms; export_idx++)
    {
	struct nlist nl;

	if (!export_symbols[export_idx].name)
	    continue;

        if (export_idx
          && export_symbols[export_idx - 1].name
          && !strcmp(export_symbols[export_idx - 1].name, export_symbols[export_idx].name))
        {
            fprintf(stderr, "duplicate export: %s\n", export_symbols[export_idx - 1].name);
            err = kKXKextManagerErrorAlreadyLoaded;
	    goto finish;
        }
	nl.n_sect  = 0;
	nl.n_desc  = 0;

	nl.n_un.n_strx = strx;
	strx += export_symbols[export_idx].name_len;

	if (export_symbols[export_idx].indirect)
	{
	    nl.n_type  = N_INDR | N_EXT;
	    nl.n_value = strx;
	    strx += export_symbols[export_idx].indirect_len;
	}
	else
	{
	    nl.n_type  = N_UNDF | N_EXT;
	    nl.n_value = 0;
	}

	if (target_arch->byteorder != host_arch->byteorder)
	    swap_nlist(&nl, 1, target_arch->byteorder);

	err = writeFile(fd, &nl, sizeof(nl));
	if (kKXKextManagerErrorNone != err)
	    goto finish;
    }

    strx = sizeof(uint32_t);
    err = writeFile(fd, &zero, strx);
    if (kKXKextManagerErrorNone != err)
	goto finish;

    for (export_idx = 0; export_idx < num_export_syms; export_idx++)
    {
	if (!export_symbols[export_idx].name)
	    continue;
	err = writeFile(fd, export_symbols[export_idx].name, 
		    export_symbols[export_idx].name_len);
        if (kKXKextManagerErrorNone != err)
            goto finish;
        if (export_symbols[export_idx].indirect)
        {
            err = writeFile(fd, export_symbols[export_idx].indirect, 
                        export_symbols[export_idx].indirect_len);
            if (kKXKextManagerErrorNone != err)
                goto finish;
        }
    }

    err = writeFile(fd, &zero, strtabpad - strtabsize);
    if (kKXKextManagerErrorNone != err)
	goto finish;
	
    close(fd);


finish:
    for (filenum = 0; filenum < num_files; filenum++) {
        // unmap file
        if (files[filenum].mapped_size)
        {
            munmap((caddr_t)files[filenum].mapped, files[filenum].mapped_size);
            files[filenum].mapped     = 0;
            files[filenum].mapped_size = 0;
        }

    }

    if (kKXKextManagerErrorNone != err)
    {
	if (output_name)
	    unlink(output_name);
        exit(1);
    }
    else
        exit(0);
    return(0);
}

