//
//  gkunpack - an ad-hoc tool for unpacking certain binary data from a detached code signature
//
//	gkunpack <detached_signature_data >prescreen_filter_data
//
#include <security_utilities/macho++.h>
#include <security_codesigning/codedirectory.h>
#include <security_codesigning/sigblob.h>

using namespace CodeSigning;


int main(int argc, const char * argv[])
{
	if (const EmbeddedSignatureBlob *top = (const EmbeddedSignatureBlob *)BlobCore::readBlob(stdin)) {
		if (top->magic() == DetachedSignatureBlob::typeMagic) {	// multiple architectures - pick the native one
			Architecture local = Architecture::local();
			const EmbeddedSignatureBlob *sig = EmbeddedSignatureBlob::specific(top->find(local.cpuType()));
			if (!sig)
				sig = EmbeddedSignatureBlob::specific(top->find(local.cpuType() & ~CPU_ARCH_MASK));
			top = sig;
		}
		if (top)
			if (const CodeDirectory *cd = top->find<const CodeDirectory>(cdCodeDirectorySlot)) {
				printf("%s\n", cd->screeningCode().c_str());
				exit(0);
			}
	}
	fprintf(stderr, "Invalid signature structure\n");
	exit(1);
}
