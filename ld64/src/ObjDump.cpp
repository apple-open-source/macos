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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <fcntl.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/stab.h>


#include "ObjectFile.h"
#include "ObjectFileMachO-all.h"

 __attribute__((noreturn))
void throwf(const char* format, ...) 
{
	va_list	list;
	char*	p;
	va_start(list, format);
	vasprintf(&p, format, list);
	va_end(list);
	
	const char*	t = p;
	throw t;
}

static void dumpStabs(std::vector<ObjectFile::StabsInfo>* stabs)
{
	// debug info
	const int stabCount = stabs->size();
	printf("stabs: (%u)\n", stabCount);
	for (int i=0; i < stabCount; ++i) {
		ObjectFile::StabsInfo& stab = (*stabs)[i];
		const char* code = "?????";
		switch (stab.type) {
			case N_GSYM:
				code = " GSYM";
				break;
			case N_FNAME:
				code = "FNAME";
				break;
			case N_FUN:
				code = "  FUN";
				break;
			case N_STSYM:
				code = "STSYM";
				break;
			case N_LCSYM:
				code = "LCSYM";
				break;
			case N_BNSYM:
				code = "BNSYM";
				break;
			case N_OPT:
				code = "  OPT";
				break;
			case N_RSYM:
				code = " RSYM";
				break;
			case N_SLINE:
				code = "SLINE";
				break;
			case N_ENSYM:
				code = "ENSYM";
				break;
			case N_SSYM:
				code = " SSYM";
				break;
			case N_SO:
				code = "   SO";
				break;
			case N_LSYM:
				code = " LSYM";
				break;
			case N_BINCL:
				code = "BINCL";
				break;
			case N_SOL:
				code = "  SOL";
				break;
			case N_PARAMS:
				code = "PARMS";
				break;
			case N_VERSION:
				code = " VERS";
				break;
			case N_OLEVEL:
				code = "OLEVL";
				break;
			case N_PSYM:
				code = " PSYM";
				break;
			case N_EINCL:
				code = "EINCL";
				break;
			case N_ENTRY:
				code = "ENTRY";
				break;
			case N_LBRAC:
				code = "LBRAC";
				break;
			case N_EXCL:
				code = " EXCL";
				break;
			case N_RBRAC:
				code = "RBRAC";
				break;
			case N_BCOMM:
				code = "BCOMM";
				break;
			case N_ECOMM:
				code = "ECOMM";
				break;
			case N_LENG:
				code =  "LENG";
				break;
		}
		printf("   %08X %02X %04X %s %s\n", (uint32_t)stab.atomOffset, stab.other, stab.desc, code, stab.string);
	}
}


static void dumpAtom(ObjectFile::Atom* atom)
{
	//printf("atom:    %p\n", atom);
	
	// name
	printf("name:    %s\n",  atom->getDisplayName());
	
	// scope
	switch ( atom->getScope() ) {
		case ObjectFile::Atom::scopeTranslationUnit:
			printf("scope:   translation unit\n");
			break;
		case ObjectFile::Atom::scopeLinkageUnit:
			printf("scope:   linkage unit\n");
			break;
		case ObjectFile::Atom::scopeGlobal:
			printf("scope:   global\n");
			break;
		default:
			printf("scope:   unknown\n");
	}
	
	// segment and section
	printf("section: %s,%s\n", atom->getSegment().getName(), atom->getSectionName());

	// attributes
	printf("attrs:   ");
	if ( atom->isWeakDefinition() )
		printf("weak ");
	if ( atom->isCoalesableByName() )
		printf("coalesce-by-name ");
	if ( atom->isCoalesableByValue() )
		printf("coalesce-by-value ");
	if ( atom->dontDeadStrip() )
		printf("dont-dead-strip ");
	if ( atom->isZeroFill() )
		printf("zero-fill ");
	printf("\n");
	
	// size
	printf("size:    0x%012llX\n", atom->getSize());
	
	// alignment
	printf("align:    %d\n", atom->getAlignment());

	// content
	uint64_t size = atom->getSize();
	if ( size < 4096 ) {
		uint8_t content[size];
		atom->copyRawContent(content);
		printf("content: ");
		if ( strcmp(atom->getSectionName(), "__cstring") == 0 ) {
			printf("\"%s\"", content);
		}
		else {
			for (unsigned int i=0; i < size; ++i)
				printf("%02X ", content[i]);
		}
	}
	printf("\n");
	
	// references
	std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
	const int refCount = references.size();
	printf("references: (%u)\n", refCount);
	for (int i=0; i < refCount; ++i) {
		ObjectFile::Reference* ref = references[i];
		printf("   %s\n", ref->getDescription());
	}
	
	// debug info
	std::vector<ObjectFile::StabsInfo>* stabs = atom->getStabsDebugInfo();
	if ( stabs != NULL )
		dumpStabs(stabs);
}


