/*
 * oidParser.cpp - parse an Intel-style OID, with the assistance of dumpasn1.cfg.
 * The config file islooked dfor int e following locations:
 *
 *  current working directory (.)
 *  parent directory (..)
 *  The directory specified by the environment variable LOCAL_BUILD_DIR
 *
 * OidParser will still work if the config file is not found, but OIDs will be
 * dispayed in raw hex format. 
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
