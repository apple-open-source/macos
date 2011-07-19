                             libDER Library Notes
			    Last update to this file Jan. 26 2006 by dmitch

This module is a very lightweight implementation of a DER encoder and 
decoder. Unlike most other DER packages, this one does no malloc or 
copies when it encodes or decodes; decoding an item yields a pointer 
and a byte count which refer to memory inside of the "thing" being 
decoded. Likewise, when encoding, the caller mustsupply a target buffer
to which the encoded item is written. 

Support for encoding sequences and for decoding sequences and sets of 
known items is also included; when you decode a sequence, you get a
sequence of pointers and byte counts - again, no mallocs or copies occur. 

The directory libDER contains the DER decoding library proper. The main
API is in DER_Decode.h. Support for RSA keys, X509 certs, X509 CRLs, and
miscellaneous OIDs can also be found in libDER. 

Command line programs to parse and display the contents of X509 certificates
and CRLs, using libDER, can be found in the Tests directory. 

Revision History
----------------

  Date        svk tag		Changes
--------    -----------		----------------------------------------
01/26/06	 libDER-5		Avoid varargs macros for portability. 
01/03/06	 libDER-4		Initial distribution in RSACertLib.
12/23/05	 libDER-3		Fix DER_DECODE_ENABLE ifdef for DER_Decode.c.
							Add MD2, MD5 OID and DigestInfo capabilities.
12/13/05	 libDER-2		Added Apple Custom RSA public key formats. 
							Added PKCS1 RSA private keys. 
11/28/05	 libDER-1		Initial tag.

