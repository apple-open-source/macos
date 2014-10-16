/*
 * Copyright (c) 2000-2001,2003-2004,2011,2014 Apple Inc. All Rights Reserved.
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


//
// cssm module loader interface - MACOS X (CFBundle/DYLD) version.
//
// This file provides a C++-style interface to CFBundles as managed by the CF-style
// system interfaces. The implementation looks a bit, well, hybrid - but the visible
// interfaces are pure C++.
//
#include "modloader.h"
#include "modload_plugin.h"
# include "modload_static.h"


namespace Security {


//
// Pull in functions for built-in plugin modules
//
#define BUILTIN(suffix) \
	extern "C" CSSM_SPI_ModuleLoadFunction CSSM_SPI_ModuleLoad ## suffix; \
	extern "C" CSSM_SPI_ModuleUnloadFunction CSSM_SPI_ModuleUnload ## suffix; \
	extern "C" CSSM_SPI_ModuleAttachFunction CSSM_SPI_ModuleAttach ## suffix; \
	extern "C" CSSM_SPI_ModuleDetachFunction CSSM_SPI_ModuleDetach ## suffix; \
	static const PluginFunctions builtin ## suffix = { \
		CSSM_SPI_ModuleLoad ## suffix, CSSM_SPI_ModuleUnload ## suffix, \
		CSSM_SPI_ModuleAttach ## suffix, CSSM_SPI_ModuleDetach ## suffix \
	};

BUILTIN(__apple_csp)
BUILTIN(__apple_file_dl)
BUILTIN(__apple_cspdl)
BUILTIN(__apple_x509_cl)
BUILTIN(__apple_x509_tp)
BUILTIN(__sd_cspdl)


//
// Construct the canonical ModuleLoader object
//
ModuleLoader::ModuleLoader()
{
#if !defined(NO_BUILTIN_PLUGINS)
    mPlugins["*AppleCSP"] = new StaticPlugin(builtin__apple_csp);
    mPlugins["*AppleDL"] = new StaticPlugin(builtin__apple_file_dl);
    mPlugins["*AppleCSPDL"] = new StaticPlugin(builtin__apple_cspdl);
    mPlugins["*AppleX509CL"] = new StaticPlugin(builtin__apple_x509_cl);
    mPlugins["*AppleX509TP"] = new StaticPlugin(builtin__apple_x509_tp);
    mPlugins["*SDCSPDL"] = new StaticPlugin(builtin__sd_cspdl);
#endif //NO_BUILTIN_PLUGINS
}


//
// "Load" a plugin, given its MDS path. At this layer, we are performing
// a purely physical load operation. No code in the plugin is called.
// If "built-in plugins" are enabled, the moduleTable will come pre-initialized
// with certain paths. Since we consult this table before going to disk, this
// means that we'll pick these up first *as long as the paths match exactly*.
// There is nothing magical in the path strings themselves, other than by
// convention. (The convention is "*NAME", which conveniently does not match
// any actual file path.)
//
Plugin *ModuleLoader::operator () (const string &path)
{
    Plugin * &plugin = mPlugins[path];
    if (!plugin) {
		secdebug("cssm", "ModuleLoader(): creating plugin %s", path.c_str());
        plugin = new LoadablePlugin(path.c_str());
	}
	else {
		secdebug("cssm", "ModuleLoader(): FOUND plugin %s, isLoaded %s", 
			path.c_str(), plugin->isLoaded() ? "TRUE" : "FALSE");
		if(!plugin->isLoaded()) {
			plugin->load();
		}
	}
    return plugin;
}


}	// end namespace Security
