/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// modload_plugin - loader interface for dynamically loaded plugin modules
//
#include <Security/modload_plugin.h>


namespace Security {


//
// During construction, a LoadablePlugin loads itself into memory and locates
// the canonical (CDSA defined) four entrypoints. If anything fails, we throw.
//    
LoadablePlugin::LoadablePlugin(const char *path) : LoadableBundle(path)
{
    load();
    findFunction(fLoad, "CSSM_SPI_ModuleLoad");
    findFunction(fAttach, "CSSM_SPI_ModuleAttach");
    findFunction(fDetach, "CSSM_SPI_ModuleDetach");
    findFunction(fUnload, "CSSM_SPI_ModuleUnload");
}


//
// Loading and unloading devolves directly onto LoadableBundle
//
void LoadablePlugin::load()
{
    CodeSigning::LoadableBundle::load();
}

void LoadablePlugin::unload()
{
    CodeSigning::LoadableBundle::unload();
}

bool LoadablePlugin::isLoaded() const
{
    return CodeSigning::LoadableBundle::isLoaded();
}


}	// end namespace Security
