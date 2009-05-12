/*
 * Copyright (c) 1998-2007 Apple Inc.  All Rights Reserved.
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

#include <sys/types.h>                       // (miscfs/devfs/devfs.h, ...)

#include <miscfs/devfs/devfs.h>              // (devfs_make_node, ...)
#include <sys/buf.h>                         // (buf_t, ...)
#include <sys/conf.h>                        // (bdevsw_add, ...)
#include <sys/fcntl.h>                       // (FWRITE, ...)
#include <sys/ioccom.h>                      // (IOCGROUP, ...)
#include <sys/proc.h>                        // (proc_is64bit, ...)
#include <sys/stat.h>                        // (S_ISBLK, ...)
#include <sys/systm.h>                       // (DEV_BSIZE, ...)
#include <sys/uio_internal.h>                // (uio_t, ...)
#include <IOKit/assert.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOMediaBSDClient.h>

#define super IOService
OSDefineMetaClassAndStructors(IOMediaBSDClient, IOService)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

const SInt32 kMajor              = 14;
const UInt32 kMinorsAddCountBits = 6;
const UInt32 kMinorsAddCountMask = (1 << kMinorsAddCountBits) - 1;
const UInt32 kMinorsAddCount     = (1 << kMinorsAddCountBits);
const UInt32 kMinorsMaxCountBits = 16;
const UInt32 kMinorsMaxCountMask = (1 << kMinorsMaxCountBits) - 1;
const UInt32 kMinorsMaxCount     = (1 << kMinorsMaxCountBits);
const UInt32 kMinorsBucketCount  = kMinorsMaxCount / kMinorsAddCount;
const UInt32 kAnchorsAddCount    = 2;
const UInt32 kAnchorsMaxCount    = kMinorsMaxCount;

#define kMsgNoWhole    "%s: No whole media found for media \"%s\".\n", getName()
#define kMsgNoLocation "%s: No location is found for media \"%s\".\n", getName()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

extern "C"
{
    int  dkclose(dev_t dev, int flags, int devtype, proc_t);
    int  dkioctl(dev_t dev, u_long cmd, caddr_t data, int flags, proc_t);
    int  dkioctl_bdev(dev_t dev, u_long cmd, caddr_t data, int flags, proc_t);
    int  dkopen(dev_t dev, int flags, int devtype, proc_t);
    int  dkread(dev_t dev, uio_t uio, int flags);
    int  dksize(dev_t dev);    
    void dkstrategy(buf_t bp);
    int  dkwrite(dev_t dev, uio_t uio, int flags);
} // extern "C"

static struct bdevsw bdevswFunctions =
{
    /* d_open     */ dkopen,
    /* d_close    */ dkclose,
    /* d_strategy */ dkstrategy,
    /* d_ioctl    */ dkioctl_bdev,
    /* d_dump     */ eno_dump,
    /* d_psize    */ dksize,
    /* d_type     */ D_DISK
};

