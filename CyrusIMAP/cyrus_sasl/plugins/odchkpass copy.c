/* DS-CHECKPASS SASL plugin
 * Rob Siemborski
 * Tim Martin 
 * $Id: odchkpass\040copy.c,v 1.2 2005/02/16 23:33:18 dasenbro Exp $
 */
/* 
 * Copyright (c) 1998-2003 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef macintosh
#include <sys/stat.h>
#endif
#include <fcntl.h>

#include <sasl.h>
#include <saslplug.h>
#include <saslutil.h>

#include "plugin_common.h"

#ifdef macintosh
#include <sasl_chkpass_plugin_decl.h>
#endif

#include <stdbool.h>
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesUtils.h>

/*****************************  Common Section  *****************************/

static const char plugin_id[] = "$Id: odchkpass\040copy.c,v 1.2 2005/02/16 23:33:18 dasenbro Exp $";

/* convert a string of 8bit chars to it's representation in hex
 * using lowercase letters
 */
static char *convert16(unsigned char *in, int inlen, const sasl_utils_t *utils)
{
    static char hex[]="0123456789abcdef";
    int lup;
    char *out;

    out = utils->malloc(inlen*2+1);
    if (out == NULL) return NULL;

    for (lup=0; lup < inlen; lup++) {
	out[lup*2] = hex[in[lup] >> 4];
	out[lup*2+1] = hex[in[lup] & 15];
    }

    out[lup*2] = 0;
    return out;
}

/*****************************  Apple Open Directory  *****************************/

/* -----------------------------------------------------------------
	sOpen_ds ()
   ----------------------------------------------------------------- */

static tDirStatus sOpen_ds ( tDirReference *inOutDirRef )
{
	tDirStatus		dsStatus	= eDSNoErr;

	dsStatus = dsOpenDirService( inOutDirRef );

	return( dsStatus );

} /* sOpen_ds */


/* -----------------------------------------------------------------
	sGet_search_node ()
   ----------------------------------------------------------------- */

tDirStatus sGet_search_node ( tDirReference inDirRef,
							 tDirNodeReference *outSearchNodeRef )
{
	tDirStatus		dsStatus	= eMemoryAllocError;
	unsigned long	uiCount		= 0;
	tDataBuffer	   *pTDataBuff	= NULL;
	tDataList	   *pDataList	= NULL;

	pTDataBuff = dsDataBufferAllocate( inDirRef, 8192 );
	if ( pTDataBuff != NULL )
	{
		dsStatus = dsFindDirNodes( inDirRef, pTDataBuff, NULL, eDSSearchNodeName, &uiCount, NULL );
		if ( dsStatus == eDSNoErr )
		{
			dsStatus = eDSNodeNotFound;
			if ( uiCount == 1 )
			{
				dsStatus = dsGetDirNodeName( inDirRef, pTDataBuff, 1, &pDataList );
				if ( dsStatus == eDSNoErr )
				{
					dsStatus = dsOpenDirNode( inDirRef, pDataList, outSearchNodeRef );
				}

				if ( pDataList != NULL )
				{
					(void)dsDataListDeAllocate( inDirRef, pDataList, true );

					free( pDataList );
					pDataList = NULL;
				}
			}
		}
		(void)dsDataBufferDeAllocate( inDirRef, pTDataBuff );
		pTDataBuff = NULL;
	}

	return( dsStatus );

} /* sGet_search_node */


/* -----------------------------------------------------------------
	sOpen_user_node ()
   ----------------------------------------------------------------- */

tDirStatus sOpen_user_node (  tDirReference inDirRef, const char *inUserLoc, tDirNodeReference *outUserNodeRef )
{
	tDirStatus		dsStatus	= eMemoryAllocError;
	tDataList	   *pUserNode	= NULL;

	pUserNode = dsBuildFromPath( inDirRef, inUserLoc, "/" );
	if ( pUserNode != NULL )
	{
		dsStatus = dsOpenDirNode( inDirRef, pUserNode, outUserNodeRef );

		(void)dsDataListDeAllocate( inDirRef, pUserNode, true );
		free( pUserNode );
		pUserNode = NULL;
	}

	return( dsStatus );

} /* sOpen_user_node */


/* -----------------------------------------------------------------
	sLook_up_user ()
   ----------------------------------------------------------------- */

