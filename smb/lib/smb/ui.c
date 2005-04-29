/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */
/*      @(#)ui.c      *
 *      (c) 2001   Apple Computer, Inc.  All Rights Reserved
 *
 *
 *      ui.c -- Routines for interacting with the user to get credentials
 *				(workgroup/domain, username, password, etc.)
 *
 *      MODIFICATION HISTORY:
 *       2-Aug-2001     Pat Dirks    New today, based on webdav_authentication.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <UNCUserNotification.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <netsmb/smb.h>
#include <charsets.h>

/* hide keychain redundancy and complexity */
#define kc_add		SecKeychainAddInternetPassword
#define kc_find		SecKeychainFindInternetPassword
#define kc_get		SecKeychainCopyDefault
#define kc_getattr	SecKeychainItemCopyContent
#define kc_release	CFRelease
#define kc_replace	SecKeychainItemModifyContent
#define kc_status	SecKeychainGetStatus

#if 0
#include <CarbonCore/MacErrors.h>
#endif

#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_netshareenum.h>

#include "charsets.h"

#define SMB_DOMAINNAME_KEY "Workgroup or Domain"
#define SMB_USERNAME_KEY "Name"
#define SMB_PASSWORD_KEY "Password"

#define SMB_LOCALIZATION_BUNDLE "/System/Library/Filesystems/smbfs.fs"
#define SMB_SERVER_ICON_PATH "/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/GenericFileServerIcon.icns"

#define SMB_AUTH_TIMEOUT 300


static void
smb_tty_prompt(char *prmpt, char *buf, size_t buflen)
{
	char temp[128];
	char *cp;

	if (buf && !buf[0]) {
		fprintf(stderr, "%s: ", prmpt);
		if (!fgets(temp, sizeof(temp), stdin))
			temp[0] = '\0';
		else if ((cp = strchr(temp, '\n')))
			*cp = '\0';	/* Strips trailing newline */
		strncpy(buf, temp, buflen);
	}
	return;
}


static void
smb_unc_val(UNCUserNotificationRef UNCref, int field_index, char *buf,
	   size_t buflen)
{
	const char *resp;

	resp = UNCUserNotificationGetResponseValue(UNCref, 
						   kUNCTextFieldValuesKey,
						   field_index);
	if (!resp || !strlen(resp)) {
		bzero(buf, buflen);
	} else {
		strncpy(buf, resp, buflen);
		buf[buflen - 1] = '\0'; /* guarantee null termination */
	}
	return;
}


void
smb_save2keychain(struct smb_ctx *ctx)
{
	int err;
	struct smbioc_ossn *ssn = &ctx->ct_ssn;
	char *srv = ssn->ioc_srvname;
	char *wkgrp = ssn->ioc_workgroup;
	char *usr = ssn->ioc_user;
	char *sh = ctx->ct_sh.ioc_share;
	char *pw = ssn->ioc_password;
	SecKeychainItemRef kcitem;

	if (ctx->ct_secblob)
		return;
	if (!(ctx->ct_flags & SMBCF_KCSAVE))
		return;

	/* ok, "add to keychain" was requested... */
	ctx->ct_flags &= ~SMBCF_KCSAVE;
	if (!strcmp(sh, "IPC$"))
		sh = "";
	err = kc_add(NULL, strlen(srv), srv, strlen(wkgrp), wkgrp,
		     strlen(usr), usr, strlen(sh), sh, SMB_TCP_PORT, 'smb ',
		     kSecAuthenticationTypeNTLM, strlen(pw), pw, NULL);
	if (err) {
		if (err != errSecDuplicateItem) {
			smb_error("add to keychain error %d", 0, err);
			return;
		}
		err = kc_find(NULL, strlen(srv), srv, strlen(wkgrp), wkgrp,
			      strlen(usr), usr, strlen(sh), sh, SMB_TCP_PORT,
			      'smb ', kSecAuthenticationTypeNTLM, NULL, NULL,
			      &kcitem);
		if (err) {
			smb_error("find dup keychain item error %d", 0, err);
			return;
		}
		err = kc_replace(kcitem, NULL, strlen(pw), pw);
		if (err) {
			smb_error("keychain modify content error %d", 0, err);
			return;
		}
	}
	if (!*sh)
		return;
	sh = "";
	err = kc_find(NULL, strlen(srv), srv, strlen(wkgrp), wkgrp,
		      strlen(usr), usr, strlen(sh), sh, SMB_TCP_PORT,
		      'smb ', kSecAuthenticationTypeNTLM, NULL, NULL, &kcitem);
	if (err != errSecItemNotFound)
		return;
	err = kc_add(NULL, strlen(srv), srv, strlen(wkgrp), wkgrp,
		     strlen(usr), usr, strlen(sh), sh, SMB_TCP_PORT, 'smb ',
		     kSecAuthenticationTypeNTLM, strlen(pw), pw, NULL);
	if (err)
		smb_error("add null to keychain error %d", 0, err);
}
 
