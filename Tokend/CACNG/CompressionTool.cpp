/*
 *  CompressionTool.cpp
 *  Tokend
 *
 *  Created by harningt on 9/30/09.
 *  Copyright 2009 TrustBearer Labs. All rights reserved.
 *
 */


#include "CompressionTool.h"
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/cssmerr.h>

#include <zlib.h>

using namespace Security;

byte_string CompressionTool::zlib_decompress(const byte_string::const_iterator &begin, const byte_string::const_iterator &end)
{
	static const int CHUNK_SIZE = 4096;
	byte_string output;
	int ret;
	z_stream strm;
	memset(&strm, 0, sizeof(strm));
	ret = inflateInit2(&strm, 15 + 32); /* Handle optional zlib/gzip headers */
	if (ret != Z_OK)
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
	strm.avail_in = end - begin;
	strm.next_in = (uint8_t*)&*begin;
	do {
		uint8_t buffer[CHUNK_SIZE];
		strm.avail_out = sizeof(buffer);
		strm.next_out = buffer;
		ret = inflate(&strm, Z_NO_FLUSH);
		switch (ret) {
		case Z_NEED_DICT:
			ret = Z_DATA_ERROR;
		case Z_DATA_ERROR:
		case Z_MEM_ERROR:
			(void)inflateEnd(&strm);
			CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
		}
		size_t available = sizeof(buffer) - strm.avail_out;
		output.insert(output.end(), buffer, buffer + available);
	} while (strm.avail_out == 0);
	(void)inflateEnd(&strm);
	if (ret != Z_STREAM_END)
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
	return output;
}