tDirStatus sLook_up_user ( tDirReference inDirRef,
						  tDirNodeReference inSearchNodeRef,
						  const char *inUserID,
						  char **outAcctName,
						  char **outUserLocation )
{
	tDirStatus				dsStatus		= eMemoryAllocError;
	int						done			= false;
	unsigned long			uiRecCount		= 0;
	tDataBuffer			   *pTDataBuff		= NULL;
	tDataList			   *pUserRecType	= NULL;
	tDataList			   *pUserAttrType	= NULL;
	tRecordEntry		   *pRecEntry		= NULL;
	tAttributeEntry		   *pAttrEntry		= NULL;
	tAttributeValueEntry   *pValueEntry		= NULL;
	tAttributeValueListRef	valueRef		= 0;
	tAttributeListRef		attrListRef		= 0;
	tContextData			pContext		= NULL;
	tDataList				tdlRecName;

	if ( inUserID == NULL )
	{
		return( SASL_BADPARAM );
	}

	memset( &tdlRecName,  0, sizeof( tDataList ) );

	pTDataBuff = dsDataBufferAllocate( inDirRef, 8192 );
	if ( pTDataBuff != NULL )
	{
		dsStatus = dsBuildListFromStringsAlloc( inDirRef, &tdlRecName, inUserID, NULL );
		if ( dsStatus == eDSNoErr )
		{
			dsStatus = eMemoryAllocError;

			pUserRecType = dsBuildListFromStrings( inDirRef, kDSStdRecordTypeUsers, NULL );
			if ( pUserRecType != NULL )
			{
				pUserAttrType = dsBuildListFromStrings( inDirRef, kDSNAttrMetaNodeLocation, NULL );
				if ( pUserAttrType != NULL )
				{
					do {
						/* Get the user record(s) that matches the name */
						dsStatus = dsGetRecordList( inSearchNodeRef, pTDataBuff, &tdlRecName, eDSiExact, pUserRecType,
													pUserAttrType, false, &uiRecCount, &pContext );

						if ( dsStatus == eDSNoErr )
						{
							dsStatus = eDSInvalidName;
							if ( uiRecCount == 1 ) 
							{
								dsStatus = dsGetRecordEntry( inSearchNodeRef, pTDataBuff, 1, &attrListRef, &pRecEntry );
								if ( dsStatus == eDSNoErr )
								{
									/* Get the record name */
									(void)dsGetRecordNameFromEntry( pRecEntry, outAcctName );
			
									dsStatus = dsGetAttributeEntry( inSearchNodeRef, pTDataBuff, attrListRef, 1, &valueRef, &pAttrEntry );
									if ( (dsStatus == eDSNoErr) && (pAttrEntry != NULL) )
									{
										dsStatus = dsGetAttributeValue( inSearchNodeRef, pTDataBuff, 1, valueRef, &pValueEntry );
										if ( (dsStatus == eDSNoErr) && (pValueEntry != NULL) )
										{
											if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
											{
												/* Get the user location */
												*outUserLocation = (char *)calloc( pValueEntry->fAttributeValueData.fBufferLength + 1, sizeof( char ) );
												memcpy( *outUserLocation, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );

												/* If we don't find duplicate users in the same node, we take the first one with
													a valid mail attribute */
												done = true;
											}
											(void)dsCloseAttributeValueList( valueRef );
										}

										(void)dsDeallocAttributeEntry( inSearchNodeRef, pAttrEntry );
										pAttrEntry = NULL;

										(void)dsCloseAttributeList( attrListRef );
									}

									if ( pRecEntry != NULL )
									{
										(void)dsDeallocRecordEntry( inSearchNodeRef, pRecEntry );
										pRecEntry = NULL;
									}
								}
							}
							else
							{
								done = true;
								if ( uiRecCount > 1 )
								{
//									syslog( LOG_NOTICE, "Duplicate users %s found in directory.", inUserID );
								}
								dsStatus = eDSAuthInvalidUserName;
							}
						}
					} while ( (pContext != NULL) && (dsStatus == eDSNoErr) && (!done) );

					if ( pContext != NULL )
					{
						(void)dsReleaseContinueData( inSearchNodeRef, pContext );
						pContext = NULL;
					}
					(void)dsDataListDeallocate( inDirRef, pUserAttrType );
					pUserAttrType = NULL;
				}
				(void)dsDataListDeallocate( inDirRef, pUserRecType );
				pUserRecType = NULL;
			}
			(void)dsDataListDeAllocate( inDirRef, &tdlRecName, true );
		}
		(void)dsDataBufferDeAllocate( inDirRef, pTDataBuff );
		pTDataBuff = NULL;
	}

	return( dsStatus );

} /* sLook_up_user */


