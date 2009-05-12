#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/fat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <mach/thread_status.h>
#include <mach/machine.h>
#include <mach/ppc/thread_status.h>
#include "sym.h"

extern int errno;

int image = 0;

int sortname(const void *a, const void *b);
int sortaddr(const void *a, const void *b);
void *bsrch(void *key, void *base, unsigned int num, unsigned int size, int (*compare) (void *, void *));
int srchaddr(void *key, void *b);
int srchname(void *key, void *b);

typedef struct sectdef {
	uint32_t start;
	uint32_t size;
	int code;
	struct section *sectp;
	char *core;
	uint32_t nxtassgn;
} sectdef;

char *sym = 0;							/* Full macho image */
char *symH = 0;							/* Stripped symbols sorted by address */
char *symN = 0;							/* Stripped symbols sorted by name */
char *strings = 0;
int  rsymc = 0;
char *nullname = 0;
sectdef sctn[256];						/* Allow up to 256 text and data sections */
int sectno = 0;
uint64_t entryaddr = 0xFFFFFFFFFFFFFFFFULL;	/* Entry point snarfed from initial thread state */
uint64_t textaddr = 0;					/* Address of text section */
uint64_t textsize = 0;					/* Size of text section */
uint32_t textsect = 0xFFFFFFFF;			/* Section number of text section */

struct section imgsect = {
	.sectname  = "Image",
	.segname   = "Image",
	.addr      = 0,
	.size      = 0,
	.offset    = 0,
	.align     = 2,
	.nreloc    = 0,
	.flags     = S_REGULAR | S_ATTR_SOME_INSTRUCTIONS,
	.reserved1 = 0,
	.reserved2 = 0
};

int syminitcore(uint32_t *coreimage, uint64_t vmaddr, uint64_t ssize) {

	symH = (char *)0;
	symN = (char *)0;
	strings = (char *)0;
	rsymc = 0;
	nullname = (char *)0;
	sectno = 0;
	entryaddr = 0xFFFFFFFFFFFFFFFFULL;

	sym = (char*)coreimage;			/* Rememberthe address of the memory image */
	textaddr = vmaddr;			/* Remember starting address */
	textsize = ssize;			/* Remember size */
	textsect = 0;				/* Remember section number */
	
	return 0;
}

#if defined(BILLANGELL)

