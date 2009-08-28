#ifndef CRC_H
#define CRC_H

/*
 * Everything in here is taken from PGP
 */

#define CRCBITS 24      /* may be 16, 24, or 32 */
#define CRCBYTES 3      /* may be 2, 3, or 4 */

#ifdef __alpha
#define crcword unsigned int		/* if CRCBITS is 24 or 32 */
#else
#define crcword unsigned long		/* if CRCBITS is 24 or 32 */
#endif

#define CCITTCRC  0x1021   /* CCITT's 16-bit CRC generator polynomial */
#define PRZCRC  0x864cfbL  /* PRZ's   24-bit CRC generator polynomial */
#define CRCINIT 0xB704CEL  /* Init value for CRC accumulator          */

#if (CRCBITS == 16) || (CRCBITS == 32)
#define maskcrc(crc) ((crcword) (crc))     /* if CRCBITS is 16 or 32 */
#else
#define maskcrc(crc) ((crc) & 0xffffffL)   /* if CRCBITS is 24 */
#endif


#define CRCHIBIT  ((crcword) (1L<<(CRCBITS-1))) /* 0x8000 if CRCBITS is 16 */
#define CRCSHIFTS (CRCBITS-8)

/*      Notes on making a good 24-bit CRC--
	The primitive irreducible polynomial of degree 23 over GF(2),
	040435651 (octal), comes from Appendix C of "Error Correcting Codes,
	2nd edition" by Peterson and Weldon, page 490.  This polynomial was
	chosen for its uniform density of ones and zeros, which has better
	error detection properties than polynomials with a minimal number of
	nonzero terms.  Multiplying this primitive degree-23 polynomial by
	the polynomial x+1 yields the additional property of detecting any
	odd number of bits in error, which means it adds parity.  This 
	approach was recommended by Neal Glover.

	To multiply the polynomial 040435651 by x+1, shift it left 1 bit and
	bitwise add (xor) the unshifted version back in.  Dropping the unused 
	upper bit (bit 24) produces a CRC-24 generator bitmask of 041446373 
	octal, or 0x864cfb hex.  

	You can detect spurious leading zeros or framing errors in the 
	message by initializing the CRC accumulator to some agreed-upon 
	nonzero "random-like" value, but this is a bit nonstandard.  
*/

#endif /* CRC */