/* -----------------------------------------------------------------
	sDoCryptAuth ()
   ----------------------------------------------------------------- */

static int sDoCryptAuth (   tDirReference inDirRef,
							tDirNodeReference inUserNodeRef,
							const char *inUserID,
							const char *inPasswd )
{
	tDirStatus				dsStatus		= eDSAuthFailed;
	long					nameLen			= 0;
	long					passwdLen		= 0;
	unsigned long			curr			= 0;
	unsigned long			len				= 0;
	unsigned long			uiBuffSzie		= 0;
	tDataBuffer			   *pAuthBuff		= NULL;
	tDataBuffer			   *pStepBuff		= NULL;
	tDataNode			   *pAuthType		= NULL;

	if ( (inUserID == NULL) || (inPasswd == NULL) )
	{
		return( SASL_BADPARAM );
	}

	nameLen = strlen( inUserID );
	passwdLen = strlen( inPasswd );

	uiBuffSzie = nameLen + passwdLen + 32;

	pAuthBuff = dsDataBufferAllocate( inDirRef, uiBuffSzie );
	if ( pAuthBuff != NULL )
	{
		/* We don't use this buffer for clear text auth */
		pStepBuff = dsDataBufferAllocate( inDirRef, 256 );
		if ( pStepBuff != NULL )
		{
			pAuthType = dsDataNodeAllocateString( inDirRef, kDSStdAuthNodeNativeClearTextOK );
			if ( pAuthType != NULL )
			{
				/* User Name */
				len = nameLen;
				memcpy( &(pAuthBuff->fBufferData[ curr ]), &len, sizeof( unsigned long ) );
				curr += sizeof( unsigned long );
				memcpy( &(pAuthBuff->fBufferData[ curr ]), inUserID, len );
				curr += len;

				/* Password */
				len = passwdLen;
				memcpy( &(pAuthBuff->fBufferData[ curr ]), &len, sizeof( unsigned long ) );
				curr += sizeof( unsigned long );
				memcpy( &(pAuthBuff->fBufferData[ curr ]), inPasswd, len );
				curr += len;

				pAuthBuff->fBufferLength = curr;

				dsStatus = dsDoDirNodeAuth( inUserNodeRef, pAuthType, true, pAuthBuff, pStepBuff, NULL );

				(void)dsDataNodeDeAllocate( inDirRef, pAuthType );
				pAuthType = NULL;
			}
			(void)dsDataNodeDeAllocate( inDirRef, pStepBuff );
			pStepBuff = NULL;
		}
		(void)dsDataNodeDeAllocate( inDirRef, pAuthBuff );
		pAuthBuff = NULL;
	}

	return( dsStatus );

} /* sDoCryptAuth */


/*****************************  Server Section  *****************************/

typedef struct server_context
{
    int state;

    char userID[ 1024 ];
    char passwd[ 1024 ];
} server_context_t;

static int
odchkpass_server_mech_new ( void *glob_context __attribute__((unused)),
							sasl_server_params_t *sparams,
							const char *challenge __attribute__((unused)),
							unsigned challen __attribute__((unused)),
							void **conn_context)
{
    server_context_t *text;

    /* holds state are in */
    text = sparams->utils->malloc( sizeof( server_context_t ) );
    if ( text == NULL )
	{
		MEMERROR( sparams->utils );
		return( SASL_NOMEM );
    }

    memset( text, 0, sizeof(server_context_t));

    text->state = 1;

    *conn_context = text;

    return( SASL_OK );

} /* odchkpass_server_mech_new */

static int odchkpass_server_step1 ( server_context_t *srvr_context,
									sasl_server_params_t *sparams,
									const char *inUser,
									unsigned inUserLen )
{

    /* we shouldn't have received anything */
    if ( (inUserLen == 0) || (inUserLen >= 1024) || (inUser == NULL) )
	{
		PARAMERROR( sparams->utils );
		return( SASL_BADPARAM );
    }

	strncpy( srvr_context->userID, inUser, inUserLen );
	srvr_context->userID[ inUserLen ] = '\0';

    srvr_context->state = 2;
    
    return( SASL_CONTINUE );

} /* odchkpass_server_step1 */


