/*
 *  si-30-keychain-upgrade.c
 *  Security
 *
 *  Created by Michael Brouwer on 4/29/08.
 *  Copyright (c) 2008,2010 Apple Inc.. All Rights Reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecInternal.h>
#include <utilities/SecCFWrappers.h>

#include <stdlib.h>
#include <sys/stat.h>
#include <sqlite3.h>

#include "Security_regressions.h"
#include "SecItemServer.h"

/* TODO: This test needs to be updated. It was originally created to test upgrades from DB prior to the introduction of versionning, circa 2008.
   We don't support upgrading from that old of keychain, but this test should be upgraded to test upgrades from v5 to v6 keychain, or more current
*/

const char *create_db_sql =
"BEGIN TRANSACTION;"
"CREATE TABLE genp(cdat REAL,mdat REAL,desc BLOB,icmt BLOB,crtr INTEGER,type INTEGER,scrp INTEGER,labl BLOB,alis BLOB,invi INTEGER,nega INTEGER,cusi INTEGER,prot BLOB,acct BLOB NOT NULL DEFAULT '',svce BLOB NOT NULL DEFAULT '',gena BLOB,data BLOB,PRIMARY KEY(acct,svce));"
"INSERT INTO \"genp\" VALUES(NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'4087574952','EnhancedVoicemail',NULL,X'34F32095A0ED6F32637629114439CE38E6FF39ADB591E761D20ED23F9FACF639258DA4F12454FD4D0189C0D39AAA9227');"
"INSERT INTO \"genp\" VALUES(NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'TlalocNet','AirPort',NULL,X'52E24441994D93D18F344DDF6A7F1F6EC43A63BCEB5F89B02FEBEEAAE108BB4933EAE73A0FB615F693C70BCFBCF034BE74BDF0280ECBEB357EEFA3B7EF03060B');"
"INSERT INTO \"genp\" VALUES(NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'weasels','AirPort',NULL,X'3FAD49851913382FBC92C9EB90D90D82A74B1DABB5F726648898B2FA2FBA405AA0B9D95D9837BBFF0F9B7C29954973249AA066F9F8AA68D79552970C687A7DA6');"
"CREATE TABLE inet(cdat REAL,mdat REAL,desc BLOB,icmt BLOB,crtr INTEGER,type INTEGER,scrp INTEGER,labl BLOB,alis BLOB,invi INTEGER,nega INTEGER,cusi INTEGER,prot BLOB,acct BLOB NOT NULL DEFAULT '',sdmn BLOB NOT NULL DEFAULT '',srvr BLOB NOT NULL DEFAULT '',ptcl INTEGER NOT NULL DEFAULT 0,atyp BLOB NOT NULL DEFAULT '',port INTEGER NOT NULL DEFAULT 0,path BLOB NOT NULL DEFAULT '',data BLOB,PRIMARY KEY(acct,sdmn,srvr,ptcl,atyp,port,path));"
"INSERT INTO \"inet\" VALUES(NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'mb.7766@gmail.com','','imap.gmail.com','imap','',143,'',X'0029D7AFBF0000E0E386C8654070569B2DF1D7DC2D641AA29223297EC9E8AD86ED91CA6DEE3D2DA0FABD8F05DE5A7AD4CC46B134A211472B6DE50595EACAC149');"
"INSERT INTO \"inet\" VALUES(NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'brouwer','','phonehome.apple.com','imap','',143,'',X'BB373BAE840427C5E1247540ADA559AB14DF3788906B786498A8E1CFF4B4C596634E4A4C7F9C55EA1B646163AFCDADA8');"
"INSERT INTO \"inet\" VALUES(NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'mb.7766@gmail.com','','smtp.gmail.com','smtp','',25,'',X'042C08A4AECD3957822F531A602734F07B89DABA3BA6629ECEFE10E264C12635F83EFBB1707C6B39FB20CCE0200D8997B690FBB0B92911BFE9B2D1E05B1CD5F5');"
"INSERT INTO \"inet\" VALUES(NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'brouwer','','phonehome.apple.com','smtp','',25,'',X'25B0775265ADC808B8AFB2F2602C44B13F5ECC1F04B1D5E6EAE1B803446F3A817CCF8401416FE673CE366E25FACF5C55');"
"CREATE TABLE cert(ctyp INTEGER NOT NULL DEFAULT 0,cenc INTEGER,labl BLOB,alis BLOB,subj BLOB,issr BLOB NOT NULL DEFAULT '',slnr BLOB NOT NULL DEFAULT '',skid BLOB,pkhh BLOB,data BLOB,PRIMARY KEY(ctyp,issr,slnr));"
"CREATE TABLE keys(kcls INTEGER NOT NULL DEFAULT 0,labl BLOB,alis BLOB,perm INTEGER,priv INTEGER,modi INTEGER,klbl BLOB NOT NULL DEFAULT '',atag BLOB NOT NULL DEFAULT '',crtr INTEGER NOT NULL DEFAULT 0,type INTEGER NOT NULL DEFAULT 0,bsiz INTEGER NOT NULL DEFAULT 0,esiz INTEGER NOT NULL DEFAULT 0,sdat REAL NOT NULL DEFAULT 0,edat REAL NOT NULL DEFAULT 0,sens INTEGER,asen INTEGER,extr INTEGER,next INTEGER,encr INTEGER,decr INTEGER,drve INTEGER,sign INTEGER,vrfy INTEGER,snrc INTEGER,vyrc INTEGER,wrap INTEGER,unwp INTEGER,data BLOB,PRIMARY KEY(kcls,klbl,atag,crtr,type,bsiz,esiz,sdat,edat));"
"CREATE INDEX ialis ON cert(alis);"
"CREATE INDEX isubj ON cert(subj);"
"CREATE INDEX iskid ON cert(skid);"
"CREATE INDEX ipkhh ON cert(pkhh);"
"CREATE INDEX ikcls ON keys(kcls);"
"CREATE INDEX iklbl ON keys(klbl);"
"CREATE INDEX iencr ON keys(encr);"
"CREATE INDEX idecr ON keys(decr);"
"CREATE INDEX idrve ON keys(drve);"
"CREATE INDEX isign ON keys(sign);"
"CREATE INDEX ivrfy ON keys(vrfy);"
"CREATE INDEX iwrap ON keys(wrap);"
"CREATE INDEX iunwp ON keys(unwp);"
"COMMIT;";

