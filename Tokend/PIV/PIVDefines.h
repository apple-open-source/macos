/*
 *  Copyright (c) 2004-2007 Apple Inc. All Rights Reserved.
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

/*
 *  PIVDefines.h
 *  TokendPIV
 */

#ifndef _PIVDEFINES_H_
#define _PIVDEFINES_H_

/*
	For the PIV tokend, refer to NIST Specical Publication 800-73-1, "Interfaces
	for Personal Identity Verification". The define for CLA_STANDARD comes from 2.3.3.1.1. [SP800731]
	See Appendix A for useful codes.
	Object identifiers: 4.2 OIDs and Tags of PIV Card Application Data Objects [SP800731]
	
	The other publication referenced here is NIST IR 6887 - 2003 Edition (GSC-IS Version 2.1) [NISTIR6887]
	
	Also useful is NIST Special Publication 800-85A [SP80085A]
	
	P1 - Reference Control Parameter
	
	See "Table 1.  SP 800-73 Data Model Containers" for RID and object IDs for data objects (p 5)
	See "Appendix A - PIV Data Model" for object IDs (p 45)
*/

#pragma mark ---------- PIV defines ----------

#define PIV_CLA_STANDARD				0x00
#define PIV_CLA_CHAIN					0x10
#define PIV_INS_SELECT_FILE				0xA4
#define PIV_INS_VERIFY_APDU				0x20	// SP800731 Section 2.3.3.2.1
#define PIV_INS_CHANGE_REFERENCE_DATA	0x24	// [SP800731 7.2.2]
#define PIV_INS_GET_DATA				0xCB	// [SP800731 7.1.2]
#define PIV_INS_GENERAL_AUTHENTICATE	0x87    // [SP800731 7.2.4]

// Placeholders for fields in the APDU to be filled in programmatically
#define TBD_ZERO			0x00
#define TBD_FF				0xFF

// These are from NISTIR6887 5.1.1.4 Select File APDU
// They are the values for the P1 field
#define SELECT_P1_EXPLICIT	0x00
#define SELECT_P1_CHILDDF	0x01
#define SELECT_P1_CHILDEF	0x02
#define SELECT_P1_PARENTDF	0x03

/*
	Reference: [SP800731] Appendix A PIV Data Model (data sizes)
	
	Name							ID		Size
	Card Capabilities Container		0xDB00	266 
	Card Holder Unique Identifier	0x3000	3377
	X.509 Certificates				------	1651
	Card Holder Fingerprints		0x6010	7768 
	Printed Information				0x3001	106
	Card Holder Facial Image		0x6030	12704
	Security Object					0x9000	1000
*/

#define PIV_MAX_DATA_SIZE           (12704+1024)		// plus some extra

#pragma mark ---------- Object IDs on Token ----------

/*
	Object IDs for objects on token. All currently 3 hex bytes.
	See 4.2 OIDs and Tags of PIV Card Application Data Objects [SP800731]

	4.1 PIV Card Application Data Objects [SP800731]
	A PIV Card Application shall contain six mandatory data objects and five optional data object for 
	interoperable use.  The six mandatory data objects for interoperable use are as follows: 

	1. Card Capability Container 
	2. Card Holder Unique Identifier  
	3. X.509 Certificate for PIV Authentication  
	4. Card Holder Fingerprint I 
	5. Card Holder Fingerprint II2 
	6. Security Object 
 
	The five optional data objects for interoperable use are as follows: 
 
	1. Card Holder Facial Image 
	2. Printed Information 
	3. X.509 Certificate for PIV Digital Signature 
	4. X.509 Certificate for PIV Key Management 
	5. X.509 Certificate for Card Authentication 
*/

//	Card Capability Container 2.16.840.1.101.3.7.1.219.0 '5FC107' M
#define PIV_OBJECT_ID_CARD_CAPABILITY_CONTAINER				0x5F, 0xC1, 0x07

//	Card Holder Unique Identifier 2.16.840.1.101.3.7.2.48.0 '5FC102' M [CHUID]
#define PIV_OBJECT_ID_CARDHOLDER_UNIQUEID					0x5F, 0xC1, 0x02

//	Card Holder Fingerprints 2.16.840.1.101.3.7.2.96.16 '5FC103' M
#define PIV_OBJECT_ID_CARDHOLDER_FINGERPRINTS				0x5F, 0xC1, 0x03

//	Printed Information 2.16.840.1.101.3.7.2.48.1 '5FC109' O
#define PIV_OBJECT_ID_PRINTED_INFORMATION					0x5F, 0xC1, 0x09

