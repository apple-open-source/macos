/*
 * Decode P12 PFX using P12Coder, reencode to file
 */

#include <security_pkcs12/pkcs12Coder.h>
#include <stdlib.h>
#include <stdio.h>
#include <Security/cssmtype.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <security_cdsa_utils/cuFileIo.h>  

/* decode --> encode */
int p12Reencode(
	const CSSM_DATA &pfx,
	CSSM_CSP_HANDLE cspHand,
	CFStringRef pwd,			// explicit passphrase, mutually exclusive with...
	bool verbose,
	unsigned loops)
{
	int 			ourRtn;
	
	for(unsigned loop=0; loop<loops; loop++) {
		{
			/* localize scope of coder for malloc test */
			P12Coder coder;
			CFDataRef cfd = CFDataCreate(NULL, pfx.Data, pfx.Length);
			ourRtn = 0;
			
			printf("...decoding...\n");
			try { 
				coder.setCsp(cspHand);
				coder.setMacPassPhrase(pwd);
				coder.decode(cfd);
			}
			catch(...) {
				printf("***decode error\n");
				return 1;
			}
			CFRelease(cfd);
			
			/* should just be able to re-encode it */
			printf("...encoding...\n");
			CFDataRef encPfx;
			try {
				coder.encode(&encPfx);
			}
			catch(...) {
				printf("***encode error\n");
				return 1;
			}
			writeFile("encoded.p12", CFDataGetBytePtr(encPfx),
				CFDataGetLength(encPfx));
			printf("...wrote %u bytes to encoded.p12\n",
				(unsigned)CFDataGetLength(encPfx));
			CFRelease(encPfx);
		}
		if(loops > 1) {
			fpurge(stdin);
			printf("CR to continue: ");
			getchar();
		}
		if(ourRtn) {
			return ourRtn;
		}
	}
	return ourRtn;
}
