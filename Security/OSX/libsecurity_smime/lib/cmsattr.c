/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * CMS attributes.
 */

#include "cmslocal.h"

#include "secoid.h"
#include "secitem.h"

#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>

/*
 * -------------------------------------------------------------------
 * XXX The following Attribute stuff really belongs elsewhere.
 * The Attribute type is *not* part of CMS but rather X.501.
 * But for now, since CMS is the only customer of attributes,
 * we define them here.  Once there is a use outside of CMS,
 * then change the attribute types and functions from internal
 * to external naming convention, and move them elsewhere!
 */


/*
 * SecCmsAttributeCreate - create an attribute
 *
 * if value is NULL, the attribute won't have a value. It can be added later
 * with SecCmsAttributeAddValue.
 */
SecCmsAttribute *
SecCmsAttributeCreate(PRArenaPool *poolp, SECOidTag oidtag, CSSM_DATA_PTR value, Boolean encoded)
{
    SecCmsAttribute *attr;
    CSSM_DATA_PTR copiedvalue;
    void *mark;

    PORT_Assert (poolp != NULL);

    mark = PORT_ArenaMark (poolp);

    attr = (SecCmsAttribute *)PORT_ArenaZAlloc(poolp, sizeof(SecCmsAttribute));
    if (attr == NULL)
	goto loser;

    attr->typeTag = SECOID_FindOIDByTag(oidtag);
    if (attr->typeTag == NULL)
	goto loser;

    if (SECITEM_CopyItem(poolp, &(attr->type), &(attr->typeTag->oid)) != SECSuccess)
	goto loser;

    if (value != NULL) {
	if ((copiedvalue = SECITEM_AllocItem(poolp, NULL, value->Length)) == NULL)
	    goto loser;

	if (SECITEM_CopyItem(poolp, copiedvalue, value) != SECSuccess)
	    goto loser;

        if (SecCmsArrayAdd(poolp, (void ***)&(attr->values), (void *)copiedvalue) != SECSuccess)
            goto loser;
    }

    attr->encoded = encoded;

    PORT_ArenaUnmark (poolp, mark);

    return attr;

loser:
    PORT_Assert (mark != NULL);
    PORT_ArenaRelease (poolp, mark);
    return NULL;
}

/*
 * SecCmsAttributeAddValue - add another value to an attribute
 */
