/*
 *  Copyright (c) 2008 Apple Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *
 *  @APPLE_LICENSE_HEADER_END@
 */

#include "TLV.h"

#include <iomanip>
#include <iostream>
#include <limits>

using namespace std;

TLV::TLV() throw()
:tag(), value(NULL), innerValues(NULL) {
}

TLV::TLV(unsigned char tag) throw()
:tag(1, tag), value(NULL), innerValues(NULL) {
}

TLV::TLV(const byte_string& tag) throw()
:tag(tag), value(NULL), innerValues(NULL) {
}

TLV::TLV(unsigned char tag, const byte_string& value) throw()
:tag(1, tag), value(new byte_string(value)), innerValues(NULL) {
}

TLV::TLV(const byte_string& tag, const byte_string& value) throw()
:tag(tag), value(new byte_string(value)), innerValues(NULL) {
}

TLV::TLV(uint8_t tag, const TLVList &tlv) throw()
:tag(1, tag), value(NULL), innerValues(new TLVList(tlv)) {
}

TLV::TLV(const byte_string &tag, const TLVList &tlv) throw()
:tag(tag), value(NULL), innerValues(new TLVList(tlv)) {
}

TLV_ref TLV::parse(const byte_string &in) throw(std::runtime_error) {
	byte_string::const_iterator begin = in.begin();
	return parse(begin, in.end());
}

byte_string TLV::encode() const throw() {
	byte_string out;
	encode(out);
	return out;
}

void TLV::encode(byte_string &out) const throw() {
	const byte_string &tag = getTag();
	// Puts the tag
	out += tag;
	// Puts the length
	encodeLength(valueLength(), out);

#if 1
	// Non-caching version since the TLV is expected to be
	// thrown away after encoding
	// If there is a value, put that
	if(value.get()) {
		out += *value;
		return;
	}
	if(!innerValues.get())
		return;
	// Else if there are innerValues, encode those out
	encodeSequence(*innerValues, out);
#else
	// Obtain the value in a cached manner
	const byte_string &value = getValue();
	out += value;
#endif
}

const TLVList &TLV::getInnerValues() const throw(std::runtime_error) {
	/* If there is a cached innervalues version, output it
	 * else parse any existing TLV data and use that */
	if(innerValues.get()) return *innerValues;
	if(!value.get()) {
		innerValues.reset(new TLVList());
		return *innerValues;
	}
	innerValues.reset(new TLVList());
	byte_string::const_iterator begin = value->begin();
	parseSequence(begin, (byte_string::const_iterator)value->end(), *innerValues);

	return *innerValues;
}

const byte_string &TLV::getValue() const throw() {
	/* If there is a cached value version, output it
	 * else encode any existing TLV data and use that */
	if(value.get()) return *value;
	if(!innerValues.get()) {
		value.reset(new byte_string());
		return *value;
	}
	value.reset(new byte_string());
	encodeSequence(*innerValues, *value);
	return *value;
}

size_t TLV::length() const throw() {
	size_t innerLength = valueLength();
	return tag.size() + encodedLength(innerLength) + innerLength;
}

void TLV::encodeLength(size_t value, byte_string &out) throw() {
	/* Encode and output the length according to BER-TLV encoding rules */
	static const size_t MAX_VALUE = std::numeric_limits<size_t>::max();
	static const size_t highbyte = (MAX_VALUE ^ (MAX_VALUE >> 8));
	static const size_t shiftbyte = (sizeof(size_t) - 1) * 8;
	if (value < 0x80) {
		out += (unsigned char)(value & 0x7F);
		return;
	}
	size_t size = sizeof(value), i;
	while(0 == (value & highbyte) && size > 0) {
		value <<= 8;
		size--;
	}
	out += (unsigned char)(0x80 | size);
	for(i = 0; i < size; i++) {
		out += (unsigned char)((value >> shiftbyte) & 0xFF);
		value <<= 8;
	}
}

size_t TLV::encodedLength(size_t value) throw() {
	if(value < 0x80)
		return 1;
	/* Values larger than 0x7F must be encoded in the form (Length-Bytes) (Length) */
	static const size_t MAX_VALUE = std::numeric_limits<size_t>::max();
	/* EX: 0xFF000000 - for size_t == 32-bit */
	static const size_t highbyte = (MAX_VALUE ^ (MAX_VALUE >> 8));
	size_t size = sizeof(value);
	/* Check for the highest byte that contains a value */
	while(0 == (value & highbyte) && size > 0) {
		value <<= 8;
		size--;
	}
	/* + 1 for byte-size byte
	 * Size encoded as (0x80 + N) [N-bytes]
	 * Max size-bytes == 127
	 */
	return size + 1;
}

void TLV::encodeSequence(const TLVList &tlv, byte_string &out) throw() {
	for(TLVList::const_iterator iter = tlv.begin(); iter < tlv.end(); iter++)
		(*iter)->encode(out);
}

size_t TLV::valueLength() const throw() {
	/* Calculate the length of a value, either by its actual value length
	 * or calculated length based on contained TLV values */
	if(value.get()) return value->size();
	if(!innerValues.get()) return 0;
	size_t retValue = 0;
	for(TLVList::const_iterator iter = innerValues->begin(); iter < innerValues->end(); iter++)
		retValue += (*iter)->length();
	return retValue;
}
