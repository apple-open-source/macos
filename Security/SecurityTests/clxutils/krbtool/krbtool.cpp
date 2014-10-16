/*
 * Copyright (c) 2004,2006,2008 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * krbtool.cpp - basic PKINIT tool 
 *
 * Created 20 May 2004 by Doug Mitchell.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include "identPicker.h"
#include "asnUtils.h"
#include <Kerberos/KerberosLogin.h>
#include <Kerberos/pkinit_cert_store.h>  

static void usage(char **argv)
{
    printf("Usage: op [option..]\n");
    printf("Ops:\n");
    printf("   l   login\n");
    printf("   s   select PKINIT client cert\n");
    printf("   d   display PKINIT client cert\n");
    printf("   D   delete PKINIT client cert setting\n");
    printf("Options:\n");
    printf("  -p principal      -- default is current user for login\n");
    printf("  -l                -- loop (for malloc debug)\n");
    printf("   TBD\n");
    exit(1);
}

typedef enum {
    KTO_Login = 0,
    KTO_Select,
    KTO_Display,
    KTO_DeleteCert
} KrbToolOp;

typedef struct {
    const char *principal;
    /* I'm sure others */
} KrbToolArgs;

static int pkinitLogin(
    const KrbToolArgs *args)
{
    /* Get a principal string one way or the other */
    const char *principalStr = args->principal;
    bool freeStr = false;
    int ourRtn = 0;
    KLLoginOptions klOpts = NULL;
    
    if(principalStr == NULL) {
        struct passwd *pw = getpwuid(getuid ());
	if(pw == NULL) {
	    printf("***Sorry, can't find current user info. Aborting.\n");
	    return -1;
	}
	principalStr = strdup(pw->pw_name);
	freeStr = true;
    }

    KLPrincipal principal = NULL;
    KLStatus klrtn;
    
    klrtn = KLCreatePrincipalFromString(principalStr, kerberosVersion_V5, &principal);
    if(klrtn) {
	printf("***KLCreatePrincipalFromString returned %d. Aborting.\n", (int)klrtn);
	ourRtn = -1;
	goto errOut;
    }
    
    /* Options, later maybe */
    /* FIXME - don't know if the login options arg is optional */

    /* By convention we use a non-NULL string as password; this may change later */
    printf("...attempting TGT acquisition\n");
    klrtn = KLAcquireNewInitialTicketsWithPassword(principal, klOpts, " ", NULL);
    if(klrtn) {
	printf("***KLAcquireInitialTicketsWithPassword returned %d\n", (int)klrtn);
    }
    else {
	printf("...TGT acquisition successful.\n");
    }
errOut:
    if(freeStr && (principalStr != NULL)) {
	free((void *)principalStr);
    }
    if(klOpts != NULL) {
	KLDisposeLoginOptions(klOpts);
    }
    if(principal != NULL) {
	KLDisposePrincipal(principal);
    }
    return ourRtn;
}

static int pkinitSelect(
    const KrbToolArgs *args)
{
    OSStatus ortn;
    SecIdentityRef idRef = NULL;
    
    if(args->principal == NULL) {
	printf("***You must supply a principal name for this operation.\n");
	return -1;
    }
    ortn = simpleIdentPicker(NULL, &idRef);
    switch(ortn) {
	case CSSMERR_CSSM_USER_CANCELED:
	    printf("...operation terminated with no change to your settings.\n");
	    return 0;
	case noErr:
	    break;
	default:
	    printf("***Operation aborted.\n");
	    return -1;
    }
    
    krb5_error_code krtn = krb5_pkinit_set_client_cert(args->principal, 
	(krb5_pkinit_signing_cert_t)idRef);
    if(krtn) {
	cssmPerror("krb5_pkinit_set_client_cert", krtn);
    }
    else {
	printf("...PKINIT client cert selection successful.\n\n");
    }
    if(idRef) {
	CFRelease(idRef);
    }
    return 0;
}