static int odchkpass_server_step2 ( server_context_t *srvr_context,
									sasl_server_params_t *sparams,
									const char *inPasswd,
									unsigned inPasswdLen,
									sasl_out_params_t *oparams )
{
	int					saslResult		= SASL_OK;
	tDirStatus			dsStatus		= eDSNoErr;
	tDirReference		dirRef			= 0;
	tDirNodeReference	searchNodeRef	= 0;
	tDirNodeReference	userNodeRef		= 0;
	char			   *pAcctName		= NULL;
	char			   *pUserLoc		= NULL;

    if ( (inPasswdLen == 0) || (inPasswdLen >= 1024) || (inPasswd == NULL) )
	{
		PARAMERROR( sparams->utils );
		return( SASL_BADPARAM );
    }

	strncpy( srvr_context->passwd, inPasswd, inPasswdLen );
	srvr_context->passwd[ inPasswdLen ] = '\0';

	dsStatus = sOpen_ds( &dirRef );
	if ( dsStatus == eDSNoErr )
	{
		dsStatus = sGet_search_node( dirRef, &searchNodeRef );
		if ( dsStatus == eDSNoErr )
		{
			dsStatus = sLook_up_user( dirRef, searchNodeRef, srvr_context->userID, &pAcctName, &pUserLoc );
			if ( dsStatus == eDSNoErr )
			{
				dsStatus = sOpen_user_node( dirRef, pUserLoc, &userNodeRef );
				if ( dsStatus == eDSNoErr )
				{
					dsStatus = sDoCryptAuth( dirRef, userNodeRef, srvr_context->userID, srvr_context->passwd );
					switch ( dsStatus )
					{
						case eDSNoErr:
							saslResult = SASL_OK;
							break;

						case eDSAuthNewPasswordRequired:
							saslResult = SASL_OK;
							break;

						case eDSAuthPasswordExpired:
							saslResult = SASL_EXPIRED;
							break;

						default:
							saslResult = SASL_BADAUTH;
							break;
					}
					(void)dsCloseDirNode( userNodeRef );
				}
				else
				{
					saslResult = SASL_NOUSER;
				}
			}
			else
			{
				saslResult = SASL_NOUSER;
			}
			(void)dsCloseDirNode( searchNodeRef );
		}
		(void)dsCloseDirService( dirRef );
	}

	if ( saslResult == SASL_OK )
	{
		oparams->doneflag = 1;
		oparams->mech_ssf = 0;
		oparams->maxoutbuf = 0;
		oparams->encode_context = NULL;
		oparams->encode = NULL;
		oparams->decode_context = NULL;
		oparams->decode = NULL;
		oparams->param_version = 0;
	}

	if ( pAcctName != NULL )
	{
		saslResult = sparams->canon_user( sparams->utils->conn,
											pAcctName, 0,
											SASL_CU_AUTHID | SASL_CU_AUTHZID, oparams );
		free( pAcctName );
		pAcctName = NULL;
	}
	else
	{
		saslResult = sparams->canon_user( sparams->utils->conn,
											srvr_context->userID, 0,
											SASL_CU_AUTHID | SASL_CU_AUTHZID, oparams );
	}

	if ( pUserLoc != NULL )
	{
		free( pUserLoc );
		pUserLoc = NULL;
	}

    return( saslResult );

} /* odchkpass_server_step2 */


static int odchkpass_server_mech_step ( void *conn_context,
										sasl_server_params_t *sparams,
										const char *clientin,
										unsigned clientinlen,
										const char **serverout,
										unsigned *serveroutlen,
										sasl_out_params_t *oparams )
{
    server_context_t *srvr_context = (server_context_t *) conn_context;

    /* this should be well more than is ever needed */
    if (clientinlen > 1024)
	{
		SETERROR(sparams->utils, "CRAM-MD5 input longer than 1024 bytes");
		return( SASL_BADPROT );
    }

    switch ( srvr_context->state )
	{
		case 1:
			return odchkpass_server_step1( srvr_context, sparams,
											clientin, clientinlen );
	
		case 2:
			return odchkpass_server_step2( srvr_context, sparams,
											clientin, clientinlen,
											oparams );
	
		default:
			sparams->utils->log( NULL, SASL_LOG_ERR,
								 "Invalid CHECKPASS server step %d\n",
								 srvr_context->state );

			return( SASL_FAIL );
    }

	return( SASL_FAIL );

} /* odchkpass_server_mech_step */


