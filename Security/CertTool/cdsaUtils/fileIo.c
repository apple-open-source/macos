/*
	File:		 fileIo.c 
	
	Description: simple file read/write utilities

	Author:		dmitch

	Copyright: 	© Copyright 2001 Apple Computer, Inc. All rights reserved.
	
	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple 
	            Computer, Inc. ("Apple") in consideration of your agreement to 
				the following terms, and your use, installation, modification 
				or redistribution of this Apple software constitutes acceptance 
				of these terms.  If you do not agree with these terms, please 
				do not use, install, modify or redistribute this Apple software.

				In consideration of your agreement to abide by the following 
				terms, and subject to these terms, Apple grants you a personal, 
				non-exclusive license, under Apple's copyrights in this 
				original Apple software (the "Apple Software"), to use, 
				reproduce, modify and redistribute the Apple Software, with 
				or without modifications, in source and/or binary forms; 
				provided that if you redistribute the Apple Software in 
				its entirety and without modifications, you must retain
				this notice and the following text and disclaimers in all 
				such redistributions of the Apple Software.  Neither the 
				name, trademarks, service marks or logos of Apple Computer, 
				Inc. may be used to endorse or promote products derived from the
				Apple Software without specific prior written permission from 
				Apple.  Except as expressly stated in this notice, no other 
				rights or licenses, express or implied, are granted by Apple 
				herein, including but not limited to any patent rights that
				may be infringed by your derivative works or by other works 
				in which the Apple Software may be incorporated.

				The Apple Software is provided by Apple on an "AS IS" basis.  
				APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING 
				WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
				MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, 
				REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE 
				OR IN COMBINATION WITH YOUR PRODUCTS.

				IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, 
				INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
				LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
				LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
				ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION 
				AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED 
				AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING 
				NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE 
				HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fileIo.h"

int writeFile(
	const char			*fileName,
	const unsigned char	*bytes,
	unsigned			numBytes)
{
	int		rtn;
	int 	fd;
	
	fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if(fd <= 0) {
		return errno;
	}
	rtn = lseek(fd, 0, SEEK_SET);
	if(rtn < 0) {
		return errno;
	}
	rtn = write(fd, bytes, (size_t)numBytes);
	if(rtn != (int)numBytes) {
		if(rtn >= 0) {
			printf("writeFile: short write\n");
		}
		rtn = EIO;
	}
	else {
		rtn = 0;
	}
	close(fd);
	return rtn;
}
	
/*
 * Read entire file. 
 */
int readFile(
	const char		*fileName,
	unsigned char	**bytes,		// mallocd and returned
	unsigned		*numBytes)		// returned
{
	int rtn;
	int fd;
	char *buf;
	struct stat	sb;
	unsigned size;
	
	*numBytes = 0;
	*bytes = NULL;
	fd = open(fileName, O_RDONLY, 0);
	if(fd <= 0) {
		return errno;
	}
	rtn = fstat(fd, &sb);
	if(rtn) {
		goto errOut;
	}
	size = sb.st_size;
	buf = malloc(size);
	if(buf == NULL) {
		rtn = ENOMEM;
		goto errOut;
	}
	rtn = lseek(fd, 0, SEEK_SET);
	if(rtn < 0) {
		goto errOut;
	}
	rtn = read(fd, buf, (size_t)size);
	if(rtn != (int)size) {
		if(rtn >= 0) {
			printf("readFile: short read\n");
		}
		rtn = EIO;
	}
	else {
		rtn = 0;
		*bytes = buf;
		*numBytes = size;
	}
errOut:
	close(fd);
	return rtn;
}
