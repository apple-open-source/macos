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

#include "cmslocal.h"

//#include "cert.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
//#include "pk11func.h"
#include "secerr.h"

/*
 * SecCmsMessageCreate - create a CMS message object
 *
 * "poolp" - arena to allocate memory from, or NULL if new arena should be created
 */
SecCmsMessage *
SecCmsMessageCreate(PLArenaPool *poolp)
{
    void *mark = NULL;
    SecCmsMessage *cmsg;
    PRBool poolp_is_ours = PR_FALSE;

    if (poolp == NULL) {
	poolp = PORT_NewArena (1024);           /* XXX what is right value? */
	if (poolp == NULL)
	    return NULL;
	poolp_is_ours = PR_TRUE;
    } 

    if (!poolp_is_ours)
	mark = PORT_ArenaMark(poolp);

    cmsg = (SecCmsMessage *)PORT_ArenaZAlloc (poolp, sizeof(SecCmsMessage));
    if (cmsg == NULL) {
	if (!poolp_is_ours) {
	    if (mark) {
		PORT_ArenaRelease(poolp, mark);
	    }
	} else
	    PORT_FreeArena(poolp, PR_FALSE);
	return NULL;
    }

    cmsg->poolp = poolp;
    cmsg->poolp_is_ours = poolp_is_ours;
    cmsg->refCount = 1;

    if (mark)
	PORT_ArenaUnmark(poolp, mark);

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
SecCmsMessageSetEncodingParams(SecCmsMessage *cmsg,
			PK11PasswordFunc pwfn, void *pwfn_arg,
			SecCmsGetDecryptKeyCallback decrypt_key_cb, void *decrypt_key_cb_arg,
			SECAlgorithmID **detached_digestalgs, CSSM_DATA **detached_digests)
{
#if 0
    // @@@ Deal with password stuff.
    if (pwfn)
	PK11_SetPasswordFunc(pwfn);
#endif
    cmsg->pwfn_arg = pwfn_arg;
    cmsg->decrypt_key_cb = decrypt_key_cb;
    cmsg->decrypt_key_cb_arg = decrypt_key_cb_arg;
    cmsg->detached_digestalgs = detached_digestalgs;
    cmsg->detached_digests = detached_digests;
}

/*
 * SecCmsMessageDestroy - destroy a CMS message and all of its sub-pieces.
 */
void
SecCmsMessageDestroy(SecCmsMessage *cmsg)
{
    PORT_Assert (cmsg->refCount > 0);
    if (cmsg->refCount <= 0)	/* oops */
	return;

    cmsg->refCount--;		/* thread safety? */
    if (cmsg->refCount > 0)
	return;

    SecCmsContentInfoDestroy(&(cmsg->contentInfo));

    /* if poolp is not NULL, cmsg is the owner of its arena */
    if (cmsg->poolp_is_ours)
	PORT_FreeArena (cmsg->poolp, PR_FALSE);	/* XXX clear it? */
}

/*
 * SecCmsMessageCopy - return a copy of the given message. 
 *
 * The copy may be virtual or may be real -- either way, the result needs
 * to be passed to SecCmsMessageDestroy later (as does the original).
 */
SecCmsMessage *
SecCmsMessageCopy(SecCmsMessage *cmsg)
{
    if (cmsg == NULL)
	return NULL;

    PORT_Assert (cmsg->refCount > 0);

    cmsg->refCount++; /* XXX chrisk thread safety? */
    return cmsg;
}

/*
 * SecCmsMessageGetArena - return a pointer to the message's arena pool
 */
PLArenaPool *
SecCmsMessageGetArena(SecCmsMessage *cmsg)
{
    return cmsg->poolp;
}

/*
 * SecCmsMessageGetContentInfo - return a pointer to the top level contentInfo
 */
SecCmsContentInfo *
SecCmsMessageGetContentInfo(SecCmsMessage *cmsg)
{
    return &(cmsg->contentInfo);
}

/*
 * Return a pointer to the actual content. 
 * In the case of those types which are encrypted, this returns the *plain* content.
 * In case of nested contentInfos, this descends and retrieves the innermost content.
 */
CSSM_DATA *
SecCmsMessageGetContent(SecCmsMessage *cmsg)
{
    /* this is a shortcut */
    return SecCmsContentInfoGetInnerContent(SecCmsMessageGetContentInfo(cmsg));
}

/*
 * SecCmsMessageContentLevelCount - count number of levels of CMS content objects in this message
 *
 * CMS data content objects do not count.
 */
int
SecCmsMessageContentLevelCount(SecCmsMessage *cmsg)
{
    int count = 0;
    SecCmsContentInfo *cinfo;

    /* walk down the chain of contentinfos */
    for (cinfo = &(cmsg->contentInfo); cinfo != NULL; cinfo = SecCmsContentInfoGetChildContentInfo(cinfo)) {
	count++;
    }
    return count;
}

/*
 * SecCmsMessageContentLevel - find content level #n
 *
 * CMS data content objects do not count.
 */
SecCmsContentInfo *
SecCmsMessageContentLevel(SecCmsMessage *cmsg, int n)
{
    int count = 0;
    SecCmsContentInfo *cinfo;

    /* walk down the chain of contentinfos */
    for (cinfo = &(cmsg->contentInfo); cinfo != NULL && count < n; cinfo = SecCmsContentInfoGetChildContentInfo(cinfo)) {
	count++;
    }

    return cinfo;
}

/*
 * SecCmsMessageContainsCertsOrCrls - see if message contains certs along the way
 */
PRBool
SecCmsMessageContainsCertsOrCrls(SecCmsMessage *cmsg)
{
    SecCmsContentInfo *cinfo;

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
PRBool
SecCmsMessageIsEncrypted(SecCmsMessage *cmsg)
{
    SecCmsContentInfo *cinfo;

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
PRBool
SecCmsMessageIsSigned(SecCmsMessage *cmsg)
{
    SecCmsContentInfo *cinfo;

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
PRBool
SecCmsMessageIsContentEmpty(SecCmsMessage *cmsg, unsigned int minLen)
{
    CSSM_DATA *item = NULL;

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