int
smb_get_authentication(char *wrkgrp, size_t wrkgrplen,
		       char *usrname, size_t usrnamelen,
		       char *passwd, size_t passwdlen,
		       const char *systemname, struct smb_ctx *ctx)
{
	char *sharename = ctx->ct_sh.ioc_share;
	int error, i, kcask, kcerr;
	unsigned response = 0;
	UNCUserNotificationRef UNCref;
	int domainindex = -1;	/* So 1st next field will = 0 */
	int nameindex = -1;
	int passindex = -1;
	const char *dialogue[100]; /* current usage is a mere 27 */
	UInt32 kcpasswdlen;
	char *kcpasswd;
	SecKeychainRef kc = NULL;
	char *gui_systemname;
	char *free_buffer = NULL;
	SecKeychainItemRef kcitem;
	SecKeychainAttribute kats[] = {
		{ kSecAccountItemAttr, 0, NULL },
		{ kSecSecurityDomainItemAttr, 0, NULL } };
	SecKeychainAttributeList katlist = { sizeof(kats) / sizeof(kats[0]),
					     kats };
	char *wgptr;
	
	if (ctx->ct_flags & SMBCF_KCFOUND) {
		ctx->ct_flags &= ~SMBCF_KCFOUND;
	} else {
		ctx->ct_flags &= ~SMBCF_KCFOUND;
		if (!strcmp(sharename, "IPC$"))
			sharename = "";
kcagain:
		kcerr = kc_find(NULL, strlen(systemname), systemname, 0, NULL,
				0, NULL, strlen(sharename), sharename,
				SMB_TCP_PORT, 'smb ',
				kSecAuthenticationTypeNTLM, &kcpasswdlen,
				(void **)&kcpasswd, &kcitem);
		if (!kcerr) {
			if (kcpasswdlen >= passwdlen) {
				smb_error("bogus password in keychain, len=%d",
					  0, kcpasswdlen);
			} else {
				ctx->ct_flags |= SMBCF_KCFOUND;
				memcpy(passwd, kcpasswd, kcpasswdlen);
				passwd[kcpasswdlen] = '\0';
				kcerr = kc_getattr(kcitem, NULL, &katlist,
						   NULL, NULL);
				if (kcerr) {
					smb_error("keychain attrs! error=%d",
						  0, kcerr);
				} else {
					if (kats[0].length > SMB_MAXUSERNAMELEN)
						smb_error("username size!", 0);
					else {
						memcpy(usrname, kats[0].data,
						       kats[0].length);
						usrname[kats[0].length] = '\0';
					}
					if (kats[1].length > SMB_MAXUSERNAMELEN)
						smb_error("workgroup size!", 0);
					else {
						memcpy(wrkgrp, kats[1].data,
						       kats[1].length);
						wrkgrp[kats[1].length] = '\0';
					}
					SecKeychainItemFreeContent(&katlist,
								   NULL);
				}
#if DEBUGKEYCHAIN
				printf("kc_getattr wkgrp %s user %s pass %s\n",
				       wrkgrp, usrname, passwd);
#endif
				return (0);
			}
		} else if (kcerr == errSecItemNotFound) {
			/*
			 * Password from a browse entry ("IPC$") is worth
			 * trying...  and essential for automount as then
			 * we use only one user/password for all shares.
			 */
			if (sharename && *sharename) {
				sharename = "";
				goto kcagain;
			}
		} else if (kcerr != -128) { /* userCanceledErr */
			smb_error("unexpected SecKeychainFind error %d",
				  0, kcerr);
		}
	}
	if (isatty(STDIN_FILENO)) { /* need command-line prompting? */
		smb_tty_prompt(SMB_DOMAINNAME_KEY, wrkgrp, wrkgrplen);
		smb_tty_prompt(SMB_USERNAME_KEY, usrname, usrnamelen);
		if (passwd && passwd[0] == '\0')
			strncpy(passwd, getpass(SMB_PASSWORD_KEY ":"),
				passwdlen);
		return (0);
	}
	kcask = 0;
	kcerr = kc_get(&kc);
	if (!kcerr) { /* keychain exists? */
		SecKeychainStatus status;
		kcask = !kc_status(kc, &status);
#if 0
		printf("kc_status %d status 0x%x\n", kcask, (int)status);
#endif
		kc_release(kc);
	} else if (kcerr != errSecNoDefaultKeychain)
		smb_error("copy 'default' keychain error %d", 0, kcerr);
	i = 0;
	dialogue[i++] = kUNCLocalizationPathKey;
	dialogue[i++] = SMB_LOCALIZATION_BUNDLE;
	dialogue[i++] = kUNCIconPathKey;
	dialogue[i++] = SMB_SERVER_ICON_PATH;
	dialogue[i++] = kUNCAlertHeaderKey;
	dialogue[i++] = "SMB_AUTH_HEADER_KEY";
	/*
	 * In the future, there will be a symbolic string constant for "AlertMessageWithParameters".
	 * It will probably be called kUNCAlertMessageWithParametersKey.
	 */
	dialogue[i++] = "AlertMessageWithParameters";
	dialogue[i++] = "SMB_AUTH_MSG_WITH_PARAMETERS_KEY";    
	/*
	 * In the future, there will be a symbolic string constant for "AlertMessageParameter".
	 * It will probably be called kUNCAlertMessageParameterKey.
	 */
	dialogue[i++] = "AlertMessageParameter";

	free_buffer = convert_wincs_to_utf8(systemname);
	/*
	 * gui_systemname points to a buffer allocated
	 * in the previous call, or NULL if the 
	 * conversion failed. If not null we will
	 * free it later.
	 */
	if (free_buffer)
		gui_systemname = free_buffer;
	else /* conversion failed */
		gui_systemname = " ";

	dialogue[i++] = gui_systemname;
	if (wrkgrp) {
		dialogue[i++] = kUNCTextFieldTitlesKey;
		dialogue[i++] = "SMB_AUTH_DOMAIN_KEY";
		domainindex = 0;
	}
	if (usrname) {
		dialogue[i++] = kUNCTextFieldTitlesKey;
		dialogue[i++] = "SMB_AUTH_USERNAME_KEY";
		nameindex = domainindex + 1;
	}
	if (passwd) {
		dialogue[i++] = kUNCTextFieldTitlesKey;
		dialogue[i++] = "SMB_AUTH_PASSWORD_KEY";
		passindex = nameindex + 1;
/*		dialogue[i++] = kUNCTextFieldValuesKey;		*/
/*		dialogue[i++] = passwd;				*/
	}
	if (wrkgrp) {
		dialogue[i++] = kUNCTextFieldValuesKey;
		dialogue[i++] = wrkgrp;
	}
	if (usrname) {
		dialogue[i++] = kUNCTextFieldValuesKey;
		dialogue[i++] = usrname;
	}
	if (kcask) {
		dialogue[i++] = kUNCCheckBoxTitlesKey;
		dialogue[i++] = "SMB_AUTH_KEYCHAIN_KEY";
	}
	dialogue[i++] = kUNCDefaultButtonTitleKey;
	dialogue[i++] = "SMB_AUTH_OK_KEY";
	dialogue[i++] = kUNCAlternateButtonTitleKey;
	dialogue[i++] = "SMB_AUTH_CANCEL_KEY";
	dialogue[i++] = 0;

	UNCref = UNCUserNotificationCreate(SMB_AUTH_TIMEOUT,
					   UNCSecureTextField(passindex),
					   &error, dialogue);
	if (error) {
		if (free_buffer)
			free(free_buffer);
		return (error);
	}
	error = UNCUserNotificationReceiveResponse(UNCref, SMB_AUTH_TIMEOUT,
						   &response);
	if (error) {
		if (free_buffer)
			free(free_buffer);
		return (error); /* probably MACH_RCV_TIMED_OUT */
	}
	if ((response & 0x3) == kUNCAlternateResponse) {
		error = ECANCELED;
	} else {	/* fill in domain, username, and password */
		if (wrkgrp) {
			/*
			 * Just leave the domain name as UTF-8.
			 * the kernel/netsmb routines convert to unicode-16
			 */
			smb_unc_val(UNCref, domainindex, wrkgrp,
				    SMB_MAXUSERNAMELEN + 1);

			/* Now uppercase the WG */
			for (wgptr = wrkgrp; *wgptr; wgptr++)
				*wgptr = toupper(*wgptr);
		}
		if (usrname)
			smb_unc_val(UNCref, nameindex, usrname, usrnamelen);
		if (passwd)
			smb_unc_val(UNCref, passindex, passwd, passwdlen);
		if (kcask && response & UNCCheckBoxChecked(0))
			ctx->ct_flags |= SMBCF_KCSAVE;
		else
			ctx->ct_flags &= ~SMBCF_KCSAVE;
	}
	UNCUserNotificationFree(UNCref);

	/* Note we (must) allow entry of null password and username */

	if (free_buffer)
		free(free_buffer);
	return (error);
}

