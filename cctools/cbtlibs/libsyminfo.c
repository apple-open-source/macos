/*3456789012345678901234567890123456789012345678901234567890123456789012345678*/
#include <mach/mach.h> /* first so to get rid of a precomp warning */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>
#include <mach-o/arch.h>
#include "stuff/bool.h"
#include "stuff/ofile.h"
#include "stuff/errors.h"
#include "stuff/allocate.h"
#include "stuff/guess_short_name.h"
char *progname = NULL;

static void nm(struct ofile *ofile,char *arch_name,void *cookie);

/* This function probably has some endianess issues */
char *get_full_path(char *short_name, struct ofile *ofile);

/* Define the structures used by SymInfo */
typedef struct {
    char *name;
    const char *arch;
    char *ordinal; // for imports only
}_SymInfoSymbol;
typedef _SymInfoSymbol *SymInfoSymbol;

typedef struct {
    char **subUmbrellas;
    char **subFrameworks;
    unsigned int nSubFrameworks;
    unsigned int nSubUmbrellas;
}_SymInfoDependencies;
typedef _SymInfoDependencies *SymInfoDependencies;

typedef struct {
    SymInfoSymbol *exports;
    SymInfoSymbol *imports;
    SymInfoDependencies dependencies;
    unsigned int nExports;
    unsigned int nImports;
    char *path;
    char *shortName;
}_SymInfoList;

typedef _SymInfoList *SymInfoList;
#define __SymInfoTypes__
#include <cbt/libsyminfo.h>


/* flags to control the nm callback */
struct cmd_flags {
    unsigned long nfiles;
    enum bool a;        /* print all symbol table entries including stabs */
    enum bool g;        /* print only global symbols */
    enum bool n;        /* sort numericly rather than alphabetically */
    enum bool o;        /* prepend file or archive element name to each line */
    enum bool p;        /* don't sort; print in symbol table order */
    enum bool r;        /* sort in reverse direction */
    enum bool u;        /* print only undefined symbols */
    enum bool d;		/* print only defined symbols, opposite of u */
    enum bool m;        /* print symbol in Mach-O symbol format */
    enum bool x;        /* print the symbol table entry in hex and the name */
    enum bool j;        /* just print the symbol name (no value or type) */
    enum bool s;        /* print only symbol in the following section */
    char *segname,      /*  segment name for -s */
	 *sectname;     /*  section name for -s */
    enum bool l;        /* print a .section_start symbol if none exists (-s) */
    enum bool f;        /* print a dynamic shared library flat */
    enum bool v;        /* sort and print by value diffences ,used with -n -s */
    enum bool b;        /* print only stabs for the following include */
    char *bincl_name;   /*  the begin include name for -b */
    enum bool i;        /* start searching for begin include at -iN index */
    enum bool getLibDeps; /* Get the library dependency info like
			   * SUB_UMBRELLA, SUB_FRAMEWORK, etc */
    enum bool import;	/* Flag to identify were looking for imported symbols
			   data */
    unsigned long index; /*  the index to start searching at */
};




/* flags set by processing a specific object file */
struct process_flags {
    unsigned long nsect; /* The nsect, address and size for the */
    unsigned long sect_addr, /*  section specified by the -s flag */
		sect_size;
    enum bool sect_start_symbol; /* For processing the -l flag, set if a */
				 /*  symbol with the start address of the */
				 /*  section is found */
    unsigned long nsects; /* For printing the symbol types, the number */
    struct section **sections;/*  of sections and an array of section ptrs */
    unsigned char text_nsect,	/* For printing symbols types, T, D, and B */
		    data_nsect,	/*  for text, data and bss symbols */
		    bss_nsect;
    unsigned long nlibs;	/* For printing the twolevel namespace */
    char **lib_names;		/*  references types, the number of libraries */
				/*  an array of pointers to library names */
};



static enum bool select_symbol(struct nlist *symbol,
			       struct cmd_flags *cmd_flags,
			       struct process_flags *process_flags);