static void odchkpass_server_mech_dispose ( void *conn_context,
											const sasl_utils_t *utils )
{
    server_context_t *context = (server_context_t *) conn_context;

    if ( !context )
	{
		return;
	}

    utils->free( context );

} /* odchkpass_server_mech_dispose */


static sasl_server_plug_t odchkpass_server_plugins[] = 
{
    {
	"OD-CHECKPASS",					/* mech_name */
	0,								/* max_ssf */
	SASL_SEC_NOPLAINTEXT
	| SASL_SEC_NOANONYMOUS,			/* security_flags */
	SASL_FEAT_WANT_CLIENT_FIRST,	/* features */
	NULL,							/* glob_context */
	&odchkpass_server_mech_new,		/* mech_new */
	&odchkpass_server_mech_step,	/* mech_step */
	&odchkpass_server_mech_dispose,	/* mech_dispose */
	NULL,							/* mech_free */
	NULL,							/* setpass */
	NULL,							/* user_query */
	NULL,							/* idle */
	NULL,							/* mech avail */
	NULL							/* spare */
    }
};

int odchkpass_server_plug_init(const sasl_utils_t *utils,
			     int maxversion,
			     int *out_version,
			     sasl_server_plug_t **pluglist,
			     int *plugcount)
{
    if (maxversion < SASL_SERVER_PLUG_VERSION) {
	SETERROR( utils, "OD-CHECKPASS version mismatch");
	return SASL_BADVERS;
    }

    *out_version = SASL_SERVER_PLUG_VERSION;
    *pluglist = odchkpass_server_plugins;
    *plugcount = 1;  

    return SASL_OK;
}

/*****************************  Client Section  *****************************/

typedef struct client_context {
    char *out_buf;
    unsigned out_buf_len;
} client_context_t;

static int odchkpass_client_mech_new(void *glob_context __attribute__((unused)), 
				   sasl_client_params_t *params,
				   void **conn_context)
{
    client_context_t *text;

    /* holds state are in */
    text = params->utils->malloc(sizeof(client_context_t));
    if (text == NULL) {
	MEMERROR(params->utils);
	return SASL_NOMEM;
    }

    memset(text, 0, sizeof(client_context_t));

    *conn_context = text;

    return SASL_OK;
}

static char *make_hashed(sasl_secret_t *sec, char *nonce, int noncelen, 
			 const sasl_utils_t *utils)
{
    unsigned char digest[24];  
    char *in16;

    if (sec == NULL) return NULL;

    /* do the hmac md5 hash output 128 bits */
    utils->hmac_md5((unsigned char *) nonce, noncelen,
		    sec->data, sec->len, digest);

    /* convert that to hex form */
    in16 = convert16(digest, 16, utils);
    if (in16 == NULL) return NULL;

    return in16;
}

