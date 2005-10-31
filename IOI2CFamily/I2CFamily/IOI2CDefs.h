/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CDefs.h,v 1.6 2005/07/01 16:09:52 bwpang Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CDefs.h,v $
 *		Revision 1.6  2005/07/01 16:09:52  bwpang
 *		[4086434] added APSL headers
 *		
 *		Revision 1.5  2004/12/15 00:14:11  jlehrer
 *		Added bytesTransfered to IOI2CCommand struct.
 *		Added options to I2CUserReadInput and I2CUserWriteInput structs.Connection to src.apple.com closed by remote host.
 *		
 *		Revision 1.4  2004/11/04 20:17:59  jlehrer
 *		Removed APSL headers.
 *		
 *		Revision 1.3  2004/09/17 20:47:30  jlehrer
 *		Updated headerDoc comments.
 *		Added external client interface calls: kIOI2CClientRead and kIOI2CClientWrite...
 *		   ...these enable simple i2c read/write without requiring an IOI2CCommand.
 *		
 *		Revision 1.2  2004/07/03 00:07:05  jlehrer
 *		Added support for dynamic max-i2c-data-length.
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */

#ifndef _IOI2CDefs_H
#define _IOI2CDefs_H


#ifdef KERNEL
 #include <IOKit/IOService.h>
 #include <IOKit/IOLib.h>
#else
 #include <IOKit/IOKitLib.h>
#endif


// String constants used for I2C callPlatformFunction symbols.
#define kIOI2CClientWrite		"IOI2CClientWrite"
#define kIOI2CClientRead		"IOI2CClientRead"
#define kWriteI2Cbus			"IOI2CWriteI2CBus"
#define kReadI2Cbus				"IOI2CReadI2CBus"
#define kLockI2Cbus				"IOI2CLockI2CBus"
#define kUnlockI2Cbus			"IOI2CUnlockI2CBus"
#define kIOI2CGetMaxI2CDataLength	"IOI2CGetMaxI2CDataLength"

/*! @constant kIOI2C_CLIENT_KEY_DEFAULT @discussion This key value is used to request an I2C transaction without requiring the client to lock/unlock the bus (see readI2C and writeI2C methods) */
#define kIOI2C_CLIENT_KEY_DEFAULT	0

/*! @constant kIOI2C_CLIENT_KEY_INVALID @discussion This key value is returned from the lockI2CBus API if an error occurs. */
#define kIOI2C_CLIENT_KEY_INVALID	0xffffffff

/*! @constant kI2CUCBufSize
	@discussion defines the size of the byte stream that is passed between user space and kernel space for both reads and writes.
	ALL I2C transactions performed through the user client are constrained by this buffer size.
	In addition, the in-kernel user client checks to make sure that the buffer is the correct size before accepting the transaction.
	Unless you are changing it in both the in-kernel code and the user-space device interface code, DO NOT CHANGE THIS VALUE!!!
*/
#define kI2CUCBufSize	32

/*! @constant kIOI2CUserClientType
	@discussion Specifies creating an IOI2CUserClient class when passed as type argument to IOServiceOpen. All other type values will create the driver default user client class.
*/
#define kIOI2CUserClientType	0x1012c

/*! @enum kI2CUCxxx Indices for IOI2CUserClient externally accessible functions... */
enum
{
	kI2CUCLock,			// ScalarIScalarO
	kI2CUCUnlock,		// ScalarIScalarO
	kI2CUCRead,			// StructIStructO
	kI2CUCWrite,		// StructIStructO
	kI2CUCRMW,			// StructIStructO

	kI2CUCNumMethods
};

/*! @enum kIOI2CCommand_xxx I2C transaction constants. */
enum
{
	kI2CCommand_Read		= 0,
	kI2CCommand_Write		= 1,
};

/*! @enum kI2CMode_xxx I2C transaction mode constants. */
enum
{
	kI2CMode_Unspecified	= 0,
	kI2CMode_Standard		= 1,
	kI2CMode_StandardSub	= 2,
	kI2CMode_Combined		= 3,
};

/*! @enum kI2COption_xxx I2C transaction option constants. */
enum
{
	kI2COption_NoInterrupts			= (1 << 31),		// (if supported) Requests non-interrupt mode transaction execution.
	kI2COption_VerboseLog			= (1 << 30),		// (if supported) Requests verbose debugging of transaction execution.
};

#if 1 // 10-bit address macros: Work In Progress / Not Supported.
#define kI2C_10Bit_AddressSet		0x0000f000
#define i2c10BitAddressToScalar(x)	(((x) & 0x600 >> 1) | ((x) & 0xff))
#define i2c10BitScalarToAddress(x)	(kI2C_10Bit_AddressSet | (((x) & 0x300) << 1) | ((x) & 0xff))
#define isI2C10BitAddress(x)		((((x) & 0x0f800)==kI2C_10Bit_AddressSet)?true:false)
// 10-bit address format: 16'b11110xx0xxxxxxxx
// 10-bit packed scalar format:  16'b000000xxxxxxxxxx
// 7-bit address format: 8'bxxxxxxx0
#endif

