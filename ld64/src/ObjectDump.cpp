/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*- 
 *
 * Copyright (c) 2005-2006 Apple Computer, Inc. All rights reserved.
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

#include "MachOReaderRelocatable.hpp"

static bool			sDumpContent= true;
static bool			sDumpStabs	= false;
static bool			sSort		= true;
static cpu_type_t	sPreferredArch = CPU_TYPE_POWERPC64;
static const char* sMatchName;
static int sPrintRestrict;
static int sPrintAlign;
static int sPrintName;


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

static void dumpStabs(std::vector<ObjectFile::Reader::Stab>* stabs)
{
	// debug info
	printf("stabs: (%lu)\n", stabs->size());
	for (std::vector<ObjectFile::Reader::Stab>::iterator it = stabs->begin(); it != stabs->end(); ++it ) {
		ObjectFile::Reader::Stab& stab = *it;
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
			case N_OSO:
				code = "  OSO";
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
		printf("  [atom=%20s] %02X %04X %s %s\n", ((stab.atom != NULL) ? stab.atom->getDisplayName() : ""), stab.other, stab.desc, code, stab.string);
	}
}


static void dumpAtom(ObjectFile::Atom* atom)
{
	if(sMatchName && strcmp(sMatchName, atom->getDisplayName()))
		return;

	//printf("atom:    %p\n", atom);
	
	// name
	if(!sPrintRestrict || sPrintName)
		printf("name:    %s\n",  atom->getDisplayName());
	
	// scope
	if(!sPrintRestrict)
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
	
	// kind
	if(!sPrintRestrict)
		switch ( atom->getDefinitionKind() ) {
		case ObjectFile::Atom::kRegularDefinition:
			printf("kind:     regular\n");
			break;
		case ObjectFile::Atom::kWeakDefinition:
			printf("kind:     weak\n");
			break;
		case ObjectFile::Atom::kTentativeDefinition:
			printf("kind:     tentative\n");
			break;
		case ObjectFile::Atom::kExternalDefinition:
			printf("kind:     import\n");
			break;
		case ObjectFile::Atom::kExternalWeakDefinition:
			printf("kind:     weak import\n");
			break;
		case ObjectFile::Atom::kAbsoluteSymbol:
			printf("kind:     absolute symbol\n");
			break;
		default:
			printf("kind:   unknown\n");
		}

	// segment and section
	if(!sPrintRestrict)
		printf("section: %s,%s\n", atom->getSegment().getName(), atom->getSectionName());

	// attributes
	if(!sPrintRestrict) {
		printf("attrs:   ");
		if ( atom->dontDeadStrip() )
			printf("dont-dead-strip ");
		if ( atom->isZeroFill() )
			printf("zero-fill ");
		printf("\n");
	}
	
	// size
	if(!sPrintRestrict)
		printf("size:    0x%012llX\n", atom->getSize());
	
	// alignment
	if(!sPrintRestrict || sPrintAlign)
		printf("align:    %u mod %u\n", atom->getAlignment().modulus, (1 << atom->getAlignment().powerOf2) );

	// content
	if (!sPrintRestrict && sDumpContent ) { 
		uint64_t size = atom->getSize();
		if ( size < 4096 ) {
			uint8_t content[size];
			atom->copyRawContent(content);
			printf("content: ");
			if ( strcmp(atom->getSectionName(), "__cstring") == 0 ) {
				printf("\"");
				for (unsigned int i=0; i < size; ++i) {
					if(content[i]<'!' || content[i]>=127)
						printf("\\%o", content[i]);
					else
						printf("%c", content[i]);
				}
				printf("\"");
			}
			else {
				for (unsigned int i=0; i < size; ++i)
					printf("%02X ", content[i]);
			}
		}
		printf("\n");
	}
	
	// references
	if(!sPrintRestrict) {
		std::vector<ObjectFile::Reference*>&  references = atom->getReferences();
		const int refCount = references.size();
		printf("references: (%u)\n", refCount);
		for (int i=0; i < refCount; ++i) {
			ObjectFile::Reference* ref = references[i];
			printf("   %s\n", ref->getDescription());
		}
	}
	
	// line info
	if(!sPrintRestrict) {
		std::vector<ObjectFile::LineInfo>* lineInfo = atom->getLineInfo();
		if ( (lineInfo != NULL) && (lineInfo->size() > 0) ) {
			printf("line info: (%lu)\n", lineInfo->size());
			for (std::vector<ObjectFile::LineInfo>::iterator it = lineInfo->begin(); it != lineInfo->end(); ++it) {
				printf("   offset 0x%04X, line %d, file %s\n", it->atomOffset, it->lineNumber, it->fileName);
			}
		}
	}

	if(!sPrintRestrict)
		printf("\n");
}

struct AtomSorter
{
     bool operator()(const ObjectFile::Atom* left, const ObjectFile::Atom* right)
     {
		if ( left == right )
			return false;
        return (strcmp(left->getDisplayName(), right->getDisplayName()) < 0);
     }
};


