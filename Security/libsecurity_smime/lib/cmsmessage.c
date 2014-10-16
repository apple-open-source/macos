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
 * CMS message methods.
 */

#include <Security/SecCmsMessage.h>

#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsSignedData.h>

#include "cmslocal.h"

#include "SecAsn1Item.h"
#include "secoid.h"

#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <security_asn1/secport.h>

/*
 * SecCmsMessageCreate - create a CMS message object
 *
 * "poolp" - arena to allocate memory from, or NULL if new arena should be created
 */
SecCmsMessageRef
SecCmsMessageCreate(void)
{
    PLArenaPool *poolp;
    SecCmsMessageRef cmsg;

    poolp = PORT_NewArena (1024);           /* XXX what is right value? */
    if (poolp == NULL)
	return NULL;

    cmsg = (SecCmsMessageRef)PORT_ArenaZAlloc (poolp, sizeof(SecCmsMessage));
    if (cmsg == NULL) {
	PORT_FreeArena(poolp, PR_FALSE);
	return NULL;
    }

    cmsg->poolp = poolp;
    cmsg->contentInfo.cmsg = cmsg;
    cmsg->refCount = 1;

    return cmsg;
}

/*
 * SecCmsMessageSetEncodingParams - set up a CMS message object for encoding or decoding
 *
 * "cmsg" - message object
 * "pwfn", pwfn_arg" - callback function for getting token password
 * "decrypt_key_cb", "decrypt_key_cb_arg" - callback function for getting bulk key for encryptedData
 * "detached_digestalgs", "detached_digests" - digests from detached content
 */
void
SecCmsMessageSetEncodingParams(SecCmsMessageRef cmsg,
			PK11PasswordFunc pwfn, void *pwfn_arg,
			SecCmsGetDecryptKeyCallback decrypt_key_cb, void *decrypt_key_cb_arg)
{
#if 0
    // @@@ Deal with password stuff.
    if (pwfn)
	PK11_SetPasswordFunc(pwfn);
#endif
    cmsg->pwfn_arg = pwfn_arg;
    cmsg->decrypt_key_cb = decrypt_key_cb;
    cmsg->decrypt_key_cb_arg = decrypt_key_cb_arg;
}

/*
 * SecCmsMessageDestroy - destroy a CMS message and all of its sub-pieces.
 */
void
SecCmsMessageDestroy(SecCmsMessageRef cmsg)
{
    PORT_Assert (cmsg->refCount > 0);
    if (cmsg->refCount <= 0)	/* oops */
	return;

    cmsg->refCount--;		/* thread safety? */
    if (cmsg->refCount > 0)
	return;

    SecCmsContentInfoDestroy(&(cmsg->contentInfo));

    PORT_FreeArena (cmsg->poolp, PR_FALSE);	/* XXX clear it? */
}

/*
 * SecCmsMessageCopy - return a copy of the given message. 
 *
 * The copy may be virtual or may be real -- either way, the result needs
 * to be passed to SecCmsMessageDestroy later (as does the original).
 */
SecCmsMessageRef
SecCmsMessageCopy(SecCmsMessageRef cmsg)
{
    if (cmsg == NULL)
	return NULL;

    PORT_Assert (cmsg->refCount > 0);

    cmsg->refCount++; /* XXX chrisk thread safety? */
    return cmsg;
}

/*
 * SecCmsMessageGetContentInfo - return a pointer to the top level contentInfo
 */
SecCmsContentInfoRef
SecCmsMessageGetContentInfo(SecCmsMessageRef cmsg)
{
    return &(cmsg->contentInfo);
}

/*
 * Return a pointer to the actual content. 
 * In the case of those types which are encrypted, this returns the *plain* content.
 * In case of nested contentInfos, this descends and retrieves the innermost content.
 */
const SecAsn1Item *
SecCmsMessageGetContent(SecCmsMessageRef cmsg)
{
    /* this is a shortcut */
    SecCmsContentInfoRef cinfo = SecCmsMessageGetContentInfo(cmsg);
    const SecAsn1Item *pItem = SecCmsContentInfoGetInnerContent(cinfo);
    return pItem;
}

/*
 * SecCmsMessageContentLevelCount - count number of levels of CMS content objects in this message
 *
 * CMS data content objects do not count.
 */
