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
#include <Security/SecKeychainAPI.h>

/* hide keychain redundancy and complexity */
#define	 kc_add		SecKeychainAddInternetPassword
#define	 kc_find	SecKeychainFindInternetPassword
#define	 kc_replace	SecKeychainItemModifyContent
#define	 kc_status	SecKeychainGetStatus
#define	 kc_get		SecKeychainCopyDefault
#define	 kc_release	SecKeychainRelease

#if 0
#include <CarbonCore/MacErrors.h>
#endif

#include <sys/mchain.h>
#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rap.h>

/*
 * Note that if any of these strings are changed, the corresponding
 * key in Localizable.strings has to be changed as well.
 */
#define SMB_HEADER_KEY "SMB/CIFS Filesystem Authentication"
#define SMB_MSG_KEY "Enter username and password for "
#define SMB_MSG_TRAILING_COLON ":"
#define SMB_DOMAINNAME_KEY "Workgroup/Domain"
#define SMB_USERNAME_KEY "Username"
#define SMB_PASSWORD_KEY "Password"
#define SMB_KEYCHAIN_KEY "Add to Keychain"
#define SMB_OK_KEY "OK"
#define SMB_CANCEL_KEY "Cancel"
#define SMB_AUTH_KEY "Authenticate"
#define SMB_REAUTH_KEY "Re-authenticate"
#define SMB_SELECT_KEY "Select a share"
#define SMB_MOUNT_KEY "SMB Mount"