#pragma mark  
#pragma mark *** IOI2CCommand ***
#pragma mark  

/*! @struct IOI2CCommand
	@abstract This data structure is used by the IOI2CFamily to pass parameters for a single I2C transaction.

	@field _reserved Reserved expansion array = 0.
	
	@field bytesTransfered Returns the number of bytes actually transfered to or from the device.
	
	@field command Type of command to perform: 0=read; 1=write.
	
	@field bus I2C bus number, sometimes referred to as port.
	
	@field address I2C slave address:

		For 7-bit addressing: use bits[7..1] only. Set bit[0] = 0.
	
		For 10-bit addressing: set bits[15..11] = 5'b11110, bits[10..9] = high order address, bit[8] = 0, bits[7..0] = low order addrsss.
		
		See I2C 2.0 spec for more information on 10-bit addressing.
		
	@field subAddress I2C subaddress:
	
		Only used on transactions with mode field set to 2=subaddress or 3=combined.

		bits[31..24] specify length: 0=8bit; 1=8bit; 2=16bit; 3=24bit.
		
		bits[23..0] specify subaddress:
		
			8bit subaddresses use bits[7..0],
			
			16bit subaddresses use bits[15..0],
			
			24bit subaddresses use bits[23..0].
			
	@field buffer Client read/write data buffer address.

	@field count number of bytes to be transfered, must be <= kI2CUCBufSize.

	@field mode I2C transaction mode:
	
		bits[3..0]: 0=unspecified; 1=standard; 2=subaddress; 3=combined
		
		If mode == unspecified then the driver or controller will choose an appropriate mode for the transaction... 
		
		or it will return an unsupported error.
		
	@field retries (If supported) Number of times to retry a failed bus transaction before giving up.

	@field timeout_uS (If supported) Maximum time in microseconds to allow for this command before giving up.

	@field speed (If supported) Override bus speed: Set to 0 for default speed, or speed in kHz.

	@field options (If supported) Option flags: see kI2COption_* enums.

	@field reserved Reserved expansion array = 0.
*/
typedef struct
{
	UInt32		_reserved[3];
	UInt32		bytesTransfered;
	UInt32		command;
	UInt32		bus;
	UInt32		address;
	UInt32		subAddress;
	UInt8		*buffer;
	UInt32		count;
	UInt32		mode;
	UInt32		retries;
	UInt32		timeout_uS;
	UInt32		speed;
	UInt32		options;
	UInt32		reserved[4];

} IOI2CCommand;


#pragma mark  
#pragma mark *** IOI2CFamily IOUserClient structures ***
#pragma mark  

// IOI2CUserClient I/O data structures...

/*! @struct I2CUserReadInput
	@abstract IOUserClient read command parameter input structure.

	@field options	kI2COption_xxx flags.

	@field mode		transaction mode.
	
	@field busNo	bus number, sometimes referred to as port
	
	@field addr		8-bit I2C address -- the R/W bit, bit 0, will be ignored
	
	@field subAddr	8-bit register subaddress
	
	@field count	number of bytes to be read, must be <= kI2CUCBufSize
	
	@field key		I2C Key returned from lockI2CBus or kIOI2C_CLIENT_KEY_DEFAULT
*/
typedef struct
{
	UInt32		options;	// kI2COption_xxx flags.
	UInt32		mode;		// transaction mode
	UInt32		busNo;		// bus number, sometimes referred to as port
	UInt32		addr;		// 8-bit I2C address -- the R/W bit, bit 0, will be ignored
	UInt32		subAddr;	// 8-bit register subaddress
	IOByteCount	count;		// number of bytes to be read, must be <= kI2CUCBufSize
	UInt32		key;		// I2C Key returned from lockI2CBus or kIOI2C_CLIENT_KEY_DEFAULT

} I2CUserReadInput;

/*! @struct I2CUserReadOutput
	@abstract IOUserClient read command parameter output structure.

	@field realCount how many bytes actually read
	
	@field buf buffer holding returned data, must be <= kI2CUCBufSize.
*/
typedef struct
{
	IOByteCount	realCount;			// how many bytes actually read
	UInt8		buf[kI2CUCBufSize];	// buffer holding returned data

} I2CUserReadOutput;