int syminit(char *fname) {

	int i, j, symfile;
	uint64_t ssize;
	struct fat_header		*fheader;
	struct mach_header		*mheader;
	struct load_command		*ncmd;
	struct segment_command	*segcmd;
	struct symtab_command	*symtab = 0;
	struct ppc_thread_state	*regstate;
	struct fat_arch		*farch;
	struct section		*sect;
	struct section		*nsect;
	struct nlist		*symbs, *symb, *symbn, *symSortAddr, *symSortName;
	unsigned int		symcount;
	unsigned int		strsize;
	uint32_t fatoff;

//	printf("Analyzing \"%s\"\n", fname);				/* Print file name */
	
	symfile = open(fname, O_RDONLY, 0);
	if (symfile < 0) {
		printf("Can't open symbol/image file, errno = %d\n", errno);
		return 1 ;
	}

//	printf("symfile = %08X\n", symfile);

	ssize = lseek(symfile, 0, SEEK_END);				/* Find the end of the file */
//	printf("size = %08X.%08X, errno = %d\n", (uint32_t)(ssize >> 32), (uint32_t)ssize, errno);
	(void)lseek(symfile, 0, SEEK_SET);					/* Rewind it */

	sym = mmap(0, ssize, PROT_READ | PROT_WRITE, MAP_PRIVATE, symfile, 0);	/* Map in the object file */
	if((uint32_t)sym == 0xFFFFFFFF) {
		printf("Failed to map symbol/image file, errno = %d\n", errno);
		return(1);
	}

	close(symfile);
	
#if 0
	sctn[0].sddr = 0;
	sctn[0].size = 0;
	sctn[0].offset = 0;
	sctn[0].sddr = 0;
	sctn[0].sddr = 0;
#endif

	textaddr = 0;									/* Remember starting address */
	textsize = ssize;								/* Remember size */
	textsect = 0;									/* Remember section number */
	
	
	if(image) {											/* Are we using a core image? */
		loadsyms(fname);								/* Yeah, suck 'em in */
		return 0;
	}
	
	fheader = (struct fat_header *)sym;					/* Point to the header */
	mheader = (struct mach_header *)sym;				/* Point to the header */
	fatoff = 0;

	if(fheader->magic == FAT_MAGIC) {					/* Is this a fat tile? */
		farch = (struct fat_arch *)((uint32_t)fheader + sizeof(struct fat_header));	/* Point to the architecture types */

		for(i = 0; i < fheader->nfat_arch; i++) {		/* Scan the architecture types */
			if(farch[i].cputype == CPU_TYPE_POWERPC) break;	/* Is this a 32-bit PPC? */
		}

		if(i >= fheader->nfat_arch) {					/* Did we find it? */
			printf("Could not find PPC 32-bit in fat header\n");
			return (1);
		}
		
		mheader = (struct mach_header *)(sym + farch[i].offset);	/* Point to the header */
		fatoff = farch[i].offset;						/* Remember offset */
	}

#define dmacho 0
	
#if dmacho
	printf("     magic = %08X\n", mheader->magic);		/* mach magic number identifier */
	printf("   cputype = %08X\n", mheader->cputype);	/* cpu specifier */
	printf("cpusubtype = %08X\n", mheader->cpusubtype);	/* machine specifier */
	printf("  filetype = %08X\n", mheader->filetype);	/* type of file */
	printf("     ncmds = %08X\n", mheader->ncmds);		/* number of load commands */
	printf("sizeofcmds = %08X\n", mheader->sizeofcmds);	/* the size of all the load commands */
	printf("     flags = %08X\n", mheader->flags);		/* flags */
	printf("\n\nCommands:\n\n");

#endif

	sectno = 0;
	ncmd = (struct load_command *)((unsigned int)mheader + sizeof(struct mach_header));	/* Point to command */

	for(i = 0; i <  mheader->ncmds; i++) {				/* Print out all of the commands */
		switch (ncmd->cmd) {							/* Process the command */
			
			case LC_SEGMENT:							/* segment of this file to be mapped */
				segcmd = (struct segment_command *)ncmd;
				
#if dmacho				
				printf("LC_SEGMENT\n");
				printf("        name = %s\n", segcmd->segname);
				printf("      vmaddr = %08X\n", segcmd->vmaddr);
				printf("      vmsize = %08X\n", segcmd->vmsize);
				printf("     fileoff = %08X (%08X)\n", segcmd->fileoff, segcmd->fileoff + fatoff);
				printf("    filesize = %08X\n", segcmd->filesize);
				printf("     maxprot = %08X\n", segcmd->maxprot);
				printf("    initprot = %08X\n", segcmd->initprot);
				printf("      nsects = %08X\n", segcmd->nsects);
				printf("       flags = %08X\n", segcmd->flags);				
				printf("\n  Sections:\n");
#endif
				
				nsect = (struct section *)((unsigned int)segcmd + sizeof(struct segment_command));
				
				for(j = 0; j < segcmd->nsects; j++) {	/* Cycle through them all */
					sect = nsect;
					nsect++;
					sectno++;							/* Bump section number */
					
#if dmacho
					printf("    Section  name = %s\n", sect->sectname);
					printf("    Segment  name = %s\n", sect->segname);
					printf("    section  numb = %8d\n", sectno);
					printf("             addr = %08X\n", sect->addr);
					printf("             size = %08X\n", sect->size);
					printf("           offset = %08X (%08X)\n", sect->offset, sect->offset + fatoff);
					printf("            align = %08X\n", sect->align);
					printf("         relocoff = %08X\n", sect->reloff);
					printf("        num reloc = %08X\n", sect->nreloc);
					printf("            flags = %08X\n", sect->flags);
					printf("        reserved1 = %08X\n", sect->reserved1);
					printf("        reserved2 = %08X\n", sect->reserved2);
#endif

					if(sect->flags & S_ATTR_PURE_INSTRUCTIONS) sctn[sectno].code = 1;	/* Mark that we have code in this section */
					else sctn[sectno].code = 0;											/* No code here */
	
					sctn[sectno].start = sect->addr;	/* Get starting address */
					sctn[sectno].size = sect->size;		/* Get size */
					sctn[sectno].sectp = sect;			/* Remember pointer to header */
					sctn[sectno].core = (char *)((unsigned int)mheader + sect->offset);	/* Point to memory */
					sctn[sectno].nxtassgn = sect->addr;		/* Initialize allocation address */

//					printf("%3d %08X %08X %08X %s %s\n", sectno, sctn[sectno].start, sctn[sectno].size, sctn[sectno].core, sect->segname, sect->sectname);
					
					if(!strcmp(sect->sectname, "__text")) {	/* Is this the text section? */
						textaddr = sect->addr;			/* Remember starting address */
						textsize = sect->size;			/* Remember size */
						textsect = sectno;				/* Remember section number */
#if dmacho
						printf("__text section: addr = %016llX, size = %016llX, sectno = %d\n", textaddr, textsize, textsect);
#endif
					}
				}
				
				break;
			
			case LC_SYMTAB:								/* link-edit stab symbol table info */
				symtab = (struct symtab_command *)ncmd;
#if dmacho
				printf("LC_SYMTAB\n");
				printf("           offset = %08X\n", symtab->symoff);
				printf("           number = %08X\n", symtab->nsyms);
				printf("    string offset = %08X (%08X)\n", symtab->stroff, symtab->stroff + fatoff);
				printf("string table size = %08X\n", symtab->strsize);
#endif
				break;
			
			case LC_SYMSEG:								/* link-edit gdb symbol table info (obsolete) */
#if dmacho
				printf("LC_SYMSEG\n");
#endif
				break;
			
			case LC_THREAD:								/* thread */
#if dmacho
				printf("LC_THREAD\n");
#endif
				goto snarfaddr;

			case LC_UNIXTHREAD:							/* unix thread (includes a stack) */

#if dmacho
				printf("LC_UNIXTHREAD\n");
#endif
snarfaddr:
				regstate = (struct ppc_thread_state *)((uint32_t)ncmd + sizeof(struct load_command) + 8);
				entryaddr = regstate->srr0;				/* Get the entry address */

#if dmacho
				printf(" starting address = %08X.%08X\n", (uint32_t)(entryaddr >> 32), (uint32_t)entryaddr);
#endif

				break;

			case LC_LOADFVMLIB:							/* load a specified fixed VM shared library */
#if dmacho
				printf("LC_LOADFVMLIB\n");
#endif
				break;

			case LC_IDFVMLIB:							/* fixed VM shared library identification */
#if dmacho
				printf("LC_IDFVMLIB\n");
#endif
				break;

			case LC_IDENT:								/* object identification info (obsolete) */
#if dmacho
				printf("LC_IDENT\n");
#endif
				break;

			case LC_FVMFILE:							/* fixed VM file inclusion (internal use) */
#if dmacho
				printf("LC_FVMFILE\n");
#endif
				break;

			case LC_PREPAGE:							/* prepage command (internal use) */
#if dmacho
				printf("LC_PREPAGE\n");
#endif
				break;

			case LC_DYSYMTAB:							/* dynamic link-edit symbol table info */
#if dmacho
				printf("LC_DYSYMTAB\n");
#endif
				break;

			case LC_LOAD_DYLIB:							/* load a dynamicly linked shared library */
#if dmacho
				printf("LC_LOAD_DYLIB\n");
#endif
				break;

			case LC_ID_DYLIB:							/* dynamically linked shared lib identification */
#if dmacho
				printf("LC_ID_DYLIB\n");
#endif
				break;

			case LC_LOAD_DYLINKER:						/* load a dynamic linker */
#if dmacho
				printf("LC_LOAD_DYLINKER\n");
#endif
				break;

			case LC_ID_DYLINKER:						/* dynamic linker identification */
#if dmacho
				printf("LC_ID_DYLINKER\n");
#endif
				break;

			case LC_PREBOUND_DYLIB:						/* modules prebound for a dynamically linked shared library */
#if dmacho
				printf("LC_PREBOUND_DYLIB\n");
#endif
				break;

			case LC_ROUTINES:							/* image routines */
#if dmacho
				printf("LC_ROUTINES\n");
#endif
				break;

			case LC_SUB_FRAMEWORK:						/* sub frameworks */
#if dmacho
				printf("LC_SUB_FRAMEWORK\n");
#endif
				break;

			case LC_SUB_UMBRELLA:						/* sub umbrella */
#if dmacho
				printf("LC_SUB_UMBRELLA\n");
#endif
				break;

			case LC_SUB_CLIENT:							/* sub client */
#if dmacho
				printf("LC_SUB_CLIENT\n");
#endif
				break;

			case LC_SUB_LIBRARY:						/* sub library */
#if dmacho
				printf("LC_SUB_LIBRARY\n");
#endif
				break;

			case LC_TWOLEVEL_HINTS:						/* two-level namespace lookup hints */
#if dmacho
				printf("LC_TWOLEVEL_HINTS\n");
#endif
				break;

			case LC_PREBIND_CKSUM:						/* prebind checksum */
#if dmacho
				printf("LC_PREBIND_CKSUM\n");
#endif
				break;

			case LC_LOAD_WEAK_DYLIB:					/* dynamic library with weak symbols */
#if dmacho
				printf("LC_LOAD_WEAK_DYLIB\n");
#endif
				break;

			case LC_SEGMENT_64:							/* 64-bit segment */
#if dmacho
				printf("LC_SEGMENT_64\n");
#endif
				break;

			case LC_ROUTINES_64:						/* 64-bit image routines */
#if dmacho
				printf("LC_ROUTINES_64\n");
#endif
				break;

			default:
#if dmacho
				printf("Unknown command: %08X\n", ncmd->cmd);
#endif
				break;
		}
		ncmd = (struct load_command *)((unsigned int)ncmd + ncmd->cmdsize);	/* Next command */
	}

	symbs = (struct nlist *)(symtab->symoff + (unsigned int)mheader);
	symcount = symtab->nsyms;
	strings = (char *)(symtab->stroff + (unsigned int)mheader);
	strsize = symtab->strsize;
	symb = symbs;										/* Point to the start of the table */
	
	rsymc = 0;

//	printf("\nSymbol table:\n");
	
	for(i = 0; i < symcount; i++) {						/* Cycle through all symbols */
		if(!(symb->n_type & N_STAB)) {					/* Eliminate debug information */
			rsymc++;									/* Count regular symbols */
		}

//		printf("    %02X %02X %04X %08X ", symb->n_type, symb->n_sect, symb->n_desc, symb->n_value);
//		if(symb->n_un.n_strx) printf("%s", &strings[symb->n_un.n_strx]);
//		printf("\n");
			
		symb++;
	}

	symH = malloc(rsymc * sizeof(struct nlist));		/* Get some memory for it */
	if(!symH) {											/* Must not be enough */
		printf("Can't get storage for address sorted symbol table\n");
		return(1);
	}

	symN = malloc(rsymc * sizeof(struct nlist));		/* Get some memory for it */
	if(!symN) {											/* Must not be enough */
		free(symH);										/* Toss the other table */
		printf("Can't get storage for name sorted symbol table\n");
		return(1);
	}

	symb = (struct nlist *)symH;						/* Point to the start of the table */
	symbn = (struct nlist *)symN;						/* Point to the start of the table */

	j = 0;
	for(i = 0; i < symcount; i++) {						/* Cycle through all symbols */
		if(!(symbs[i].n_type & N_STAB)) {				/* Only copy if a regular symbol */
			symb[j] = symbs[i];							/* Copy it */
			symbn[j] = symbs[i];						/* Copy it */
			j++;										/* Bump index */
//			if(sctn[symbs[i].n_sect].code) printf("CODE    %02X %02X %04X %08X %s\n", symbs[i].n_type, symbs[i].n_sect, symbs[i].n_desc, symbs[i].n_value, &strings[symbs[i].n_un.n_strx]);
//			else printf("DATA    %02X %02X %04X %08X %s\n", symbs[i].n_type, symbs[i].n_sect, symbs[i].n_desc, symbs[i].n_value, &strings[symbs[i].n_un.n_strx]);
		}
		else {
//			printf("SKIP    %02X %02X %04X %08X %s\n", symbs[i].n_type, symbs[i].n_sect, symbs[i].n_desc, symbs[i].n_value, &strings[symbs[i].n_un.n_strx]);
		
		}
	}

	symSortAddr = symb;									/* Get the list */
	qsort((void *)symSortAddr, rsymc, sizeof(struct nlist), sortaddr);	/* Sort it by address */

//	for(i = 0; i < rsymc; i++) {						/* Cycle through all symbols */
//		if(sctn[symSortAddr[i].n_sect].code) printf("CODE    %02X %02X %04X %08X %s\n", symSortAddr[i].n_type, symSortAddr[i].n_sect, symSortAddr[i].n_desc, symSortAddr[i].n_value, &strings[symSortAddr[i].n_un.n_strx]);
//		else printf("DATA    %02X %02X %04X %08X %s\n", symSortAddr[i].n_type, symSortAddr[i].n_sect, symSortAddr[i].n_desc, symSortAddr[i].n_value, &strings[symSortAddr[i].n_un.n_strx]);
//	}

	symSortName = symbn;								/* Get the address of the buffer */
	qsort((void *)symSortName, rsymc, sizeof(struct nlist), sortname);	/* Sort it by name */
	
	return 0;
}