OSStatus
SecCmsAttributeAddValue(PLArenaPool *poolp, SecCmsAttribute *attr, CSSM_DATA_PTR value)
{
    CSSM_DATA copiedvalue;
    void *mark;

    PORT_Assert (poolp != NULL);

    mark = PORT_ArenaMark(poolp);

    /* XXX we need an object memory model #$%#$%! */
    if (SECITEM_CopyItem(poolp, &copiedvalue, value) != SECSuccess)
	goto loser;

    if (SecCmsArrayAdd(poolp, (void ***)&(attr->values), (void *)&copiedvalue) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_Assert (mark != NULL);
    PORT_ArenaRelease (poolp, mark);
    return SECFailure;
}

/*
 * SecCmsAttributeGetType - return the OID tag
 */
SECOidTag
SecCmsAttributeGetType(SecCmsAttribute *attr)
{
    SECOidData *typetag;

    typetag = SECOID_FindOID(&(attr->type));
    if (typetag == NULL)
	return SEC_OID_UNKNOWN;

    return typetag->offset;
}

/*
 * SecCmsAttributeGetValue - return the first attribute value
 *
 * We do some sanity checking first:
 * - Multiple values are *not* expected.
 * - Empty values are *not* expected.
 */
CSSM_DATA_PTR
SecCmsAttributeGetValue(SecCmsAttribute *attr)
{
    CSSM_DATA_PTR value;

    if (attr == NULL)
	return NULL;

    value = attr->values[0];

    if (value == NULL || value->Data == NULL || value->Length == 0)
	return NULL;

    if (attr->values[1] != NULL)
	return NULL;

    return value;
}

/*
 * SecCmsAttributeCompareValue - compare the attribute's first value against data
 */
Boolean
SecCmsAttributeCompareValue(SecCmsAttribute *attr, CSSM_DATA_PTR av)
{
    CSSM_DATA_PTR value;
    
    if (attr == NULL)
	return PR_FALSE;

    value = SecCmsAttributeGetValue(attr);

    return (value != NULL && value->Length == av->Length &&
	PORT_Memcmp (value->Data, av->Data, value->Length) == 0);
}

/*
 * templates and functions for separate ASN.1 encoding of attributes
 *
 * used in SecCmsAttributeArrayReorder
 */

/*
 * helper function for dynamic template determination of the attribute value
 */
static const SecAsn1Template *
cms_attr_choose_attr_value_template(void *src_or_dest, Boolean encoding, const char *buf, size_t len, void *dest)
{
    const SecAsn1Template *theTemplate;
    SecCmsAttribute *attribute;
    SECOidData *oiddata;
    Boolean encoded;

    PORT_Assert (src_or_dest != NULL);
    if (src_or_dest == NULL)
	return NULL;

    attribute = (SecCmsAttribute *)src_or_dest;

    if (encoding && attribute->encoded)
	/* we're encoding, and the attribute value is already encoded. */
	return SEC_ASN1_GET(kSecAsn1AnyTemplate);

    /* get attribute's typeTag */
    oiddata = attribute->typeTag;
    if (oiddata == NULL) {
	oiddata = SECOID_FindOID(&attribute->type);
	attribute->typeTag = oiddata;
    }

    if (oiddata == NULL) {
	/* still no OID tag? OID is unknown then. en/decode value as ANY. */
	encoded = PR_TRUE;
	theTemplate = SEC_ASN1_GET(kSecAsn1AnyTemplate);
    } else {
	switch (oiddata->offset) {
	case SEC_OID_PKCS9_SMIME_CAPABILITIES:
	case SEC_OID_SMIME_ENCRYPTION_KEY_PREFERENCE:
	    /* these guys need to stay DER-encoded */
	default:
	    /* same goes for OIDs that are not handled here */
	    encoded = PR_TRUE;
	    theTemplate = SEC_ASN1_GET(kSecAsn1AnyTemplate);
	    break;
	    /* otherwise choose proper template */
	case SEC_OID_PKCS9_EMAIL_ADDRESS:
	case SEC_OID_RFC1274_MAIL:
	case SEC_OID_PKCS9_UNSTRUCTURED_NAME:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(kSecAsn1IA5StringTemplate);
	    break;
	case SEC_OID_PKCS9_CONTENT_TYPE:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(kSecAsn1ObjectIDTemplate);
	    break;
	case SEC_OID_PKCS9_MESSAGE_DIGEST:
        case SEC_OID_APPLE_HASH_AGILITY:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(kSecAsn1OctetStringTemplate);
	    break;
	case SEC_OID_PKCS9_SIGNING_TIME:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(kSecAsn1UTCTimeTemplate); // @@@ This should be a choice between UTCTime and GeneralizedTime -- mb
	    break;
	  /* XXX Want other types here, too */
	}
    }

    if (encoding) {
	/*
	 * If we are encoding and we think we have an already-encoded value,
	 * then the code which initialized this attribute should have set
	 * the "encoded" property to true (and we would have returned early,
	 * up above).  No devastating error, but that code should be fixed.
	 * (It could indicate that the resulting encoded bytes are wrong.)
	 */
	PORT_Assert (!encoded);
    } else {
	/*
	 * We are decoding; record whether the resulting value is
	 * still encoded or not.
	 */
	attribute->encoded = encoded;
    }
    return theTemplate;
}

static const SecAsn1TemplateChooserPtr cms_attr_chooser
	= cms_attr_choose_attr_value_template;

const SecAsn1Template nss_cms_attribute_template[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecCmsAttribute) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(SecCmsAttribute,type) },
    { SEC_ASN1_DYNAMIC | SEC_ASN1_SET_OF,
	  offsetof(SecCmsAttribute,values),
	  &cms_attr_chooser },
    { 0 }
};

const SecAsn1Template nss_cms_set_of_attribute_template[] = {
    { SEC_ASN1_SET_OF, 0, nss_cms_attribute_template }
};

/* =============================================================================
 * Attribute Array methods
 */

/*
 * SecCmsAttributeArrayEncode - encode an Attribute array as SET OF Attributes
 *
 * If you are wondering why this routine does not reorder the attributes
 * first, and might be tempted to make it do so, see the comment by the
 * call to ReorderAttributes in cmsencode.c.  (Or, see who else calls this
 * and think long and hard about the implications of making it always
 * do the reordering.)
 */