/*! @struct I2CUserWriteInput
	@abstract IOUserClient write command parameter input structure.
	@discussion Write operations pass an I2CUserWriteInput struct.
	After the write is performed, the number of bytes actually written is returned in
	an I2CWriteOutput struct.
	If the operation failes, realCount will be set to zero and an appropriate error code will be returned.

	@field options	kI2COption_xxx flags.

	@field mode I2C transaction mode.

	@field busNo Bus number, sometimes referred to as port.

	@field addr 8-bit I2C address -- the R/W bit, bit 0, will be ignored.

	@field subAddr 8-bit register subaddress.

	@field count Number of bytes to be written, must be <= kI2CUCBufSize.

	@field key I2C Key returned from lockI2CBus or kIOI2C_CLIENT_KEY_DEFAULT.

	@field buf Buffer holding write data, must be <= kI2CUCBufSize.
*/
typedef struct
{
	UInt32		options;	// kI2COption_xxx flags.
	UInt32		mode;		// transaction mode
	UInt32		busNo;		// bus number, sometimes referred to as port
	UInt32		addr;		// 8-bit I2C address -- the R/W bit, bit 0, will be ignored
	UInt8		subAddr;	// 8-bit register subaddress
	IOByteCount	count;		// number of bytes to be written, must be <= kI2CUCBufSize
	UInt32		key;		// I2C Key returned from lockI2CBus or kIOI2C_CLIENT_KEY_DEFAULT
	UInt8		buf[kI2CUCBufSize];	// buffer holding write data

} I2CUserWriteInput;

/*! @struct I2CUserWriteOutput
	@abstract IOUserClient write command parameter output structure.
	
	@field realCount Return value: how many bytes actually written.
*/
typedef struct
{
	IOByteCount	realCount;	// Return value: how many bytes actually written

} I2CUserWriteOutput;



#pragma mark  
#pragma mark *** PPCI2CInterface IOUserClient structures ***
#pragma mark  


/*! @struct I2CReadInput
	@abstract IOUserClient read command parameter input structure.

	@discussion Clients to PPCI2CInterface are required to bracket their bus usage
	by opening and closing the I2C bus.  This is done by calling
	openI2CBus and closeI2CBus methods.  PPCI2CInterface does checks to
	make sure that the thread making read, write, mode change or close
	requests is the same one that made the last open request.  Presumably,
	if a thread opens the I2C bus and then dies without closing the bus,
	the bus is stuck open and all other clients are permanently locked
	out of the bus.

	In order to avoid this from happening, I2CUserClient must ensure that
	any operation which requires a bus open operation is completed
	atomically before the thread returns to the calling application.

	Read operations pass an I2CReadInput struct and get their results
	in an I2CReadOutput struct.  The number of bytes requested is passed
	with the inputs, and the number of bytes successfully read is returned
	as an output, along with the data.  If the operation fails, an error
	code will be returned and realCount will be set to zero.
*/
typedef struct
{
	UInt8		mode;		// transaction mode
	UInt8		busNo;		// bus number, sometimes referred to as port
	UInt8		addr;		// 8-bit I2C address -- the R/W bit, bit 0, will be ignored
	UInt8		subAddr;	// 8-bit register subaddress
	IOByteCount	count;		// number of bytes to be read, must be <= kI2CUCBufSize
} I2CReadInput;

typedef struct
{
	IOByteCount	realCount;	// how many bytes actually read
	UInt8		buf[kI2CUCBufSize];	// buffer holding returned data
} I2CReadOutput;

/*! @struct I2CWriteInput
	@abstract IOUserClient write command parameter input structure.
	Write operations pass an I2CWriteInput struct.  After the write
	is performed, the number of bytes actually written is returned in
	an I2CWriteOutput struct.  If the operation failes, realCount
	will be set to zero and an appropriate error code will be returned. 
*/
typedef struct
{
	UInt8		mode;		// transaction mode
	UInt8		busNo;		// bus number, sometimes referred to as port
	UInt8		addr;		// 8-bit I2C address -- the R/W bit, bit 0, will be ignored
	UInt8		subAddr;	// 8-bit register subaddress
	IOByteCount	count;		// number of bytes to be written, must be <= kI2CUCBufSize
	UInt8		buf[kI2CUCBufSize];	// buffer holding write data
} I2CWriteInput;

typedef struct
{
	IOByteCount	realCount;	// how many bytes actually written
} I2CWriteOutput;

/*! @struct I2CRMWInput
	@abstract IOUserClient read-modify-write command parameter input structure.
	For a single byte read-modify-write cycle, use I2CRMWInput.  There
	is no data to be returned after such an operation, just success or
	failure, so there is no output structure used.  The caller must
	specify the transaction mode for both the read and the subsequent
	write operation, between which the bus is held open to make the
	operation atomic.  A byte is read from the target, and is modified
	as such:
	  newbyte = (byte & ~mask) | (value & mask);
	Then newbyte is written back to the target (at the same addr/subaddr),
	the bus is released and status is returned to the caller.
*/
typedef struct
{
	UInt32		readMode;	// transaction mode for initial read transaction
	UInt32		writeMode;	// transaction mode for writing results back to I2C
	UInt32		busNo;		// bus number, sometimes referred to as port
	UInt32		addr;		// 8-bit I2C address -- the R/W bit, bit 0, will be ignored
	UInt32		subAddr;	// 8-bit register subaddress
	IOByteCount	count;		// number of bytes to be written, must be <= kI2CUCBufSize
	UInt32		key;
	UInt8		value[kI2CUCBufSize];	// buffer holding values
	UInt8		mask[kI2CUCBufSize];	// buffer holding masks
} I2CRMWInput;

#endif // _IOI2CDefs_H
