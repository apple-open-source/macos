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
// cssm module loader interface - MACOS X (CFBundle/DYLD) version.
//
// This file provides a C++-style interface to CFBundles as managed by the CF-style
// system interfaces. The implementation looks a bit, well, hybrid - but the visible
// interfaces are pure C++.
//
#include "modloader.h"
#include "modload_plugin.h"

#if defined(BUILTIN_PLUGINS)
# include "modload_static.h"
# include "AppleCSP/AppleCSP/AppleCSP.h"
# include "AppleCSPDL/CSPDLPlugin.h"
# include "AppleDL/AppleFileDL.h"
# include "AppleX509CL/AppleX509CL.h"
# include "AppleX509TP/AppleTP.h"
#endif //BUILTIN_PLUGINS


namespace Security {


//
// Construct a ModuleLoader object.
//
ModuleLoader::ModuleLoader()
{
#if defined(BUILTIN_PLUGINS)
    mPlugins["*AppleCSP"] = new StaticPlugin<AppleCSPPlugin>;
    mPlugins["*AppleDL"] = new StaticPlugin<AppleFileDL>;
    mPlugins["*AppleCSPDL"] = new StaticPlugin<CSPDLPlugin>;
    mPlugins["*AppleX509CL"] = new StaticPlugin<AppleX509CL>;
    mPlugins["*AppleX509TP"] = new StaticPlugin<AppleTP>;
#endif //BUILTIN_PLUGINS
}


//
// "Load" a plugin, given its MDS path. At this layer, we are performing
// a purely physical load operation. No code in the plugin is called.
// If "built-in plugins" are enabled, the moduleTable will come pre-initialized
// with certain paths. Since we consult this table before going to disk, this
// means that we'll pick these up first *as long as the paths match exactly*.
// There is nothing magical in the path strings themselves, other than by
// convention.
//
Plugin *ModuleLoader::operator () (const char *path)
{
    Plugin * &plugin = mPlugins[path];
    if (!plugin)
        plugin = new LoadablePlugin(path);
    return plugin;
}


}	// end namespace Security