void kc_dbhandle_reset(void);

#ifdef NO_SERVER
static void ensureKeychainExists(void) {
    CFDictionaryRef query = CFDictionaryCreateForCFTypes(0, kSecClass,kSecClassInternetPassword, NULL);
    CFTypeRef results = NULL;
    is_status(SecItemCopyMatching(query, &results), errSecItemNotFound, "expected nothing got %@", results);
    CFReleaseNull(query);
    CFReleaseNull(results);
}
#endif

/* Test basic add delete update copy matching stuff. */
static void tests(void)
{
#ifndef NO_SERVER
    plan_skip_all("No testing against server.");
#else
    char *keychain_name;

    ensureKeychainExists();

    CFStringRef dbPath = __SecKeychainCopyPath();
    keychain_name = CFStringToCString(dbPath);
    CFRelease(dbPath);

    /* delete the keychain file, and let sqllite recreate it */
    ok_unix(unlink(keychain_name), "delete keychain file");

    sqlite3 *db;
    is(sqlite3_open(keychain_name, &db), SQLITE_OK, "create keychain");
    is(sqlite3_exec(db, create_db_sql, NULL, NULL, NULL), SQLITE_OK,
        "populate keychain");
    free(keychain_name);

    kc_dbhandle_reset();

    int v_eighty = 80;
    CFNumberRef eighty = CFNumberCreate(NULL, kCFNumberSInt32Type, &v_eighty);
    const char *v_data = "test";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(query, kSecClass, kSecClassInternetPassword);
    CFDictionaryAddValue(query, kSecAttrServer, CFSTR("members.spamcop.net"));
    CFDictionaryAddValue(query, kSecAttrAccount, CFSTR("smith"));
    CFDictionaryAddValue(query, kSecAttrPort, eighty);
    CFDictionaryAddValue(query, kSecAttrProtocol, kSecAttrProtocolHTTP);
    CFDictionaryAddValue(query, kSecAttrAuthenticationType, kSecAttrAuthenticationTypeDefault);
    CFDictionaryAddValue(query, kSecValueData, pwdata);
    ok_status(SecItemAdd(query, NULL), "add internet password");
    is_status(SecItemAdd(query, NULL), errSecDuplicateItem,
	"add internet password again");

    ok_status(SecItemCopyMatching(query, NULL), "Found the item we added");

    ok_status(SecItemDelete(query), "Deleted the item we added");

    CFReleaseSafe(eighty);
    CFReleaseSafe(pwdata);
    CFReleaseSafe(query);
#endif
}

int si_30_keychain_upgrade(int argc, char *const *argv)
{
	plan_tests(8);

	tests();

	return 0;
}