#endif /* BILLANGELL */

void symfree(void) {

	if(symH) free(symH);								/* Release this table */
	symH = 0;											/* Clear pointer */
	if(symN) free(symN);								/* Release this one too */
	symN = 0;											/* Clear pointer */
	return;
}

int sortaddr(const void *a, const void *b) {
	if(((struct nlist *)a)->n_value == ((struct nlist *)b)->n_value) return 0;
	if((uint32_t)((struct nlist *)a)->n_value > (uint32_t)((struct nlist *)b)->n_value) return 1;
	return -1;
}

int sortname(const void *a, const void *b) {
	
	return(strcmp((char *)(&strings[((struct nlist *)a)->n_un.n_strx]),
	  (char *)(&strings[((struct nlist *)b)->n_un.n_strx])));
}

/*
 *		This binary search returns either the one just before where a hit would be, or the hit
 */

void *bsrch(void *key, void *base, unsigned int num, unsigned int size, int (*compare) (void *, void *)) {
	unsigned int top, bot, probe;
	int rslt;
	
	bot = 0;
	top = num - 1;

	while(1) {										/* Go until we're done */
		
		
		probe = (top + bot) >> 1;					/* Get probe spot */
		
		rslt = compare(key, (void *)&(((char *)base)[probe * size]));	/* See if this is the one */
		if(!rslt) return ((void *)&(((char *)base)[probe * size]));		/* Yup it's it... */
		if(rslt < 0) {								/* Is the key smaller than the probe? */
			if(!probe) return (void *)0;			/* If we are below the first entry, return a zero */
			top = probe - 1;						/* Yeah */
		}
		else bot = probe + 1;						/* No, set the bottom up */
		
		if(top <= bot) {
			if(0 > compare(key, (void *)&(((char *)base)[bot * size]))) bot = bot - 1;
			return ((void *)&(((char *)base)[(bot) * size]));	/* Couldn't find nothing... */
		}
	}

}