CSSM_DATA_PTR
SecCmsAttributeArrayEncode(PRArenaPool *poolp, SecCmsAttribute ***attrs, CSSM_DATA_PTR dest)
{
    return SEC_ASN1EncodeItem (poolp, dest, (void *)attrs, nss_cms_set_of_attribute_template);
}

/*
 * SecCmsAttributeArrayReorder - sort attribute array by attribute's DER encoding
 *
 * make sure that the order of the attributes guarantees valid DER (which must be
 * in lexigraphically ascending order for a SET OF); if reordering is necessary it
 * will be done in place (in attrs).
 */
OSStatus
SecCmsAttributeArrayReorder(SecCmsAttribute **attrs)
{
    return SecCmsArraySortByDER((void **)attrs, nss_cms_attribute_template, NULL);
}

/*
 * SecCmsAttributeArrayFindAttrByOidTag - look through a set of attributes and
 * find one that matches the specified object ID.
 *
 * If "only" is true, then make sure that there is not more than one attribute
 * of the same type.  Otherwise, just return the first one found. (XXX Does
 * anybody really want that first-found behavior?  It was like that when I found it...)
 */
SecCmsAttribute *
SecCmsAttributeArrayFindAttrByOidTag(SecCmsAttribute **attrs, SECOidTag oidtag, Boolean only)
{
    SECOidData *oid;
    SecCmsAttribute *attr1, *attr2;

    if (attrs == NULL)
	return NULL;

    oid = SECOID_FindOIDByTag(oidtag);
    if (oid == NULL)
	return NULL;

    while ((attr1 = *attrs++) != NULL) {
	if (attr1->type.Length == oid->oid.Length && PORT_Memcmp (attr1->type.Data,
							    oid->oid.Data,
							    oid->oid.Length) == 0)
	    break;
    }

    if (attr1 == NULL)
	return NULL;

    if (!only)
	return attr1;

    while ((attr2 = *attrs++) != NULL) {
	if (attr2->type.Length == oid->oid.Length && PORT_Memcmp (attr2->type.Data,
							    oid->oid.Data,
							    oid->oid.Length) == 0)
	    break;
    }

    if (attr2 != NULL)
	return NULL;

    return attr1;
}

/*
 * SecCmsAttributeArrayAddAttr - add an attribute to an
 * array of attributes. 
 */
OSStatus
SecCmsAttributeArrayAddAttr(PLArenaPool *poolp, SecCmsAttribute ***attrs, SecCmsAttribute *attr)
{
    SecCmsAttribute *oattr;
    void *mark;
    SECOidTag type;

    mark = PORT_ArenaMark(poolp);

    /* find oidtag of attr */
    type = SecCmsAttributeGetType(attr);

    /* see if we have one already */
    oattr = SecCmsAttributeArrayFindAttrByOidTag(*attrs, type, PR_FALSE);
    PORT_Assert (oattr == NULL);
    if (oattr != NULL)
	goto loser;	/* XXX or would it be better to replace it? */

    /* no, shove it in */
    if (SecCmsArrayAdd(poolp, (void ***)attrs, (void *)attr) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

/*
 * SecCmsAttributeArraySetAttr - set an attribute's value in a set of attributes
 */
OSStatus
SecCmsAttributeArraySetAttr(PLArenaPool *poolp, SecCmsAttribute ***attrs, SECOidTag type, CSSM_DATA_PTR value, Boolean encoded)
{
    SecCmsAttribute *attr;
    void *mark;

    mark = PORT_ArenaMark(poolp);

    /* see if we have one already */
    attr = SecCmsAttributeArrayFindAttrByOidTag(*attrs, type, PR_FALSE);
    if (attr == NULL) {
	/* not found? create one! */
	attr = SecCmsAttributeCreate(poolp, type, value, encoded);
	if (attr == NULL)
	    goto loser;
	/* and add it to the list */
	if (SecCmsArrayAdd(poolp, (void ***)attrs, (void *)attr) != SECSuccess)
	    goto loser;
    } else {
	/* found, shove it in */
	/* XXX we need a decent memory model @#$#$!#!!! */
	attr->values[0] = value;
	attr->encoded = encoded;
    }

    PORT_ArenaUnmark (poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease (poolp, mark);
    return SECFailure;
}

