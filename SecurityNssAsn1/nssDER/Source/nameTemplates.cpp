/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
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
 */
/*
 * nameTemplates.c - ASN1 templates for X509 Name, GeneralName, etc.
 */

#include <secasn1.h>
#include "nameTemplates.h"
#include "X509Templates.h"
#include "keyTemplates.h"
#include <assert.h>
#include <Security/certextensions.h>


#pragma mark ----- Generalized NSS_TaggedItem template chooser support -----

/*
 * Generalized Template chooser.
 */
const SEC_ASN1Template * NSS_TaggedTemplateChooser(
	/* Four args passed to specific SEC_ASN1TemplateChooser */
	void *arg, 				// currently not used
	PRBool enc,			
	const char *buf,
	void *dest,
	/* array of tag/template pairs */
	const NSS_TagChoice *chooser)
{
	unsigned char tag = 0;
	const SEC_ASN1Template *templ = NULL;
	NSS_TaggedItem *item = (NSS_TaggedItem *)dest;
	
	assert(item != NULL);
	assert((chooser != NULL) && (chooser->templ != NULL));

	if(enc) {
		/* encoding: tag from an NSS_TaggedItem at *dest */
		tag = item->tag;
	}
	else {
		/* decoding: tag from raw bytes being decoded */
		tag = buf[0] & SEC_ASN1_TAGNUM_MASK;
		/* and tell caller what's coming */
		item->tag = tag;
	}
	
	/* infer template from tag */
	const NSS_TagChoice *thisChoice;
	for(thisChoice=chooser; thisChoice->templ != NULL; thisChoice++) {
		if(tag == thisChoice->tag) {
			templ = thisChoice->templ;
			break;
		}
	}
	if(templ == NULL) {
		/* 
		 * Tag not found. On decoding, this is the caller's fault
		 * and they'll have to deal with it. 
		 * On decode, pick a template guaranteed to cause a decoding
		 * failure - the template from the first array of 
		 * NSS_TagChoices should do the trick since its tag didn't match. 
		 */
		templ = chooser[0].templ;
	}
	return templ;
}

#pragma mark ----- X509 Name, RDN ------

/* AttributeTypeAndValue */

/*
 * NSS_ATV Template chooser.
 */
static const NSS_TagChoice atvChoices[] = {
	{ SEC_ASN1_PRINTABLE_STRING, SEC_PrintableStringTemplate} ,
	{ SEC_ASN1_TELETEX_STRING, SEC_TeletexStringTemplate },
	{ SEC_ASN1_UNIVERSAL_STRING, SEC_UniversalStringTemplate },
	{ SEC_ASN1_UTF8_STRING, SEC_UTF8StringTemplate },
	{ SEC_ASN1_BMP_STRING, SEC_BMPStringTemplate },
	{ SEC_ASN1_IA5_STRING, SEC_IA5StringTemplate },
	{ 0, NULL}
};

static const SEC_ASN1Template * NSS_ATVChooser(
	void *arg, 
	PRBool enc,
	const char *buf,
	void *dest)
{
	return NSS_TaggedTemplateChooser(arg, enc, buf, dest, atvChoices);
}

static const SEC_ASN1TemplateChooserPtr NSS_ATVChooserPtr = NSS_ATVChooser;

const SEC_ASN1Template NSS_ATVTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_ATV) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSS_ATV,type), },
    { SEC_ASN1_INLINE | SEC_ASN1_DYNAMIC,
	  offsetof(NSS_ATV,value),
	  &NSS_ATVChooserPtr },
    { 0, }
};

/* RelativeDistinguishedName */
const SEC_ASN1Template NSS_RDNTemplate[] = {
    { SEC_ASN1_SET_OF,
	  offsetof(NSS_RDN,atvs), NSS_ATVTemplate, sizeof(NSS_RDN) }
};

/* X509 Name */
const SEC_ASN1Template NSS_NameTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 
	  offsetof(NSS_Name,rdns), NSS_RDNTemplate, sizeof(NSS_Name) }
};

#pragma mark ----- OtherName, GeneralizedName -----

/*
 * CE_OtherName.value expressed as ASN_ANY, not en/decoded.
 */
const SEC_ASN1Template NSS_OtherNameTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CE_OtherName) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(CE_OtherName,typeId), },
    { SEC_ASN1_ANY,
	  offsetof(CE_OtherName,value), },
    { 0, }
};

/* 
 * For decoding an OtherName when it's a context-specific CHOICE
 * of a GeneralName.
 */
const SEC_ASN1Template NSS_GenNameOtherNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | NGT_OtherName,
	  0, NSS_OtherNameTemplate, sizeof(CE_OtherName) }
};