static int odchkpass_client_mech_step(void *conn_context,
				    sasl_client_params_t *params,
				    const char *serverin,
				    unsigned serverinlen,
				    sasl_interact_t **prompt_need,
				    const char **clientout,
				    unsigned *clientoutlen,
				    sasl_out_params_t *oparams)
{
    client_context_t *text = (client_context_t *) conn_context;
    const char *authid;
    sasl_secret_t *password = NULL;
    unsigned int free_password = 0; /* set if we need to free password */
    int auth_result = SASL_OK;
    int pass_result = SASL_OK;
    int result;
    int maxsize;
    char *in16 = NULL;

    *clientout = NULL;
    *clientoutlen = 0;

    /* First check for absurd lengths */
    if (serverinlen > 1024) {
	params->utils->seterror(params->utils->conn, 0,
				"OD-CHECKPASS input longer than 1024 bytes");
	return SASL_BADPROT;
    }

    /* check if sec layer strong enough */
    if (params->props.min_ssf > params->external_ssf) {
	SETERROR( params->utils, "SSF requested of OD-CHECKPASS plugin");
	return SASL_TOOWEAK;
    }

    /* try to get the userid */
    if (oparams->authid == NULL) {
	auth_result=_plug_get_authid(params->utils, &authid, prompt_need);
	
	if ((auth_result != SASL_OK) && (auth_result != SASL_INTERACT))
	    return auth_result;
    }

    /* try to get the password */
    if (password == NULL) {
	pass_result=_plug_get_password(params->utils, &password,
				       &free_password, prompt_need);
	
	if ((pass_result != SASL_OK) && (pass_result != SASL_INTERACT))
	    return pass_result;
    }

    /* free prompts we got */
    if (prompt_need && *prompt_need) {
	params->utils->free(*prompt_need);
	*prompt_need = NULL;
    }

    /* if there are prompts not filled in */
    if ((auth_result == SASL_INTERACT) || (pass_result == SASL_INTERACT)) {
	/* make the prompt list */
	result =
	    _plug_make_prompts(params->utils, prompt_need,
			       NULL, NULL,
			       auth_result == SASL_INTERACT ?
			       "Please enter your authentication name" : NULL,
			       NULL,
			       pass_result == SASL_INTERACT ?
			       "Please enter your password" : NULL, NULL,
			       NULL, NULL, NULL,
			       NULL, NULL, NULL);
	if (result != SASL_OK) goto cleanup;
	
	return SASL_INTERACT;
    }

    if (!password) {
	PARAMERROR(params->utils);
	return SASL_BADPARAM;
    }

    result = params->canon_user(params->utils->conn, authid, 0,
				SASL_CU_AUTHID | SASL_CU_AUTHZID, oparams);
    if (result != SASL_OK) goto cleanup;

    /*
     * username SP digest (keyed md5 where key is passwd)
     */

    in16 = make_hashed(password, (char *) serverin, serverinlen,
		       params->utils);

    if (in16 == NULL) {
	SETERROR(params->utils, "whoops, make_hashed failed us this time");
	result = SASL_FAIL;
	goto cleanup;
    }

    maxsize = 32+1+strlen(oparams->authid)+30;
    result = _plug_buf_alloc(params->utils, &(text->out_buf),
			     &(text->out_buf_len), maxsize);
    if (result != SASL_OK) goto cleanup;

    snprintf(text->out_buf, maxsize, "%s %s", oparams->authid, in16);

    *clientout = text->out_buf;
    *clientoutlen = strlen(*clientout);

    /* set oparams */
    oparams->doneflag = 1;
    oparams->mech_ssf = 0;
    oparams->maxoutbuf = 0;
    oparams->encode_context = NULL;
    oparams->encode = NULL;
    oparams->decode_context = NULL;
    oparams->decode = NULL;
    oparams->param_version = 0;

    result = SASL_OK;

  cleanup:
    /* get rid of private information */
    if (in16) _plug_free_string(params->utils, &in16);

    /* get rid of all sensitive info */
    if (free_password) _plug_free_secret(params-> utils, &password);

    return result;
}

static void odchkpass_client_mech_dispose(void *conn_context,
					const sasl_utils_t *utils)
{
    client_context_t *text = (client_context_t *) conn_context;

    if (!text) return;

    if (text->out_buf) utils->free(text->out_buf);

    utils->free(text);
}

static sasl_client_plug_t odchkpass_client_plugins[] = 
{
    {
	"OD-CHECKPASS",					/* mech_name */
	0,								/* max_ssf */
	SASL_SEC_NOPLAINTEXT
	| SASL_SEC_NOANONYMOUS,			/* security_flags */
	SASL_FEAT_WANT_CLIENT_FIRST,	/* features */
	NULL,							/* required_prompts */
	NULL,							/* glob_context */
	&odchkpass_client_mech_new,		/* mech_new */
	&odchkpass_client_mech_step,	/* mech_step */
	&odchkpass_client_mech_dispose,	/* mech_dispose */
	NULL,							/* mech_free */
	NULL,							/* idle */
	NULL,							/* spare */
	NULL							/* spare */
    }
};

int odchkpass_client_plug_init(const sasl_utils_t *utils,
			     int maxversion,
			     int *out_version,
			     sasl_client_plug_t **pluglist,
			     int *plugcount)
{
    if (maxversion < SASL_CLIENT_PLUG_VERSION) {
	SETERROR( utils, "OD-CHECKPASS version mismatch");
	return SASL_BADVERS;
    }

    *out_version = SASL_CLIENT_PLUG_VERSION;
    *pluglist = odchkpass_client_plugins;
    *plugcount = 1;

    return SASL_OK;
}
