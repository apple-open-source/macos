/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 */

#ifndef _I2CUSERCLIENT_H
#define _I2CUSERCLIENT_H

// kI2CUCBufSize defines the size of the byte stream that is passed between
// user space and kernel space for both reads and writes.  ALL I2C
// transactions performed through the user client are constrained by
// this buffer size.  In addition, the in-kernel user client checks to
// make sure that the buffer is the correct size before accepting the
// transaction.  Unless you are changing it in both the in-kernel code
// and the user-space device interface code, DO NOT CHANGE THIS VALUE!!!
#define kI2CUCBufSize	32

// Indices for externally accessible functions
enum {
	kI2CUCOpen,			// ScalarIScalarO
	kI2CUCClose,		// ScalarIScalarO
	kI2CUCRead,			// StructIStructO
	kI2CUCWrite,		// StructIStructO
	kI2CUCRMW,			// StructIStructO
	kI2CUCNumMethods
};

// I2C transaction modes
enum {
	kI2CUCDumbMode,      		//polled mode used
	kI2CUCStandardMode,			//polled mode used
	kI2CUCStandardSubMode,		//polled mode used
	kI2CUCCombinedMode,			//polled mode used
	kI2CUCDumbIntMode,			//interrupts mode used
	kI2CUCStandardIntMode,		//interrupts mode used
	kI2CUCStandardSubIntMode,	//interrupts mode used
	kI2CUCCombinedIntMode		//interrupts mode used
};

// Clients to PPCI2CInterface are required to bracket their bus usage
// by opening and closing the I2C bus.  This is done by calling
// openI2CBus and closeI2CBus methods.  PPCI2CInterface does checks to
// make sure that the thread making read, write, mode change or close
// requests is the same one that made the last open request.  Presumably,
// if a thread opens the I2C bus and then dies without closing the bus,
// the bus is stuck open and all other clients are permanently locked
// out of the bus.
//
// In order to avoid this from happening, I2CUserClient must ensure that
// any operation which requires a bus open operation is completed
// atomically before the thread returns to the calling application.

// Read operations pass an I2CReadInput struct and get their results
// in an I2CReadOutput struct.  The number of bytes requested is passed
// with the inputs, and the number of bytes successfully read is returned
// as an output, along with the data.  If the operation fails, an error
// code will be returned and realCount will be set to zero.
typedef struct {
	UInt8		mode;		// transaction mode
	UInt8		busNo;		// bus number, sometimes referred to as port
	UInt8		addr;		// 8-bit I2C address -- the R/W bit, bit 0, will be ignored
	UInt8		subAddr;	// 8-bit register subaddress
	IOByteCount	count;		// number of bytes to be read, must be <= kI2CUCBufSize
} I2CReadInput;

typedef struct {
	IOByteCount	realCount;	// how many bytes actually read
	UInt8		buf[kI2CUCBufSize];	// buffer holding returned data
} I2CReadOutput;

// Write operations pass an I2CWriteInput struct.  After the write
// is performed, the number of bytes actually written is returned in
// an I2CWriteOutput struct.  If the operation failes, realCount
// will be set to zero and an appropriate error code will be returned. 
typedef struct {
	UInt8		mode;		// transaction mode
	UInt8		busNo;		// bus number, sometimes referred to as port
	UInt8		addr;		// 8-bit I2C address -- the R/W bit, bit 0, will be ignored
	UInt8		subAddr;	// 8-bit register subaddress
	IOByteCount	count;		// number of bytes to be written, must be <= kI2CUCBufSize
	UInt8		buf[kI2CUCBufSize];	// buffer holding write data
} I2CWriteInput;

typedef struct {
	IOByteCount	realCount;	// how many bytes actually written
} I2CWriteOutput;

// For a single byte read-modify-write cycle, use I2CRMWInput.  There
// is no data to be returned after such an operation, just success or
// failure, so there is no output structure used.  The caller must
// specify the transaction mode for both the read and the subsequent
// write operation, between which the bus is held open to make the
// operation atomic.  A byte is read from the target, and is modified
// as such:
//   newbyte = (byte & ~mask) | (value & mask);
// Then newbyte is written back to the target (at the same addr/subaddr),
// the bus is released and status is returned to the caller.
typedef struct {
	UInt8		readMode;	// transaction mode for initial read transaction
	UInt8		writeMode;	// transaction mode for writing results back to I2C
	UInt8		busNo;		// bus number, sometimes referred to as port
	UInt8		addr;		// 8-bit I2C address -- the R/W bit, bit 0, will be ignored
	UInt8		subAddr;	// 8-bit register subaddress
	UInt8		value;	// buffer holding values
	UInt8		mask;	// buffer holding masks
} I2CRMWInput;

#endif