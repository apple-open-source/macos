/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2005-2007 Apple Inc. All rights reserved.
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
		LibraryOptions			options;
	};

	class Writer : public ObjectFile::Reader
	{
	public:
		virtual						~Writer() {};

		virtual const char*								getPath() = 0;
		virtual std::vector<class ObjectFile::Atom*>&	getAtoms() = 0;
		virtual std::vector<class ObjectFile::Atom*>*	getJustInTimeAtomsFor(const char* name) = 0;
		virtual ObjectFile::Atom&						makeObjcInfoAtom(ObjectFile::Reader::ObjcConstraint objcContraint, 
																		bool objcReplacementClasses) = 0;
		virtual class ObjectFile::Atom*					getUndefinedProxyAtom(const char* name) = 0;
		virtual uint64_t								write(std::vector<class ObjectFile::Atom*>& atoms,
															  std::vector<class ObjectFile::Reader::Stab>& stabs,
															  class ObjectFile::Atom* entryPointAtom,
															  class ObjectFile::Atom* dyldClassicHelperAtom,
															  class ObjectFile::Atom* dyldCompressedHelperAtom,
															  class ObjectFile::Atom* dyldLazyDylibHelperAtom,
															  bool createUUID, bool canScatter,
															  ObjectFile::Reader::CpuConstraint cpuConstraint,
															  bool biggerThanTwoGigs,
															  std::set<const class ObjectFile::Atom*>& atomsThatOverrideWeak,
															  bool hasExternalWeakDefinitions) = 0;

	protected:
									Writer(std::vector<DyLibUsed>&) {};
	};

};

#endif // __EXECUTABLEFILE__