/* qsort comparison function for sorting the SMB share list */
static int 
compFn(const void *ptr1, const void *ptr2) 
{
	if (ptr1 == NULL || ptr2 == NULL)
		return 0;

	/*
	 * XXX - this compares UTF-8 strings; should we do a
	 * dictionary comparison?
	 */
	return strcmp((*(struct share_info **) ptr1)->netname,
		      (*(struct share_info **) ptr2)->netname);
}


static int
smb_browse_int(struct smb_ctx *ctx, int anon)
{  
	struct share_info *share_info = NULL, *ep;
	struct share_info **sortbuf = NULL;
	int error, entries, total, ch, maxch;
	char **choices = NULL;

	CFUserNotificationRef un;
	CFMutableDictionaryRef d = NULL;
	CFMutableArrayRef shares = NULL;
	CFOptionFlags respflags;
	CFDictionaryRef rd = NULL;
	SInt32 i;
	CFURLRef urlRef;
	void * nvalue;
	int connected = 0;

	(void)smb_ctx_setshare(ctx, "IPC$", SMB_ST_ANY);
	error = smb_ctx_lookup(ctx, SMBL_SHARE, SMBLK_CREATE);
	if (error) {
		(void)smb_ctx_setshare(ctx, "", SMB_ST_ANY); /* done w/ IPC$ */
		/* if attempting anon we must swallow auth errors */
		if (anon && smb_autherr(error)) {
			error = 0;
		} else
			smb_error("could not login to server %s", error,
				  ctx->ct_ssn.ioc_srvname);
		error &= ~SMB_ERRTYPE_MASK;
		goto exit;
	}
	connected = 1;

	error = smb_netshareenum(ctx, &entries, &total, &share_info);
	(void)smb_ctx_setshare(ctx, "", SMB_ST_ANY); /* all done with IPC$ */
	if (error) {
		if (anon && smb_autherr(error)) {
			error = 0;
		} else
			smb_error("unable to list resources", error);
		error &= ~SMB_ERRTYPE_MASK;
		goto exit;
	}

	smb_save2keychain(ctx);

	/* XXX handle all theoretical errors from CF calls */

	if (!(ctx->ct_flags & SMBCF_XXX)) {
		d = CFDictionaryCreateMutable(NULL, 0,
					      &kCFTypeDictionaryKeyCallBacks,
					      &kCFTypeDictionaryValueCallBacks);
		shares = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}

	/* sort the list of shares */
	sortbuf = malloc( entries * sizeof(struct share_info *));
	if (sortbuf) {
	    for(i = 0; i < entries; i++)
		sortbuf[i] = share_info+i;
	
	    qsort(sortbuf, entries, sizeof(struct share_info *), compFn);
	}

	choices = malloc((entries+2) * sizeof(char *)); /* XXX handle failure */
	for (i = 0, ch = 0; i < entries; i++) {
		CFStringRef s;
		int nlen;

		if (sortbuf)
			ep = sortbuf[i];
		else ep = share_info+i;

		if (ep->type != 0) /* we want "disk" type only */
			continue;
		nlen = strlen(ep->netname);
		if (nlen == 0 || ep->netname[nlen - 1] == '$')
			continue;	/* hide administrative shares */
		if (!(ctx->ct_flags & SMBCF_XXX)) {
			s = CFStringCreateWithCString(NULL, ep->netname,
			    kCFStringEncodingUTF8);
			if (s == NULL) {
				smb_error("skipping \"%s\"", -1, ep->netname);
				continue;
			}
			CFArrayAppendValue(shares, s);
			CFRelease(s);
		}
		choices[ch++] = ep->netname;
	}
	if (ctx->ct_flags & SMBCF_XXX) {
		if (ctx->ct_maxxxx && ch > ctx->ct_maxxxx)
			ch = 0; /* mount none if there are too many */

		/*
		 * Put in a null terminator so our caller knows
		 * where the end of the list is, and store a pointer
		 * to the raw share list so they can free it when
		 * they're done with it.
		 */
		choices[ch] = NULL;
		choices[ch+1] = (char *)share_info;
		ctx->ct_xxx = choices;
		error = 0;
		goto exit2;	/* free neither choices nor share_info */
	}
	if (ch == 0) {
		/* XXX probably should be quiet */
		smb_error("no shares found", 0);
		goto exit;
	}
	maxch = ch;

	urlRef = CFURLCreateFromFileSystemRepresentation(NULL, SMB_LOCALIZATION_BUNDLE, strlen(SMB_LOCALIZATION_BUNDLE), true);
	CFDictionaryAddValue(d, kCFUserNotificationLocalizationURLKey,
			     urlRef);
	CFRelease(urlRef);
	urlRef = CFURLCreateFromFileSystemRepresentation(NULL, SMB_SERVER_ICON_PATH, strlen(SMB_SERVER_ICON_PATH), true);
	CFDictionaryAddValue(d, kCFUserNotificationIconURLKey,
			     urlRef);
	CFRelease(urlRef);
	CFDictionaryAddValue(d, kCFUserNotificationAlertHeaderKey,
			     CFSTR("SMB_SELECT_HEADER_KEY"));
	CFDictionaryAddValue(d, kCFUserNotificationPopUpTitlesKey, shares);
	CFRelease(shares);

	CFDictionaryAddValue(d, kCFUserNotificationDefaultButtonTitleKey,
			     CFSTR("SMB_SELECT_OK_KEY"));
	CFDictionaryAddValue(d, kCFUserNotificationAlternateButtonTitleKey,
			     CFSTR("SMB_SELECT_CANCEL_KEY"));
	if (!ctx->ct_secblob) {
		if (anon) {
			CFDictionaryAddValue(d, kCFUserNotificationOtherButtonTitleKey, CFSTR("SMB_SELECT_AUTHENTICATE_KEY"));
		} else {
			CFDictionaryAddValue(d, kCFUserNotificationOtherButtonTitleKey, CFSTR("SMB_SELECT_REAUTHENTICATE_KEY"));
		}
	}
	un = CFUserNotificationCreate(NULL, 0, kCFUserNotificationNoteAlertLevel|CFUserNotificationSecureTextField(1), (SInt32 *)&error, d);
	CFRelease(d);
	if (!un) {
		smb_error("UNC send error", error);
		exit(error); /* XXX handle gracefully? */
	}

	error = CFUserNotificationReceiveResponse(un, 0, &respflags);
	if (error) {
		smb_error("UNC receive error", error);
		exit(error); /* XXX handle gracefully? */
	}

	if ((respflags & 0x3) == kCFUserNotificationDefaultResponse) {
		ch = maxch; /* cause EINVAL if we can't get value */
		errno = 0;
		rd = CFUserNotificationGetResponseDictionary(un);
		if (!rd)
			smb_error("UNC no resp dict, rd=0x%x", 0, (int)rd);
		else if (!CFDictionaryGetValueIfPresent(rd,
					kCFUserNotificationPopUpSelectionKey,
					(const void **)&nvalue)) {
			smb_error("UNC no selection key, rd=0x%x", 0, (int)rd);
		} else {
			CFTypeID t = CFGetTypeID(nvalue);
			if (t == CFNumberGetTypeID()) {
				if (!CFNumberGetValue(nvalue,
						      kCFNumberSInt32Type,
						      &ch))
					smb_error("UNC NumberGetValue", 0);
			} else if (t == CFStringGetTypeID()) {
				char cs[64];
				/*
				 * Not using CFStringGetIntValue here as that
				 * api os broken - it can't fold zeroes in with
				 * errors
				 */
				if (!CFStringGetCString(nvalue, cs, sizeof cs,
							kCFStringEncodingASCII))
					smb_error("UNC GetCString", 0);
				else {
					ch = strtol(cs, NULL, 0);
					if (errno)
						smb_error("UNC strtol", 0);
				}
			} else
				smb_error("UNC api change?, t=0x%x", 0, (int)t);
		}
		if (errno)
			smb_error("UNC selection error", 0);
		else if (ch >= maxch) {
			smb_error("UNC selection out of bounds %d > %d", 0,
				  ch, maxch);
			error = EINVAL;
		} else
			error = smb_ctx_setshare(ctx, choices[ch], SMB_ST_DISK);
	} else if ((respflags & 0x3) == kCFUserNotificationOtherResponse) {
		ctx->ct_flags |= SMBCF_AUTHREQ;
	} else
		error = ECANCELED;
exit:
	if (choices)
		free(choices);
	if (share_info)
		free(share_info);
exit2:
	if (connected) {
		connected = smb_ctx_tdis(ctx);
		if (connected)	/* unable to clean up?! */
			exit(connected);
	}
	if (sortbuf)
		free(sortbuf);
	return (error);
}


int
smb_browse(struct smb_ctx *ctx, int anon)
{  
	struct smbioc_ossn *ssn = &ctx->ct_ssn;
	char saveduser[SMB_MAXUSERNAMELEN+1];
	int error = 0, resolved;

	if (anon) {
		strncpy(saveduser, ssn->ioc_user, SMB_MAXUSERNAMELEN);
		ssn->ioc_user[0] = '\0'; /* force anon browsing */
	}

	resolved = (ctx->ct_flags & SMBCF_RESOLVED);
	ctx->ct_flags |= SMBCF_RESOLVED; /* fool smb_ctx_lookup */

	error = smb_browse_int(ctx, anon);

	if (!resolved)
		ctx->ct_flags &= ~SMBCF_RESOLVED;

	if (anon)
		strncpy(ssn->ioc_user, saveduser, SMB_MAXUSERNAMELEN);
	return (error);
}