static void dumpFile(ObjectFile::Reader* reader)
{
	// stabs debug info
	if ( sDumpStabs && (reader->getDebugInfoKind() == ObjectFile::Reader::kDebugInfoStabs) ) {
		std::vector<ObjectFile::Reader::Stab>* stabs = reader->getStabs();
		if ( stabs != NULL )
			dumpStabs(stabs);
	}
	
	// get all atoms
	std::vector<ObjectFile::Atom*> atoms = reader->getAtoms();
	
	// make copy of vector and sort (so output is canonical)
	std::vector<ObjectFile::Atom*> sortedAtoms(atoms);
	if ( sSort )
		std::sort(sortedAtoms.begin(), sortedAtoms.end(), AtomSorter());
	
	for(std::vector<ObjectFile::Atom*>::iterator it=sortedAtoms.begin(); it != sortedAtoms.end(); ++it)
		dumpAtom(*it);
}


static ObjectFile::Reader* createReader(const char* path, const ObjectFile::ReaderOptions& options)
{
	struct stat stat_buf;
	
	int fd = ::open(path, O_RDONLY, 0);
	if ( fd == -1 )
		throwf("cannot open file: %s", path);
	::fstat(fd, &stat_buf);
	uint8_t* p = (uint8_t*)::mmap(NULL, stat_buf.st_size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
	::close(fd);
	const mach_header* mh = (mach_header*)p;
	if ( mh->magic == OSSwapBigToHostInt32(FAT_MAGIC) ) {
		const struct fat_header* fh = (struct fat_header*)p;
		const struct fat_arch* archs = (struct fat_arch*)(p + sizeof(struct fat_header));
		for (unsigned long i=0; i < OSSwapBigToHostInt32(fh->nfat_arch); ++i) {
			if ( OSSwapBigToHostInt32(archs[i].cputype) == (uint32_t)sPreferredArch ) {
				p = p + OSSwapBigToHostInt32(archs[i].offset);
				mh = (struct mach_header*)p;
			}
		}
	}
	if ( mach_o::relocatable::Reader<x86>::validFile(p) )
		return new mach_o::relocatable::Reader<x86>::Reader(p, path, 0, options, 0);
	else if ( mach_o::relocatable::Reader<ppc>::validFile(p) )
		return new mach_o::relocatable::Reader<ppc>::Reader(p, path, 0, options, 0);
	else if ( mach_o::relocatable::Reader<ppc64>::validFile(p) )
		return new mach_o::relocatable::Reader<ppc64>::Reader(p, path, 0, options, 0);
	else if ( mach_o::relocatable::Reader<x86_64>::validFile(p) )
		return new mach_o::relocatable::Reader<x86_64>::Reader(p, path, 0, options, 0);
	throwf("not a mach-o object file: %s", path);
}

static
void
usage()
{
	fprintf(stderr, "ObjectDump options:\n"
			"\t-no_content\tdon't dump contents\n"
			"\t-stabs\t\tdump stabs\n"
			"\t-arch aaa\tonly dump info about arch aaa\n"
			"\t-only sym\tonly dump info about sym\n"
			"\t-align\t\tonly print alignment info\n"
			"\t-name\t\tonly print symbol names\n"
		);
}

int main(int argc, const char* argv[])
{
	if(argc<2) {
		usage();
		return 0;
	}

	ObjectFile::ReaderOptions options;
	try {
		for(int i=1; i < argc; ++i) {
			const char* arg = argv[i];
			if ( arg[0] == '-' ) {
				if ( strcmp(arg, "-no_content") == 0 ) {
					sDumpContent = false;
				}
				else if ( strcmp(arg, "-stabs") == 0 ) {
					sDumpStabs = true;
				}
				else if ( strcmp(arg, "-no_sort") == 0 ) {
					sSort = false;
				}
				else if ( strcmp(arg, "-arch") == 0 ) {
					const char* arch = ++i<argc? argv[i]: "";
					if ( strcmp(arch, "ppc64") == 0 )
						sPreferredArch = CPU_TYPE_POWERPC64;
					else if ( strcmp(arch, "ppc") == 0 )
						sPreferredArch = CPU_TYPE_POWERPC;
					else if ( strcmp(arch, "i386") == 0 )
						sPreferredArch = CPU_TYPE_I386;
					else if ( strcmp(arch, "x86_64") == 0 )
						sPreferredArch = CPU_TYPE_X86_64;
					else 
						throwf("unknown architecture %s", arch);
				}
				else if ( strcmp(arg, "-only") == 0 ) {
					sMatchName = ++i<argc? argv[i]: NULL;
				}
				else if ( strcmp(arg, "-align") == 0 ) {
					sPrintRestrict = true;
					sPrintAlign = true;
				}
				else if ( strcmp(arg, "-name") == 0 ) {
					sPrintRestrict = true;
					sPrintName = true;
				}
				else {
					usage();
					throwf("unknown option: %s\n", arg);
				}
			}
			else {
				ObjectFile::Reader* reader = createReader(arg, options);
				dumpFile(reader);
			}
		}
	}
	catch (const char* msg) {
		fprintf(stderr, "ObjDump failed: %s\n", msg);
		return 1;
	}
	
	return 0;
}
