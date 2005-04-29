/*! @header
    This header tests single-line and c-style headerdoc
    markup embedded in the body of a struct or other block.
*/

/*!
    @struct SimpleStruct
    @abstract Abstract for this API.
    @discussion Discussion that applies to the entire simple struct.
*/

struct _structTag {
    short firstField; /*! @field firstField 1 Description of first field */
    unsigned long secondField; /*! @field secondField 1 Description of second field */
} SimpleStruct;


/*!
    @typedef TypedefdSimpleStruct
    @abstract Abstract for this API.
    @discussion Discussion that applies to the entire typedef'd simple struct.
*/

typedef struct _structTag {
    short  //! @field firstField 1 Description of first field
	firstField;
    unsigned long secondFieldX //! 1 Description of second field
} TypedefdSimpleStruct;

/*!
    @typedef TypedefdEnum
    @abstract Abstract for this API.
    @discussion Discussion that applies to the entire typedef'd enum.
*/

typedef enum {
    kCFCompareLessThan = -1, /*! @constant kCFCompareLessThan Description of first constant. */
    kCFCompareEqualTo = 0, /*! @constant kCFCompareEqualTo Description of second constant. */
    kCFCompareGreaterThan = 1 /*! Description of third constant. */
} TypedefdEnum;


/*!
    @typedef simpleCallback
    @abstract Abstract for this API.
    @discussion Discussion that applies to the entire callback.
    @result Returns what it can when it is possible to do so.
*/
typedef long (*simpleCallback)(
	short inFirstParameter, /*! @param inFirstParameter Description of the callback's first parameter. */
	unsigned long long *outSecondParameter /*! @param outSecondParameter Description of the callback's second parameter. */
);


/*! @typedef This is a TypedefdStructWithCallbacks
    @abstract Abstract for this API.
    @discussion Defines the basic interface for Command DescriptorBlock (CDB) commands, which you use
    to control SCSI Architectural Model (SAM) devices, including ATAPI, Firewire SBP2, and SCSI devices.
        
    @field firstField 1 Description of first field.

    @callback getPointers Gets the location of the data buffer for an I/O command. The buffer
    can be in one or more, possibly discontiguous, locations.
    
    The getPointers function has the following parameters:
    @param cmd A pointer to the CDB command interface instance for the current command.
    You must always pass this reference.

    @param outSGList A pointer to a pointer to a scatter/gather list (scatter for reading, gather for writing)
    containing one or more records that specify the ranges of virtual memory in the command's
    I/O buffer. Each element in a scatter/gather list describes the location and size of one buffer,
    allowing you to group multiple buffers of any size into a single virtual buffer for an
    I/O transaction. On return, the list specifies the I/O buffer for the command.

    @param outSGEntries On return, specifies the number of records in the scatter/gather list pointed to
    by the outSGList parameter.

    @param outTransferCount On return, the maximum data transfer count in bytes for the
    command.

    @param outIsWrite On return, specifies whether the command is a write command (true),
    or a read command (false)

    @field lastField Description of the struct's last field.
*/
typedef struct _someTag {
    IUNKNOWN_C_GUTS;
    short firstField;

    /* Set the data pointers */
    /*! 
    @callback setPointers Specifies the location of the data buffer for an I/O command, 
    as well as the maximum data transfer count and data transfer direction.
    
    The setPointers function has the following parameters:
    @param cmd A pointer to the CDB command interface instance for the current command.
    You must always pass this reference.

    @param sgList A pointer to a scatter/gather list (scatter for reading, gather for writing)
    containing one or more records that specify the ranges of virtual memory in the command's
    I/O buffer. Each element in a scatter/gather list describes the location and size of one buffer,
    allowing you to group multiple buffers of any size into a single virtual buffer for an
    I/O transaction. You can pass NULL for this parameter (for a command that doesn't require
    a data buffer).
    
    @result An IOReturn structure which returns the return value in the structure returned.  
    */

    IOReturn (*setPointers)(void *cmd,
                            IOVirtualRange *sgList);
    void (*getPointers)(void *cmd, IOVirtualRange **outSGList, int *outSGEntries, UInt32 *outTransferCount, Boolean *outIsWrite);

    unsigned long lastField;
} TypedefdStructWithCallbacks;

