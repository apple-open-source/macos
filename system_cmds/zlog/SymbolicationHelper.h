//
//  SymbolicationHelper.h
//  zlog
//
//  Created by Rasha Eqbal on 2/26/18.
//

#ifndef SymbolicationHelper_h
#define SymbolicationHelper_h

#include <CoreFoundation/CoreFoundation.h>
#include <CoreSymbolication/CoreSymbolication.h>

/*
 * Call this function on each address that needs to be symbolicated.
 *
 * sym: The CSSymbolicatorRef which will be used for symbolication. For example, to symbolicate
 *      kernel addresses create a CSSymbolicatorRef by calling CSSymbolicatorCreateWithMachKernel().
 * addr: The address that needs to be symbolicated.
 * binaryImages: The dictionary that aggregates binary image info for offline symbolication.
 */
void PrintSymbolicatedAddress(CSSymbolicatorRef sym, mach_vm_address_t addr, CFMutableDictionaryRef binaryImages);

/*
 * Call this function to dump binary image info required for offline symbolication.
 *
 * binaryImages: The dictionary that stores this info.
 *
 * The preferred way to use this is to create a CFMutableDictionaryRef with a call to CFDictionaryCreateMutable()
 * and pass it in to PrintSymbolicatedAddress() when symbolicating addresses. This will auto-populate the dictionary,
 * which just needs to be passed in here to print the relevant information.
 */
void PrintBinaryImagesInfo(CFMutableDictionaryRef binaryImages);

#endif /* SymbolicationHelper_h */