//	Card Holder Facial Image 2.16.840.1.101.3.7.2.96.48 '5FC108' O
#define PIV_OBJECT_ID_CARDHOLDER_FACIAL_IMAGE				0x5F, 0xC1, 0x08

//	X.509 Certificate for PIV Authentication 2.16.840.1.101.3.7.2.1.1 '5FC105' M
#define PIV_OBJECT_ID_X509_CERTIFICATE_PIV_AUTHENTICATION	0x5F, 0xC1, 0x05

//	X.509 Certificate for Digital Signature 2.16.840.1.101.3.7.2.1.0 '5FC10A' O
#define PIV_OBJECT_ID_X509_CERTIFICATE_DIGITAL_SIGNATURE	0x5F, 0xC1, 0x0A

//	X.509 Certificate for Key Management 2.16.840.1.101.3.7.2.1.2 '5FC10B' O
#define PIV_OBJECT_ID_X509_CERTIFICATE_KEY_MANAGEMENT		0x5F, 0xC1, 0x0B

//	X.509 Certificate for Card Authentication 2.16.840.1.101.3.7.2.5.0 '5FC101' O
#define PIV_OBJECT_ID_X509_CERTIFICATE_CARD_AUTHENTICATION	0x5F, 0xC1, 0x01

// ----------------------------------------------------------------------------
/*
	Verify APDU	[NISTIR6887 5.1.2.4]
	Function Code 0x08 
	CLA 0x00
	INS 0x20
	P1 0x00
	P2 0x00 for default key, 0x01 to 0x30 for key number 
	Lc Length of data field 
	Data Field Authentication data (i.e., password or PIN) 
	Le Empty
	
	Note:  If the Lc is 0x00 and the Data Field is empty, VERIFY returns the 
	number of tries remaining on the referenced PIN.
	
	NB: "empty" in these documents seems to mean "not present", as opposed to zeros
*/

//											0x00			0x20			P1		P2
#define PIV_VERIFY_APDU				PIV_CLA_STANDARD, PIV_INS_VERIFY_APDU, 0x00, TBD_ZERO

// Template for supplying a PIN to be verified
//													  Lc
#define PIV_VERIFY_APDU_TEMPLATE	PIV_VERIFY_APDU, 0x08, TBD_FF, TBD_FF, TBD_FF, TBD_FF, \
															TBD_FF, TBD_FF, TBD_FF, TBD_FF
// Template used to check on the lock state only
#define PIV_VERIFY_APDU_STATUS		PIV_VERIFY_APDU, 0x00
													 
#define PIV_VERIFY_APDU_INDEX_KEY	3	// Index into APDU for PIN number (i.e. which PIN)
#define PIV_VERIFY_APDU_INDEX_LEN	4	// Index into APDU for data length (always 8)
#define PIV_VERIFY_APDU_INDEX_DATA	5	// Index into APDU for PIN data

// Allowable values for P2 in VERIFY APDU
// P2 0x00 for default key, 0x01 to 0x30 for key number 
#define PIV_VERIFY_KEY_NUMBER_DEFAULT	0x00
#define PIV_VERIFY_KEY_NUMBER_MAX		0x30

#define PIV_VERIFY_PIN_LENGTH_MIN		4
#define PIV_VERIFY_PIN_LENGTH_MAX		8

// ----------------------------------------------------------------------------
/*
	CHANGE REFERENCE DATA Card Command (i.e. change PIN) [SP800731 7.2.2]
	Function Code 0x08 
	CLA			0x00
	INS			0x24
	P1			0x00
	P2			0x00 for default key, 0x01 to 0x30 for key number 
	Lc			Length of data field (always 0x10)
	Data Field	Current PIN reference data concatenated without delimitation with the 
				new PIN reference data, both PINs as described in 3.5.3 
	Le			Empty
*/

#define PIV_CHANGE_REFERENCE_DATA_APDU		PIV_CLA_STANDARD, PIV_INS_CHANGE_REFERENCE_DATA, 0x00, TBD_ZERO, TBD_ZERO
// Template for supplying a PIN to be changed
// similar to PIV_VERIFY_APDU_TEMPLATE except with space for 2 PINs
#define PIV_CHANGE_REFERENCE_DATA_APDU_TEMPLATE		\
	PIV_CHANGE_REFERENCE_DATA_APDU,					\
	TBD_FF, TBD_FF, TBD_FF, TBD_FF, TBD_FF, TBD_FF, TBD_FF, TBD_FF,	\
	TBD_FF, TBD_FF, TBD_FF, TBD_FF, TBD_FF, TBD_FF, TBD_FF, TBD_FF,	\
	0x00

// Index into APDU for new PIN data
#define PIV_CHANGE_REFERENCE_DATA_APDU_INDEX_DATA2	(PIV_VERIFY_APDU_INDEX_DATA + PIV_VERIFY_PIN_LENGTH_MAX)