#define SMB_LOCALIZATION_BUNDLE "/System/Library/CoreServices/smb.bundle"

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

	if (ctx->ct_flags & SMBCF_KCFOUND) {
		ctx->ct_flags &= ~SMBCF_KCFOUND;
	} else {
		ctx->ct_flags &= ~SMBCF_KCFOUND;
		if (!strcmp(sharename, "IPC$"))
			sharename = "";
kcagain:
		kcerr = kc_find(NULL, strlen(systemname), systemname,
				strlen(wrkgrp), wrkgrp,
				strlen(usrname), usrname,
				strlen(sharename), sharename, SMB_TCP_PORT,
				'smb ', kSecAuthenticationTypeNTLM,
				&kcpasswdlen, (void **)&kcpasswd, NULL);
		if (!kcerr) {
			if (kcpasswdlen >= passwdlen) {
				smb_error("bogus password in keychain, len=%d",
					  0, kcpasswdlen);
			} else {
				ctx->ct_flags |= SMBCF_KCFOUND;
				memcpy(passwd, kcpasswd, kcpasswdlen);
				passwd[kcpasswdlen] = '\0';
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
	dialogue[i++] = kUNCAlertHeaderKey;
	dialogue[i++] = SMB_HEADER_KEY;
	dialogue[i++] = kUNCAlertMessageKey;
	dialogue[i++] = SMB_MSG_KEY;    
	dialogue[i++] = kUNCAlertMessageKey;
	dialogue[i++] = systemname;
	dialogue[i++] = kUNCAlertMessageKey;
	dialogue[i++] = SMB_MSG_TRAILING_COLON;
	if (wrkgrp) {
		dialogue[i++] = kUNCTextFieldTitlesKey;
		dialogue[i++] = SMB_DOMAINNAME_KEY;
		domainindex = 0;
	}
	if (usrname) {
		dialogue[i++] = kUNCTextFieldTitlesKey;
		dialogue[i++] = SMB_USERNAME_KEY;
		nameindex = domainindex + 1;
	}
	if (passwd) {
		dialogue[i++] = kUNCTextFieldTitlesKey;
		dialogue[i++] = SMB_PASSWORD_KEY;
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
		dialogue[i++] = SMB_KEYCHAIN_KEY;
	}
	dialogue[i++] = kUNCDefaultButtonTitleKey;
	dialogue[i++] = SMB_OK_KEY;
	dialogue[i++] = kUNCAlternateButtonTitleKey;
	dialogue[i++] = SMB_CANCEL_KEY;
	dialogue[i++] = 0;

	UNCref = UNCUserNotificationCreate(SMB_AUTH_TIMEOUT,
					   UNCSecureTextField(passindex),
					   &error, dialogue);
	if (error)
		return (error);
	error = UNCUserNotificationReceiveResponse(UNCref, SMB_AUTH_TIMEOUT,
						   &response);
	if (error)
		return (error); /* probably MACH_RCV_TIMED_OUT */
	if ((response & 0x3) == kUNCAlternateResponse) {
		error = ECANCELED;
	} else {	/* fill in domain, username, and password */
		if (wrkgrp)
			smb_unc_val(UNCref, domainindex, wrkgrp, wrkgrplen);
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

	return (error);
}


static int
smb_browse_int(struct smb_ctx *ctx, int anon)
{  
	struct smb_share_info_1 *rpbuf = NULL, *ep;
	int error, bufsize, entries, total, ch;
	char **choices = NULL;

	CFUserNotificationRef un;
	CFMutableDictionaryRef d;
	CFMutableArrayRef shares = NULL;
	CFOptionFlags respflags;
	SInt32 i;
	CFURLRef localizationURL;

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

	bufsize = 0xffe0;	/* samba states win2k bug for 65535 */
	rpbuf = malloc(bufsize); /* XXX handle malloc failure */

	error = smb_rap_NetShareEnum(ctx, 1, rpbuf, bufsize, &entries, &total);
	(void)smb_ctx_setshare(ctx, "", SMB_ST_ANY); /* all done with IPC$ */
	if (error &&
	    error != (SMB_ERROR_MORE_DATA | SMB_RAP_ERROR)) {
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

	choices = malloc((entries+2) * sizeof(char *)); /* XXX handle failure */
	for (ep = rpbuf, i = 0, ch = 0; i < entries; i++, ep++) {
		CFStringRef s;
		u_int16_t type = letohs(ep->shi1_type);
		int nlen;

		if (type != 0) /* we want "disk" type only */
			continue;
		ep->shi1_pad = '\0'; /* ensure null termination */
		nlen = strlen(ep->shi1_netname);
		if (nlen == 0  || ep->shi1_netname[nlen - 1] == '$')
			continue;	/* hide administrative shares */
		if (!(ctx->ct_flags & SMBCF_XXX)) {
			s = CFStringCreateWithCString(NULL, ep->shi1_netname,
						   kCFStringEncodingDOSLatinUS);
			if (s == NULL) {
				smb_error("%s failed on \"%s\"", -1,
					  "CFStringCreateWithCString",
					  ep->shi1_netname);
				continue;
			}
			CFArrayAppendValue(shares, s);
			CFRelease(s);
		}
		choices[ch++] = ep->shi1_netname;
	}
	if (ctx->ct_flags & SMBCF_XXX) {
		if (ctx->ct_maxxxx && ch > ctx->ct_maxxxx)
			ch = 0; /* mount none if there are too many */
		choices[ch] = NULL;
		choices[ch+1] = (char *)rpbuf;
		ctx->ct_xxx = choices;
		return (0); /* free neither choices nor rpbuf */
	}
	if (ch == 0) {
		/* XXX probably should be quiet */
		smb_error("no shares found", 0);
		goto exit;
	}

	localizationURL = CFURLCreateFromFileSystemRepresentation(NULL, SMB_LOCALIZATION_BUNDLE, strlen(SMB_LOCALIZATION_BUNDLE), true);
	CFDictionaryAddValue(d, kCFUserNotificationLocalizationURLKey,
			     localizationURL);
        CFRelease(localizationURL);
	CFDictionaryAddValue(d, kCFUserNotificationAlertHeaderKey,
			     CFSTR(SMB_MOUNT_KEY));
	CFDictionaryAddValue(d, kCFUserNotificationAlertMessageKey,
			     CFSTR(SMB_SELECT_KEY));
        CFDictionaryAddValue(d, kCFUserNotificationPopUpTitlesKey, shares);
        CFRelease(shares);

        CFDictionaryAddValue(d, kCFUserNotificationDefaultButtonTitleKey,
			     CFSTR(SMB_OK_KEY));
        CFDictionaryAddValue(d, kCFUserNotificationAlternateButtonTitleKey,
			     CFSTR(SMB_CANCEL_KEY));
	if (anon) {
        	CFDictionaryAddValue(d, kCFUserNotificationOtherButtonTitleKey,
			     CFSTR(SMB_AUTH_KEY));
	} else {
        	CFDictionaryAddValue(d, kCFUserNotificationOtherButtonTitleKey,
			     CFSTR(SMB_REAUTH_KEY));
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
		ch = respflags >> 24;
		error = smb_ctx_setshare(ctx, choices[ch], SMB_ST_DISK);
	} else if ((respflags & 0x3) == kCFUserNotificationOtherResponse) {
		ctx->ct_flags |= SMBCF_AUTHREQ;
	} else
		error = ECANCELED;
exit:
	if (choices)
		free(choices);
	if (rpbuf)
		free(rpbuf);
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