int
SecCmsMessageContentLevelCount(SecCmsMessageRef cmsg)
{
    int count = 0;
    SecCmsContentInfoRef cinfo;

    /* walk down the chain of contentinfos */
    for (cinfo = &(cmsg->contentInfo); cinfo != NULL; ) {
	count++;
	cinfo = SecCmsContentInfoGetChildContentInfo(cinfo);
    }
    return count;
}

/*
 * SecCmsMessageContentLevel - find content level #n
 *
 * CMS data content objects do not count.
 */
SecCmsContentInfoRef
SecCmsMessageContentLevel(SecCmsMessageRef cmsg, int n)
{
    int count = 0;
    SecCmsContentInfoRef cinfo;

    /* walk down the chain of contentinfos */
    for (cinfo = &(cmsg->contentInfo); cinfo != NULL && count < n; cinfo = SecCmsContentInfoGetChildContentInfo(cinfo)) {
	count++;
    }

    return cinfo;
}

/*
 * SecCmsMessageContainsCertsOrCrls - see if message contains certs along the way
 */
Boolean
SecCmsMessageContainsCertsOrCrls(SecCmsMessageRef cmsg)
{
    SecCmsContentInfoRef cinfo;

    /* descend into CMS message */
    for (cinfo = &(cmsg->contentInfo); cinfo != NULL; cinfo = SecCmsContentInfoGetChildContentInfo(cinfo)) {
	if (SecCmsContentInfoGetContentTypeTag(cinfo) != SEC_OID_PKCS7_SIGNED_DATA)
	    continue;	/* next level */
	
	if (SecCmsSignedDataContainsCertsOrCrls(cinfo->content.signedData))
	    return PR_TRUE;
    }
    return PR_FALSE;
}

/*
 * SecCmsMessageIsEncrypted - see if message contains a encrypted submessage
 */
Boolean
SecCmsMessageIsEncrypted(SecCmsMessageRef cmsg)
{
    SecCmsContentInfoRef cinfo;

    /* walk down the chain of contentinfos */
    for (cinfo = &(cmsg->contentInfo); cinfo != NULL; cinfo = SecCmsContentInfoGetChildContentInfo(cinfo))
    {
	switch (SecCmsContentInfoGetContentTypeTag(cinfo)) {
	case SEC_OID_PKCS7_ENVELOPED_DATA:
	case SEC_OID_PKCS7_ENCRYPTED_DATA:
	    return PR_TRUE;
	default:
	    break;
	}
    }
    return PR_FALSE;
}

/*
 * SecCmsMessageIsSigned - see if message contains a signed submessage
 *
 * If the CMS message has a SignedData with a signature (not just a SignedData)
 * return true; false otherwise.  This can/should be called before calling
 * VerifySignature, which will always indicate failure if no signature is
 * present, but that does not mean there even was a signature!
 * Note that the content itself can be empty (detached content was sent
 * another way); it is the presence of the signature that matters.
 */
Boolean
SecCmsMessageIsSigned(SecCmsMessageRef cmsg)
{
    SecCmsContentInfoRef cinfo;

    /* walk down the chain of contentinfos */
    for (cinfo = &(cmsg->contentInfo); cinfo != NULL; cinfo = SecCmsContentInfoGetChildContentInfo(cinfo))
    {
	switch (SecCmsContentInfoGetContentTypeTag(cinfo)) {
	case SEC_OID_PKCS7_SIGNED_DATA:
	    if (!SecCmsArrayIsEmpty((void **)cinfo->content.signedData->signerInfos))
		return PR_TRUE;
	    break;
	default:
	    break;
	}
    }
    return PR_FALSE;
}

/*
 * SecCmsMessageIsContentEmpty - see if content is empty
 *
 * returns PR_TRUE is innermost content length is < minLen
 * XXX need the encrypted content length (why?)
 */
Boolean
SecCmsMessageIsContentEmpty(SecCmsMessageRef cmsg, unsigned int minLen)
{
    SecAsn1Item * item = NULL;

    if (cmsg == NULL)
	return PR_TRUE;

    item = SecCmsContentInfoGetContent(SecCmsMessageGetContentInfo(cmsg));

    if (!item) {
	return PR_TRUE;
    } else if(item->Length <= minLen) {
	return PR_TRUE;
    }

    return PR_FALSE;
}
