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

template<typename ForwardIterator>
void TLV::parseSequence(ForwardIterator &iter, const ForwardIterator &end, TLVList &tlv) throw(std::runtime_error) {
	/* While there is still data inbetween the iterators */
	while(iter < end) {
		/* parse TLV structures and append them to the list */
		TLV_ref ref = TLV::parse(iter, end);
		tlv.push_back(ref);
	}
}

template<typename ForwardIterator>
TLV_ref TLV::parse(ForwardIterator &iter, const ForwardIterator &end) throw(std::runtime_error) {
	byte_string tag;
	uint8_t ch;
	if(iter >= end) throw std::runtime_error("Invalid TLV-encoding");
	/* Read the first byte as the tag */
	ch = *iter++;
	tag += ch;
	if(iter >= end) throw std::runtime_error("Invalid TLV-encoding");
	/* If the tag is flagged as a multibyte tag */
	if((ch & 0x1F) == 0x1F) { /* Multibyte tag */
		do {
			ch = *iter++;
			tag += ch;
			if(iter >= end) throw std::runtime_error("Invalid TLV-encoding");
			/* Read more until there are no more bytes w/o the high-bit set */
		} while((ch & 0x80) != 0);
	}
	/* Parse the length of the contained value */
	size_t length = parseLength(iter, end);
	ForwardIterator begin = iter;
	iter += length;
	/* The iterator is permitted to be at the very and at this point */
	if(iter > end) throw std::runtime_error("Invalid TLV-encoding");
	/* Return a new TLV with the calculated tag and value */
	return TLV_ref(new TLV(tag, byte_string(begin, iter)));
}

/*
	BER-TLV
	Reference: http://www.cardwerk.com/smartcards/smartcard_standard_ISO7816-4_annex-d.aspx

	In short form, the length field consists of a single byte where the bit B8 shall be set to 0 and
	the bits B7-B1 shall encode an integer equal to the number of bytes in the value field. Any length
	from 0-127 can thus be encoded by 1 byte.

	In long form, the length field consists of a leading byte where the bit B8 shall be set to 1 and
	the B7-B1 shall not be all equal, thus encoding a positive integer equal to the number of subsequent
	bytes in the length field. Those subsequent bytes shall encode an integer equal to the number of bytes
	in the value field. Any length within the APDU limit (up to 65535) can thus be encoded by 3 bytes.

	NOTE - ISO/IEC 7816 does not use the indefinite lengths specified by the basic encoding rules of
	ASN.1 (see ISO/IEC 8825).

	Sample data (from a certficate GET DATA):

	00000000  53 82 04 84 70 82 04 78  78 da 33 68 62 db 61 d0
	00000010  c4 ba 60 01 33 13 23 13  13 97 e2 dc 88 f7 0c 40
	00000020  20 da 63 c0 cb c6 a9 d5  e6 d1 f6 9d 97 91 91 95
	....
	00000460  1f 22 27 83 ef fe ed 5e  7a f3 e8 b6 dc 6b 3f dc
	00000470  4c be bc f5 bf f2 70 7e  6b d0 4c 00 80 0d 3f 1f
	00000480  71 01 80 72 03 49 44 41

*/
template<typename ForwardIterator>
size_t TLV::parseLength(ForwardIterator &iter, const ForwardIterator &end) throw(std::runtime_error) {
	// Parse a BER length field. Returns the value of the length
	uint8_t ch = *iter++;
	if (!(ch & 0x80))	// single byte
		return static_cast<uint32_t>(ch);
	size_t result = 0;
	uint8_t byteLen = ch & 0x7F;
	for(;byteLen > 0; byteLen--) {
		if(iter == end)
			throw std::runtime_error("Invalid BER-encoded length");
		ch = *iter++;
		result = (result << 8) | static_cast<uint8_t>(ch);
	}
	return result;
}
