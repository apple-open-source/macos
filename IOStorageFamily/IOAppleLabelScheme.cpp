/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/assert.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/storage/IOAppleLabelScheme.h>
#include <libkern/OSByteOrder.h>
///m:workaround:added:start
static uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static uint32_t
crc32(uint32_t crc, const void *buf, size_t size)
{
	const uint8_t *p;

	p = buf;
	crc = crc ^ ~0U;

	while (size--)
		crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

	return crc ^ ~0U;
}
///m:workaround:added:stop

#define super IOFilterScheme
OSDefineMetaClassAndStructors(IOAppleLabelScheme, IOFilterScheme);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Notes
//
// o the on-disk structure's fields are: 64-bit packed, big-endian formatted
// o the al_offset value is relative to the media container
//

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#define kIOMediaBaseKey "Base"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOAppleLabelScheme::init(OSDictionary * properties)
{
    //
    // Initialize this object's minimal state.
    //

    // State our assumptions.

    assert(sizeof(applelabel) == 512);              // (compiler/platform check)

    // Ask our superclass' opinion.

    if (super::init(properties) == false)  return false;

    // Initialize our state.

    _content = 0;

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOAppleLabelScheme::free()
{
    //
    // Free all of this object's outstanding resources.
    //

    if ( _content )  _content->release();

    super::free();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOService * IOAppleLabelScheme::probe(IOService * provider, SInt32 * score)
{
    //
    // Determine whether the provider media contains an Apple label scheme.
    //

    // State our assumptions.

    assert(OSDynamicCast(IOMedia, provider));

    // Ask superclass' opinion.

    if (super::probe(provider, score) == 0)  return 0;

    // Scan the provider media for an Apple label scheme.

    _content = scan(score);

    return ( _content ) ? this : 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOAppleLabelScheme::start(IOService * provider)
{
    //
    // Publish the new media object which represents our content.
    //

    // State our assumptions.

    assert(_content);

    // Ask our superclass' opinion.

    if ( super::start(provider) == false )  return false;

    // Attach and register the new media object representing our content.

    _content->attach(this);

    attachMediaObjectToDeviceTree(_content);

    _content->registerService();

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOAppleLabelScheme::stop(IOService * provider)
{
    //
    // Clean up after the media object we published before terminating.
    //

    // State our assumptions.

    assert(_content);

    // Detach the media objects we previously attached to the device tree.

    detachMediaObjectFromDeviceTree(_content);

    super::stop(provider);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOMedia * IOAppleLabelScheme::scan(SInt32 * score)
{
    //
    // Scan the provider media for an Apple label scheme.
    //

    IOBufferMemoryDescriptor * buffer         = 0;
    UInt64                     bufferBase     = 0;
    UInt32                     bufferSize     = 0;
    applelabel *               headerMap      = 0;
    UInt64                     labelBase      = 0;
    UInt32                     labelCheck     = 0;
    char *                     labelMap       = 0;
    UInt32                     labelSize      = 0;
    IOMedia *                  media          = getProvider();
    UInt64                     mediaBlockSize = media->getPreferredBlockSize();
    bool                       mediaIsOpen    = false;
    IOMedia *                  newMedia       = 0;
    OSDictionary *             properties     = 0;
    IOReturn                   status         = kIOReturnError;

    // Determine whether this media is formatted.

    if ( media->isFormatted() == false )  goto scanErr;

    // Determine whether this media has an appropriate block size.

    if ( (mediaBlockSize % sizeof(applelabel)) )  goto scanErr;

    // Allocate a buffer large enough to hold one map, rounded to a media block.

    bufferSize = IORound(sizeof(applelabel), mediaBlockSize);
    buffer     = IOBufferMemoryDescriptor::withCapacity(
                                           /* capacity      */ bufferSize,
                                           /* withDirection */ kIODirectionIn );
    if ( buffer == 0 )  goto scanErr;

    // Open the media with read access.

    mediaIsOpen = media->open(this, 0, kIOStorageAccessReader);
    if ( mediaIsOpen == false )  goto scanErr;

    // Read the label header into our buffer.

    status = media->read(this, 0, buffer);
    if ( status != kIOReturnSuccess )  goto scanErr;

    headerMap = (applelabel *) buffer->getBytesNoCopy();

    // Determine whether the label header signature is present.

    if ( OSSwapBigToHostInt16(headerMap->al_magic) != AL_MAGIC )
    {
        goto scanErr;
    }

    // Determine whether the label header version is valid.

    if ( OSSwapBigToHostInt16(headerMap->al_type) != AL_TYPE_DEFAULT )
    {
        goto scanErr;
    }

    // Compute the relative byte position and size of the label.

    labelBase  = OSSwapBigToHostInt64(headerMap->al_offset);
    labelCheck = OSSwapBigToHostInt32(headerMap->al_checksum);
    labelSize  = OSSwapBigToHostInt32(headerMap->al_size);

    // Allocate a buffer large enough to hold one map, rounded to a media block.

    buffer->release();

    bufferBase = IOTrunc(labelBase, mediaBlockSize);
    bufferSize = IORound(labelBase + labelSize, mediaBlockSize) - bufferBase;
    buffer     = IOBufferMemoryDescriptor::withCapacity(
                                           /* capacity      */ bufferSize,
                                           /* withDirection */ kIODirectionIn );
    if ( buffer == 0 )  goto scanErr;

    // Determine whether the label leaves the confines of the container.

    if ( bufferBase + bufferSize > media->getSize() )  goto scanErr;

    // Read the label into our buffer.

    status = media->read(this, bufferBase, buffer);
    if ( status != kIOReturnSuccess )  goto scanErr;

    labelMap = (char *) buffer->getBytesNoCopy() + (labelBase % mediaBlockSize);

    // Determine whether the label checksum is valid.

    if ( crc32(0, labelMap, labelSize) != labelCheck )
    {
        goto scanErr;
    }

    // Obtain the properties.

    properties = (OSDictionary *) OSUnserializeXML(labelMap);

    if ( OSDynamicCast(OSDictionary, properties) == 0 )
    {
        goto scanErr;
    }

    // Determine whether the content is corrupt.

    if ( isContentCorrupt(properties) )
    {
        goto scanErr;
    }

    // Determine whether the content is corrupt.

    if ( isContentInvalid(properties) )
    {
        goto scanErr;
    }

    // Create a media object to represent the content.

    newMedia = instantiateMediaObject(properties);

    if ( newMedia == 0 )
    {
        goto scanErr;
    }

    // Release our resources.

    media->close(this);
    buffer->release();
    properties->release();

    return newMedia;

scanErr:

    // Release our resources.

    if ( mediaIsOpen )  media->close(this);
    if ( buffer )  buffer->release();
    if ( properties )  properties->release();

    return 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOAppleLabelScheme::isContentCorrupt(OSDictionary * properties)
{
    //
    // Ask whether the given content appears to be corrupt.
    //

    return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOAppleLabelScheme::isContentInvalid(OSDictionary * properties)
{
    //
    // Ask whether the given content appears to be invalid.
    //

    UInt64     contentBase = 0;
    UInt64     contentSize = 0;
    IOMedia *  media       = getProvider();
    OSObject * object      = 0;

    // Compute the relative byte position and size of the new content.

    object = properties->getObject(kIOMediaBaseKey);

    if ( OSDynamicCast(OSNumber, object) )
    {
        contentBase = ((OSNumber *) object)->unsigned64BitValue();
    }

    object = properties->getObject(kIOMediaSizeKey);

    if ( OSDynamicCast(OSNumber, object) )
    {
        contentSize = ((OSNumber *) object)->unsigned64BitValue();
    }

    // Determine whether the content is a placeholder.

    if ( contentSize == 0 )  return true;

    // Determine whether the new content leaves the confines of the container.

    if ( contentBase + contentSize > media->getSize() )  return true;

    return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOMedia * IOAppleLabelScheme::instantiateMediaObject(OSDictionary * properties)
{
    //
    // Instantiate a new media object to represent the given content.
    //

    IOMediaAttributeMask contentAttributes = 0;
    UInt64               contentBase       = 0;
    UInt64               contentBlockSize  = 0;
    const char *         contentHint       = 0;
    bool                 contentIsWhole    = false;
    bool                 contentIsWritable = false;
    UInt64               contentSize       = 0;
    IOMedia *            media             = getProvider();
    IOMedia *            newMedia          = 0; 
    OSObject *           object            = 0;

    contentAttributes = media->getAttributes();
    contentBlockSize  = media->getPreferredBlockSize();
    contentIsWhole    = media->isWhole();
    contentIsWritable = media->isWritable();
    contentSize       = media->getSize();

    properties = OSDictionary::withDictionary(properties);
    if ( properties == 0 )  return 0;

    // Obtain the initialization properties.

    object = properties->getObject(kIOMediaBaseKey);

    if ( OSDynamicCast(OSNumber, object) )
    {
        contentBase = ((OSNumber *) object)->unsigned64BitValue();
    }

    properties->removeObject(kIOMediaBaseKey);

    object = properties->getObject(kIOMediaContentKey);

    if ( OSDynamicCast(OSString, object) )
    {
        contentHint = ((OSString *) object)->getCStringNoCopy();
    }

    properties->removeObject(kIOMediaContentKey);

    object = properties->getObject(kIOMediaContentHintKey);

    if ( OSDynamicCast(OSString, object) )
    {
        contentHint = ((OSString *) object)->getCStringNoCopy();
    }

    properties->removeObject(kIOMediaContentHintKey);

    object = properties->getObject(kIOMediaEjectableKey);

    if ( OSDynamicCast(OSBoolean, object) )
    {
        if ( ((OSBoolean *) object)->getValue() )
        {
            contentAttributes |= kIOMediaAttributeEjectableMask;
        }
        else
        {
            contentAttributes &= ~kIOMediaAttributeEjectableMask;
        }
    }

    properties->removeObject(kIOMediaEjectableKey);

    object = properties->getObject(kIOMediaPreferredBlockSizeKey);

    if ( OSDynamicCast(OSNumber, object) )
    {
        contentBlockSize = ((OSNumber *) object)->unsigned64BitValue();
    }

    properties->removeObject(kIOMediaPreferredBlockSizeKey);

    object = properties->getObject(kIOMediaRemovableKey);

    if ( OSDynamicCast(OSBoolean, object) )
    {
        if ( ((OSBoolean *) object)->getValue() )
        {
            contentAttributes |= kIOMediaAttributeRemovableMask;
        }
        else
        {
            contentAttributes &= ~kIOMediaAttributeRemovableMask;
        }
    }

    properties->removeObject(kIOMediaRemovableKey);

    object = properties->getObject(kIOMediaWholeKey);

    if ( OSDynamicCast(OSBoolean, object) )
    {
        contentIsWhole = ((OSBoolean *) object)->getValue();
    }

    properties->removeObject(kIOMediaWholeKey);

    object = properties->getObject(kIOMediaSizeKey);

    if ( OSDynamicCast(OSNumber, object) )
    {
        contentSize = ((OSNumber *) object)->unsigned64BitValue();
    }

    properties->removeObject(kIOMediaSizeKey);

    object = properties->getObject(kIOMediaWritableKey);

    if ( OSDynamicCast(OSBoolean, object) )
    {
        contentIsWritable = ((OSBoolean *) object)->getValue();
    }

    properties->removeObject(kIOMediaWritableKey);

    // Create the new media object.

    newMedia = instantiateDesiredMediaObject(properties);

    if ( newMedia )
    {
        if ( newMedia->init(
                /* base               */ contentBase,
                /* size               */ contentSize,
                /* preferredBlockSize */ contentBlockSize,
                /* attributes         */ contentAttributes,
                /* isWhole            */ contentIsWhole,
                /* isWritable         */ contentIsWritable,
                /* contentHint        */ contentHint ) )
        {
            // Set a location.

            newMedia->setLocation("1");

            // Set the properties.

            newMedia->getPropertyTable()->merge(properties);
        }
        else
        {
            newMedia->release();
            newMedia = 0;
        }
    }

    properties->release();

    return newMedia;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOMedia * IOAppleLabelScheme::instantiateDesiredMediaObject(
                                                     OSDictionary * properties )
{
    //
    // Allocate a new media object (called from instantiateMediaObject).
    //

    return new IOMedia;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOAppleLabelScheme::attachMediaObjectToDeviceTree( IOMedia * media )
{
    //
    // Attach the given media object to the device tree plane.
    //

    IORegistryEntry * child;

    if ( (child = getParentEntry(gIOServicePlane)) )
    {
        IORegistryEntry * parent;

        if ( (parent = child->getParentEntry(gIODTPlane)) )
        {
            const char * location = child->getLocation(gIODTPlane);
            const char * name     = child->getName(gIODTPlane);

            if ( media->attachToParent(parent, gIODTPlane) )
            {
                media->setLocation(location, gIODTPlane);
                media->setName(name, gIODTPlane);

                child->detachFromParent(parent, gIODTPlane);

                return true;
            }
        }
    }

    return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOAppleLabelScheme::detachMediaObjectFromDeviceTree( IOMedia * media )
{
    //
    // Detach the given media object from the device tree plane.
    //

    IORegistryEntry * child;

    if ( (child = getParentEntry(gIOServicePlane)) )
    {
        IORegistryEntry * parent;

        if ( (parent = media->getParentEntry(gIODTPlane)) )
        {
            const char * location = media->getLocation(gIODTPlane);
            const char * name     = media->getName(gIODTPlane);

            if ( child->attachToParent(parent, gIODTPlane) )
            {
                child->setLocation(location, gIODTPlane);
                child->setName(name, gIODTPlane);
            }

            media->detachFromParent(parent, gIODTPlane);
        }
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 0);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 1);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 2);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 3);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 4);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 5);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 6);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 7);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 8);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 9);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 10);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 11);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 12);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 13);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 14);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 15);
