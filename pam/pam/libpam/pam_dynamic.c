#include "_pam_aconf.h"
#ifdef PAM_SHL
# include <dl.h>
#elif defined(PAM_DYLD)
# include <mach-o/dyld.h>
#else /* PAM_SHL */
# include <dlfcn.h>
#endif /* PAM_SHL */

#include "pam_private.h"

#ifndef SHLIB_SYM_PREFIX
#define SHLIB_SYM_PREFIX "_"
#endif

void *_pam_dlopen(const char *mod_path)
{
#ifdef PAM_SHL
	return shl_load(mod_path, BIND_IMMEDIATE, 0L);
#elif defined(PAM_DYLD)
	NSObjectFileImage ofile;
	void *ret = NULL;

	if (NSCreateObjectFileImageFromFile(mod_path, &ofile) != 
			NSObjectFileImageSuccess ) 
		return NULL;

	ret = NSLinkModule(ofile, mod_path, NSLINKMODULE_OPTION_PRIVATE | NSLINKMODULE_OPTION_BINDNOW);
	NSDestroyObjectFileImage(ofile);

	return ret;
#else
	return dlopen(mod_path, RTLD_NOW);
#endif
}

servicefn _pam_dlsym(void *handle, const char *symbol) 
{
#ifdef PAM_SHL
	char *_symbol = NULL;
	servicefn ret;

	if( symbol == NULL )
		return NULL;

	if( shl_findsym(&handle, symbol, (short) TYPE_PROCEDURE, &ret ){
		_symbol = malloc( strlen(symbol) + sizeof(SHLIB_SYM_PREFIX) + 1 );
		if( _symbol == NULL )
			return NULL;
		strcpy(_symbol, SHLIB_SYM_PREFIX);
		strcat(_symbol, symbol);
		if( shl_findsym(&handle, _symbol, 
				(short) TYPE_PROCEDURE, &ret ){
			free(_symbol);
			return NULL;
		}
		free(_symbol);
	}

	return ret;
	
#elif defined(PAM_DYLD)
	NSSymbol nsSymbol;
	char *_symbol;

	if( symbol == NULL )
		return NULL;
	_symbol = malloc( strlen(symbol) + 2 );
	if( _symbol == NULL )
		return NULL;
	strcpy(_symbol, SHLIB_SYM_PREFIX);
	strcat(_symbol, symbol);

	nsSymbol = NSLookupSymbolInModule(handle, _symbol);
	if( nsSymbol == NULL )
		return NULL;
	free(_symbol);

	return (servicefn)NSAddressOfSymbol(nsSymbol);
#else
	return (servicefn) dlsym(handle, symbol);
#endif
}

void _pam_dlclose(void *handle)
{
#ifdef PAM_SHL
	shl_unload(handle);
#elif defined(PAM_DYLD)
	NSUnLinkModule((NSModule)handle, NSUNLINKMODULE_OPTION_NONE);
#else
	dlclose(handle);
#endif

	return;
}
