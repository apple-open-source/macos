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
writeFile(int fd, const void * data, vm_size_t length);


__private_extern__ KXKextManagerError
writeFile(int fd, const void * data, vm_size_t length)
{
    KXKextManagerError err;

    if (length != write(fd, data, length))
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

	*objSize = stat_buf.st_size;

	if( KERN_SUCCESS != map_fd(fd, 0, objAddr, TRUE, *objSize)) {
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
    return (c > ' ');
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
count_symbols(char * file)
{
    char *   where;
    char *   eol;
    char *   next;
    uint32_t nsyms;

    for (nsyms = 0, where = file; true; where = next)
    {
	eol = strchr(where, '\n');
	if (!eol)
	    break;

	next = eol + 1;
	if (eol == where)
	    continue;
	if (where[0] == '#')
	    continue;

	while (!issymchar(*eol))
	    eol--;

	where = eol - 1;
	while (issymchar(*where))
	    where--;
	where++;

	if (where[0] == '.')
	    continue;

	nsyms++;
    }

    return nsyms;
}

static uint32_t
store_symbols(char * file, struct symbol * symbols, uint32_t idx, uint32_t max_symbols)
{
    char *   where;
    char *   eol;
    char *   next;
    uint32_t strtabsize;
    char *   indirect;
    uint32_t indirect_len;

    strtabsize = 0;

    for (where = file; true; where = next)
    {
	eol = strchr(where, '\n');
	if (!eol)
	    break;
	next = eol + 1;
	if (eol == where)
	    continue;
	if (where[0] == '#')
	    continue;

	while (!issymchar(*eol))
	    eol--;
	eol++;

	*eol = 0;

	where = eol - 1;
	indirect = NULL;
	indirect_len = 0;
	while (issymchar(*where)) {
	    if (':' == *where)
	    {
		indirect = where + 1;
		indirect_len = (eol - indirect + 1);
		eol = where;
		*eol = 0;
	    }
	    where--;
	}
	where++;

	if (where[0] == '.')
	    continue;

	if(idx >= max_symbols)
	{
	    fprintf(stderr, "symbol[%d] overflow %s\n", idx, where);
	    exit(1);
	}
	symbols[idx].name = where;
	symbols[idx].name_len = (eol - where + 1);
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
    int			fd;
    const char *	output_name = NULL;
    uint32_t		i, zero = 0, num_files = 0;
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
        files[filenum].nsyms = count_symbols((char *) files[filenum].mapped);
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
	    store_symbols((char *) files[filenum].mapped,
					import_symbols, import_idx, num_import_syms);
	    import_idx += files[filenum].nsyms;
	}
	else
	{
	    strtabsize += store_symbols((char *) files[filenum].mapped,
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
	    if (!bsearch(export_symbols[export_idx].name, import_symbols, 
			    num_import_syms, sizeof(struct symbol), &bsearch_cmp))
	    {
		if (require_imports)
		    fprintf(stderr, "exported name not in import list: %s\n", 
				export_symbols[export_idx].name);
		found = false;
	    }
    
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
		    export_symbols[export_idx].name_len + export_symbols[export_idx].indirect_len);
	if (kKXKextManagerErrorNone != err)
	    goto finish;
    }

    err = writeFile(fd, &zero, strtabpad - strtabsize);
    if (kKXKextManagerErrorNone != err)
	goto finish;
	
    close(fd);


finish:
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
