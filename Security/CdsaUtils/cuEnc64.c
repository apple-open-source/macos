/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All Rights Reserved.
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
 * cuEnc64.c - encode/decode in 64-char IA5 format, per RFC 1421
 */

#include "cuEnc64.h"
#include <stdlib.h>

#ifndef	NULL
#define NULL ((void *)0)
#endif	/* NULL */

/*
 * map a 6-bit binary value to a printable character.
 */
static const
unsigned char bintoasc[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * Map an 7-bit printable character to its corresponding binary value.
 * Any illegal characters return high bit set.
 */
static const
unsigned char asctobin[] =
{
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x3e, 0x80, 0x80, 0x80, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0x80, 0x80, 0x80, 0x80, 0x80
};

/*
 * map 6 bits to a printing char
 */
#define ENC(c) (bintoasc[((c) & 0x3f)])

#define PAD		'='

/*
 * map one group of up to 3 bytes at inp to 4 bytes at outp.
 * Count is number of valid bytes in *inp; if less than 3, the
 * 1 or two extras must be zeros.
 */
static void encChunk(const unsigned char *inp,
	unsigned char *outp,
	int count)
{
	unsigned char c1, c2, c3, c4;

	c1 = *inp >> 2;
	c2 = ((inp[0] << 4) & 0x30) | ((inp[1] >> 4) & 0xf);
	c3 = ((inp[1] << 2) & 0x3c) | ((inp[2] >> 6) & 0x3);
	c4 = inp[2] & 0x3f;
	*outp++ = ENC(c1);
	*outp++ = ENC(c2);
	if (count == 1) {
	    *outp++ = PAD;
	    *outp   = PAD;
	} else {
	    *outp++ = ENC(c3);
	    if (count == 2) {
		*outp = PAD;
	    }
	    else {
		*outp = ENC(c4);
	    }
	}
}

/*
 * Given input buffer inbuf, length inlen, encode to 64-char IA5 format.
 * Result is fmalloc'd and returned; it is terminated by Microsoft-style
 * newline and NULL. Its length (including the trailing newline and NULL)
 * is returned in *outlen.
 */

unsigned char *cuEnc64(const unsigned char *inbuf,
	unsigned inlen,
	unsigned *outlen)		// RETURNED
{
	return cuEnc64WithLines(inbuf, inlen, 0, outlen);
}

unsigned char *cuEnc64WithLines(const unsigned char *inbuf,
	unsigned inlen,
	unsigned linelen,
	unsigned *outlen)
{
	unsigned		outTextLen;
	unsigned 		len;			// to malloc, liberal
	unsigned		olen = 0;		// actual output size
	unsigned char 	*outbuf;
	unsigned char 	endbuf[3];
	unsigned		i;
	unsigned char 	*outp;
	unsigned		numLines;
	unsigned		thisLine;

	outTextLen = ((inlen + 2) / 3) * 4;
	if(linelen) {
	    /*
	     * linelen must be 0 mod 4 for this to work; round up...
	     */
	    if((linelen & 0x03) != 0) {
	        linelen = (linelen + 3) & 0xfffffffc;
	    }
	    numLines = (outTextLen + linelen - 1)/ linelen;
	}
	else {
	    numLines = 1;
	}

	/*
	 * Total output size = encoded text size plus one newline per
	 * line of output, plus trailing NULL. We always generate newlines 
	 * as \n; when decoding, we tolerate \r\n (Microsoft) or \n.
	 */
	len = outTextLen + (2 * numLines) + 1;
	outbuf = (unsigned char*)malloc(len);
	outp = outbuf;
	thisLine = 0;

	while(inlen) {
	    if(inlen < 3) {
			for(i=0; i<3; i++) {
				if(i < inlen) {
					endbuf[i] = inbuf[i];
				}
				else {
					endbuf[i] = 0;
				}
			}
			encChunk(endbuf, outp, inlen);
			inlen = 0;
	    }
	    else {
			encChunk(inbuf, outp, 3);
			inlen -= 3;
			inbuf += 3;
	    }
	    outp += 4;
	    thisLine += 4;
	    olen += 4;
	    if((linelen != 0) && (thisLine >= linelen) && inlen) {
	        /*
			 * last trailing newline added below
			 * Note we don't split 4-byte output chunks over newlines
			 */
	    	*outp++ = '\n';
			olen++;
			thisLine = 0;
	    }
	}
	*outp++ = '\n';
	*outp = '\0';
	olen += 2;
	*outlen = olen;
	return outbuf;
}

static inline int isWhite(unsigned char c)
{
	switch(c) {
	    case '\n':
	    case '\r':
	    case ' ':
	    case '\t':
	    case '\0':
			return 1;
	    default:
			return 0;
	}
}

/*
 * Strip off all whitespace from a (supposedly) enc64-format string.
 * Returns a malloc'd string.
 */
static unsigned char *stringCleanse(const unsigned char *inbuf,
	unsigned inlen,
	unsigned *outlen)
{
	unsigned char	*news;			// cleansed inbuf
	unsigned		newsDex;		// index into news
	unsigned		i;

	news = (unsigned char*)malloc(inlen);
	newsDex = 0;
	for(i=0; i<inlen; i++) {
	    if(!isWhite(inbuf[i])) {
	        news[newsDex++] = inbuf[i];
	    }
	}
	*outlen = newsDex;
	return news;
}

/*
 * Given input buffer inbuf, length inlen, decode from 64-char IA5 format to
 * binary. Result is malloced and returned; its length is returned in *outlen.
 * NULL return indicates corrupted input.
 *
 * All whitespace in input is ignored.
 */
unsigned char *cuDec64(const unsigned char *inbuf,
	unsigned inlen,
	unsigned *outlen)
{
	unsigned char 		*outbuf;
	unsigned char 		*outp;			// malloc'd outbuf size
	unsigned 			obuflen;
	const unsigned char	*bp;
	unsigned 			olen = 0;		// actual output size
	unsigned char 		c1, c2, c3, c4;
	unsigned char 		j;
	unsigned			thisOlen;
	unsigned char		*news;			// cleansed inbuf
	unsigned			newsLen;

	/*
	 * Strip out all whitespace; remainder must be multiple of four
	 * characters
	 */
	news = stringCleanse(inbuf, inlen, &newsLen);
	if((newsLen & 0x03) != 0) {
	    free(news);
	    return (unsigned char*) NULL;
	}
	inlen = newsLen;
	bp = news;

	obuflen = (inlen / 4) * 3;
	outbuf = (unsigned char*)malloc(obuflen);
	outp = outbuf;

	while (inlen) {
	    /*
	     * Note inlen is always a multiple of four here
	     */
	    if (*bp & 0x80 || (c1 = asctobin[*bp]) & 0x80) {
	        goto errorOut;
	    }
	    inlen--;
	    bp++;
	    if (*bp & 0x80 || (c2 = asctobin[*bp]) & 0x80){
	        goto errorOut;
	    }
	    inlen--;
	    bp++;
	    if (*bp == PAD) {
			/*
			 * two input bytes, one output byte
			 */
			c3 = c4 = 0;
			thisOlen = 1;
			if (c2 & 0xf) {
				goto errorOut;
			}
			bp++;
			inlen--;
			if (*bp == PAD) {
				bp++;
				inlen--;
				if(inlen > 0) {
					goto errorOut;
				}
			}
			else {
				goto errorOut;
			}
	    } else if (*bp & 0x80 || (c3 = asctobin[*bp]) & 0x80) {
	    	goto errorOut;
	    } else {
	        bp++;
		inlen--;
		if (*bp == PAD) {
		    /*
		     * Three input bytes, two output
		     */
		    c4 = 0;
		    thisOlen = 2;
		    if (c3 & 3) {
				goto errorOut;
		    }
		} else if (*bp & 0x80 || (c4 = asctobin[*bp]) & 0x80) {
		    goto errorOut;
		} else {
		    /*
		     * Normal non-pad case
		     */
		    thisOlen = 3;
		}
		bp++;
		inlen--;
	    }
	    j = (c1 << 2) | (c2 >> 4);
	    *outp++ = j;
	    if(thisOlen > 1) {
			j = (c2 << 4) | (c3 >> 2);
			*outp++ = j;
			if(thisOlen == 3) {
				j = (c3 << 6) | c4;
				*outp++ = j;
			}
	    }
	    olen += thisOlen;
	}
	free(news);
	*outlen = olen;
	return outbuf;			/* normal return */

errorOut:
	free(news);
	free(outbuf);
	return (unsigned char*) NULL;
}

/*
 * Determine if specified input data is valid enc64 format. Returns 1
 * if valid, 0 if not.
 * This doesn't do a full enc64 parse job; it scans for legal characters
 * and proper sync when a possible pad is found.
 */
int cuIsValidEnc64(const unsigned char *inbuf,
	unsigned inlen)
{
	int padChars = 0;	// running count of PAD chars
	int validEncChars = 0;
	unsigned char c;

	/*
	 *   -- scan inbuf
	 *   -- skip whitespace
	 *   -- count valid chars
	 *   -- ensure not more than 2 PAD chars, only at end
	 *   -- ensure valid chars mod 4 == 0
	 */

	while(inlen) {
	    c = *inbuf++;
	    inlen--;
	    if(isWhite(c)) {
	        continue;
	    }
	    if(c == PAD) {
			if(++padChars > 2) {
				return 0;		// max of 2 PAD chars at end
			}
	    }
	    else if(padChars > 0) {
			return 0;		// no normal chars after seeing PAD
	    }
	    else if((c & 0x80) || ((asctobin[c]) & 0x80)) {
			return 0;		// invalid encoded char
	    }
	    validEncChars++;
	}
	if((validEncChars & 0x03) != 0) {
	    return 0;
	}
	else {
	    return 1;
	}
}