// ----------------------------------------------------------------------------

/*
	Reference: [SP800731]
	
	7.1.2 GET DATA Card Command 
	The GET DATA card command retrieves the data content of the single data object
	whose tag is given in the data field. 
	
	Command Syntax 
	CLA			0x00
	INS			0xCB
	P1			0x3F
	P2			0xFF
	Lc			0x10
	Data Field	See Table 16. 
	Le			Number of data content bytes to be retrieved. 
 
	Table 16.  Data Objects in the Data Field of the GET DATA Card Command 
	Name		Tag		M/O		Comment 
	Tag list	0x5C	M		BER-TLV tag of the data object to be retrieved.  See Table 6. 
 
	Response Syntax 
	Data Field BER-TLV with the tag '53' containing in the value field the requested 
	data object.  
	SW1-SW2 Status word 
 
	SW1	 SW2		Meaning 
	'61' 'xx'	Successful execution where SW2 encodes the number of response 
				data bytes still available 
	'69' '82'	Security status not satisfied 
	'6A' '82'	Data object not found 
	'90' '00'	Successful execution
	
	Reference:
	Get Cert
	APDU: 00 CB 3F FF 05 5C 03 5F C1 05 
	APDU: 61 00 

	Get Printed Data
	APDU: 00 CB 3F FF 05 5C 03 5F C1 09 
	APDU: 61 44 
*/

//										0x00				0xCB
#define PIV_GETDATA_APDU			PIV_CLA_STANDARD, PIV_INS_GET_DATA, 0x3F, 0xFF
// Template for getting data
//									 00 CB 3F FF		Lc		Tag	  Len	    OID1	  OID2	  OID3
#define PIV_GETDATA_APDU_TEMPLATE	PIV_GETDATA_APDU, TBD_ZERO, 0x5C, TBD_ZERO, TBD_FF, TBD_FF, TBD_FF

#define PIV_GETDATA_APDU_INDEX_LEN		4	// Index into APDU for APDU data length (this is TLV<OID>) [Lc]
#define PIV_GETDATA_APDU_INDEX_OIDLEN	6	// Index into APDU for requested length of data
#define PIV_GETDATA_APDU_INDEX_OID		7	// Index into APDU for object ID

#define PIV_GETDATA_CONT_APDU_TEMPLATE	0x00, 0xC0, 0x00, 0x00, TBD_ZERO

#define PIV_GETDATA_CONT_APDU_INDEX_LEN	4	// Index into CONT APDU for requested length of data

#define PIV_GETDATA_RESPONSE_TAG		0x53
#define PIV_GETDATA_TAG_CERTIFICATE		0x70
#define PIV_GETDATA_TAG_CERTINFO		0x71
#define PIV_GETDATA_TAG_MSCUID			0x72
#define PIV_GETDATA_TAG_ERRORDETECTION	0xFE

/*
	Reference: [SP800731]	Appendix A PIV Data Model
	
	CertInfo::= BIT STRING { 
	   CompressionTypeMsb(0), // 0 = no compression and 1 = gzip compression. 
	   CompressionTypeLsb(1), // shall be set to "0" for PIV Applications 
	   IsX509(2),   // shall be set to "0" for PIV Applications 
	   RFU3(3), 
	   RFU4(4), 
	   RFU5(5), 
	   RFU6(6), 
	   RFU7(7) 
	   }
	   
	Note: the compression mask below should only be 0x80, but NASA cards use 0x01 (??)
*/
#define PIV_GETDATA_COMPRESSION_MASK	0x81

// ----------------------------------------------------------------------------

/*
Card Identifier 0xF0 Fixed 21 
Capability Container version number 0xF1 Fixed 1 
Capability Grammar version number 0xF2 Fixed 1 
Applications CardURL 0xF3 Variable 128 
PKCS#15 0xF4 Fixed 1 
Registered Data Model number 0xF5 Fixed 1 
Access Control Rule Table 0xF6 Fixed 17 
CARD APDUs 0xF7 Fixed 0 
Redirection Tag 0xFA Fixed 0 
Capability Tuples (CTs) 0xFB Fixed 0 
Status Tuples (STs) 0xFC Fixed 0 
*/

