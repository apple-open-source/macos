#define __BSD_VISIBLE 1
#include "dlfcn.h"
#include <stdlib.h>
#include <stdio.h>
#include <mach-o/dyld.h>

int main(int argc, const char* argv[])
{
	int retCode = 0;
	const char * syms[] = {"_printf","_dlopen","_main",0};
	int i=0;
	struct dl_info info;
	NSSymbol syml;
	void* addr;
	while (syms[i]) {
		syml = NSLookupAndBindSymbol(syms[i]);
		if (syml)
		{
			addr = NSAddressOfSymbol(syml);
			dladdr(addr,&info);
			fprintf(stdout,"Symbol: %s\nNSSym: %x\nAddress: %x\nFName: %s\nBase: %x\nSymbol: %s\nAddress: %x\n\n\n",
				syms[i],syml,addr,info.dli_fname,info.dli_fbase,info.dli_sname,info.dli_saddr);
			if (addr != info.dli_saddr) retCode++;
			if (strcmp(syms[i],info.dli_sname)) retCode++;
		}
		i++;
	}
	return retCode;
}