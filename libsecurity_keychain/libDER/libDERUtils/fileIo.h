/*
 * Copyright (c) 2005-2007,2010 Apple Inc. All Rights Reserved.
 */

#ifndef	_DER_FILE_IO_H_
#define _DER_FILE_IO_H_

/*
 * Read entire file. 
 */
#ifdef __cplusplus
extern "C" {
#endif

int readFile(
	const char			*fileName,
	unsigned char		**bytes,		// mallocd and returned
	unsigned			*numBytes);		// returned

int writeFile(
	const char			*fileName,
	const unsigned char	*bytes,
	unsigned			numBytes);

#ifdef __cplusplus
}
#endif

#endif	/* _DER_FILE_IO_H_ */
