/*
 * Copyright (c) 2002-2003,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 *
 * cuOidParser.cpp - parse an Intel-style OID, with the assistance of 
 * dumpasn1.cfg.
 *
 * The config file is looked for in the following locations:
 *
 *  current working directory (.)
 *  parent directory (..)
 *  The directory specified by the environment variable LOCAL_BUILD_DIR
 *
 * OidParser will still work if the config file is not found, but OIDs 
 * will be dispayed in raw hex format. 
 */

#ifndef	_OID_PARSER_H_
#define _OID_PARSER_H_

#include <Security/cssmtype.h>

/*
 * Generated strings go into a client-allocated char array of 
 * this size.
 */
#define OID_PARSER_STRING_SIZE	120

class OidParser
{
private:
	CSSM_DATA_PTR		configData;		// contents of  dumpasn1.cfg
public:
	/* costruct with noConfig true - skip reading config file */
	OidParser(bool noConfig=false);
	~OidParser();

	/*
	 * Parse an Intel-style OID, generating a C string in 
	 * caller-supplied buffer.
	 */
	void oidParse(
		const unsigned char	*oidp,
		unsigned			oidLen,
		char 				*strBuf);

};

#endif	/* _OID_PARSER_H_ */