static struct nlist *select_symbols(struct ofile *ofile,
				    struct symtab_command *st,
				    struct dysymtab_command *dyst,
				    struct cmd_flags *cmd_flags,
				    struct process_flags *process_flags,
				    unsigned long *nsymbols,
				    struct nlist *all_symbols);
/*
 * Super hack so values can get passed back from  the nm callback.
 * The only other way I see to do this is to copy in the ofile_process code
 * and edit it to return a value but that wouldn't work to well.  This is
 * ugly but effective.
 *
 */
static struct selectedSymbolListInfo *gInfo;
static SymInfoList self;
int selectCounter=0;

SymInfoList SymInfoCreate(char *fileName)
{
    SymInfoList rList;
    struct cmd_flags cmd_flags = { 0 };
    kern_return_t r;

    /* Allocate return value and point the global pointer self at it */
    rList = malloc(sizeof(_SymInfoList));
    bzero(rList,sizeof(_SymInfoList));
    if(!rList)
	return(rList);
    self = rList;
    gInfo = allocate(sizeof(struct selectedSymbolListInfo));
    bzero(gInfo,sizeof(struct selectedSymbolListInfo));
    
    /* We want to process all the ofiles possible for this file */
    /* The cmd_flags are set in nm depending on what is being accomplished */
    ofile_process(fileName,NULL,0,TRUE,FALSE,FALSE,TRUE,nm,&cmd_flags);

    /* Clean up */
    if((r = vm_deallocate(mach_task_self(),(vm_address_t)gInfo->mappedFile,
			  (vm_size_t)gInfo->mappedFileSize)) != KERN_SUCCESS){
	my_mach_error(r, "Can't vm_deallocate mapped memory for file: "
	       "%s",fileName);
    }
    free(gInfo->fileName);
    return(rList);
}
void SymInfoFreeSymbol(SymInfoSymbol symbol)
{

    if(symbol == NULL){
	printf("Null symbol, not doing a thing\n");
	return;
    }
    if(symbol->name)
	free(symbol->name);
    if(symbol->ordinal)
	free(symbol->ordinal);
    if(symbol)
	free(symbol);
}
void SymInfoFree(SymInfoList nmList)
{
    unsigned long i;
    
    if(!nmList){
	return;
    }
    
    /* Free the Symbols */
    for(i=0; i<nmList->nExports; i++){
	SymInfoFreeSymbol(nmList->exports[i]);
    }
    if(nmList->exports)
	free(nmList->exports);
    for(i=0; i<nmList->nImports; i++){
	SymInfoFreeSymbol(nmList->imports[i]);
    }
    if(nmList->imports)
	free(nmList->imports);
    
    /* Free the Dependencies */
    SymInfoFreeDependencies(nmList->dependencies);

    /* Free the rest */
    if(nmList->path)
	free(nmList->path);
    
    if(nmList->shortName)
	free(nmList->shortName);
    if(nmList)
	free(nmList);
    if(gInfo)
	free(gInfo);
}
void SymInfoFreeDependencies(SymInfoDependencies deps)
{
    unsigned long i;
    
    if(!deps)
	return;
    
    /* Free the subumbrellas */
    for(i=0;i<deps->nSubUmbrellas;i++){
	if(deps->subUmbrellas[i])
	    free(deps->subUmbrellas[i]);
    }
    if(deps->subUmbrellas)
	free(deps->subUmbrellas);
    
    /* Free the subFrameworks */
    for(i=0;i<deps->nSubFrameworks;i++){
	if(deps->subFrameworks[i])
	    free(deps->subFrameworks[i]);
    }
    if(deps->subFrameworks)
	free(deps->subFrameworks);
    free(deps);
}