static void dumpFile(ObjectFile::Reader* reader)
{
#if 0
	// debug info
	std::vector<ObjectFile::StabsInfo>* stabs = reader->getStabsDebugInfo();
	if ( stabs != NULL )
		dumpStabs(stabs);
#endif
	// atom content
	std::vector<ObjectFile::Atom*> atoms = reader->getAtoms();
	const int atomCount = atoms.size();
	for(int i=0; i < atomCount; ++i) {
		dumpAtom(atoms[i]);
		printf("\n");
	}
}


static ObjectFile::Reader* createReader(const char* path, const ObjectFile::ReaderOptions& options)
{
	struct stat stat_buf;
	
	int fd = ::open(path, O_RDONLY, 0);
	if ( fd == -1 )
		throw "cannot open file";
	::fstat(fd, &stat_buf);
	char* p = (char*)::mmap(NULL, stat_buf.st_size, PROT_READ, MAP_FILE, fd, 0);
	::close(fd);
	const mach_header* mh = (mach_header*)p;
	if ( mh->magic == OSSwapBigToHostInt32(FAT_MAGIC) ) {
		const struct fat_header* fh = (struct fat_header*)p;
		const struct fat_arch* archs = (struct fat_arch*)(p + sizeof(struct fat_header));
		for (unsigned long i=0; i < fh->nfat_arch; ++i) {
			if ( archs[i].cputype == CPU_TYPE_POWERPC64 ) {
				p = p + archs[i].offset;
				mh = (struct mach_header*)p;
			}
		}
	}
	if ( mh->magic == MH_MAGIC ) {
		if ( mh->filetype == MH_OBJECT ) {
			switch ( mh->cputype ) {
				case CPU_TYPE_I386:
					return i386::ObjectFileMachO::MakeReader((class i386::macho_header*)mh, path, options);
				case CPU_TYPE_POWERPC:
					return ppc::ObjectFileMachO::MakeReader((class ppc::macho_header*)mh, path, options);
				case CPU_TYPE_POWERPC64:
					return ppc64::ObjectFileMachO::MakeReader((class ppc64::macho_header*)mh, path, options);
				default:
					throw "unknown mach-o cpu type";
			}
		}
		if ( mh->filetype == MH_DYLIB )
			return ppc::ObjectFileDylibMachO::MakeReader((class ppc::macho_header*)mh, path, options);
		throw "unknown mach-o file type";
	}
	else if ( mh->magic == MH_MAGIC_64 ) {
		if ( mh->filetype == MH_OBJECT )
			return ppc64::ObjectFileMachO::MakeReader((class ppc64::macho_header*)mh, path, options);
		if ( mh->filetype == MH_DYLIB )
			return ppc64::ObjectFileDylibMachO::MakeReader((class ppc64::macho_header*)mh, path, options);
		throw "unknown mach-o file type";
	}
	else if ( mh->magic == OSSwapInt32(MH_MAGIC) ) {
		if ( mh->filetype == OSSwapInt32(MH_OBJECT) ) {
			switch ( OSSwapInt32(mh->cputype) ) {
				case CPU_TYPE_I386:
					return i386::ObjectFileMachO::MakeReader((class i386::macho_header*)mh, path, options);
				case CPU_TYPE_POWERPC:
					return ppc::ObjectFileMachO::MakeReader((class ppc::macho_header*)mh, path, options);
				case CPU_TYPE_POWERPC64:
					return ppc64::ObjectFileMachO::MakeReader((class ppc64::macho_header*)mh, path, options);
				default:
					throw "unknown mach-o cpu type";
			}
		}
		if ( mh->filetype == OSSwapInt32(MH_DYLIB) )
			return ppc::ObjectFileDylibMachO::MakeReader((class ppc::macho_header*)mh, path, options);
		throw "unknown mach-o file type";
	}
	else if ( mh->magic == OSSwapInt32(MH_MAGIC_64) ) {
		if ( mh->filetype == OSSwapInt32(MH_OBJECT)  )
			return ppc64::ObjectFileMachO::MakeReader((class ppc64::macho_header*)mh, path, options);
		if ( mh->filetype == OSSwapInt32(MH_DYLIB) )
			return ppc64::ObjectFileDylibMachO::MakeReader((class ppc64::macho_header*)mh, path, options);
		throw "unknown mach-o file type";
	}
	throw "unknown file type";
}


int main(int argc, const char* argv[])
{
	ObjectFile::ReaderOptions options;
	//const char* path = argv[1];
	//ObjectFile::Reader* reader = ObjectFile::Reader::createReader(path);
	try {
		ObjectFile::Reader* reader = createReader("/tmp/gcov-1.o", options);

		dumpFile(reader);
	}
	catch (const char* msg) {
		fprintf(stderr, "ObjDump failed: %s\n", msg);
	}
	
	return 0;
}



