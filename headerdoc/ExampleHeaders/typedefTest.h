/*! @header TypeDefTest.h
    @discussion This header tests the supported types of typedef declarations, along with their corresponding HeaderDoc tags.    
    @charset iso-8859-1
*/

/*!
    @struct SimpleStruct
    @abstract Abstract for this API.
    @discussion Discussion that applies to the entire simple struct.
    @field firstField Description of first field
    @field secondField Description of second field
*/

struct _structTag {
    short firstField;
    unsigned long secondField;
} SimpleStruct;


/*!	@typedef SimpleTypeDef
    @discussion Test of simple typedef.
*/

typedef struct Bar *SimpleTypeDef;

/*!
    @typedef TypedefdSimpleStruct
    @abstract Abstract for this API.
    @discussion Discussion that applies to the entire typedef'd simple struct.
    @field firstField Description of first field
    @field secondField Description of second field
*/

typedef struct _structTag_TypedefdSimpleStruct {
    short firstField;
    unsigned long secondField
} TypedefdSimpleStruct;

/*!
    @typedef TypedefdEnum
    @abstract Abstract for this API.
    @discussion Discussion that applies to the entire typedef'd enum.
    @constant kCFCompareLessThan Description of first constant.
    @constant kCFCompareEqualTo Description of second constant.
    @constant kCFCompareGreaterThan Description of third constant.
*/

typedef enum {
    kCFCompareLessThan = -1,
    kCFCompareEqualTo = 0,
    kCFCompareGreaterThan = 1
} TypedefdEnum;


/*!
    @typedef simpleCallback
    @abstract Abstract for this API.
    @discussion Discussion that applies to the entire callback.
    @param inFirstParameter Description of the callback's first parameter.
    @param outSecondParameter Description of the callback's second parameter.
    @result Returns what it can when it is possible to do so.
*/
typedef long (*simpleCallback)(short inFirstParameter, unsigned long long *outSecondParameter);


/*!	@typedef CGPathElement
    @discussion Test of struct/typedef combo.
*/

struct CGPathElement {
	CGPathElementType type;
	CGPoint *points;
};

typedef struct CGPathElement CGPathElement;


/*! @typedef This is a TypedefdStructWithCallbacks
    @abstract Abstract for this API.
    @discussion Defines the basic interface for Command DescriptorBlock (CDB) commands, which you use
    to control SCSI Architectural Model (SAM) devices, including ATAPI, Firewire SBP2, and SCSI devices.
        
    @field firstField Description of first field.
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
    IOReturn (*setPointers)(void *cmd,
                            IOVirtualRange *sgList);
    void (*getPointers)(void *cmd,
                        IOVirtualRange **outSGList,
                        int *outSGEntries,
                        UInt32 *outTransferCount,
                        Boolean *outIsWrite);

    unsigned long lastField;
} TypedefdStructWithCallbacks;


/*! @typedef TypedefdStructWithCallbacksAndStructs
    @abstract Abstract for this API.
    @discussion This is completely bogus.
        
    @field firstField Description of first field.
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
typedef struct _someOtherTag {
    IUNKNOWN_C_GUTS;
    short firstField;

    struct {
	TEST;
	int a;
	char *b;
	void *c;
	IOReturn (*massagePointers)(void *cmd,
                            IOVirtualRange *sgList);
    } nestedStruct;

    /* Set the data pointers */
    IOReturn (*setPointers)(void *cmd,
        IOVirtualRange *sgList);
    void (*getPointers)(void *cmd, IOVirtualRange **outSGList, int *outSGEntries, UInt32 *outTransferCount, Boolean *outIsWrite);

    unsigned long lastField;
} TypedefdStructWithCallbacksAndStructs;