/* Access functions */
SymInfoSymbol *SymInfoGetImports(SymInfoList nmList)
{
    if(!nmList)
	return(NULL);
    return(nmList->imports);
}
SymInfoSymbol *SymInfoGetExports(SymInfoList nmList)
{
    if(!nmList)
	return(NULL);
    return(nmList->exports);
}
SymInfoDependencies SymInfoGetLibraryInfo(SymInfoList nmList)
{
    if(!nmList)
	return(NULL);
    return(nmList->dependencies);
}
unsigned int SymInfoGetSubFrameworkCount(SymInfoDependencies deps)
{
    if(!deps)
	return(NULL);
    return(deps->nSubFrameworks);
}
unsigned int SymInfoGetSubUmbrellaCount(SymInfoDependencies deps)
{
    if(!deps)
	return(NULL);
    return(deps->nSubUmbrellas);
}
char **SymInfoGetSubUmbrellas(SymInfoDependencies deps)
{
    if(!deps)
	return(NULL);
    return(deps->subUmbrellas);
}
char **SymInfoGetSubFrameworks(SymInfoDependencies deps)
{
    if(!deps)
	return(NULL);
    return(deps->subFrameworks);
}
char *SymInfoGetSymbolName(SymInfoSymbol symbol)
{
    if(!symbol)
	return(NULL);
    return(symbol->name);
}
const char *SymInfoGetSymbolArch(SymInfoSymbol symbol)
{
    if(!symbol)
	return(NULL);    
    return(symbol->arch);
}
char *SymInfoGetSymbolOrdinal(SymInfoSymbol symbol)
{
    if(!symbol)
	return(NULL);
    return(symbol->ordinal);
}
unsigned int SymInfoGetExportCount(SymInfoList nmList)
{
    if(!nmList)
	return(NULL);
    return(nmList->nExports);
}
unsigned int SymInfoGetImportCount(SymInfoList nmList)
{
    if(!nmList)
	return(NULL);
    return(nmList->nImports);
}
char *SymInfoGetShortName(SymInfoList nmList)
{
    if(!nmList)
	return(NULL);
    return(nmList->shortName);
}

/* Function for creating SymInfoSymbol */
SymInfoSymbol SymInfoCreateSymbols(char *name, char *arch, char *ordinal) 
{
    SymInfoSymbol symbol;
    symbol = malloc(sizeof(_SymInfoSymbol));
    symbol->name = name;
    symbol->arch = arch;
    symbol->ordinal = ordinal;
    return(symbol);
}

SymInfoDependencies SymInfoCreateDependencies(char **subUmbrellas, char **subFrameworks,
				    int nSubUmbrellas, int nSubFrameworks)
{
    SymInfoDependencies deps;
    deps = malloc(sizeof(_SymInfoDependencies));
    bzero(deps,sizeof(_SymInfoDependencies));
    deps->subUmbrellas = subUmbrellas;
    deps->subFrameworks = subFrameworks;
    deps->nSubUmbrellas = nSubUmbrellas;
    deps->nSubFrameworks = nSubFrameworks;
    return(deps);
}
/*
 * nm() is the processor routine that will extract export and import info as
 * as the library information.
 */