#define PIV_CCC_TAG_CARD_IDENTIFIER		0xF0
#define PIV_CCC_TAG_CARD_CONTAINER_VERS	0xF1
#define PIV_CCC_TAG_CARD_GRAMMAR_VERS	0xF2
#define PIV_CCC_TAG_APPS_URL			0xF3
#define PIV_CCC_TAG_IS_PKCS15			0xF4
#define PIV_CCC_TAG_DATA_MODEL_NUMBER	0xF5
#define PIV_CCC_TAG_ACL_RULE_TABLE		0xF6
#define PIV_CCC_TAG_CARD_APDUS			0xF7
#define PIV_CCC_TAG_REDIRECTION			0xFA
#define PIV_CCC_TAG_CAPABILITY_TUPLES	0xFB
#define PIV_CCC_TAG_STATUS_TUPLES		0xFC
#define PIV_CCC_TAG_NEXT_CCC			0xFD
#define PIV_CCC_TAG_EXTENDED_APP_URL	0xE3
#define PIV_CCC_TAG_SEC_OBJECT_BUFFER	0xB4
#define PIV_CCC_TAG_ERROR_DETECTION		0xFE

#define PIV_CCC_SZ_CARD_IDENTIFIER		21

// ----------------------------------------------------------------------------

/*
	Reference: [SP800-78-1]  6. Identifiers for PIV Card Interfaces

	Key References:
*/
#define PIV_KEYREF_PIV_AUTHENTICATION      0x9A
#define PIV_KEYREF_PIV_CARD_MANAGEMENT     0x9B
#define PIV_KEYREF_PIV_DIGITAL_SIGNATURE   0x9C
#define PIV_KEYREF_PIV_KEY_MANAGEMENT      0x9D
#define PIV_KEYREF_PIV_CARD_AUTHENTICATION 0x9E

/*
	Algorithm Identifiers:
	(Listing Only RSA)
*/
/* NOTE: After 2008/12/31 user keys will no longer be issued as 1024 */
#define PIV_KEYALG_RSA_1024    0x06
#define PIV_KEYALG_RSA_2048    0x07

/*
	Reference: [SP800-73-1]

	7.2.4 General Authenticate Command
	The GENERAL AUTHENTICATE card command performs a cryptographic operation such as an
	authentication protocol using the data provided in the data field of the command and returns the result of
	the cryptographic operation in the response data field.
	The GENERAL AUTHENTICATE command shall be used to authenticate the card or a card application
	to the client-application (INTERNAL AUTHENTICATE), to authenticate an entity to the card
	(EXTERNAL AUTHENTICATE), and to perform a mutual authentication between the card and an entity
	external to the card (MUTUAL AUTHENTICATE).
	The GENERAL AUTHENTICATE command shall be used to realize the signing functionality on the
	PIV client-application programming interface.  Data sent to the card is expected to be hashed off-card.
	The GENERAL AUTHENTICATE command supports command chaining to permit the uninterrupted
	transmission of long command data fields to the PIV Card Application.  If a card command other than the
	GENERAL AUTHENTICATICATE command is received by the PIV Card Application before the
	termination of a GENERAL AUTHENTICATE chain, the PIV Card Application shall rollback to the
	state it was in immediately prior to the reception of the first command in the interrupted chain. In other
	words, an interrupted GENERAL AUTHENTICATE chain has no effect on the PIV Card Application.

	Command Syntax
	CLA        '00' or '10' indicating command chaining.
	INS        '87'
	P1         Algorithm reference
	P2         Key reference
	Lc         Length of data field
	Data Field See Table 17.
	Le         Absent or length of expected response

	Table 17. Data Objects in the Dynamic Authentication Template (Tag '7C')
	Name           Tag   M/O Description
	Witness        '80'  C   Demonstration of knowledge of a fact without revealing
                             the fact.  An empty witness is a request for a witness.
	Challenge      '81'  C   One or more random numbers or byte sequences to be
                             used in the authentication protocol.
	Response       '82'  C   A sequence of bytes encoding a response step in an
                             authentication protocol.
	Committed      '83'  C   Hash-code of a large random number including one or  
	  challenge              more challenges
	Authentication '84'  C   Hash-code of one or more data fields and a witness data code object. 

	The data objects that appear in the dynamic authentication template (tag '7C') in the data field of the
	GENERAL AUTHENTICATE card command depend on the authentication protocol being executed.

	Response Syntax
	Data Field         Absent or authentication-related data
	SW1-SW2            Status word

	== How to use for signing/decrypting ==
	Build output data structure:
	0x7C BER-LENGTH     // Dynamic Auth Template
		0x82 0x00       // Request for Response
		0x81 BER-LENGTH // 'Challenge' the card for crypto
			data
	Assuming 256-bytes sendable each time
	while remaining data left
		if there will be more after this
			SEND 0x10 0x87 ALG KEY LEN (data chunk)
		else
			SEND 0x00 0x87 ALG KEY LEN (data chunk)
*/

// ----------------------------------------------------------------------------

#endif /* !_PIVDEFINES_H_ */
