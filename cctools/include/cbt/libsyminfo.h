#include <mach/mach.h>

struct selectedSymbolListInfo {
	vm_size_t mappedFileSize;
	void *mappedFile;
	char *fileName;
	char *cachedFileName;
	int byteSex;
	struct nlist *all_symbols;
	struct symtab_command *st;
};

#ifndef __SymInfoTypes__
typedef void *SymInfoList;
typedef void *SymInfoSymbol;
typedef void *SymInfoDependencies;
#endif // __SymInfoTypes__

/* Creates a SymInfoList structure from a binary */
SymInfoList SymInfoCreate(char *fileName);

/*Access to main structures in the SymInfoList */
SymInfoSymbol *SymInfoGetImports(SymInfoList nmList);
SymInfoSymbol *SymInfoGetExports(SymInfoList nmList);
SymInfoDependencies SymInfoGetLibraryInfo(SymInfoList nmList);
char *SymInfoGetShortName(SymInfoList nmList);

/* Access to data inside the SymInfoSymbol type */
char *SymInfoGetSymbolName(SymInfoSymbol symbol);
const char *SymInfoGetSymbolArch(SymInfoSymbol symbol);
char *SymInfoGetSymbolOrdinal(SymInfoSymbol symbol);
unsigned int SymInfoGetExportCount(SymInfoList nmList);
unsigned int SymInfoGetImportCount(SymInfoList nmList);

/* Access to data inside the SymInfoDependencies type */
char **SymInfoGetSubFrameworks(SymInfoDependencies deps);
char **SymInfoGetSubUmbrellas(SymInfoDependencies deps);
unsigned int SymInfoGetSubUmbrellaCount(SymInfoDependencies deps);
unsigned int SymInfoGetSubFrameworkCount(SymInfoDependencies deps);

/* Functions for freeing the SymInfoList structure */
void SymInfoFree(SymInfoList nmList);
void SymInfoFreeSymbols(SymInfoSymbol symbol);
void SymInfoFreeDependencies(SymInfoDependencies deps);

/* Function for creating SymInfoSymbol */
SymInfoSymbol SymInfoCreateSymbols(char *name,
			  	    char *arch,
			 	    char *ordinal);

/* Function for creating SymInfoDependencies */
SymInfoDependencies SymInfoCreateDependencies(char **subUmbrellas,
				 	      char **subFrameworks,
				    	      int nSubUmbrellas,
				  	      int nSubFrameworks);