static
void
nm(
struct ofile *ofile,
char *arch_name,
void *cookie)
{
    struct cmd_flags *cmd_flags;
    struct process_flags process_flags;
    unsigned long i, j, k;
    struct load_command *lc;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct segment_command *sg;
    struct section *s;
    struct dylib_command *dl;
    unsigned long library_ordinal;
    unsigned long strsize = 0;
    char *strings = NULL; 
    struct nlist *symbols = NULL;
    unsigned long nsymbols;
    unsigned long nlibnames = 0;
    char *short_name, *has_suffix;
    enum bool is_framework;
    int symbolIndex;
    struct nlist *all_symbols;
    struct dylib_module m;
    struct dylib_reference *refs;
    
    cmd_flags = (struct cmd_flags *)cookie;
    process_flags.nsect = -1;
    process_flags.sect_addr = 0;
    process_flags.sect_size = 0;
    process_flags.sect_start_symbol = FALSE;
    process_flags.nsects = 0;
    process_flags.sections = NULL;
    process_flags.text_nsect = NO_SECT;
    process_flags.data_nsect = NO_SECT;
    process_flags.bss_nsect = NO_SECT;
    process_flags.nlibs = 0;
    process_flags.lib_names = NULL;
    
    st = NULL;
    dyst = NULL;
    lc = ofile->load_commands;
    for(i = 0; i < ofile->mh->ncmds; i++){
	if(st == NULL && lc->cmd == LC_SYMTAB){
	    st = (struct symtab_command *)lc;
	}
	else if(dyst == NULL && lc->cmd == LC_DYSYMTAB){
	    dyst = (struct dysymtab_command *)lc;
	}
	else if(lc->cmd == LC_SEGMENT){
	    sg = (struct segment_command *)lc;
	    process_flags.nsects += sg->nsects;
	}
	else if((ofile->mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL &&
		(lc->cmd == LC_LOAD_DYLIB ||
		    lc->cmd == LC_LOAD_WEAK_DYLIB)){
	    process_flags.nlibs++;
	}
	lc = (struct load_command *)((char *)lc + lc->cmdsize);
    }
    if(st == NULL || st->nsyms == 0){
	/* If there is no name list we don't really care */
	return;
    }
    if(process_flags.nsects > 0){
	process_flags.sections = 
	    (struct section **) malloc(sizeof(struct section *)
					* process_flags.nsects);
	k = 0;
	lc = ofile->load_commands;
	for (i = 0; i < ofile->mh->ncmds; i++){
	    if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		s = (struct section *)
			((char *)sg + sizeof(struct segment_command));
		for(j = 0; j < sg->nsects; j++){
		    if(strcmp((s + j)->sectname, SECT_TEXT) == 0 &&
			strcmp((s + j)->segname, SEG_TEXT) == 0)
			process_flags.text_nsect = k + 1;
		    else if(strcmp((s + j)->sectname, SECT_DATA) == 0 &&
			    strcmp((s + j)->segname, SEG_DATA) == 0)
			process_flags.data_nsect = k + 1;
		    else if(strcmp((s + j)->sectname, SECT_BSS) == 0 &&
			    strcmp((s + j)->segname, SEG_DATA) == 0)
			process_flags.bss_nsect = k + 1;
		    if(cmd_flags->segname != NULL){
			if(strncmp((s + j)->sectname, cmd_flags->sectname,
				    sizeof(s->sectname)) == 0 &&
			    strncmp((s + j)->segname, cmd_flags->segname,
				    sizeof(s->segname)) == 0){
			    process_flags.nsect = k + 1;
			    process_flags.sect_addr = (s + j)->addr;
			    process_flags.sect_size = (s + j)->size;
			}
		    }
		    process_flags.sections[k++] = s + j;
		}
	    }
	    lc = (struct load_command *)
		    ((char *)lc + lc->cmdsize);
	}
    }
    if((ofile->mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL &&
	process_flags.nlibs > 0){
	process_flags.lib_names = (char **)
		    malloc(sizeof(char *) * process_flags.nlibs);
	j = 0;
	lc = ofile->load_commands;
	for (i = 0; i < ofile->mh->ncmds; i++){
	    if(lc->cmd == LC_LOAD_DYLIB || lc->cmd == LC_LOAD_WEAK_DYLIB){
		dl = (struct dylib_command *)lc;
		process_flags.lib_names[j] =
		    savestr((char *)dl + dl->dylib.name.offset);
		short_name = guess_short_name(process_flags.lib_names[j],
						&is_framework, &has_suffix);
		if(has_suffix)
		    free(has_suffix);

		if(short_name != NULL)
		    process_flags.lib_names[j] = short_name;
		j++;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	nlibnames = j;
    }
    /* Correct the endianness of this ofile */
    all_symbols = (struct nlist *)(ofile->object_addr + st->symoff);

    if(ofile->object_byte_sex != get_host_byte_sex()){
	/* 
	 * Cache the symbols whose endianness has been flipped so it doesn't
	 * have to be done again
	 */
	
	if(gInfo->cachedFileName &&
	      (strcmp(gInfo->cachedFileName,ofile->file_name) == 0) &&
	      gInfo->byteSex == (int)ofile->object_byte_sex && 
	      st->symoff == gInfo->st->symoff &&
	      st->nsyms == gInfo->st->nsyms &&
	      st->stroff == gInfo->st->stroff &&
	      st->strsize == gInfo->st->strsize) {
	    all_symbols = gInfo->all_symbols;
	} else {
	    swap_nlist(all_symbols, st->nsyms, get_host_byte_sex());
	    gInfo->byteSex =  ofile->object_byte_sex;
	    if(gInfo->cachedFileName) {
		free(gInfo->cachedFileName);
		gInfo->cachedFileName = NULL;
	    }
	    gInfo->cachedFileName = savestr(ofile->file_name);
	    gInfo->all_symbols = all_symbols;
	    gInfo->st = st;
	}
    }
    if(ofile->dylib_module != NULL){
	m = *ofile->dylib_module;
	refs = (struct dylib_reference *)(ofile->object_addr +
				    dyst->extrefsymoff);
	if(ofile->object_byte_sex != get_host_byte_sex()){
	    swap_dylib_module(&m, 1, get_host_byte_sex());
	    swap_dylib_reference(refs + m.irefsym, m.nrefsym,
		    get_host_byte_sex());
	}
    }
    /* select export symbols to return */
    cmd_flags->g = TRUE;
    cmd_flags->d = TRUE;
    symbols = select_symbols(ofile, st, dyst, cmd_flags, &process_flags,
				&nsymbols,all_symbols);
    strings = ofile->object_addr + st->stroff;
    strsize = st->strsize;
    for(i = 0; i < nsymbols; i++){
	if(symbols[i].n_un.n_strx == 0)
	    symbols[i].n_un.n_name = "";
	else if(symbols[i].n_un.n_strx < 0 ||
		(unsigned long)symbols[i].n_un.n_strx > st->strsize){
	    symbols[i].n_un.n_name = "bad string index";
	    	    printf ("Setting bad string index in exports\n");

	    }
	else
	    symbols[i].n_un.n_name = symbols[i].n_un.n_strx + strings;
	if((symbols[i].n_type & N_TYPE) == N_INDR){
	    if(symbols[i].n_value == 0)
		symbols[i].n_value = (long)"";
	    else if(symbols[i].n_value > st->strsize){
		symbols[i].n_value = (long)"bad string index";
		printf ("Setting bad string index in exports2\n");

	    }
	    else
		symbols[i].n_value = (long)(symbols[i].n_value + strings);
	}
    }
    self->nExports += nsymbols;
    
    /* Store all this info so that it can be cleaned up later*/
    gInfo->mappedFile = ofile->file_addr;
    gInfo->mappedFileSize = ofile->file_size;
    gInfo->fileName = ofile->file_name;

    /* Reallocate the array of SymInfoSymbol export structs in self */
    self->exports =
	reallocate(self->exports,sizeof(SymInfoSymbol *)*(self->nExports));

    /* Loop through saving the symbol information in SymInfoSymbol structs */
    for (i=self->nExports-nsymbols;i < self->nExports; i++){
	const NXArchInfo *archInfo;
	symbolIndex = i - (self->nExports - nsymbols);
	self->exports[i] = malloc(sizeof(_SymInfoSymbol));

	/* Save the symbol info */
	archInfo = NXGetArchInfoFromCpuType(ofile->mh->cputype,
                                            CPU_SUBTYPE_MULTIPLE);
	self->exports[i]->name = savestr(symbols[symbolIndex].n_un.n_name);
	/* If we don't know the arch name use the number */
	if(!archInfo) {
	    char archString[10];
	    sprintf(archString,"%d",ofile->mh->cputype);
	    self->exports[i]->arch = savestr(archString);
	} else {
	    self->exports[i]->arch = archInfo->name;
	}
	self->exports[i]->ordinal = NULL;
    }
    free(symbols);
    /* Unset the cmd_flags used to find exports */
    cmd_flags->d = FALSE;

    /* Set up the flags to get the symbol info for the imports */
    cmd_flags->g = TRUE;
    cmd_flags->u = TRUE;
    cmd_flags->import = TRUE;
    
    symbols = select_symbols(ofile, st, dyst, cmd_flags, &process_flags,
			&nsymbols,all_symbols);
    strings = ofile->object_addr + st->stroff;
    strsize = st->strsize;
    for(i = 0; i < nsymbols; i++){
	if(symbols[i].n_un.n_strx == 0)
	    symbols[i].n_un.n_name = "";
	else if(symbols[i].n_un.n_strx < 0 ||
		(unsigned long)symbols[i].n_un.n_strx > st->strsize){
	    symbols[i].n_un.n_name = "bad string index";
	    printf ("Setting bad string index in imports\n");
	}
	else
	    symbols[i].n_un.n_name = symbols[i].n_un.n_strx + strings;
	if((symbols[i].n_type & N_TYPE) == N_INDR){
	    if(symbols[i].n_value == 0)
		symbols[i].n_value = (long)"";
	    else if(symbols[i].n_value > st->strsize){
		symbols[i].n_value = (long)"bad string index";
		printf ("Setting bad string index in imports\n");

	    }
	    else
		symbols[i].n_value = (long)(symbols[i].n_value + strings);
	}
    }

    self->nImports += nsymbols;

    /* Reallocate the array of SymInfoSymbol imports structs in self */
    self->imports =
	reallocate(self->imports,sizeof(SymInfoSymbol *)*(self->nImports));

    /* Loop through saving the symbol information in SymInfoSymbol structs */
    for (i=self->nImports-nsymbols;i < self->nImports; i++){
	const NXArchInfo *archInfo;
	symbolIndex = i - (self->nImports - nsymbols);
	self->imports[i] = malloc(sizeof(_SymInfoSymbol));
	archInfo = NXGetArchInfoFromCpuType(ofile->mh->cputype, 
                                            CPU_SUBTYPE_MULTIPLE);

	/* Save the name and arch */
	self->imports[i]->name = savestr(symbols[symbolIndex].n_un.n_name);
	self->imports[i]->arch = archInfo->name;

	/* Now extract the ordinal info and save the short library name */
	library_ordinal = GET_LIBRARY_ORDINAL(symbols[symbolIndex].n_desc);
	if(library_ordinal != 0){
	    if(library_ordinal == EXECUTABLE_ORDINAL)
		self->imports[i]->ordinal = savestr("from executable");
	    else if(library_ordinal == DYNAMIC_LOOKUP_ORDINAL)
		self->imports[i]->ordinal = savestr("dynamically looked up");
	    else if(library_ordinal-1 >= process_flags.nlibs)
		self->imports[i]->ordinal = savestr("bad library ordinal");
	    else 
		/* Find the full path to the library */
		self->imports[i]->ordinal = 
		    get_full_path(process_flags.lib_names[library_ordinal-1],
				  ofile);
	}else 
	    self->imports[i]->ordinal= NULL;
    }
    free(symbols);

    cmd_flags->g = FALSE;
    cmd_flags->u = FALSE;
    cmd_flags->import = FALSE;

    /* Now get the dependency information from the load commands */
    if(!self->dependencies){
	/* Allocate the memory */
	self -> dependencies = malloc(sizeof(_SymInfoDependencies));
	bzero(self -> dependencies,sizeof(_SymInfoDependencies));
	lc = ofile->load_commands;

	/* Now get the dependency info */
	for(j = 0; j < ofile->mh->ncmds; j++){
	    char *p;
	    if(lc->cmd == LC_SUB_UMBRELLA || lc->cmd == LC_SUB_LIBRARY){
		struct sub_umbrella_command usub;
		memset((char *)&usub, '\0',
			sizeof(struct sub_umbrella_command));
		memcpy((char *)&usub, (char *)lc, lc->cmdsize);
		p = (char *)lc + usub.sub_umbrella.offset;
		self->dependencies->subUmbrellas =
		    reallocate(self->dependencies->subUmbrellas,
		(self->dependencies->nSubUmbrellas+1)*
		sizeof(char **));
		self->dependencies ->
		    subUmbrellas[self->dependencies->nSubUmbrellas] =
							     savestr(p);
		self->dependencies->nSubUmbrellas++;
	    } else if(lc->cmd == LC_SUB_FRAMEWORK) { 
		struct sub_framework_command subFramework;
		memset((char *)&subFramework,'\0',
		sizeof(struct sub_framework_command));
		memcpy((char *)&subFramework,(char *)lc,lc->cmdsize);
		p = (char *)lc + subFramework.umbrella.offset;
		self->dependencies-> subFrameworks =
			reallocate(self->dependencies->subFrameworks,
		(self->dependencies->nSubFrameworks+1)
		*sizeof(char **));
		self->dependencies->
		    subFrameworks[self->dependencies->nSubFrameworks] =
								savestr(p);
		self->dependencies->nSubFrameworks++;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

	/*
	* Save the short name for this binary, to be used
	* to map short name to full path
	*/
	short_name =
	    guess_short_name(ofile->file_name,&is_framework, &has_suffix);
	if(has_suffix)
	    free(has_suffix);
	if(short_name){
	    self->shortName = short_name;
	}
    }
    
    /* Free the memory that was malloced in this function */
    for(i=0; i<nlibnames; i++) {
	free(process_flags.lib_names[i]);
    }
    free(process_flags.sections);
    if(process_flags.lib_names)
	free(process_flags.lib_names);
}

char *get_full_path(char *short_name, struct ofile *ofile) 
{
    unsigned long j;
    struct dylib_command *dl;
    struct load_command *lc;
    char *has_suffix;
    enum bool is_framework;
    
    
    lc = ofile->load_commands;

    for(j = 0; j < ofile->mh->ncmds; j++) {
	if(lc->cmd == LC_LOAD_DYLIB || lc->cmd == LC_LOAD_WEAK_DYLIB) {
	    char *longName;
	    char *shortName;
	    char *returnLongName;
	    dl = (struct dylib_command *)lc;
	    longName = (char *)lc + dl->dylib.name.offset;
	    shortName = guess_short_name(longName,&is_framework,&has_suffix);
	    if(shortName && strcmp(shortName,short_name) == 0) {
		free(shortName);
		returnLongName = malloc(strlen(longName)+2);
		returnLongName[0] = '.';
		returnLongName[1] = '\0';
		strcat(returnLongName, longName);
		return(returnLongName);
	    }
	}
	lc = (struct load_command *)((char *)lc + lc->cmdsize);
    }
    //fprintf(stderr,"WARNING: Couldn't find full path for: %s\n",short_name);
    return(savestr(short_name));
}
static
struct nlist *
select_symbols(struct ofile *ofile,
	       struct symtab_command *st,
	       struct dysymtab_command *dyst,
	       struct cmd_flags *cmd_flags,
	       struct process_flags *process_flags,
	       unsigned long *nsymbols,
	       struct nlist *all_symbols)
{
    unsigned long i, flags, nest;
    struct nlist *selected_symbols, undefined;
    struct dylib_module m;
    struct dylib_reference *refs;
    enum bool found;
    char *strings = NULL; 

	selected_symbols = allocate(sizeof(struct nlist) * st->nsyms);
	*nsymbols = 0;

	if(ofile->dylib_module != NULL){
	    m = *ofile->dylib_module;
	    refs = (struct dylib_reference *)(ofile->object_addr +
				       dyst->extrefsymoff);
	    
	    if(ofile->object_byte_sex != get_host_byte_sex())
		swap_dylib_module(&m, 1, get_host_byte_sex());
		
	    for(i = 0; i < m.nrefsym; i++){
		flags = refs[i + m.irefsym].flags;
		if(flags == REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
		   flags == REFERENCE_FLAG_UNDEFINED_LAZY ||
		   flags == REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY ||
		   flags == REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY){
		    undefined = all_symbols[refs[i + m.irefsym].isym];
		    if(flags == REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
		       flags == REFERENCE_FLAG_UNDEFINED_LAZY ||
		       cmd_flags->m == TRUE)
			undefined.n_type = N_UNDF | N_EXT;
		    else
			undefined.n_type = N_UNDF;
			undefined.n_desc = (undefined.n_desc &~ REFERENCE_TYPE)|
					    flags;
			undefined.n_value = 0;
			if(select_symbol(&undefined, cmd_flags, process_flags))
			    selected_symbols[(*nsymbols)++] = undefined;
		}
	    }
	    for(i = 0; i < m.nextdefsym && m.iextdefsym + i < st->nsyms; i++){
		if(select_symbol(all_symbols + (m.iextdefsym + i), cmd_flags,
				    process_flags)){
		    selected_symbols[(*nsymbols)++] =
				all_symbols[m.iextdefsym + i];
		    selectCounter++;
		}
	    }
	    for(i = 0; i < m.nlocalsym && m.ilocalsym + i < st->nsyms; i++){
		if(select_symbol(all_symbols + (m.ilocalsym + i), cmd_flags,
					process_flags))
		    selected_symbols[(*nsymbols)++] =
				all_symbols[m.ilocalsym + i];
		}
	}
	else if(cmd_flags->b == TRUE){
	    found = FALSE;
	    strings = ofile->object_addr + st->stroff;
	    if(cmd_flags->i == TRUE)
		    i = cmd_flags->index;
	    else
		    i = 0;
	    for( ; i < st->nsyms; i++){
		if(all_symbols[i].n_type == N_BINCL &&
		   all_symbols[i].n_un.n_strx != 0 &&
		   (unsigned long)all_symbols[i].n_un.n_strx < st->strsize &&
		   strcmp(cmd_flags->bincl_name,
		   strings + all_symbols[i].n_un.n_strx) == 0){
		    selected_symbols[(*nsymbols)++] = all_symbols[i];
		    found = TRUE;
		    nest = 0;
		    for(i = i + 1 ; i < st->nsyms; i++){
			if(all_symbols[i].n_type == N_BINCL)
			    nest++;
			else if(all_symbols[i].n_type == N_EINCL){
			    if(nest == 0){
				selected_symbols[(*nsymbols)++] =
							all_symbols[i];
				break;
			    }
			    nest--;
			}
			else if(nest == 0)
			    selected_symbols[(*nsymbols)++] = all_symbols[i];
		    }
		}
		if(found == TRUE)
		    break;
	    }
	}
	else{
	    for(i = 0; i < st->nsyms; i++){
		if(select_symbol(all_symbols + i, cmd_flags, process_flags)){
		    selected_symbols[(*nsymbols)++] = all_symbols[i];
		}
	    }
	}
	/*
	 * Could reallocate selected symbols to the exact size but it is more
	 * of a time waste than a memory savings.
	 */
	return(selected_symbols);
}

/*
 * select_symbol() returns TRUE or FALSE if the specified symbol is to be
 * printed based on the flags.
 */

static enum bool
select_symbol(struct nlist *symbol,
	      struct cmd_flags *cmd_flags,
	      struct process_flags *process_flags) {
    if((cmd_flags->import == TRUE) && (process_flags->nlibs > 0)
       && GET_LIBRARY_ORDINAL(symbol->n_desc) == 0)
	return(FALSE);
    
    if(cmd_flags->u == TRUE){
	if((symbol->n_type == (N_UNDF | N_EXT) && symbol->n_value == 0) ||
	    symbol->n_type == (N_PBUD | N_EXT))
	    return(TRUE);
	else
	    return(FALSE);
    }
    if(cmd_flags->d == TRUE){
	if((((symbol->n_type & N_TYPE) != N_UNDF)
		&& ((symbol->n_type & N_EXT) != 0))
		&& ((symbol->n_type & N_TYPE) != N_PBUD))
	    return(TRUE);
	else
	    return(FALSE);
    }
    if(cmd_flags->g == TRUE && (symbol->n_type & N_EXT) == 0)
	return(FALSE);
    if(cmd_flags->s == TRUE){
	if(((symbol->n_type & N_TYPE) == N_SECT) &&
	   (symbol->n_sect == process_flags->nsect)){
	    if(cmd_flags->l &&  symbol->n_value == process_flags->sect_addr){
		process_flags->sect_start_symbol = TRUE;
	    }
	}
	else
	    return(FALSE);
    }
    if((symbol->n_type & N_STAB) &&
	(cmd_flags->a == FALSE || cmd_flags->g == TRUE ||
	cmd_flags->u == TRUE))
	    return(FALSE);
    return(TRUE);
}

