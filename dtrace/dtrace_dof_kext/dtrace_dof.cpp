#include <IOKit/IOLib.h>
#include "dtrace_dof.h"
extern "C" {
#include <pexpert/pexpert.h> //This is for debugging purposes ONLY
}
 
extern "C" {
kern_return_t _dtrace_register_anon_DOF(char *, unsigned char *, unsigned int);
}
 
// Define my superclass
#define super IOService
 
// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.
OSDefineMetaClassAndStructors(com_apple_driver_dtraceDOF, IOService)

// There are byte-order dependancies in the dof_hdr emitted by the byte code compiler.
// Rather than swizzle, we'll instead insist that each endian-ness is stored on a
// distinguished property (and only that property is manipulated or referenced by its
// matching architecture.)
#if defined(__BIG_ENDIAN__)
#define BYTE_CODE_CONTAINER "Anonymous DOF"
#else
#define BYTE_CODE_CONTAINER "DOF Anonymous"
#endif

bool com_apple_driver_dtraceDOF::init(OSDictionary *dict)
{
    bool res = super::init(dict);

	OSDictionary *AnonymousDOF = OSDynamicCast(OSDictionary, dict->getObject(BYTE_CODE_CONTAINER));
	
	if (AnonymousDOF) {
	
		OSCollectionIterator *keyIterator = OSCollectionIterator::withCollection(AnonymousDOF); // must release
		OSString *key = NULL;                 // do not release
		
		while ((key = OSDynamicCast(OSString, keyIterator->getNextObject()))) {
			OSData *dof = NULL;
			
			dof = OSDynamicCast(OSData, AnonymousDOF->getObject(key));
			
			if (dof) {
				const char *name = key->getCStringNoCopy();
				int len = dof->getLength();
				
				IOLog("com_apple_driver_dtraceDOF getLength(%s) = %d\n", name, len);
				
				if (len > 0) {
					kern_return_t ret = 
						_dtrace_register_anon_DOF((char *)name, (unsigned char *)(dof->getBytesNoCopy()), len);
					
					if (KERN_SUCCESS != ret)
						IOLog("com_apple_driver_dtraceDOF FAILED to register %s!\n", name);
				}
			}
		}
		keyIterator->release();
	}
	
    return res;
}
 
void com_apple_driver_dtraceDOF::free(void)
{
    super::free();
}
 
IOService *com_apple_driver_dtraceDOF::probe(IOService *provider, SInt32 *score)
{
    IOService *res = super::probe(provider, score);
    return res;
}
 
bool com_apple_driver_dtraceDOF::start(IOService *provider)
{
    bool res = super::start(provider);
    return res;
}
 
void com_apple_driver_dtraceDOF::stop(IOService *provider)
{
    super::stop(provider);
}
