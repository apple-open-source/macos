#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>

#include "AppleDTUpdater.h"


OSDefineMetaClassAndStructors(AudioDeviceTreeUpdater, IOService)

#define super IOService
// Guts
void
AudioDeviceTreeUpdater::mergeProperties(OSObject* inDest, OSObject* inSrc)
{
	OSDictionary*	dest = OSDynamicCast(OSDictionary, inDest) ;
	OSDictionary*	src = OSDynamicCast(OSDictionary, inSrc) ;

	if (!src || !dest)
		return ;

	OSCollectionIterator*	srcIterator = OSCollectionIterator::withCollection(src) ;
	
	OSSymbol*	keyObject = NULL ;
	OSObject*	destObject = NULL ;
	while (NULL != (keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject())))
	{
		if (NULL != (destObject = dest->getObject(keyObject)) && (OSDynamicCast(OSDictionary, src->getObject(keyObject))))
			mergeProperties(destObject, src->getObject(keyObject)) ;
		else
			dest->setObject(keyObject, src->getObject(keyObject)) ;

		//keyObject->release() ;
	}
        srcIterator->release();
}

// IOService method overrides
// Always return false, because we load purely to update the device tree.
bool AudioDeviceTreeUpdater::start(IOService * provider )
{
    super::start(provider);
    IOLog("Starting Up\n");
	OSArray*	arrayObj = OSDynamicCast(OSArray, getProperty("DeviceTreePatches"));
    
    do {
        if(!arrayObj)
            break;
            
        OSCollectionIterator*	patchIterator = OSCollectionIterator::withCollection(arrayObj) ;
    
        OSDictionary *	patchDict;
        while (NULL != (patchDict = OSDynamicCast(OSDictionary, patchIterator->getNextObject()))) {
            // Find the device tree node to patch.
            OSString *parentPath;
            OSString *nodeName;
            char nodePath[128];
            
            parentPath = OSDynamicCast(OSString, patchDict->getObject("Parent"));
            if(!parentPath)
                break;
            nodeName = OSDynamicCast(OSString, patchDict->getObject("Node"));
            if(!nodeName)
                break;
            
            if((parentPath->getLength() + nodeName->getLength() + 2) > sizeof(nodePath))
                    break;
            strcpy(nodePath, parentPath->getCStringNoCopy());
            strcat(nodePath, "/");
            strcat(nodePath, nodeName->getCStringNoCopy());
            
            IORegistryEntry *patchParent;
            IORegistryEntry *patch;
            patchParent = IORegistryEntry::fromPath(parentPath->getCStringNoCopy(), gIODTPlane);
            if(!patchParent)
                break;
            patch = IORegistryEntry::fromPath(nodePath, gIODTPlane);
            
            if(!patch) {
                // Have to create the node as well as set its properties
                patch = new IORegistryEntry;
                if(!patch)
                    break;                    
                if(!patch->init(OSDynamicCast(OSDictionary, patchDict->getObject("NodeData")))) {
                    patch->release();	
                    patch = NULL;
                    break;
                }
                // And add it into the device tree
                if(!patch->attachToParent(patchParent, gIODTPlane)) {
                    patch->release();
                    patch = NULL;
                    break;
                }
                patch->setName(nodeName->getCStringNoCopy());
            }
            else
                mergeProperties(patch->getPropertyTable(), patchDict->getObject("NodeData"));
        }
    } while (false);
    return false;
}
