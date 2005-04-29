/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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


#ifndef __EXECUTABLEFILE__
#define __EXECUTABLEFILE__

#include <stdint.h>
#include <vector>

#include "ObjectFile.h"
#include "Options.h"


namespace ExecutableFile {

	struct DyLibUsed 
	{
		ObjectFile::Reader*		reader;
		DynamicLibraryOptions	options;
		bool					indirect;		// library found indirect.  Do not make load command
		ObjectFile::Reader*		directReader;	// direct library which re-exports this library
	};
	
	class Writer : public ObjectFile::Reader
	{
	public:		
		virtual								~Writer() {};
		
		virtual const char*								getPath() = 0;
		virtual std::vector<class ObjectFile::Atom*>&	getAtoms() = 0;
		virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name) = 0;
		virtual std::vector<ObjectFile::StabsInfo>*		getStabsDebugInfo() = 0;

		virtual class ObjectFile::Atom*					getUndefinedProxyAtom(const char* name) = 0;
		virtual void									write(std::vector<class ObjectFile::Atom*>& atoms, class ObjectFile::Atom* entryPointAtom) = 0;



	protected:
											Writer(std::vector<DyLibUsed>&) {};
	};

};




#endif // __EXECUTABLEFILE__