static int pkinitDisplay(
    const KrbToolArgs *args)
{
    krb5_pkinit_signing_cert_t idRef = NULL;
    krb5_error_code krtn;
        
    if(args->principal == NULL) {
	printf("***You must supply a principal name for this operation.\n");
	return -1;
    }
    krtn = krb5_pkinit_get_client_cert(args->principal, &idRef);
    switch(krtn) {
	case errSecItemNotFound:
	    printf("...No PKINIT client cert configured for %s.\n",
		args->principal ? args->principal : "Default");
	    break;
	case noErr:
	{
	    SecCertificateRef certRef = NULL;
	    OSStatus ortn;
	    CSSM_DATA cdata;
	    
	    ortn = SecIdentityCopyCertificate((SecIdentityRef)idRef, &certRef);
	    if(ortn) {
		cssmPerror("SecIdentityCopyCertificate", ortn);
		break;
	    }
	    ortn = SecCertificateGetData(certRef, &cdata);
	    if(ortn) {
		cssmPerror("SecCertificateGetData", ortn);
		break;
	    }
	    printf("--------- PKINIT Client Certificate ---------\n");
	    printCertName(cdata.Data, cdata.Length, NameBoth);
	    printf("---------------------------------------------\n\n");
	    
	    char *cert_hash = NULL;
	    krb5_data kcert = {0, cdata.Length, (char *)cdata.Data};
	    cert_hash = krb5_pkinit_cert_hash_str(&kcert);
	    if(cert_hash == NULL) {
		printf("***Error obtaining cert hash\n");
	    }
	    else {
		printf("Cert hash string : %s\n\n", cert_hash);
		free(cert_hash);
	    }
	    CFRelease(certRef);
	    break;
	}
	default:
	    cssmPerror("krb5_pkinit_get_client_cert", krtn);
	    printf("***Error obtaining client cert\n");
	    break;
    }
    if(idRef) {
	krb5_pkinit_release_cert(idRef);
    }

    return 0;
}

static int pkinitDeleteCert(
    const KrbToolArgs *args)
{
    krb5_error_code krtn;

    krtn = krb5_pkinit_set_client_cert(args->principal, NULL);
    if(krtn) {
	cssmPerror("krb5_pkinit_set_client_cert(NULL)", krtn);
	printf("***Error deleting client cert entry\n");
    }
    else {
	printf("...client cert setting for %s deleted\n", args->principal ? 
	    args->principal : "Default principal");
    }
    return krtn;
}

int main(int argc, char **argv)
{
    if(argc < 2) {
	usage(argv);
    }

    KrbToolOp op = KTO_Login;
    switch(argv[1][0]) {
	case 'l':
	    op = KTO_Login;
	    break;
	case 's':
	    op = KTO_Select;
	    break;
	case 'd':
	    op = KTO_Display;
	    break;
	case 'D':
	    op = KTO_DeleteCert;
	    break;
	default:
	    usage(argv);
    }
    
    extern int optind;
    extern char *optarg;
    int arg;
    int ourRtn = 0;
    KrbToolArgs args;
    bool doLoop = false;
    
    memset(&args, 0, sizeof(args));
    optind = 2;
    while ((arg = getopt(argc, argv, "p:lh")) != -1) {
	switch (arg) {
	    case 'p':
		args.principal = optarg;
		break;
	    case 'l':
		doLoop = true;
		break;
	    case 'h':
	    default:
		usage(argv);
	}
    }
    
    do {
	switch(op) {
	    case KTO_Login:
		ourRtn = pkinitLogin(&args);
		break;
	    case KTO_Select:
		ourRtn = pkinitSelect(&args);
		break;
	    case KTO_Display:
		ourRtn = pkinitDisplay(&args);
		break;
	    case KTO_DeleteCert:
		ourRtn = pkinitDeleteCert(&args);
		break;
	    default:
		printf("***BRRZAP! Internal error.\n");
		exit(1);
	}
	if(doLoop) {
	    char resp;
	    
	    fpurge(stdin);
	    printf("q to quit, anything else to loop: ");
	    resp = getchar();
	    if(resp == 'q') {
		break;
	    }
	}
    } while(doLoop);
    /* cleanup */
    return ourRtn;

}