int srchaddr(void *key, void *b) {
	
	if(*(unsigned int *)key == ((struct nlist *)b)->n_value) return 0;
	if(*(unsigned int *)key > ((struct nlist *)b)->n_value) return 1;
	return -1;
}

int srchname(void *key, void *a) {
	
	return(strcmp((char *)key, (char *)(&strings[((struct nlist *)a)->n_un.n_strx])));
}

int symindex(uint32_t index, symdesc *symd) {

	struct nlist *symb;
	char *sy;
	int eat;
	
	symb = (struct nlist *)symH;
	
	if(index == 0) {							/* Special case zeroth entry */
		symd->val = 0;							/* Value is 0 */
		symd->index = 0;						/* Index is 0 */
		symd->type = 0;							/* Set symbol type to data */
		symd->size = 0;							/* Zero size */
		symd->sect = 0;							/* No section */
		symd->name = nullname;					/* Return pointer to null name */
		return 0;
	}
	else if(index == -1) {						/* Do we want the text section? */

		if(textsize) {							/* Do we have a __text section? */
			symd->val = textaddr;				/* Set address of __text section */
			symd->index = -1;					/* Index is -1 */
			symd->type = 1;						/* Set symbol type to instructions */
			symd->size = textsize;				/* Set size of __text section */
			symd->sect = textsect;				/* Point to text section number */
			symd->name = "__text";				/* Return pointer to "__text" */
			return 0;
		}
		else if(image) {						/* Are we processing an image file? */
			symd->val = 0;						/* Set address of image to 0 */
			symd->index = -1;					/* Index is -1 */
			symd->type = 1;						/* Set symbol type to instructions */
			symd->size = textsize;				/* Set size of image */
			symd->sect = 0;						/* Set section to 0 */
			symd->name = "CoreImage";			/* Return pointer to "CoreImage" */
			return 0;
		}
		else return 1;							/* No text section */
	}

	if(index >= rsymc) return 1;				/* Invalid index */
	
	symd->val = (uint64_t)symb[index].n_value;	/* Return the value */
	symd->index = index;						/* Return the index */
	symd->type = sctn[symb[index].n_sect].code;	/* Set symbol type */
	symd->sect = symb[index].n_sect;			/* Pass back section */

	if((index <= rsymc) && (symd->sect == symb[index + 1].n_sect)) {	/* Not the last symbol and next symbol is in same section */
		symd->size = (uint64_t)symb[index + 1].n_value - symd->val;		/* Yes */
	}
	else {
		symd->size = sctn[symb[index].n_sect].size - (symd->val - sctn[symb[index].n_sect].start);	/* Calculate to end of section */
//		printf("%08X %08X %08X %08X.%08X\n", sctn[symb[index].n_sect].size, sctn[symb[index].n_sect].start, sctn[symb[index].n_sect].size + sctn[symb[index].n_sect].start, 
//		(uint32_t)(symd->val >> 32), (uint32_t)symd->val);
	}

	sy = &strings[symb[index].n_un.n_strx];			/* Point to the full symbol */
	if(sy[0] && (sy[0] == '_')) eat = 1;			/* Leading underbar if anything? */
	else eat = 0;									/* No */
	symd->name = &sy[eat];							/* Return pointer to the name */
	return 0;

}