/* 
 * NSS_GeneralName template chooser.
 * First, a crufty set of templates specific to this context.
 * All offsets are zero (the fundamental type is a NSS_TaggedItem).
 *
 * NOTE WELL: RFC2459 says that all of the choices within a 
 * GeneralName (which these templates implement) have implicit
 * context-specific tags. 
 * HOWEVER: RFC2538 and the real world indicate that the directoryName
 * choice is EXPLICITLY tagged. This causes an extra layer of DER - 
 * the "thing" is wrapped in a header consisting of the tag byte 
 * (SEC_ASN1_CONTEXT_SPECIFIC plus context tag plus SEC_ASN1_CONSTRUCTED)
 * and the length field. 
 *
 * To actually implement this in the current pile-of-cruft context,
 * the directoryName and otherName choices are processed here with 
 * NSS_InnerAnyTemplate which strips off the explicit tag layer, leaving
 * further processing to the app. 
 * 
 * I sure hope we don't find certs that actually conform to RFC2459 on 
 * this. We might have to handle both. Be forewarned.
 */
 
/* inner contents of an ASN_ANY */
static const SEC_ASN1Template NSS_InnerAnyTemplate[] = {
    { SEC_ASN1_ANY | SEC_ASN1_INNER, 0, NULL, sizeof(SECItem) }
};

#define NSS_GEN_NAME_OFFSET	(offsetof(NSS_GeneralName,item))
#define NSS_GEN_NAME_SIZE	(sizeof(NSS_GeneralName))

const SEC_ASN1Template NSSOtherNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | NGT_OtherName,
      NSS_GEN_NAME_OFFSET, SEC_AnyTemplate, NSS_GEN_NAME_SIZE }
};
const SEC_ASN1Template NSSRFC822NameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | NGT_RFC822Name,
      NSS_GEN_NAME_OFFSET, SEC_IA5StringTemplate, NSS_GEN_NAME_SIZE }
};
const SEC_ASN1Template NSSDNSNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | NGT_DNSName,
      NSS_GEN_NAME_OFFSET, SEC_IA5StringTemplate, NSS_GEN_NAME_SIZE }
};
const SEC_ASN1Template NSSX400AddressTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC |  SEC_ASN1_CONSTRUCTED | NGT_X400Address,
      NSS_GEN_NAME_OFFSET, SEC_AnyTemplate, NSS_GEN_NAME_SIZE }
};
#if 0
const SEC_ASN1Template NSSDirectoryNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | NGT_DirectoryName,
      NSS_GEN_NAME_OFFSET, NSS_InnerAnyTemplate, NSS_GEN_NAME_SIZE }
};
#else
const SEC_ASN1Template NSSDirectoryNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_EXPLICIT | NGT_DirectoryName,
      NSS_GEN_NAME_OFFSET, SEC_AnyTemplate, NSS_GEN_NAME_SIZE }
	};
#endif
const SEC_ASN1Template NSSEdiPartyNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC |  SEC_ASN1_CONSTRUCTED | NGT_EdiPartyName,
      NSS_GEN_NAME_OFFSET, SEC_AnyTemplate, NSS_GEN_NAME_SIZE }
};
const SEC_ASN1Template NSSURITemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | NGT_URI,
      NSS_GEN_NAME_OFFSET, SEC_IA5StringTemplate, NSS_GEN_NAME_SIZE }
};
const SEC_ASN1Template NSSIPAddressTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | NGT_IPAddress,
      NSS_GEN_NAME_OFFSET, SEC_OctetStringTemplate, NSS_GEN_NAME_SIZE }
};
const SEC_ASN1Template NSSRegisteredIDTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | NGT_RegisteredID,
      NSS_GEN_NAME_OFFSET, SEC_ObjectIDTemplate, NSS_GEN_NAME_SIZE }
};

static const NSS_TagChoice genNameChoices[] = {
	{ NGT_OtherName, NSSOtherNameTemplate} ,
	{ NGT_RFC822Name, NSSRFC822NameTemplate },
	{ NGT_DNSName, NSSDNSNameTemplate },
	{ NGT_X400Address, NSSX400AddressTemplate },
	{ NGT_DirectoryName, NSSDirectoryNameTemplate },
	{ NGT_EdiPartyName, NSSEdiPartyNameTemplate },
	{ NGT_URI, NSSURITemplate },
	{ NGT_IPAddress, NSSIPAddressTemplate },
	{ NGT_RegisteredID, NSSRegisteredIDTemplate },
	{ 0, NULL}
};

static const SEC_ASN1Template * NSS_genNameChooser(  
	void *arg, 
	PRBool enc,
	const char *buf,
	void *dest) 
{
	return NSS_TaggedTemplateChooser(arg, enc, buf, dest, genNameChoices);
}

static const SEC_ASN1TemplateChooserPtr NSS_genNameChooserPtr =
	NSS_genNameChooser;

const SEC_ASN1Template NSS_GeneralNameTemplate[] = {
    { SEC_ASN1_DYNAMIC | SEC_ASN1_CONTEXT_SPECIFIC,
	  offsetof(NSS_GeneralName,item),		// Needed?
	  &NSS_genNameChooserPtr },
    { 0, }									// Needed?
};									