struct cdevsw cdevswFunctions =
{
    /* d_open     */ dkopen,
    /* d_close    */ dkclose,
    /* d_read     */ dkread,
    /* d_write    */ dkwrite,
    /* d_ioctl    */ dkioctl,
    /* d_stop     */ eno_stop,
    /* d_reset    */ eno_reset,
    /* d_ttys     */ 0,
    /* d_select   */ eno_select,
    /* d_mmap     */ eno_mmap,
    /* d_strategy */ eno_strat,
    /* d_getc     */ eno_getc,
    /* d_putc     */ eno_putc,
    /* d_type     */ D_DISK
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

struct dio { dev_t dev; uio_t uio; void * drvdata; };

typedef struct dio *                      dio_t;
typedef void *                            dkr_t;       /* dkreadwrite request */
typedef enum { DKRTYPE_BUF, DKRTYPE_DIO } dkrtype_t;

static int  dkreadwrite(dkr_t dkr, dkrtype_t dkrtype);
static void dkreadwritecompletion(void *, void *, IOReturn, UInt64);

inline int32_t getminor(dev_t dev)
{
    return minor(dev);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

const UInt32 kInvalidAnchorID = (UInt32) (-1);

struct AnchorSlot
{
    UInt32       isAssigned:1; // (slot is occupied)
    UInt32       isObsolete:1; // (slot is to be removed once references gone)

    IOService *  anchor;       // (anchor object)
    void *       key;          // (anchor key)
    IONotifier * notifier;     // (anchor termination notification, post-stop)
};

class AnchorTable
{
protected:
    AnchorSlot * _table;
    UInt32       _tableCount;

    static IOReturn anchorWasNotified( void *      target,
                                       void *      parameter,
                                       UInt32      messageType,
                                       IOService * provider,
                                       void *      messageArgument,
                                       vm_size_t   messageArgumentSize );

public:
    AnchorTable();
    ~AnchorTable();

    UInt32 insert(IOService * anchor, void * key);
    UInt32 locate(IOService * anchor);
    UInt32 locate(IOService * anchor, void * key);
    void   obsolete(UInt32 anchorID);
    void   remove(UInt32 anchorID);
    UInt32 update(IOService * anchor, void * key);

    bool   isObsolete(UInt32 anchorID);
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

const UInt32 kInvalidMinorID = (UInt32) (-1);

struct MinorSlot
{
    UInt32             isAssigned:1;  // (slot is occupied)
    UInt32             isObsolete:1;  // (slot is to be removed, close pending)
    UInt32             isOrphaned:1;  // (slot is in  open flux, close pending)

    UInt32             anchorID;      // (minor's associated anchor ID)
    IOMediaBSDClient * client;        // (minor's media bsd client object)
    IOMedia *          media;         // (minor's media object)
    char *             name;          // (minor's name, private allocation)

    UInt64             bdevBlockSize; // (block device's preferred block size)
    void *             bdevNode;      // (block device's devfs node)
    UInt32             bdevOpen;      // (block device's open count)
    IOStorageAccess    bdevOpenLevel; // (block device's open level)

    void *             cdevNode;      // (character device's devfs node)
    UInt32             cdevOpen;      // (character device's open count)
    IOStorageAccess    cdevOpenLevel; // (character device's open level)
};

class MinorTable
{
protected:
    class
    {
    public:
        MinorSlot ** buckets;

        inline MinorSlot & operator[](const int i)
        {
            return (buckets[i >> kMinorsAddCountBits])[i & kMinorsAddCountMask];
        }
    } _table;

    UInt32 _tableCount;

public:
    MinorTable();
    ~MinorTable();

    UInt32      insert( IOMedia *          media,
                        UInt32             anchorID,
                        IOMediaBSDClient * client,
                        char *             slicePath );

    UInt32      update( IOMedia *          media,
                        UInt32             anchorID,
                        IOMediaBSDClient * client,
                        char *             slicePath );

    UInt32      locate(IOMedia * media);
    void        obsolete(UInt32 minorID);
    void        remove(UInt32 minorID);

    bool        isObsolete(UInt32 minorID);

    MinorSlot * getMinor(UInt32 minorID);

    UInt32      getOpenCountForAnchorID(UInt32 anchorID);
    bool        hasReferencesToAnchorID(UInt32 anchorID, bool excludeOrphans);
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

class IOMediaBSDClientGlobals
{
protected:
    AnchorTable * _anchors;           // (table of anchors)
    MinorTable *  _minors;            // (table of minors)

    UInt32        _bdevswInstalled:1; // (are bdevsw functions installed?)
    UInt32        _cdevswInstalled:1; // (are cdevsw functions installed?)

    IOLock *      _openLock;          // (lock for opens, closes)
    IOLock *      _stateLock;         // (lock for state, tables)

public:
    IOMediaBSDClientGlobals();
    ~IOMediaBSDClientGlobals();

    AnchorTable * getAnchors();
    MinorTable *  getMinors();
    MinorSlot *   getMinor(UInt32 minorID);

    bool          isValid();

    void          lockOpen();
    void          unlockOpen();

    void          lockState();
    void          unlockState();
};

static IOMediaBSDClientGlobals gIOMediaBSDClientGlobals;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOMediaBSDClient::init(OSDictionary * properties)
{
    //
    // Initialize this object's minimal state.
    //

    // Ask our superclass' opinion.

    if ( super::init(properties) == false )  return false;

    // Determine whether our minimal global state has been initialized.

    if ( gIOMediaBSDClientGlobals.isValid() == false )  return false;

    // Initialize this object's minimal state.

    _anchors = gIOMediaBSDClientGlobals.getAnchors();
    _minors  = gIOMediaBSDClientGlobals.getMinors();

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOMediaBSDClient::free()
{
    //
    // Free all of this object's outstanding resources.
    //

    super::free();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOMediaBSDClient::start(IOService * provider)
{
    //
    // This method is called once we have been attached to the provider object.
    //

    IOMedia * media = (IOMedia *) provider;

    // Ask our superclass' opinion.

    if ( super::start(provider) == false )  return false;

    // Disable access to tables.

    gIOMediaBSDClientGlobals.lockState();

    // Create bdevsw and cdevsw nodes for the new media object.

    createNodes(media);

    // Enable access to tables.

    gIOMediaBSDClientGlobals.unlockState();

    // Register this object so it can be found via notification requests. It is
    // not being registered to have I/O Kit attempt to have drivers match on it,
    // which is the reason most other services are registered -- that's not the
    // intention of this registerService call.

    registerService();

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOMediaBSDClient::terminate(IOOptionBits options)
{
    //
    // This method is called when we are to terminate from the provider object.
    //

    UInt32 minorID;

    // Ask our superclass' opinion.

    if ( super::terminate(options) == false )  return false;

    // Disable access to tables.

    gIOMediaBSDClientGlobals.lockState();

    // Find the minor assigned to this media.

    minorID = gIOMediaBSDClientGlobals.getMinors()->locate(getProvider());

    if ( minorID != kInvalidMinorID )
    {
        MinorSlot * minor;

        minor = gIOMediaBSDClientGlobals.getMinors()->getMinor(minorID);

        // Remove the minor from the minor table.  If an open is still
        // outstanding, we mark the minor as obsolete for removal when
        // the close comes in later.

        if ( minor->bdevOpen || minor->cdevOpen )
        {
            gIOMediaBSDClientGlobals.getMinors()->obsolete(minorID);
        }
        else
        {
            gIOMediaBSDClientGlobals.getMinors()->remove(minorID);
        }
    }

    // Enable access to tables.

    gIOMediaBSDClientGlobals.unlockState();

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOMedia * IOMediaBSDClient::getWholeMedia( IOMedia * media,
                                           UInt32 *  slicePathSize,
                                           char *    slicePath )
{
    //
    // Find the whole media that roots this media tree.  A null return value
    // indicates no whole media was found or a malformed tree was detected.
    //
    // If slicePathSize is non-zero, the size required to fit the slice path
    // (including the zero terminator) is passed back as a result.
    //
    // If slicePathSize and slicePath are both non-zero, the slice path will
    // be written into the slicePath buffer.  The value slicePathSize points
    // to must be the size of the slicePath buffer, which is used for sanity
    // checking in this method.
    //
    // This method assumes that the table (and termination) lock is held.
    //

    UInt32      depth    = 1;
    UInt32      position = sizeof('\0');
    IOService * service  = 0;

    assert(slicePath == 0 || slicePathSize != 0);

    // Search the registry for the parent whole media for this media.

    for ( service = media; service; service = service->getProvider() )
    {
        if ( OSDynamicCast(IOMedia, service) )               // (is it a media?)
        {
            if ( ((IOMedia *)service)->isWhole() )     // (is it a whole media?)
            {
                if ( slicePath )            // (are we building the slice path?)
                {
                    slicePath[*slicePathSize - 1] = 0;  // (zero terminate path)

                    if ( position < *slicePathSize )     // (need to move path?)
                    {
                        memmove( slicePath,    // (move path to start of buffer)
                                 slicePath + (*slicePathSize - position),
                                 position );
                    }
                }
                else if ( slicePathSize ) // (report size req'd for slice path?)
                {
                    *slicePathSize = position;
                }

                return (IOMedia *)service;           // (return the whole media)
            }

            // Determine whether this non-whole media has a location value.  It
            // must, by definition of a non-whole media, but if it does not, we
            // should return an error condition.

            const char * location = service->getLocation();

            if ( location == 0 )            // (no location on non-whole media?)
            {
                if ( service == media ) IOLog(kMsgNoLocation, media->getName());
                return 0;
            }

            // Otherwise, it's a valid non-whole media: we compute the required
            // size for the slice path or build the slice path, if so requested.
            // Note that the slice path is built backwards from the ends of the
            // supplied buffer to the beginning of the buffer.

            position += sizeof('s') + strlen(location);

            if ( slicePath )                          // (build the slice path?)
            {
                char * path = slicePath + *slicePathSize - position;

                if ( position > *slicePathSize )  { assert(0);  return 0; }

                *path = 's';
                strncpy(path + sizeof('s'), location, strlen(location));
            }

            depth += 1;
        }
    }

    // If we've fallen through, then the whole media was never found.

    if ( depth == 1 )  IOLog(kMsgNoWhole, media->getName());

    return 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOMediaBSDClient::createNodes(IOMedia * media)
{
    //
    // Create bdevsw and cdevsw nodes for the given media object.
    //
    // This method assumes that the table (and termination) lock is held.
    //

    IOService *   anchor;
    AnchorTable * anchors   = gIOMediaBSDClientGlobals.getAnchors();
    UInt32        anchorID;
    bool          anchorNew = false;
    MinorTable *  minors    = gIOMediaBSDClientGlobals.getMinors();
    UInt32        minorID;
    char *        slicePath = 0;
    UInt32        slicePathSize;
    IOMedia *     whole;

    //
    // Find the anchor that roots this media tree.  The anchor is defined as the
    // parent of the whole media that roots this media tree.  It is an important
    // object to us because this object stays in place when media is ejected, so
    // we can continue to maintain the "unit number" of the "drive" such that if
    // media is re-inserted, it will show up under the same "unit number".   You
    // can think of the typical anchor as being the drive, if it helps, although
    // it could be one of many other kinds of drivers (eg. a RAID scheme).
    //

    whole = getWholeMedia(media, &slicePathSize);
    if ( whole == 0 )  return false;

    anchor = whole->getProvider();
    if ( anchor == 0 )  return false;

    //
    // Determine whether the anchor already exists in the anchor table (obsolete
    // occurences are skipped in the search, as appropriate,  since those anchor
    // IDs are to be removed soon). If the anchor does not exist, insert it into
    // anchor table.
    //

    anchorID = anchors->locate(anchor, whole);

    if ( anchorID == kInvalidAnchorID )
    {
        //
        // The anchor and key pair does not exist in the table, however we still
        // have more to check.  The anchor might in fact exist in the table, but
        // have a different key.  If such a slot exists, and it isn't referenced
        // in the minor table, we reuse the slot.
        //

        anchorID = anchors->update(anchor, whole);
    }

    if ( anchorID == kInvalidAnchorID )
    {
        anchorID = anchors->insert(anchor, whole);        // (get new anchor ID)
        if ( anchorID == kInvalidAnchorID )  return false;
        anchorNew = true;
    }

    //
    // Allocate space for and build the slice path for the device node names.
    //

    slicePath = (char *) IOMalloc(slicePathSize);
    if ( slicePath == 0 )  goto createNodesErr;

    whole = getWholeMedia(media, &slicePathSize, slicePath);
    assert(whole);

    //
    // Insert the new media into our minor table (we're almost done :-).
    //

    minorID = minors->update(media, anchorID, this, slicePath);

    if ( minorID == kInvalidMinorID )
    {
        minorID = minors->insert(media, anchorID, this, slicePath);
        if ( minorID == kInvalidMinorID )  goto createNodesErr;
    }

    //
    // Create the required properties on the media.
    //

    media->setProperty(kIOBSDNameKey,  minors->getMinor(minorID)->name);
    media->setProperty(kIOBSDUnitKey,  anchorID, 32);           // ("BSD Unit" )
    media->setProperty(kIOBSDMajorKey, kMajor,   32);           // ("BSD Major")
    media->setProperty(kIOBSDMinorKey, minorID,  32);           // ("BSD Minor")

    //
    // Clean up outstanding resources.
    //

    IOFree(slicePath, slicePathSize);

    return true; // (success)

createNodesErr:

    if (anchorNew)  anchors->remove(anchorID);
    if (slicePath)  IOFree(slicePath, slicePathSize);

    return false; // (failure)
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

AnchorTable * IOMediaBSDClient::getAnchors()
{
    //
    // Obtain the table of anchors.
    //
 
    return _anchors;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

MinorTable * IOMediaBSDClient::getMinors()
{
    //
    // Obtain the table of anchors.
    //

    return _minors;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

MinorSlot * IOMediaBSDClient::getMinor(UInt32 minorID)
{
    //
    // Obtain information for the specified minor ID.
    //

    return _minors->getMinor(minorID);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOMedia * IOMediaBSDClient::getProvider() const
{
    //
    // Obtain this object's provider.  We override the superclass's method to
    // return a more specific subclass of IOService -- IOMedia.   This method
    // serves simply as a convenience to subclass developers.
    //

    return (IOMedia *) IOService::getProvider();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int IOMediaBSDClient::ioctl( dev_t   dev,
                             u_long  cmd,
                             caddr_t /* data */,
                             int     /* flags */,
                             proc_t  /* proc */ )
{
    //
    // Process a foreign ioctl.
    //

    int error = 0;

    switch ( cmd )
    {
        default:
        {
            //
            // A foreign ioctl was received.  Log an error to the console.
            //

            IOLog( "%s: ioctl(%s\'%c\',%d,%d) is unsupported.\n",
                   gIOMediaBSDClientGlobals.getMinor(getminor(dev))->name,
                   ((cmd & IOC_INOUT) == IOC_INOUT) ? ("_IOWR,") :
                     ( ((cmd & IOC_OUT) == IOC_OUT) ? ("_IOR,") :
                       ( ((cmd & IOC_IN) == IOC_IN) ? ("_IOW,") :
                         ( ((cmd & IOC_VOID) == IOC_VOID) ? ("_IO,") : "" ) ) ),
                   (char) IOCGROUP(cmd),
                   (int)  (cmd & 0xff),
                   (int)  IOCPARM_LEN(cmd) );

            error = ENOTTY;

        } break;
    }

    return error;                                       // (return error status)
}

OSMetaClassDefineReservedUsed(IOMediaBSDClient, 0);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 1);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 2);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 3);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 4);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 5);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 6);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 7);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 8);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 9);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 10);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 11);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 12);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 13);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 14);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOMediaBSDClient, 15);

// =============================================================================
// BSD Functions