void symaddr(uint32_t addr, char *symbol) {

	struct nlist *symb;
	uint32_t disp;
	int32_t eat;
	char *sy;

	if(!rsymc) {									/* Are there any symbols? */
		sprintf(symbol, "%X", addr);				/* Nope, just return the address */
		return;
	}

	symb = bsrch((void *)&addr, (void *)symH, rsymc, sizeof(struct nlist), srchaddr);
	if(!(uint32_t)symb) {							/* Does the symbol fall before the start of the table? */
		sprintf(symbol, "%X", addr);				/* Yes, just return the address */
		return;
	}
	
	disp = addr - symb->n_value;

	sy = &strings[symb->n_un.n_strx];				/* Point to the full symbol */
	if(sy[0] && (sy[0] == '_')) eat = 1;			/* Leading underbar if anything? */
	else eat = 0;									/* No */

	if(!disp)sprintf(symbol, "%s", &sy[eat]);
	else sprintf(symbol, "%s+%X", &sy[eat], disp);

	return;

}

uint64_t symsym(char *symbol) {

	struct nlist *symb;
	char reqsym[256];
	
	if(!rsymc) {									/* Are there any symbols? */
		return 0xFFFFFFFFFFFFFFFFLL;				/* Nope, return failure... */
	}
	
	reqsym[0] = '_';								/* Prime with the external character */
	strncpy(&reqsym[1], symbol, 255);				/* Copy the requested symbol to add the underbore */

	symb = bsrch(&reqsym[0], (void *)symN, rsymc, sizeof(struct nlist), srchname);	/* First try with the underbore */
		
	if(strcmp((char *)(&strings[symb->n_un.n_strx]), &reqsym[0])) {	/* If equal, we found it */
		symb = bsrch(&reqsym[1], (void *)symN, rsymc, sizeof(struct nlist), srchname);	/* Next try without */
		if(strcmp((char *)(&strings[symb->n_un.n_strx]), &reqsym[1])) {	/* If equal, we found it */
			return 0xFFFFFFFFFFFFFFFFLL;			/* Return failure... */
		}
	}

	return symb->n_value;

}

