/*
 *  CompressionTool.h
 *  Tokend
 *
 *  Created by harningt on 9/30/09.
 *  Copyright 2009 TrustBearer Labs. All rights reserved.
 *
 */


#include "byte_string.h"

class CompressionTool {
public:
	static byte_string zlib_decompress(const byte_string &compressedData) {
		return zlib_decompress(compressedData.begin(), compressedData.end());
	}
	static byte_string zlib_decompress(const byte_string::const_iterator &begin, const byte_string::const_iterator &end);
};