static IOStorageAccess DK_ADD_ACCESS(IOStorageAccess a1, IOStorageAccess a2)
{
    static UInt8 table[4][4] =
    {            /* Rea, Wri, R|S, W|S */
        /* Rea */ { 000, 001, 002, 003 },
        /* Wri */ { 001, 001, 001, 001 },
        /* R|S */ { 002, 001, 002, 003 },
        /* W|S */ { 003, 001, 003, 003 }
    };

    if ( a1 == kIOStorageAccessNone )  return a2;
    if ( a2 == kIOStorageAccessNone )  return a1;

    a1 = (a1 - 1) >> 1;
    a2 = (a2 - 1) >> 1;

    if ( a1 > 003 )  return kIOStorageAccessNone;
    if ( a2 > 003 )  return kIOStorageAccessNone;

    return (table[a1][a2] << 1) + 1;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static bool DKIOC_IS_RESERVED(caddr_t data, uint32_t reserved)
{
    UInt32 index;

    for ( index = 0; index < sizeof(reserved) * 8; index++, reserved >>= 1 )
    {
        if ( (reserved & 1) )
        {
            if ( data[index] )  return true;
        }
    }

    return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int dkopen(dev_t dev, int flags, int devtype, proc_t /* proc */)
{
    //
    // dkopen opens the device (called on each open).
    //

    IOStorageAccess access;
    int             error;
    IOStorageAccess level;
    IOStorageAccess levelOut;
    IOMedia *       media;
    MinorSlot *     minor;

    assert(S_ISBLK(devtype) || S_ISCHR(devtype));

    gIOMediaBSDClientGlobals.lockOpen();    // (disable access to opens, closes)
    gIOMediaBSDClientGlobals.lockState();   // (disable access to state, tables)

    access  = kIOStorageAccessReader;
    access |= (flags &   FWRITE) ? kIOStorageAccessReaderWriter  : 0;
    access |= (flags & O_SHLOCK) ? kIOStorageAccessSharedLock    : 0;
    access |= (flags & O_EXLOCK) ? kIOStorageAccessExclusiveLock : 0;

    error = 0;
    media = 0;
    minor = gIOMediaBSDClientGlobals.getMinor(getminor(dev));

    //
    // Process the open.
    //

    if ( minor == 0 )                                       // (is minor valid?)
    {
        error = ENXIO;
    }
    else if ( minor->isOrphaned )                         // (is minor in flux?)
    {
        error = EBUSY;
    }
    else
    {
        level    = DK_ADD_ACCESS(minor->bdevOpenLevel, minor->cdevOpenLevel);
        levelOut = DK_ADD_ACCESS(level, access);

        if ( levelOut == kIOStorageAccessNone )            // (is access valid?)
        {
            error = EBUSY;
        }
        else if ( (flags & FWRITE) )                    // (is client a writer?)
        {
            if ( minor->media->isWritable() == false )
            {
                error = EACCES;
            }
        }
    }

    if ( error == 0 )                                                   // (go?)
    {
        IOStorageAccess wasOpenLevel;

        if ( S_ISBLK(devtype) )                                // (update state)
        {
            minor->bdevOpen++;
            wasOpenLevel = minor->bdevOpenLevel;
            minor->bdevOpenLevel = DK_ADD_ACCESS(wasOpenLevel, access);
        }
        else
        {
            minor->cdevOpen++;
            wasOpenLevel = minor->cdevOpenLevel;
            minor->cdevOpenLevel = DK_ADD_ACCESS(wasOpenLevel, access);
        }

        gIOMediaBSDClientGlobals.unlockState();     // (enable access to tables)

        if ( level != levelOut )                        // (issue open/upgrade?)
        {
            bool success;

            media = minor->media;
            minor->media->retain();

            success = minor->media->open(minor->client, 0, levelOut);    // (go)

            if ( success == false )
            {
                gIOMediaBSDClientGlobals.lockState();        // (disable access)

                if ( S_ISBLK(devtype) )                          // (undo state)
                {
                    minor->bdevOpen--;
                    minor->bdevOpenLevel = wasOpenLevel;
                }
                else
                {
                    minor->cdevOpen--;
                    minor->cdevOpenLevel = wasOpenLevel;
                }

                assert(minor->isOrphaned == false);

                if ( !minor->bdevOpen && !minor->cdevOpen && minor->isObsolete )
                {
                    gIOMediaBSDClientGlobals.getMinors()->remove(getminor(dev));
                }

                gIOMediaBSDClientGlobals.unlockState();       // (enable access)

                error = EBUSY;
            }
        }
    }
    else
    {
        gIOMediaBSDClientGlobals.unlockState();     // (enable access to tables)
    }

    gIOMediaBSDClientGlobals.unlockOpen();   // (enable access to opens, closes)

    //
    // Wait until I/O Kit has finished to attempt to match storage drivers
    // or terminate storage drivers, should the media object have been re-
    // registered or its storage driver been terminated as a result of the
    // open.
    //

    if ( media )
    {
        media->waitQuiet();
        media->release();
    }

    return error;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int dkclose(dev_t dev, int /* flags */, int devtype, proc_t /* proc */)
{
    //
    // dkclose closes the device (called on last close).
    //

    IOStorageAccess level;
    IOStorageAccess levelOut;
    IOMedia *       media;
    MinorSlot *     minor;

    assert(S_ISBLK(devtype) || S_ISCHR(devtype));

    gIOMediaBSDClientGlobals.lockOpen();    // (disable access to opens, closes)
    gIOMediaBSDClientGlobals.lockState();   // (disable access to state, tables)

    media = 0;
    minor = gIOMediaBSDClientGlobals.getMinor(getminor(dev));

    level = DK_ADD_ACCESS(minor->bdevOpenLevel, minor->cdevOpenLevel);

    if ( S_ISBLK(devtype) )                                    // (update state)
    {
        minor->bdevBlockSize = minor->media->getPreferredBlockSize();
        minor->bdevOpen      = 0;
        minor->bdevOpenLevel = kIOStorageAccessNone;
    }
    else
    {
        minor->cdevOpen      = 0;
        minor->cdevOpenLevel = kIOStorageAccessNone;
    }

    levelOut = DK_ADD_ACCESS(minor->bdevOpenLevel, minor->cdevOpenLevel);

    if ( minor->isOrphaned )                              // (is minor in flux?)
    {
        //
        // We have determined that the specified minor is in "open flux".  This
        // means we are in a state where the media object has been closed, only
        // the device node is still open.  This happens to the minor subsequent
        // to a DKIOCEJECT ioctl -- this close resets the flux state to normal.
        //

        minor->isOrphaned = false;

        // If this minor is marked as obsolete, then we've already received the
        // media's termination notification, but the minor is yet to be removed
        // from the table -- remove it now.

        assert(minor->bdevOpen == 0);
        assert(minor->cdevOpen == 0);

        if ( minor->isObsolete )
        {
            gIOMediaBSDClientGlobals.getMinors()->remove(getminor(dev));
        }

        gIOMediaBSDClientGlobals.unlockState();     // (enable access to tables)
    }
    else if ( !minor->bdevOpen && !minor->cdevOpen )
    {
        //
        // We communicate the close down to the media object once all opens are
        // gone, on both the block and character device nodes.
        //

        IOMediaBSDClient * client;

        client = minor->client;
        minor->client->retain();

        media = minor->media;
        minor->media->retain();

        // If this minor is marked as obsolete, then we've already received the
        // media's termination notification, but the minor is yet to be removed
        // from the table -- remove it now.

        if ( minor->isObsolete )
        {
            gIOMediaBSDClientGlobals.getMinors()->remove(getminor(dev));
        }

        gIOMediaBSDClientGlobals.unlockState();     // (enable access to tables)

        media->close(client);                                            // (go)

        client->release();
    }
    else if ( level != levelOut )
    {
        //
        // We communicate the downgrade down to the media object.
        //

        media = minor->media;
        minor->media->retain();

        gIOMediaBSDClientGlobals.unlockState();     // (enable access to tables)

        minor->media->open(minor->client, 0, levelOut);                  // (go)
    }
    else
    {
        gIOMediaBSDClientGlobals.unlockState();     // (enable access to tables)
    }

    gIOMediaBSDClientGlobals.unlockOpen();   // (enable access to opens, closes)

    //
    // Wait until I/O Kit has finished to attempt to match storage drivers,
    // should the media object have been re-registered as a result of this
    // close.
    //

    if ( media )
    {
        media->waitQuiet();
        media->release();
    }

    return 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int dkread(dev_t dev, uio_t uio, int /* flags */)
{
    //
    // dkread reads data from a device.
    //

    struct dio dio = { dev, uio };

    return dkreadwrite(&dio, DKRTYPE_DIO);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int dkwrite(dev_t dev, uio_t uio, int /* flags */)
{
    //
    // dkwrite writes data to a device.
    //

    struct dio dio = { dev, uio };

    return dkreadwrite(&dio, DKRTYPE_DIO);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void dkstrategy(buf_t bp)
{
    //
    // dkstrategy starts an asynchronous read or write operation.  It returns
    // to the caller as soon as the operation is queued, and completes it via
    // the buf_biodone function.
    //

    dkreadwrite(bp, DKRTYPE_BUF);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int dkioctl(dev_t dev, u_long cmd, caddr_t data, int flags, proc_t proc)
{
    //
    // dkioctl performs operations other than a read or write.
    //

    int         error = 0;
    MinorSlot * minor = gIOMediaBSDClientGlobals.getMinor(getminor(dev));

    if ( minor->isOrphaned )  return EBADF;               // (is minor in flux?)

    //
    // Process the ioctl.
    //

    switch ( cmd )
    {
        case DKIOCGETBLOCKSIZE:                                  // (uint32_t *)
        {
            //
            // This ioctl returns the preferred block size of the media object.
            //

            *(uint32_t *)data = minor->media->getPreferredBlockSize();

        } break;

        case DKIOCGETBLOCKCOUNT32:                               // (uint32_t *)
        {
            //
            // This ioctl returns the size of the media object in blocks.  The
            // implied block size is returned by DKIOCGETBLOCKSIZE.
            //

            if ( minor->media->getPreferredBlockSize() )
                *(uint32_t *)data = ( minor->media->getSize()               / 
                                      minor->media->getPreferredBlockSize() );
            else
                *(uint32_t *)data = 0;

        } break;

        case DKIOCGETBLOCKCOUNT:                                 // (uint64_t *)
        {
            //
            // This ioctl returns the size of the media object in blocks.  The
            // implied block size is returned by DKIOCGETBLOCKSIZE.
            //

            if ( minor->media->getPreferredBlockSize() )
                *(uint64_t *)data = ( minor->media->getSize()               / 
                                      minor->media->getPreferredBlockSize() );
            else
                *(uint64_t *)data = 0;

        } break;

        case DKIOCGETMAXBLOCKCOUNTREAD:                          // (uint64_t *)
        {
            //
            // This ioctl returns the maximum block count for reads.
            //

            OSNumber * number = OSDynamicCast(
                         /* class  */ OSNumber,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOMaximumBlockCountReadKey,
                                 /* plane */ gIOServicePlane ) );
            if ( number )
                *(uint64_t *)data = number->unsigned64BitValue();
            else
                *(uint64_t *)data = 0;

        } break;

        case DKIOCGETMAXBLOCKCOUNTWRITE:                         // (uint64_t *)
        {
            //
            // This ioctl returns the maximum block count for writes.
            //

            OSNumber * number = OSDynamicCast(
                         /* class  */ OSNumber,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOMaximumBlockCountWriteKey,
                                 /* plane */ gIOServicePlane ) );
            if ( number )
                *(uint64_t *)data = number->unsigned64BitValue();
            else
                *(uint64_t *)data = 0;

        } break;

        case DKIOCGETMAXBYTECOUNTREAD:                           // (uint64_t *)
        {
            //
            // This ioctl returns the maximum byte count for reads.
            //

            OSNumber * number = OSDynamicCast(
                         /* class  */ OSNumber,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOMaximumByteCountReadKey,
                                 /* plane */ gIOServicePlane ) );
            if ( number )
                *(uint64_t *)data = number->unsigned64BitValue();
            else
                *(uint64_t *)data = 0;

        } break;

        case DKIOCGETMAXBYTECOUNTWRITE:                          // (uint64_t *)
        {
            //
            // This ioctl returns the maximum byte count for writes.
            //

            OSNumber * number = OSDynamicCast(
                         /* class  */ OSNumber,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOMaximumByteCountWriteKey,
                                 /* plane */ gIOServicePlane ) );
            if ( number )
                *(uint64_t *)data = number->unsigned64BitValue();
            else
                *(uint64_t *)data = 0;

        } break;

        case DKIOCGETMAXSEGMENTCOUNTREAD:                        // (uint64_t *)
        {
            //
            // This ioctl returns the maximum segment count for reads.
            //

            OSNumber * number = OSDynamicCast(
                         /* class  */ OSNumber,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOMaximumSegmentCountReadKey,
                                 /* plane */ gIOServicePlane ) );
            if ( number )
                *(uint64_t *)data = number->unsigned64BitValue();
            else
                *(uint64_t *)data = 0;

        } break;

        case DKIOCGETMAXSEGMENTCOUNTWRITE:                       // (uint64_t *)
        {
            //
            // This ioctl returns the maximum segment count for writes.
            //

            OSNumber * number = OSDynamicCast(
                         /* class  */ OSNumber,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOMaximumSegmentCountWriteKey,
                                 /* plane */ gIOServicePlane ) );
            if ( number )
                *(uint64_t *)data = number->unsigned64BitValue();
            else
                *(uint64_t *)data = 0;

        } break;

        case DKIOCGETMAXSEGMENTBYTECOUNTREAD:                    // (uint64_t *)
        {
            //
            // This ioctl returns the maximum segment byte count for reads.
            //

            OSNumber * number = OSDynamicCast(
                         /* class  */ OSNumber,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOMaximumSegmentByteCountReadKey,
                                 /* plane */ gIOServicePlane ) );
            if ( number )
                *(uint64_t *)data = number->unsigned64BitValue();
            else
                *(uint64_t *)data = 0;

        } break;

        case DKIOCGETMAXSEGMENTBYTECOUNTWRITE:                   // (uint64_t *)
        {
            //
            // This ioctl returns the maximum segment byte count for writes.
            //

            OSNumber * number = OSDynamicCast(
                         /* class  */ OSNumber,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOMaximumSegmentByteCountWriteKey,
                                 /* plane */ gIOServicePlane ) );
            if ( number )
                *(uint64_t *)data = number->unsigned64BitValue();
            else
                *(uint64_t *)data = 0;

        } break;

        case DKIOCGETMINSEGMENTALIGNMENTBYTECOUNT:               // (uint64_t *)
        {
            //
            // This ioctl returns the minimum segment alignment in bytes.
            //

            OSNumber * number = OSDynamicCast(
                         /* class  */ OSNumber,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOMinimumSegmentAlignmentByteCountKey,
                                 /* plane */ gIOServicePlane ) );
            if ( number )
                *(uint64_t *)data = number->unsigned64BitValue();
            else
                *(uint64_t *)data = 0;

        } break;

        case DKIOCGETMAXSEGMENTADDRESSABLEBITCOUNT:              // (uint64_t *)
        {
            //
            // This ioctl returns the maximum segment width in bits.
            //

            OSNumber * number = OSDynamicCast(
                         /* class  */ OSNumber,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOMaximumSegmentAddressableBitCountKey,
                                 /* plane */ gIOServicePlane ) );
            if ( number )
                *(uint64_t *)data = number->unsigned64BitValue();
            else
                *(uint64_t *)data = 0;

        } break;

        case DKIOCGETPHYSICALBLOCKSIZE:                          // (uint32_t *)
        {
            //
            // This ioctl returns the preferred block size of the device.
            //

            OSNumber * number = OSDynamicCast(
                         /* class  */ OSNumber,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOPropertyPhysicalBlockSizeKey,
                                 /* plane */ gIOServicePlane ) );
            if ( number )
                *(uint32_t *)data = number->unsigned32BitValue();
            else
                *(uint32_t *)data = minor->media->getPreferredBlockSize();

        } break;

        case DKIOCISFORMATTED:                                   // (uint32_t *)
        {
            //
            // This ioctl returns truth if the media object is formatted.
            //

            *(uint32_t *)data = minor->media->isFormatted();

        } break;

        case DKIOCISWRITABLE:                                    // (uint32_t *)
        {
            //
            // This ioctl returns truth if the media object is writable.
            //

            *(uint32_t *)data = minor->media->isWritable();

        } break;

        case DKIOCEJECT:                                               // (void)
        {
            //
            // This ioctl asks that the media object be ejected from the device.
            //

            IOMediaBSDClient *     client;
            IOBlockStorageDriver * driver;
            MinorTable *           minors;
            IOReturn               status;

            client = minor->client;
            driver = (IOBlockStorageDriver *) minor->media->getProvider();
            driver = OSDynamicCast(IOBlockStorageDriver, driver);
            minors = gIOMediaBSDClientGlobals.getMinors();

            // Determine whether this media has an IOBlockStorageDriver parent.

            if ( driver == 0 )  { error = ENOTTY;  break; }

            // Disable access to opens, closes, tables.

            gIOMediaBSDClientGlobals.lockOpen();
            gIOMediaBSDClientGlobals.lockState();

            // Determine whether there are other opens on the device nodes that
            // are associated with this anchor -- the one valid open is the one
            // that issued this eject.  If all is well, we then attempt to open
            // the block storage driver to make the ejection request.

            if ( minors->getOpenCountForAnchorID(minor->anchorID) == 1 &&
                 driver->open(client, 0, kIOStorageAccessReaderWriter) )
            {
                // Mark the minor as being in "open flux".  This means we are in
                // a state where the media object has been closed but the device
                // node is still open; we must reject all future accesses to the
                // device node until it is closed.  Note that we do this both on
                // success and failure of the ejection call.

                minor->isOrphaned = true;

                // Enable access to opens, closes, tables.

                gIOMediaBSDClientGlobals.unlockState();
                gIOMediaBSDClientGlobals.unlockOpen();

                // Close the media object before the ejection request is made.

                minor->media->close(client);

                // Retain the media's BSD client object, as it is about
                // to be terminated, and we still need it for the close.

                client->retain();

                // Eject the media from the drive.

                status = driver->ejectMedia();
                error  = driver->errnoFromReturn(status);

                // Close the block storage driver.

                driver->close(client);

                // Release the media's BSD client object.

                client->release();
            }
            else
            {
                error = EBUSY;

                // Enable access to opens, closes, tables.

                gIOMediaBSDClientGlobals.unlockState();
                gIOMediaBSDClientGlobals.unlockOpen();
            }

        } break;

        case DKIOCFORMAT:                            // (dk_format_capacity_t *)
        {
            //
            // This ioctl asks that the media object be formatted.
            //

            IOMediaBSDClient *     client;
            IOBlockStorageDriver * driver;
            MinorTable *           minors;
            dk_format_capacity_t * request;
            IOReturn               status;

            client  = minor->client;
            driver  = (IOBlockStorageDriver *) minor->media->getProvider();
            driver  = OSDynamicCast(IOBlockStorageDriver, driver);
            minors  = gIOMediaBSDClientGlobals.getMinors();
            request = (dk_format_capacity_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0xF000) )  { error = EINVAL;  break; }

            // Determine whether this media has an IOBlockStorageDriver parent.

            if ( driver == 0 )  { error = ENOTTY;  break; }

            // Disable access to opens, closes, tables.

            gIOMediaBSDClientGlobals.lockOpen();
            gIOMediaBSDClientGlobals.lockState();

            // Determine whether there are other opens on the device nodes that
            // are associated with this anchor -- the one valid open is the one
            // that issued the format.  If all is well, we then attempt to open
            // the block storage driver to make the formatting request.

            if ( minors->getOpenCountForAnchorID(minor->anchorID) == 1 &&
                 driver->open(client, 0, kIOStorageAccessReaderWriter) )
            {
                UInt64 capacity = request->blockCount * request->blockSize;

                // Mark the minor as being in "open flux".  This means we are in
                // a state where the media object has been closed but the device
                // node is still open; we must reject all future accesses to the
                // device node until it is closed.  Note that we do this both on
                // success and failure of the formatting call.

                minor->isOrphaned = true;

                // Enable access to opens, closes, tables.

                gIOMediaBSDClientGlobals.unlockState();
                gIOMediaBSDClientGlobals.unlockOpen();

                // Close the media object before the formatting request is made.

                minor->media->close(client);

                // Retain the media's BSD client object, as it is about
                // to be terminated, and we still need it for the close.

                client->retain();

                // Format the media in the drive.

                status = driver->formatMedia(capacity);
                error  = driver->errnoFromReturn(status);

                // Wait until I/O Kit has finished to attempt to match storage
                // drivers, since the media object will have been re-published.
                // This shall ensure the new IOMediaBSDClient reconnects prior
                // to our return from DKIOCFORMAT.  Note that we still recover
                // correctly in case the media object doesn't get re-published,
                // as though an ejection had taken place.

                driver->waitQuiet();

                // Close the block storage driver.

                driver->close(client);

                // Release the media's BSD client object.

                client->release();
            }
            else
            {
                error = EBUSY;

                // Enable access to opens, closes, tables.

                gIOMediaBSDClientGlobals.unlockState();
                gIOMediaBSDClientGlobals.unlockOpen();
            }

        } break;

        case DKIOCGETFORMATCAPACITIES:             // (dk_format_capacities_t *)
        {
            //
            // This ioctl returns the feasible format capacities for this media
            // object.
            //

            typedef struct
            {
                user_addr_t capacities;
                uint32_t    capacitiesCount;

                uint8_t     reserved0096[4];
            } dk_format_capacities_64_t;

            UInt64                      blockSize;
            UInt64 *                    capacities;
            UInt32                      capacitiesCount;
            UInt32                      capacitiesMaxCount;
            IOBlockStorageDriver *      driver;
            dk_format_capacities_64_t * request;
            dk_format_capacities_t *    request32;
            dk_format_capacities_64_t   request64 = { 0 };

            driver    = (IOBlockStorageDriver *) minor->media->getProvider();
            driver    = OSDynamicCast(IOBlockStorageDriver, driver);
            request   = (dk_format_capacities_64_t *) data;
            request32 = (dk_format_capacities_t *) data;

            if ( proc_is64bit(proc) == 0 )
            {
                request64.capacities      = CAST_USER_ADDR_T(request32->capacities);
                request64.capacitiesCount = request32->capacitiesCount;

                request = &request64;
            }

            if ( DKIOC_IS_RESERVED(data, 0xF000) )  { error = EINVAL;  break; }

            // Determine whether this media has an IOBlockStorageDriver parent.

            if ( driver == 0 )  { error = ENOTTY;  break; }

            // Obtain the format capacities list from the block storage driver.

            capacitiesCount    = request->capacitiesCount;
            capacitiesMaxCount = driver->getFormatCapacities(0, 0);

            if ( capacitiesCount )
            {
                if ( request->capacities == 0 )  { error = EINVAL;  break; }

                capacitiesCount = min(capacitiesCount, capacitiesMaxCount);
                capacities      = IONew(UInt64, capacitiesCount);

                if ( capacities == 0 )  { error = ENOMEM;  break; }

                driver->getFormatCapacities(capacities, capacitiesCount);

                blockSize = minor->media->getPreferredBlockSize();
                if ( blockSize == 0 )  blockSize = DEV_BSIZE;

                // Construct the format capacities list for client consumption.

                for ( UInt32 index = 0; index < capacitiesCount; index++ )
                {
                    dk_format_capacity_t capacity = { 0 };

                    capacity.blockCount = capacities[index] / blockSize;
                    capacity.blockSize  = blockSize;

                    error = copyout(
                      /* kaddr */ &capacity,
                      /* uaddr */ request->capacities + index * sizeof(dk_format_capacity_t),
                      /* len   */ sizeof(dk_format_capacity_t) );
                    if ( error )  break; 
                }

                IODelete(capacities, UInt64, capacitiesCount);

                if ( capacitiesCount < capacitiesMaxCount )  { error = E2BIG; }
            }

            request->capacitiesCount = capacitiesMaxCount;

            if ( proc_is64bit(proc) == 0 )
            {
                request32->capacitiesCount = request64.capacitiesCount;
            }

        } break;

        case DKIOCSYNCHRONIZECACHE:                                    // (void)
        {
            //
            // This ioctl asks that the media object be flushed onto the device.
            //

            IOReturn status;

            // Flush the media onto the drive.

            status = minor->media->synchronizeCache(minor->client);
            error  = minor->media->errnoFromReturn(status);

        } break;

        case DKIOCDISCARD:                                     // (dk_discard_t)
        {
            //
            // This ioctl asks that the media object delete unused data.
            //

            dk_discard_t * request;
            IOReturn       status;

            request = (dk_discard_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0xFFFF0000) )  { error = EINVAL;  break; }

            // Delete unused data from the media.

            status = minor->media->discard( /* client    */ minor->client,
                                            /* byteStart */ request->offset,
                                            /* byteCount */ request->length );

            error = minor->media->errnoFromReturn(status);

        } break;

        case DKIOCREQUESTIDLE:                                         // (void)
        {
            //
            // This ioctl asks that the device enter an idle state.
            //

            IOBlockStorageDriver * driver;
            IOReturn               status;

            driver = (IOBlockStorageDriver *) minor->media->getProvider();
            driver = OSDynamicCast(IOBlockStorageDriver, driver);

            // Determine whether this media has an IOBlockStorageDriver parent.

            if ( driver == 0 )  { error = ENOTTY;  break; }

            // Request that the drive enter an idle state.

            status = driver->requestIdle();
            error  = minor->media->errnoFromReturn(status);

        } break;

        case DKIOCGETBSDUNIT:                                    // (uint32_t *)
        {
            //
            // This ioctl returns the BSD unit of the media object.
            //

            OSNumber * number = OSDynamicCast(
                         /* class  */ OSNumber,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOBSDUnitKey ) );
            if ( number )
                *(uint32_t *)data = number->unsigned32BitValue();
            else
                *(uint32_t *)data = 0;

        } break;

        case DKIOCGETFIRMWAREPATH:                     // (dk_firmware_path_t *)
        {
            //
            // This ioctl returns the open firmware path for this media object.
            //

            int    l = sizeof(((dk_firmware_path_t *)data)->path);
            char * p = ((dk_firmware_path_t *)data)->path;

            if ( minor->media->getPath(p, &l, gIODTPlane) && strchr(p, ':') )
                strlcpy(p, strchr(p, ':') + 1, l);     // (strip the plane name)
            else
                error = EINVAL;

        } break;

        case DKIOCISVIRTUAL:                                     // (uint32_t *)
        {
            //
            // This ioctl returns truth if the device is virtual.
            //

            OSDictionary * dictionary = OSDynamicCast(
                         /* class  */ OSDictionary,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOPropertyProtocolCharacteristicsKey,
                                 /* plane */ gIOServicePlane ) );

            *(uint32_t *)data = false;

            if ( dictionary )
            {
                OSString * string = OSDynamicCast( 
                         /* class  */ OSString,
                         /* object */ dictionary->getObject(
                                 /* key   */ kIOPropertyPhysicalInterconnectTypeKey ) );

                if ( string && string->isEqualTo( kIOPropertyPhysicalInterconnectTypeVirtual ) )
                    *(uint32_t *)data = true;
            }

        } break;

        case DKIOCGETBASE:                                       // (uint64_t *)
        {
            //
            // This ioctl returns the base of the media object.
            //

            *(uint64_t *)data = minor->media->getBase();

        } break;

        case DKIOCGETFEATURES:                                   // (uint32_t *)
        {
            //
            // This ioctl returns the features of the media object.
            //

            OSDictionary * dictionary = OSDynamicCast(
                         /* class  */ OSDictionary,
                         /* object */ minor->media->getProperty(
                                 /* key   */ kIOStorageFeaturesKey,
                                 /* plane */ gIOServicePlane ) );

            *(uint32_t *)data = 0;

            if ( dictionary )
            {
                OSBoolean * boolean;

                boolean = OSDynamicCast( 
                         /* class  */ OSBoolean,
                         /* object */ dictionary->getObject(
                                 /* key   */ kIOStorageFeatureDiscard ) );

                if ( boolean == kOSBooleanTrue )
                    *(uint32_t *)data |= DK_FEATURE_DISCARD;

                boolean = OSDynamicCast( 
                         /* class  */ OSBoolean,
                         /* object */ dictionary->getObject(
                                 /* key   */ kIOStorageFeatureForceUnitAccess ) );

                if ( boolean == kOSBooleanTrue )
                    *(uint32_t *)data |= DK_FEATURE_FORCE_UNIT_ACCESS;
            }

        } break;

        default:
        {
            //
            // Call the foreign ioctl handler for all other ioctls.
            //

            error = minor->client->ioctl(dev, cmd, data, flags, proc);

        } break;
    }

    return error;                                       // (return error status)
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int dkioctl_bdev(dev_t dev, u_long cmd, caddr_t data, int flags, proc_t proc)
{
    //
    // dkioctl_bdev performs operations other than a read or write, specific to
    // the block device.
    //

    int         error = 0;
    MinorSlot * minor = gIOMediaBSDClientGlobals.getMinor(getminor(dev));

    if ( minor->isOrphaned )  return EBADF;               // (is minor in flux?)

    //
    // Process the ioctl.
    //

    switch ( cmd )
    {
        case DKIOCGETBLOCKSIZE:                                  // (uint32_t *)
        {
            //
            // This ioctl returns the preferred (or overrided) block size of the
            // media object.
            //

            *(uint32_t *)data = minor->bdevBlockSize;

        } break;

        case DKIOCSETBLOCKSIZE:                                  // (uint32_t *)
        {
            //
            // This ioctl overrides the block size for the media object, for the
            // duration of all block device opens at this minor.
            //

            if ( *(uint32_t *)data > 0 )
                minor->bdevBlockSize = (UInt64) (*(uint32_t *)data);
            else
                error = EINVAL;

        } break;

        case DKIOCGETBLOCKCOUNT32:                               // (uint32_t *)
        {
            //
            // This ioctl returns the size of the media object in blocks.  The
            // implied block size is returned by DKIOCGETBLOCKSIZE.
            //

            if ( minor->bdevBlockSize )
                *(uint32_t *)data = ( minor->media->getSize() /
                                      minor->bdevBlockSize    );
            else
                *(uint32_t *)data = 0;

        } break;

        case DKIOCGETBLOCKCOUNT:                                 // (uint64_t *)
        {
            //
            // This ioctl returns the size of the media object in blocks.  The
            // implied block size is returned by DKIOCGETBLOCKSIZE.
            //

            if ( minor->bdevBlockSize )
                *(uint64_t *)data = ( minor->media->getSize() /
                                      minor->bdevBlockSize    );
            else
                *(uint64_t *)data = 0;

        } break;

        default:
        {
            //
            // Call the common ioctl handler for all other ioctls.
            //

            error = dkioctl(dev, cmd, data, flags, proc);

        } break;
    }

    return error;                                       // (return error status)
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int dksize(dev_t dev)
{
    //
    // dksize returns the block size of the media.
    //
    // This is a departure from BSD 4.4's definition of this function, that is,
    // it will not return the size of the disk partition, as would be expected
    // in a BSD 4.4 implementation.
    //

    MinorSlot * minor = gIOMediaBSDClientGlobals.getMinor(getminor(dev));

    if ( minor->isOrphaned )  return 0;                   // (is minor in flux?)

    return (int) minor->bdevBlockSize;                    // (return block size)
}

// =============================================================================
// Support For BSD Functions

extern "C" task_t get_aiotask();

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline task_t get_kernel_task()
{
    return kernel_task;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline task_t get_user_task()
{
    task_t task;

    task = get_aiotask();

    if ( task == 0 )  task = current_task();

    return task;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline dev_t DKR_GET_DEV(dkr_t dkr, dkrtype_t dkrtype)
{
    return (dkrtype == DKRTYPE_BUF)
           ? buf_device((buf_t)dkr)
           : ((dio_t)dkr)->dev;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline UInt64 DKR_GET_BYTE_COUNT(dkr_t dkr, dkrtype_t dkrtype)
{
    return (dkrtype == DKRTYPE_BUF)
           ? buf_count((buf_t)dkr)
           : uio_resid(((dio_t)dkr)->uio);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline UInt64 DKR_GET_BYTE_START(dkr_t dkr, dkrtype_t dkrtype)
{
    if (dkrtype == DKRTYPE_BUF)
    {
        buf_t       bp = (buf_t)dkr;
        MinorSlot * minor;

        minor = gIOMediaBSDClientGlobals.getMinor(getminor(buf_device(bp)));

        return (UInt64)buf_blkno(bp) * minor->bdevBlockSize;
    }

    return uio_offset(((dio_t)dkr)->uio);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline bool DKR_IS_READ(dkr_t dkr, dkrtype_t dkrtype)
{
    return (dkrtype == DKRTYPE_BUF)
           ? ((buf_flags((buf_t)dkr) & B_READ) == B_READ)
           : ((uio_rw(((dio_t)dkr)->uio)) == UIO_READ);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline bool DKR_IS_ASYNCHRONOUS(dkr_t dkr, dkrtype_t dkrtype)
{
    return (dkrtype == DKRTYPE_BUF)
           ? true
           : false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline bool DKR_IS_RAW(dkr_t dkr, dkrtype_t dkrtype)
{
    return (dkrtype == DKRTYPE_BUF)
           ? false
           : true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline void DKR_SET_BYTE_COUNT(dkr_t dkr, dkrtype_t dkrtype, UInt64 bcount)
{
    if (dkrtype == DKRTYPE_BUF)
        buf_setresid((buf_t)dkr, buf_count((buf_t)dkr) - bcount);
    else
        uio_setresid(((dio_t)dkr)->uio, uio_resid(((dio_t)dkr)->uio) - bcount);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline void DKR_RUN_COMPLETION(dkr_t dkr, dkrtype_t dkrtype, IOReturn status)
{
    if (dkrtype == DKRTYPE_BUF)
    {
        buf_t       bp = (buf_t)dkr;
        MinorSlot * minor;

        minor = gIOMediaBSDClientGlobals.getMinor(getminor(buf_device(bp)));

        buf_seterror(bp, minor->media->errnoFromReturn(status));     // (error?)
        buf_biodone(bp);                                   // (complete request)
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline IOMemoryDescriptor * DKR_GET_BUFFER(dkr_t dkr, dkrtype_t dkrtype)
{
    if (dkrtype == DKRTYPE_BUF)
    {
        buf_t bp = (buf_t)dkr;
        int   flags;

        flags = buf_flags(bp);

        if ( (flags & B_CLUSTER) )
        {
            IOOptionBits options = kIOMemoryTypeUPL | kIOMemoryAsReference;

            options |= (flags & B_READ) ? kIODirectionIn : kIODirectionOut;

            return IOMemoryDescriptor::withOptions(          // (multiple-range)
                buf_upl(bp),
                buf_count(bp),
                buf_uploffset(bp),
                0,
                options );
        }
        else
        {
            return IOMemoryDescriptor::withAddress(            // (single-range)
                buf_dataptr(bp),
                buf_count(bp),
                (flags & B_READ) ? kIODirectionIn : kIODirectionOut,
                (flags & B_PHYS) ? get_user_task() : get_kernel_task() );
        }
    }
    else
    {
        IOOptionBits options = kIOMemoryTypeUIO | kIOMemoryAsReference;
        uio_t        uio     = ((dio_t)dkr)->uio;

        options |= (uio_rw(uio) == UIO_READ) ? kIODirectionIn : kIODirectionOut;

        return IOMemoryDescriptor::withOptions(              // (multiple-range)
            uio,
            uio_iovcnt(uio),
            0,
            (uio_isuserspace(uio)) ? get_user_task() : get_kernel_task(),
            options );
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline void * DKR_GET_DRIVER_DATA(dkr_t dkr, dkrtype_t dkrtype)
{
    return (dkrtype == DKRTYPE_BUF)
           ? buf_drvdata((buf_t)dkr)
           : ((dio_t)dkr)->drvdata;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline void DKR_SET_DRIVER_DATA(dkr_t dkr, dkrtype_t dkrtype, void * drvdata)
{
    if (dkrtype == DKRTYPE_BUF)
        buf_setdrvdata((buf_t)dkr, drvdata);
    else
        ((dio_t)dkr)->drvdata = drvdata;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline IOStorageAttributes DKR_GET_ATTRIBUTES(dkr_t dkr, dkrtype_t dkrtype)
{
    IOStorageAttributes attributes = { 0 };

    if (dkrtype == DKRTYPE_BUF)
    {
        buf_t bp = (buf_t)dkr;
        int   flags;

        flags = buf_flags(bp);

        attributes.options |= (flags & B_FUA) ? kIOStorageOptionForceUnitAccess : 0;
    }

    return attributes;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int dkreadwrite(dkr_t dkr, dkrtype_t dkrtype)
{
    //
    // dkreadwrite performs a read or write operation.
    //

    IOMemoryDescriptor * buffer;
    register UInt64      byteCount;
    register UInt64      byteStart;
    UInt64               mediaSize;
    MinorSlot *          minor;
    IOReturn             status;

    DKR_SET_DRIVER_DATA(dkr, dkrtype, 0);

    minor = gIOMediaBSDClientGlobals.getMinor(getminor(DKR_GET_DEV(dkr, dkrtype)));

    if ( minor->isOrphaned )                              // (is minor in flux?)
    {
        status = kIOReturnNoMedia;
        goto dkreadwriteErr;
    }

    if ( minor->media->isFormatted() == false )       // (is media unformatted?)
    {
        status = kIOReturnUnformattedMedia;
        goto dkreadwriteErr;
    }

    byteCount = DKR_GET_BYTE_COUNT(dkr, dkrtype);            // (get byte count)
    byteStart = DKR_GET_BYTE_START(dkr, dkrtype);            // (get byte start)
    mediaSize = minor->media->getSize();                     // (get media size)

    //
    // Reads that start at (or perhaps past) the end-of-media are not considered
    // errors, even though no data is transferred, while writes at (or past) the
    // end-of-media do indeed return errors under BSD semantics.
    // 

    if ( byteStart >= mediaSize )     // (is start at or past the end-of-media?)
    {
        status = DKR_IS_READ(dkr,dkrtype) ? kIOReturnSuccess : kIOReturnIOError;
        goto dkreadwriteErr;
    }

    //
    // Reads and writes, via the character device, that do not start or end on a
    // media block boundary are considered errors under BSD semantics.
    //

    if ( DKR_IS_RAW(dkr, dkrtype) )
    {
        UInt64 mediaBlockSize = minor->media->getPreferredBlockSize();

        if ( (byteStart % mediaBlockSize) || (byteCount % mediaBlockSize) )
        {
            status = kIOReturnNotAligned;
            goto dkreadwriteErr;
        }
    }

    //
    // Build a descriptor which describes the buffer involved in the transfer.
    //

    buffer = DKR_GET_BUFFER(dkr, dkrtype);

    if ( buffer == 0 )                                           // (no buffer?)
    {
        status = kIOReturnNoMemory;
        goto dkreadwriteErr;
    }

    //
    // Reads and writes that extend beyond the end-of-media are not considered
    // errors under BSD semantics.  We are to transfer as many bytes as can be
    // read or written from the medium and return no error.  This differs from
    // IOMedia semantics which is to fail the entire request without copying a
    // single byte should it include something past the end-of-media.  We must
    // adapt the IOMedia semantics to look like BSD semantics here.
    // 
    // Clip the transfer buffer should this be a short read or write request.
    //

    if ( byteCount > mediaSize - byteStart )           // (clip at end-of-media)
    {
        IOMemoryDescriptor * originalBuffer = buffer;

        buffer = IOMemoryDescriptor::withSubRange(
                           /* descriptor    */ originalBuffer,
                           /* withOffset    */ 0,
                           /* withLength    */ mediaSize - byteStart,
                           /* withDirection */ originalBuffer->getDirection() );

        originalBuffer->release();   // (either retained above or about to fail)

        if ( buffer == 0 )                                      // (no buffer?)
        {
            status = kIOReturnNoMemory;
            goto dkreadwriteErr;
        }
    }

    //
    // Prepare the transfer.
    //

    if ( buffer->prepare() != kIOReturnSuccess )         // (prepare the buffer)
    {
        buffer->release();
        status = kIOReturnVMError;            // (wiring or permissions failure)
        goto dkreadwriteErr;
    }

    //
    // Execute the transfer.
    //

    DKR_SET_DRIVER_DATA(dkr, dkrtype, buffer);

    if ( DKR_IS_ASYNCHRONOUS(dkr, dkrtype) )       // (an asynchronous request?)
    {
        IOStorageAttributes attributes;
        IOStorageCompletion completion;

        attributes           = DKR_GET_ATTRIBUTES(dkr, dkrtype);

        completion.target    = dkr;
        completion.action    = dkreadwritecompletion;
        completion.parameter = (void *) dkrtype;

        if ( DKR_IS_READ(dkr, dkrtype) )                            // (a read?)
        {
            minor->media->read(  /* client     */ minor->client,
                                 /* byteStart  */ byteStart,
                                 /* buffer     */ buffer,
                                 /* attributes */ &attributes,
                                 /* completion */ &completion );         // (go)
        }
        else                                                       // (a write?)
        {
            minor->media->write( /* client     */ minor->client,
                                 /* byteStart  */ byteStart,
                                 /* buffer     */ buffer,
                                 /* attributes */ &attributes,
                                 /* completion */ &completion );         // (go)
        }

        status = kIOReturnSuccess;
    }
    else                                             // (a synchronous request?)
    {
        if ( DKR_IS_READ(dkr, dkrtype) )                            // (a read?)
        {
            status = minor->media->read(
                                 /* client          */ minor->client,
                                 /* byteStart       */ byteStart,
                                 /* buffer          */ buffer,
                                 /* actualByteCount */ &byteCount );     // (go)
        }
        else                                                       // (a write?)
        {
            status = minor->media->write(
                                 /* client          */ minor->client,
                                 /* byteStart       */ byteStart,
                                 /* buffer          */ buffer,
                                 /* actualByteCount */ &byteCount );     // (go)
        }

        dkreadwritecompletion(dkr, (void *)dkrtype, status, byteCount);
    }

    return minor->media->errnoFromReturn(status);       // (return error status)

dkreadwriteErr:

    dkreadwritecompletion(dkr, (void *)dkrtype, status, 0);

    return minor->media->errnoFromReturn(status);       // (return error status)
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void dkreadwritecompletion( void *   target,
                            void *   parameter,
                            IOReturn status,
                            UInt64   actualByteCount )
{
    //
    // dkreadwritecompletion cleans up after a read or write operation.
    //

    dkr_t       dkr     = (dkr_t) target;
    dkrtype_t   dkrtype = (dkrtype_t) (int) parameter;
    dev_t       dev     = DKR_GET_DEV(dkr, dkrtype);
    void *      drvdata = DKR_GET_DRIVER_DATA(dkr, dkrtype);
    MinorSlot * minor   = gIOMediaBSDClientGlobals.getMinor(getminor(dev));

    if ( drvdata )                                            // (has a buffer?)
    {
        IOMemoryDescriptor * buffer = (IOMemoryDescriptor *) drvdata;

        buffer->complete();                             // (complete the buffer)
        buffer->release();                 // (release our retain on the buffer)
    }

    if ( status != kIOReturnSuccess )                         // (has an error?)
    {
        IOLog("%s: %s.\n", minor->name, minor->media->stringFromReturn(status));
    }

    if ( DKR_IS_ASYNCHRONOUS(dkr, dkrtype) )       // (an asynchronous request?)
    {
        DKR_SET_BYTE_COUNT(dkr, dkrtype, actualByteCount);   // (set byte count)
        DKR_RUN_COMPLETION(dkr, dkrtype, status);            // (run completion)
    }
    else
    {
        DKR_SET_BYTE_COUNT(dkr, dkrtype, actualByteCount);   // (set byte count)
    }
}

// =============================================================================
// AnchorTable Class

AnchorTable::AnchorTable()
{
    //
    // Initialize this object's minimal state.
    //

    _table      = 0;
    _tableCount = 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

AnchorTable::~AnchorTable()
{
    //
    // Free all of this object's outstanding resources.
    //

    for ( UInt32 anchorID = 0; anchorID < _tableCount; anchorID++ )
        if ( _table[anchorID].isAssigned )  remove(anchorID);

    if ( _table )  IODelete(_table, AnchorSlot, _tableCount);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

UInt32 AnchorTable::insert(IOService * anchor, void * key)
{
    //
    // This method inserts the specified anchor into an unassigned slot in the
    // anchor table and returns its ID (or kInvalidAnchorID on a failure).
    //
    // Note that the anchor is transparently removed from the table should the
    // anchor terminate (or it is at least marked obsolete,  should references
    // to the anchor still exist in the minor table).
    //

    UInt32       anchorID;
    IONotifier * notifier;

    // Search for an unassigned slot in the anchor table.

    for ( anchorID = 0; anchorID < _tableCount; anchorID++ )
        if ( _table[anchorID].isAssigned == false )  break;

    // Was an unassigned slot found?  If not, grow the table.

    if ( anchorID == _tableCount )
    {
        AnchorSlot * newTable;
        UInt32       newTableCount;

        // We must expand the anchor table since no more slots are available.

        if ( _tableCount >= kAnchorsMaxCount )  return kInvalidAnchorID;

        newTableCount = min(kAnchorsAddCount + _tableCount, kAnchorsMaxCount);
        newTable      = IONew(AnchorSlot, newTableCount);

        if ( newTable == 0 )  return kInvalidAnchorID;

        bzero(newTable, newTableCount * sizeof(AnchorSlot));

        // Copy over the old table's entries, then free the old table.

        if ( _table )
        {
            bcopy(_table, newTable, _tableCount * sizeof(AnchorSlot));
            IODelete(_table, AnchorSlot, _tableCount);
        }
    
        // Obtain the next unassigned index (simple since we know the size of
        // the old table),  then update our instance variables to reflect the
        // new tables.

        anchorID    = _tableCount;
        _table      = newTable;
        _tableCount = newTableCount;
    }

    // Create a notification handler for the anchor's termination (post-stop);
    // the handler will remove the anchor transparently from the table if the
    // anchor terminates (or at least marks it obsolete, if references to the
    // anchor still exist in the minor table).

    notifier = anchor->registerInterest(
                          /* type        */ gIOGeneralInterest,
                          /* action      */ anchorWasNotified,
                          /* target      */ this,
                          /* parameter   */ 0 );
  
    if ( notifier == 0 )  return kInvalidAnchorID;

    // Zero the new slot, fill it in, and retain the anchor object.

    bzero(&_table[anchorID], sizeof(AnchorSlot)); // (zero slot)

    _table[anchorID].isAssigned = true;           // (fill in slot)
    _table[anchorID].isObsolete = false;
    _table[anchorID].anchor     = anchor;
    _table[anchorID].key        = key;
    _table[anchorID].notifier   = notifier;

    _table[anchorID].anchor->retain();            // (retain anchor)

    return anchorID;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void AnchorTable::remove(UInt32 anchorID)
{
    //
    // This method removes the specified anchor from the anchor table.
    //

    assert(anchorID < _tableCount);
    assert(_table[anchorID].isAssigned);

    // Release the resources retained in the anchor slot and zero it.

    _table[anchorID].notifier->remove();
    _table[anchorID].anchor->release();           // (release anchor)

    bzero(&_table[anchorID], sizeof(AnchorSlot)); // (zero slot)
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void AnchorTable::obsolete(UInt32 anchorID)
{
    //
    // This method obsoletes the specified anchor, that is, the slot is marked
    // as obsolete and will be removed later via the minor table remove method
    // once it detects references to the anchor ID drop to 0.   Once obsoleted,
    // the anchor can be considered to be removed, since it will not appear in
    // locate searches, even though behind the scenes it still occupies a slot.
    //

    assert(anchorID < _tableCount);
    assert(_table[anchorID].isAssigned);

    // Mark the anchor as obsolete so that it can be removed from the table as
    // soon as all its references go away (minor table's responsibility).

    _table[anchorID].isObsolete = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

UInt32 AnchorTable::locate(IOService * anchor)
{
    //
    // This method searches for the specified anchor in the anchor table and
    // returns its ID (or kInvalidAnchorID on a failure).  It would find the
    // first occurrence of the anchor in case multiple entries with the same
    // anchor object exist.  It ignores slots marked as obsolete.
    //

    for (UInt32 anchorID = 0; anchorID < _tableCount; anchorID++)
    {
        if ( _table[anchorID].isAssigned != false  &&
             _table[anchorID].isObsolete == false  &&
             _table[anchorID].anchor     == anchor )  return anchorID;
    }

    return kInvalidAnchorID;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

UInt32 AnchorTable::locate(IOService * anchor, void * key)
{
    //
    // This method searches for the specified anchor and key pair in the anchor
    // table and returns its ID (or kInvalidAnchorID on a failure).  It ignores
    // slots marked as obsolete.
    //

    for (UInt32 anchorID = 0; anchorID < _tableCount; anchorID++)
    {
        if ( _table[anchorID].isAssigned != false  &&
             _table[anchorID].isObsolete == false  &&
             _table[anchorID].anchor     == anchor &&
             _table[anchorID].key        == key    )  return anchorID;
    }

    return kInvalidAnchorID;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

UInt32 AnchorTable::update(IOService * anchor, void * key)
{
    //
    // This method searches for the specified anchor in the anchor table and
    // updates its key value if no references to it exist in the minor table
    // or if the references in the minor table are all obsolete.  It returns
    // the updated anchor ID (or kInvalidAnchorID on a failure).  It ignores
    // slots marked as obsolete.
    //

    MinorTable * minors = gIOMediaBSDClientGlobals.getMinors();

    for (UInt32 anchorID = 0; anchorID < _tableCount; anchorID++)
    {
        if ( _table[anchorID].isAssigned != false  &&
             _table[anchorID].isObsolete == false  &&
             _table[anchorID].anchor     == anchor )
        {
            if ( minors->hasReferencesToAnchorID(anchorID, true) == false )
            {
                _table[anchorID].key = key;
                return anchorID;
            }
        }
    }

    return kInvalidAnchorID;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool AnchorTable::isObsolete(UInt32 anchorID)
{
    //
    // Determine whether the specified anchor ID is marked as obsolete.
    //

    assert(anchorID < _tableCount);
    assert(_table[anchorID].isAssigned);

    return _table[anchorID].isObsolete ? true : false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn AnchorTable::anchorWasNotified( void *      /* target */,
                                         void *      /* parameter */,
                                         UInt32      messageType,
                                         IOService * anchor,
                                         void *      /* messageArgument */,
                                         vm_size_t   /* messageArgumentSize */ )
{
    //
    // Notification handler for anchors.
    //

    AnchorTable * anchors = gIOMediaBSDClientGlobals.getAnchors();
    UInt32        anchorID;
    MinorTable *  minors  = gIOMediaBSDClientGlobals.getMinors();

    // Determine whether this is a termination notification (post-stop).

    if ( messageType != kIOMessageServiceIsTerminated )
        return kIOReturnSuccess;

    // Disable access to tables.

    gIOMediaBSDClientGlobals.lockState();

    // Determine whether this anchor is in the anchor table (obsolete occurences
    // are skipped in the search, as appropriate, since those anchor IDs will be
    // removed as it is).

    while ( (anchorID = anchors->locate(anchor)) != kInvalidAnchorID )
    {
        // Determine whether this anchor still has references from the minor
        // table.  If it does, we mark the the anchor as obsolete so that it
        // will be removed later, once references to it go to zero (which is
        // handled by MinorTable::remove).

        if ( minors->hasReferencesToAnchorID(anchorID, false) )
            anchors->obsolete(anchorID);
        else
            anchors->remove(anchorID);
    }

    // Enable access to tables.

    gIOMediaBSDClientGlobals.unlockState();   

    return kIOReturnSuccess;
}

// =============================================================================
// MinorTable Class

MinorTable::MinorTable()
{
    //
    // Initialize this object's minimal state.
    //

    _table.buckets = IONew(MinorSlot *, kMinorsBucketCount);
    _tableCount    = 0;

    if ( _table.buckets )
        bzero(_table.buckets, kMinorsBucketCount * sizeof(MinorSlot *));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

MinorTable::~MinorTable()
{
    //
    // Free all of this object's outstanding resources.
    //

    for ( UInt32 minorID = 0; minorID < _tableCount; minorID++ )
        if ( _table[minorID].isAssigned )  remove(minorID);

    if ( _table.buckets )
    {
        for ( UInt32 bucketID = 0; _table.buckets[bucketID]; bucketID++ )
            IODelete(_table.buckets[bucketID], MinorSlot, kMinorsAddCount);

        IODelete(_table.buckets, MinorSlot *, kMinorsBucketCount);
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

UInt32 MinorTable::insert( IOMedia *          media,
                           UInt32             anchorID,
                           IOMediaBSDClient * client,
                           char *             slicePath )
{
    //
    // This method inserts the specified media/anchorID pair into an unassigned
    // slot in the minor table and returns its ID (or kInvalidMinorID on error).
    //
    // Note that the bdev and cdev nodes are published as a result of this call,
    // with the name "[r]disk<anchorID><slicePath>".  For instance, "disk2s3s1"
    // for an anchorID of 2 and slicePath of "s3s1".
    //

    void *       bdevNode;
    void *       cdevNode;
    UInt32       minorID;
    char *       minorName;
    UInt32       minorNameSize;

    if ( _table.buckets == 0 )  return kInvalidMinorID;

    // Search for an unassigned slot in the minor table.

    for ( minorID = 0; minorID < _tableCount; minorID++ )
        if ( _table[minorID].isAssigned == false )  break;

    // Was an unassigned slot found?  If not, grow the table.

    if ( minorID == _tableCount )
    {
        UInt32 bucketID = _tableCount / kMinorsAddCount;

        // We must expand the minor table since no more slots are available.

        if ( bucketID >= kMinorsBucketCount )  return kInvalidMinorID;

        _table.buckets[bucketID] = IONew(MinorSlot, kMinorsAddCount);

        if ( _table.buckets[bucketID] == 0 )  return kInvalidMinorID;

        bzero(_table.buckets[bucketID], kMinorsAddCount * sizeof(MinorSlot));

        _tableCount += kMinorsAddCount;
    }

    // Create a buffer large enough to hold the full name of the minor.

    minorNameSize = strlen("disk#");
    for (unsigned temp = anchorID; temp >= 10; temp /= 10)  minorNameSize++;
    minorNameSize += strlen(slicePath);
    minorNameSize += 1;
    minorName = IONew(char, minorNameSize);

    // Create a block and character device node in BSD for this media.

    bdevNode = devfs_make_node( /* dev        */ makedev(kMajor, minorID),
                                /* type       */ DEVFS_BLOCK, 
                                /* owner      */ UID_ROOT,
                                /* group      */ GID_OPERATOR,
                                /* permission */ 0640,
                                /* name (fmt) */ "disk%d%s",
                                /* name (arg) */ anchorID,
                                /* name (arg) */ slicePath );

    cdevNode = devfs_make_node( /* dev        */ makedev(kMajor, minorID),
                                /* type       */ DEVFS_CHAR, 
                                /* owner      */ UID_ROOT,
                                /* group      */ GID_OPERATOR,
                                /* permission */ 0640,
                                /* name (fmt) */ "rdisk%d%s",
                                /* name (arg) */ anchorID,
                                /* name (arg) */ slicePath );

    if ( minorName == 0 || bdevNode == 0 || cdevNode == 0 )
    {
        if ( cdevNode )   devfs_remove(cdevNode);
        if ( bdevNode )   devfs_remove(bdevNode);
        if ( minorName )  IODelete(minorName, char, minorNameSize);

        return kInvalidMinorID;
    }

    // Construct a name for the node.

    snprintf(minorName, minorNameSize, "disk%ld%s", anchorID, slicePath);
    assert(strlen(minorName) + 1 == minorNameSize);

    // Zero the new slot, fill it in, and retain the appropriate objects.

    bzero(&_table[minorID], sizeof(MinorSlot));    // (zero slot)

    _table[minorID].isAssigned    = true;          // (fill in slot)
    _table[minorID].isObsolete    = false;
    _table[minorID].isOrphaned    = false;
    _table[minorID].anchorID      = anchorID;
    _table[minorID].client        = client;
    _table[minorID].media         = media;
    _table[minorID].name          = minorName;
    _table[minorID].bdevBlockSize = media->getPreferredBlockSize();
    _table[minorID].bdevNode      = bdevNode;
    _table[minorID].bdevOpen      = 0;
    _table[minorID].bdevOpenLevel = kIOStorageAccessNone;
    _table[minorID].cdevNode      = cdevNode;
    _table[minorID].cdevOpen      = 0;
    _table[minorID].cdevOpenLevel = kIOStorageAccessNone;

    _table[minorID].client->retain();              // (retain client)
    _table[minorID].media->retain();               // (retain media)

    return minorID;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void MinorTable::remove(UInt32 minorID)
{
    //
    // This method removes the specified minor from the minor table.
    //

    UInt32 anchorID;

    assert(minorID < _tableCount);
    assert(_table[minorID].isAssigned);

    assert(_table[minorID].isOrphaned == false);
    assert(_table[minorID].bdevOpen == 0);
    assert(_table[minorID].cdevOpen == 0);

    anchorID = _table[minorID].anchorID;

    // Release the resources retained in the minor slot and zero it.

    devfs_remove(_table[minorID].cdevNode);
    devfs_remove(_table[minorID].bdevNode);
    IODelete(_table[minorID].name, char, strlen(_table[minorID].name) + 1);
    _table[minorID].client->release();             // (release client)
    _table[minorID].media->release();              // (release media)

    bzero(&_table[minorID], sizeof(MinorSlot));    // (zero slot)

    // Determine whether the associated anchor ID is marked as obsolete.  If it
    // is and there are no other references to the anchor ID in the minor table,
    // we remove the anchor ID from the anchor table.

    if ( gIOMediaBSDClientGlobals.getAnchors()->isObsolete(anchorID) )
    {
        if ( hasReferencesToAnchorID(anchorID, false) == false )
            gIOMediaBSDClientGlobals.getAnchors()->remove(anchorID);
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

UInt32 MinorTable::update( IOMedia *          media,
                           UInt32             anchorID,
                           IOMediaBSDClient * client,
                           char *             slicePath )
{
    //
    // This method searches for the specified anchor ID and slice path pair in
    // the minor table and updates it.  An update would be an unusual occasion
    // as new anchors are assigned when two media trees are found to share the
    // same anchor.  It would occur in one specific circumstance: on the minor
    // slot through which a DKIOCFORMAT was issued.  The minor slot would have
    // been marked in "open flux", the format would have been issued, then the
    // media objects terminated, the minor slot marked obsolete, and the media
    // objects republished.  The anchor ID would have one reference, the minor
    // slot with the DKIOCFORMAT still outstanding.  AnchorTable::update would
    // notice the one reference is orphaned and accept the reuse of the anchor
    // ID.  MinorTable::update would notice the orphaned minor slot and update
    // it with the new media object and media bsd client object, and clear its
    // obsolete state and "open flux" state, once the new media object arrives.
    //

    UInt32 minorID;
    char * minorName;
    UInt32 minorNameSize;

    // Create a buffer large enough to hold the full name of the minor.

    minorNameSize = strlen("disk#");
    for (unsigned temp = anchorID; temp >= 10; temp /= 10)  minorNameSize++;
    minorNameSize += strlen(slicePath);
    minorNameSize += 1;
    minorName = IONew(char, minorNameSize);

    if ( minorName == 0 )  return kInvalidMinorID;

    // Construct a name for the node.

    snprintf(minorName, minorNameSize, "disk%ld%s", anchorID, slicePath);
    assert(strlen(minorName) + 1 == minorNameSize);

    // Search for an orphaned slot in the minor table with our minor name.

    for ( minorID = 0; minorID < _tableCount; minorID++ )
    {
        if ( _table[minorID].isAssigned              != false    &&
             _table[minorID].isObsolete              != false    &&
             _table[minorID].isOrphaned              != false    &&
             _table[minorID].anchorID                == anchorID &&
             strcmp(_table[minorID].name, minorName) == 0        )  break;
    }

    IODelete(minorName, char, minorNameSize);

    if ( minorID == _tableCount )  return kInvalidMinorID;

    // Update the slot and retain the appropriate objects.

    _table[minorID].client->release();             // (release client)
    _table[minorID].media->release();              // (release media)

    _table[minorID].isObsolete = false;            // (update slot)
    _table[minorID].isOrphaned = false;
    _table[minorID].client     = client;
    _table[minorID].media      = media;

    _table[minorID].client->retain();              // (retain client)
    _table[minorID].media->retain();               // (retain media)

    return minorID;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

UInt32 MinorTable::locate(IOMedia * media)
{
    //
    // This method searches for the specified media in the minor table and
    // returns its ID (or kInvalidMinorID on an error).   It ignores slots
    // marked as obsolete.
    //

    for (UInt32 minorID = 0; minorID < _tableCount; minorID++)
    {
        if ( _table[minorID].isAssigned != false &&
             _table[minorID].isObsolete == false &&
             _table[minorID].media      == media )  return minorID;
    }

    return kInvalidMinorID;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

UInt32 MinorTable::getOpenCountForAnchorID(UInt32 anchorID)
{
    //
    // This method obtains a count of opens on the minors associated with the
    // specified anchor ID.  A block device open is counted separately from a
    // character device open.
    //

    UInt32 opens = 0;

    for ( UInt32 minorID = 0; minorID < _tableCount; minorID++ )
    {
        if ( _table[minorID].isAssigned != false    &&
             _table[minorID].anchorID   == anchorID )
        {
            opens += _table[minorID].bdevOpen;
            opens += _table[minorID].cdevOpen;
        }
    }

    return opens;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool MinorTable::hasReferencesToAnchorID(UInt32 anchorID, bool excludeOrphans)
{
    //
    // This method determines whether there are assigned minors in the minor
    // table that refer to the specified anchor ID.  It ignores slots marked
    // as obsolete and orphaned, unless excludeOrphans is false.
    //

    for ( UInt32 minorID = 0; minorID < _tableCount; minorID++ )
    {
        if ( _table[minorID].isAssigned != false    &&
             _table[minorID].anchorID   == anchorID )
        {
            if ( excludeOrphans             == false )  return true;
            if ( _table[minorID].isObsolete == false )  return true;
            if ( _table[minorID].isOrphaned == false )  return true;
        }
    }

    return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

MinorSlot * MinorTable::getMinor(UInt32 minorID)
{
    //
    // Obtain the structure describing the specified minor.
    //

    if ( minorID < _tableCount && _table[minorID].isAssigned )
        return &_table[minorID];
    else
        return 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void MinorTable::obsolete(UInt32 minorID)
{
    //
    // This method obsoletes the specified minor, that is, the slot is marked
    // as obsolete and will be removed later via the dkclose function once it
    // detects the last close arrive.  Once obsoleted, the minor can be cons-
    // idered to be removed, since it will not appear in locate searches.
    //

    assert(minorID < _tableCount);
    assert(_table[minorID].isAssigned);

    // Mark the minor as obsolete so that it can be removed from the table as
    // soon as the last close arrives (dkclose function's responsibility).

    _table[minorID].isObsolete = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool MinorTable::isObsolete(UInt32 minorID)
{
    //
    // Determine whether the specified minor ID is marked as obsolete.
    //

    assert(minorID < _tableCount);
    assert(_table[minorID].isAssigned);

    return _table[minorID].isObsolete ? true : false;
}

// =============================================================================
// IOMediaBSDClientGlobals Class

IOMediaBSDClientGlobals::IOMediaBSDClientGlobals()
{
    //
    // Initialize the minimal global state.
    //

    _anchors         = new AnchorTable();
    _minors          = new MinorTable();

    _bdevswInstalled = (bdevsw_add(kMajor, &bdevswFunctions) == kMajor);
    _cdevswInstalled = (cdevsw_add(kMajor, &cdevswFunctions) == kMajor);

    _openLock        = IOLockAlloc();
    _stateLock       = IOLockAlloc();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOMediaBSDClientGlobals::~IOMediaBSDClientGlobals()
{
    //
    // Free all of the outstanding global resources.
    //

    if ( _openLock )         IOLockFree(_openLock);
    if ( _stateLock )        IOLockFree(_stateLock);

    if ( _cdevswInstalled )  cdevsw_remove(kMajor, &cdevswFunctions); 
    if ( _bdevswInstalled )  bdevsw_remove(kMajor, &bdevswFunctions);

    if ( _minors )           delete _minors;
    if ( _anchors )          delete _anchors;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

AnchorTable * IOMediaBSDClientGlobals::getAnchors()
{
    //
    // Obtain the table of anchors.
    //

    return _anchors;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

MinorTable * IOMediaBSDClientGlobals::getMinors()
{
    //
    // Obtain the table of minors.
    //

    return _minors;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

MinorSlot * IOMediaBSDClientGlobals::getMinor(UInt32 minorID)
{
    //
    // Obtain information for the specified minor ID.
    //

    return _minors->getMinor(minorID);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOMediaBSDClientGlobals::isValid()
{
    //
    // Determine whether the minimal global state has been initialized.
    //

    return ( _anchors         ) &&
           ( _minors          ) &&
           ( _bdevswInstalled ) &&
           ( _cdevswInstalled ) &&
           ( _openLock        ) &&
           ( _stateLock       );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOMediaBSDClientGlobals::lockOpen()
{
    //
    // Disable access to the opens and closes.
    //

    IOLockLock(_openLock);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOMediaBSDClientGlobals::unlockOpen()
{
    //
    // Enable access to the opens and closes.
    //

    IOLockUnlock(_openLock);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOMediaBSDClientGlobals::lockState()
{
    //
    // Disable access to the global state.
    //

    IOLockLock(_stateLock);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOMediaBSDClientGlobals::unlockState()
{
    //
    // Enable access to the global state.
    //

    IOLockUnlock(_stateLock);
}