// Fetches will automatically pad up to "align" - 1 bytes on front 
// "align" must be a power of 2, e.g., 1, 2, 4, 8, ...

uint32_t symfetch(uint64_t addr, uint32_t bytes, uint32_t align, char *buffer) {

	int i, gsect;
	uint32_t rem, pad, sz, sstart, ssize;
	uint64_t end;
	char *curbyte, *score;
	
	if(!align) align = 1;
	
	if(!image) {									/* Are we working with a core image */
	
		gsect = -1;
	
		for(i = 1; i < (sectno + 1); i++) {			/* Find where the data is */
	
			if(sctn[i].size == 0) continue;			/* Skip a zero length section */
			end = (uint64_t)sctn[i].start + sctn[i].size - 1;	/* Calculate section end */
//			printf("***** find1  %016llX %016llX %016llX\n", addr, (uint64_t)sctn[i].start, (uint64_t)sctn[i].size);
//			printf("***** find2  %016llX %016llX %016llX\n", addr, ((uint64_t)sctn[i].start & -align), end);
			if((addr < ((uint64_t)sctn[i].start & -align)) || (addr > end)) {	/* Is our address within the section (start aligned downwards)? */
				if(((uint64_t)sctn[i].start - addr) >= align) continue;	/* And not within the alignment region at start? */
			}
			
			gsect = i;								/* Remember if it fits */
			sstart = sctn[gsect].start;				/* Set start of section */
			ssize = sctn[gsect].size;				/* Set size of section */
			score = sctn[gsect].core;				/* Set address */
			break;
		}
		
		if(gsect < 0) return 0;						/* Couldn't find it at all... */
	}
	else {											/* We are working with a core image */
		end = textaddr + textsize - 1;	/* Calculate section end */
//		printf("***** find1  %016llX %016llX %016llX\n", addr, (uint64_t)sctn[i].start, (uint64_t)sctn[i].size);
//		printf("***** find2  %016llX %016llX %016llX\n", addr, ((uint64_t)sctn[i].start & -align), end);
		if((addr < (textaddr & -align)) || (addr > end)) {	/* Is our address within the section (start aligned downwards)? */
			if((textaddr - addr) >= align) return 0;	/* No, leave... */
		}
		sstart = textaddr;							/* Set start of section */
		ssize = textsize;							/* Set size of section */
		score = sym;								/* Set address */
	}
	
	pad = 0;
	sz = bytes;
	if(addr < (uint64_t)sstart) {					/* Was start aligned before actual? */
		pad = (uint64_t)sstart - addr;				/* Yeah, find out how far to pad */

		for(i = 0; i < pad; i++) buffer[i] = 0xAA;	/* Mark padded area */

		addr += pad;								/* Adjust requested address by padding amount */
		buffer += pad;								/* Adjust output address by padding amount */
		sz -= pad;									/* Adjust bytes by padding amount */
	}
	
	
	rem = (sstart + ssize) - addr;					/* Get length remaining */
	if(rem < sz) sz = rem;							/* Pin at what is left in section */

	curbyte = (char *)(score + (addr - sstart));	/* Get memory address */
	bcopy(curbyte, buffer, sz);						/* Copy it over to the output buffer */
	
	rem = bytes - pad - sz;							/* See if we moved all requested */
	if(rem >= bytes) {								/* We moved more than requested */
		printf("Blood squeezed from turnip, addr = %08X.%08X, size = %d\n", (uint32_t)(addr >> 32), (uint32_t)addr, bytes);
		exit(1);
	}
	
	for(i = 0; i < rem; i++) buffer[sz + i] = 0xAA;	/* Pad out with known value */
	
	return 1;

}


uint64_t symstart(void) {

	return entryaddr;
	
}

void loadsyms(char *image) {

	char symfile[512];

	symfile[0] = 0;
	strncpy(symfile, image, 255);					/* Copy the image filename */
	strncat(symfile, ".sym", 511);					/* Add .sym suffix */
	
	return;
}
