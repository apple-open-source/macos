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
// modload_static - pseudo-loading of statically linked plugins
//
#ifndef _H_MODLOAD_STATIC
#define _H_MODLOAD_STATIC

#include "modloader.h"
#include "cssmint.h"
#include <Security/utilities.h>
#include <Security/callback.h>


namespace Security {


template <class PluginClass>
class StaticPlugin : public Plugin {
public:    
    void load()				{ }
    void unload()			{ }
    bool isLoaded() const	{ return true; }
    
#define SPIPREFIX		/* empty */
#include <Security/pluginspi.h>

private:
    PluginClass mPlugin;
    
    PluginClass &plugin()		{ return mPlugin; }
};


} // end namespace Security


#endif //_H_MODLOAD_STATIC
