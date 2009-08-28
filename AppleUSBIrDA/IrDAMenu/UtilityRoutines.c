/*
 *  UtilityRoutines.c
 *  IrDAExtra
 *
 *  Created by jwilcox on Fri Jun 01 2001.
 *  Copyright (c) 2001 __CompanyName__. All rights reserved.
 *
 */

#include "UtilityRoutines.h"

kern_return_t doCommand(io_connect_t con, unsigned char commandID, void *inputData, unsigned long inputDataSize, void *outputData, size_t *outputDataSize)
{
	kern_return_t   err = KERN_SUCCESS;
	//mach_msg_type_number_t  outSize = outputDataSize;
	IrDACommandPtr command = NULL;
	
	// Creates a command block:
	command = (IrDACommandPtr)malloc (inputDataSize + sizeof (unsigned char));
	if (!command)
		return KERN_FAILURE;
	command->commandID = commandID;
	// Adds the data to the command block:
	if ((inputData != NULL) && (inputDataSize != 0))
		memcpy(command->data, inputData, inputDataSize);
	// Now we can (hopefully) transfer everything:
	err = IOConnectCallStructMethod(
			con,
			0,										/* method index */
			(char *) command,						/* input[] */
			inputDataSize+sizeof(unsigned char),	/* inputCount */
			(char *) outputData,					/* output */
			outputDataSize);						/* buffer size, then result */
	free (command);
	return err;
}

/* ==========================================
 * Look through the registry and search for an
 * IONetworkInterface objects with the given
 * name.
 * If a match is found, the object is returned.
 * =========================================== */
io_object_t getInterfaceWithName(mach_port_t masterPort, char *className)
{
    kern_return_t	kr;
    io_iterator_t	ite;
    io_object_t		obj = 0;

    kr = IORegistryCreateIterator(masterPort, kIOServicePlane, true, &ite);
    if (kr != kIOReturnSuccess) {
        printf("IORegistryCreateIterator() error %08lx\n", (unsigned long)kr);
        return 0;
    }
    while ((obj = IOIteratorNext(ite))) {
        if (IOObjectConformsTo(obj, (char *) className)) {
            break;
        }
		else {
			io_name_t name;
			kern_return_t rc;
			rc = IOObjectGetClass(obj, name);
		}
        IOObjectRelease(obj);
        obj = 0;
    }
    IOObjectRelease(ite);
    return obj;
}
