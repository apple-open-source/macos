#include <Kerberos/Kerberos.h>
#include <Carbon/Carbon.h>
#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>

/* Prototypes */
void Initialize(void);
void TestErrorHandling (void);
void TestHighLevelAPI (void);
void TestKLPrincipal (void);
void TestKerberosRealms (void);
void TestLoginOptions (void);
char* TimeToString (char* timeString, long time);
void TestApplicationOptions (void);
void MyKerberosLoginIdleCallback (
        KLRefCon 			inAppData);

void Initialize(void)
{
    Handle menuBar;
    menuBar = GetNewMBar (128);
    SetMenuBar (menuBar);
    InsertMenu (GetMenu (128), 0);
    InsertMenu (GetMenu (129), 0);
    InsertMenu (GetMenu (130), 0);
    AppendResMenu (GetMenuHandle (128), 'DRVR');
    DrawMenuBar ();
}


int main(void)
{
    KLTime time;
    KLStatus err;

    Initialize();

    // make sure that if we are debugging that we do not get terminal login
    fclose (stdin);

    err = KLLastChangedTime(&time);
    printf ("KLLastChangedTime returned %d (err = %d)\n", time, err);

    //TestKerberosRealms ();
    //TestKLPrincipal ();
    //TestLoginOptions ();
    TestApplicationOptions ();
    //TestErrorHandling ();
    TestHighLevelAPI ();

    err = KLLastChangedTime(&time);
    printf ("KLLastChangedTime returned %d (err = %d)\n", time, err);

    return 0;	
}

void TestErrorHandling (void)
{
    OSStatus err;
    char*	errorString;

    err = KLGetErrorString (KDC_PR_UNKNOWN, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       

    err = KLGetErrorString (klCredentialsBadAddressErr, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       

    err = KLGetErrorString (klCacheDoesNotExistErr, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       
        
    err = KLGetErrorString (klPasswordMismatchErr, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       

    err = KLGetErrorString (klInsecurePasswordErr, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       

    err = KLGetErrorString (klPasswordChangeFailedErr, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       

    err = KLGetErrorString (klDialogDoesNotExistErr, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       

    err = KLGetErrorString (klDialogAlreadyExistsErr, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       

    err = KLGetErrorString (klNotInForegroundErr, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       

    err = KLGetErrorString (klNoAppearanceErr, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       

    err = KLGetErrorString (klFatalDialogErr, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       

    err = KLGetErrorString (klCarbonUnavailableErr, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       

    err = KLGetErrorString (klCantContactServerErr, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       

    err = KLGetErrorString (klCantDisplayUIErr, &errorString);
 	printf ("KLGetErrorString() returned %s (err = %ld)\n", errorString, err);       
}

void TestHighLevelAPI (void)
{
    KLStatus err;
    KLPrincipal	inPrincipal, outPrincipal, outPrincipal2;
    char *outCredCacheName, *outCredCacheName2;
    KLTime	expirationTime;
    char*	principalString;
    char	timeString[256];
    Boolean	valid;

    err = KLCreatePrincipalFromTriplet ("systest", "", "ATHENA.MIT.EDU", &inPrincipal);
    printf ("KLCreatePrincipalFromTriplet(systest@ATHENA.MIT.EDU) (err = %d)\n", err);
    if (err == klNoErr) {
        err = KLAcquireNewInitialTicketsWithPassword (inPrincipal, NULL, "!mwm_user", &outCredCacheName);
        if (err != klNoErr) {
            printf ("KLAcquireNewInitialTicketsWithPassword() returned err = %d\n", err);
        } else {
            printf ("KLAcquireNewInitialTicketsWithPassword() returned '%s'\n", outCredCacheName);
            KLDisposeString (outCredCacheName);
        }
        KLDisposePrincipal (inPrincipal);
    }

    err = KLAcquireNewInitialTickets (NULL, NULL, &inPrincipal, &outCredCacheName);
    printf ("KLAcquireNewInitialTickets() (err = %d)\n", err);
    if (err == klNoErr) {
        KLDisposeString (outCredCacheName);
        err = KLAcquireInitialTickets (inPrincipal, NULL, &outPrincipal, &outCredCacheName);
        printf ("KLAcquireInitialTickets() (err = %d)\n", err);
        if (err == klNoErr) {
            KLDisposeString (outCredCacheName);
            KLDisposePrincipal (outPrincipal);
        }
        KLDisposePrincipal (inPrincipal);
    }

    err = KLSetDefaultLoginOption (loginOption_LoginName, "lxs", 3);
    printf ("KLSetDefaultLoginOption(loginOption_LoginName) to lxs (err = %d)\n", err);
    if (err == klNoErr) {
        err = KLSetDefaultLoginOption (loginOption_LoginInstance, "dialup", 6);
        printf ("KLSetDefaultLoginOption(loginOption_LoginInstance) to dialup (err = %d)\n", err);
    }

    err = KLAcquireNewInitialTickets (NULL, NULL, &inPrincipal, &outCredCacheName);
    printf ("KLAcquireNewInitialTickets() (err = %d)\n", err);
    if (err == klNoErr) {
        KLDisposeString (outCredCacheName);
        KLDisposePrincipal (inPrincipal);
    }

    // Principal == nil
    while (KLAcquireNewInitialTickets (NULL, NULL, &outPrincipal, &outCredCacheName) == klNoErr) {
        err = KLTicketExpirationTime (outPrincipal, kerberosVersion_All, &expirationTime);
        err = KLCacheHasValidTickets (outPrincipal, kerberosVersion_All, &valid, &outPrincipal2, &outCredCacheName2);
        if (err == noErr) {
            err = KLGetStringFromPrincipal (outPrincipal2, kerberosVersion_V4, &principalString);
            if (err == klNoErr) {
                printf ("KLGetStringFromPrincipal returned string '%s'\n", principalString);
                KLDisposeString (principalString);
            }
            KLDisposePrincipal (outPrincipal2);
            KLDisposeString (outCredCacheName2);
            err = KLCacheHasValidTickets (outPrincipal, kerberosVersion_All, &valid, nil, nil);
            if (err != noErr) {
                printf ("KLCacheHasValidTickets returned error = %d\n", err);
            }
        }
        err = KLCacheHasValidTickets (outPrincipal, kerberosVersion_All, &valid, nil, nil);
        KLDisposeString (outCredCacheName);
        KLDisposePrincipal (outPrincipal);
    }

    err = KLAcquireNewInitialTickets (NULL, NULL, &outPrincipal, &outCredCacheName);
    if (err == klNoErr) {
        KLDisposeString (outCredCacheName);
        KLDisposePrincipal (outPrincipal);
    }
	

    err = KLCreatePrincipalFromTriplet ("systest", "", "ATHENA.MIT.EDU", &inPrincipal);
    printf ("KLCreatePrincipalFromTriplet(systest@ATHENA.MIT.EDU) (err = %d)\n", err);
    if (err == klNoErr) {
        err = KLAcquireNewInitialTickets (inPrincipal, NULL, &outPrincipal, &outCredCacheName);
        printf ("KLAcquireNewInitialTickets(systest@ATHENA.MIT.EDU) (err = %d)\n", err);
        if (err == klNoErr) {
            KLDisposeString (outCredCacheName);
            KLDisposePrincipal (outPrincipal);
        }
        err = KLDestroyTickets (inPrincipal);

        KLDisposePrincipal (inPrincipal);
    }

    err = KLCreatePrincipalFromTriplet ("lxs", "", "ATHENA.MIT.EDU", &inPrincipal);
    printf ("KLCreatePrincipalFromTriplet(lxs@ATHENA.MIT.EDU) (err = %d)\n", err);
    if (err == klNoErr) {
        err = KLAcquireInitialTickets (inPrincipal, NULL, &outPrincipal, &outCredCacheName);
        printf ("KLAcquireInitialTickets(lxs@ATHENA.MIT.EDU) (err = %d)\n", err);
        if (err == klNoErr) {
            KLDisposeString (outCredCacheName);
            KLDisposePrincipal (outPrincipal);
        }

        err = KLAcquireNewInitialTickets (inPrincipal, NULL, &outPrincipal, &outCredCacheName);
        if (err == klNoErr) {
            err = KLGetStringFromPrincipal (outPrincipal, kerberosVersion_V5, &principalString);
            if (err == klNoErr) {
                err = KLTicketExpirationTime (outPrincipal, kerberosVersion_All, &expirationTime);
                printf ("Tickets for principal '%s' expire on %s\n",
                        principalString, TimeToString(timeString, expirationTime));

                KLDisposeString (principalString);
            }
            KLDisposeString (outCredCacheName);
            KLDisposePrincipal (outPrincipal);
        }

        err = KLChangePassword (inPrincipal);
        printf ("KLChangePassword() (err = %d)\n", err);

        err = KLDestroyTickets (inPrincipal);
        printf ("KLDestroyTickets() (err = %d)\n", err);

        KLDisposePrincipal (inPrincipal);
    }

}


void TestKLPrincipal (void)
{
    KLStatus err = klNoErr;
    KLPrincipal extraLongPrincipal = NULL;
    KLPrincipal	lxsPrincipal = NULL;
    KLPrincipal lxsDialupPrincipal = NULL;
    KLPrincipal lxsRootPrincipalV4 = NULL;
    KLPrincipal lxsRootPrincipalV5 = NULL;
    char *principalString = NULL;
    char *user = NULL;
    char *instance = NULL;
    char *realm = NULL;

    printf ("Entering TestKLPrincipal()\n");
    printf ("----------------------------------------------------------------\n");

    err = KLCreatePrincipalFromString ("thisprincipalnameislongerthanissupportedbyKerberos4@ATHENA.MIT.EDU",
                                       kerberosVersion_V5, &extraLongPrincipal);
    printf ("KLCreatePrincipalFromString "
            "('thisprincipalnameislongerthanissupportedbyKerberos4@ATHENA.MIT.EDU') "
            "(err = %s)\n", error_message(err));

    printf ("----------------------------------------------------------------\n");

    err = KLCreatePrincipalFromTriplet ("lxs", "", "ATHENA.MIT.EDU", &lxsPrincipal);
    printf ("KLCreatePrincipalFromTriplet ('lxs' '' 'ATHENA.MIT.EDU') (err = %s)\n",
            error_message(err));

    if (err == klNoErr) {
        err = KLGetStringFromPrincipal (lxsPrincipal, kerberosVersion_V5, &principalString);
        if (err == klNoErr) {
            printf ("KLGetStringFromPrincipal (lxs@ATHENA.MIT.EDU, v5) returned string '%s'\n", principalString);
            KLDisposeString (principalString);
        } else {
            printf ("KLGetStringFromPrincipal(lxs@ATHENA.MIT.EDU, v5) returned (err = %s)\n", error_message(err));
        }

        err = KLGetStringFromPrincipal (lxsPrincipal, kerberosVersion_V4, &principalString);
        if (err == klNoErr) {
            printf ("KLGetStringFromPrincipal (lxs@ATHENA.MIT.EDU, v4) returned string '%s'\n", principalString);
            KLDisposeString (principalString);
        } else {
            printf ("KLGetStringFromPrincipal(lxs@ATHENA.MIT.EDU, v4) returned (err = %s)\n", error_message(err));
        }
        
        err = KLGetTripletFromPrincipal (lxsPrincipal, &user, &instance, &realm);
        if (err == klNoErr) {
            printf ("KLGetTripletFromPrincipal (lxs@ATHENA.MIT.EDU) returned triplet %s' '%s' '%s'\n",
                    user, instance, realm);
            KLDisposeString (user);
            KLDisposeString (instance);
            KLDisposeString (realm);
        } else {
            printf ("KLGetTripletFromPrincipal(lxs@ATHENA.MIT.EDU) returned (err = %s)\n", error_message(err));
        }            
    }

    printf ("----------------------------------------------------------------\n");

    err = KLCreatePrincipalFromTriplet ("lxs", "dialup", "ATHENA.MIT.EDU", &lxsDialupPrincipal);
    printf ("KLCreatePrincipalFromTriplet ('lxs' 'dialup' 'ATHENA.MIT.EDU') (err = %d)\n", err);

    if (err == klNoErr) {
        err = KLGetStringFromPrincipal (lxsDialupPrincipal, kerberosVersion_V5, &principalString);
        if (err == klNoErr) {
            printf ("KLGetStringFromPrincipal (lxs/dialup@ATHENA.MIT.EDU, v5) returned string '%s'\n", principalString);
            KLDisposeString (principalString);
        } else {
            printf ("KLGetStringFromPrincipal(lxs/dialup@ATHENA.MIT.EDU, v5) returned (err = %d)\n", err);
        }

        err = KLGetStringFromPrincipal (lxsDialupPrincipal, kerberosVersion_V4, &principalString);
        if (err == klNoErr) {
            printf ("KLGetStringFromPrincipal (lxs/dialup@ATHENA.MIT.EDU, v4) returned string '%s'\n", principalString);
            KLDisposeString (principalString);
        } else {
            printf ("KLGetStringFromPrincipal(lxs/dialup@ATHENA.MIT.EDU, v4) returned (err = %d)\n", err);
        }

        err = KLGetTripletFromPrincipal (lxsDialupPrincipal, &user, &instance, &realm);
        if (err == klNoErr) {
            printf ("KLGetTripletFromPrincipal (lxs/dialup@ATHENA.MIT.EDU) returned triplet %s' '%s' '%s'\n",
                    user, instance, realm);
            KLDisposeString (user);
            KLDisposeString (instance);
            KLDisposeString (realm);
        } else {
            printf ("KLGetTripletFromPrincipal(lxs/dialup@ATHENA.MIT.EDU) returned (err = %d)\n", err);
        }
    }

    printf ("----------------------------------------------------------------\n");

    err = KLCreatePrincipalFromString ("lxs/root@ATHENA.MIT.EDU", kerberosVersion_V5, &lxsRootPrincipalV5);
    printf ("KLCreatePrincipalFromString ('lxs/root@ATHENA.MIT.EDU', v5) (err = %d)\n", err);
    if (err == klNoErr) {
        err = KLGetStringFromPrincipal (lxsRootPrincipalV5, kerberosVersion_V5, &principalString);
        if (err == klNoErr) {
            printf ("KLGetStringFromPrincipal (lxs/root@ATHENA.MIT.EDU, v5) returned string '%s'\n", principalString);
            KLDisposeString (principalString);
        } else {
            printf ("KLGetStringFromPrincipal(lxs/root@ATHENA.MIT.EDU, v5) returned (err = %d)\n", err);
        }

        err = KLGetStringFromPrincipal (lxsRootPrincipalV5, kerberosVersion_V4, &principalString);
        if (err == klNoErr) {
            printf ("KLGetStringFromPrincipal (lxs/root@ATHENA.MIT.EDU, v4) returned string '%s'\n", principalString);
            KLDisposeString (principalString);
        } else {
            printf ("KLGetStringFromPrincipal(lxs/root@ATHENA.MIT.EDU, v4) returned (err = %d)\n", err);
        }

        err = KLGetTripletFromPrincipal (lxsRootPrincipalV5, &user, &instance, &realm);
        if (err == klNoErr) {
            printf ("KLGetTripletFromPrincipal (lxs/root@ATHENA.MIT.EDU) returned triplet %s' '%s' '%s'\n",
                    user, instance, realm);
            KLDisposeString (user);
            KLDisposeString (instance);
            KLDisposeString (realm);
        } else {
            printf ("KLGetTripletFromPrincipal(lxs/root@ATHENA.MIT.EDU) returned (err = %d)\n", err);
        }
    }

    printf ("----------------------------------------------------------------\n");

    err = KLCreatePrincipalFromString ("lxs.root@ATHENA.MIT.EDU", kerberosVersion_V4, &lxsRootPrincipalV4);
    printf ("KLCreatePrincipalFromString ('lxs.root@ATHENA.MIT.EDU') (err = %d)\n", err);
    if (err == klNoErr) {
        err = KLGetStringFromPrincipal (lxsRootPrincipalV4, kerberosVersion_V5, &principalString);
        if (err == klNoErr) {
            printf ("KLGetStringFromPrincipal (lxs.root@ATHENA.MIT.EDU, v5) returned string '%s'\n", principalString);
            KLDisposeString (principalString);
        } else {
            printf ("KLGetStringFromPrincipal(lxs.root@ATHENA.MIT.EDU, v5) returned (err = %d)\n", err);
        }

        err = KLGetStringFromPrincipal (lxsRootPrincipalV4, kerberosVersion_V4, &principalString);
        if (err == klNoErr) {
            printf ("KLGetStringFromPrincipal (lxs.root@ATHENA.MIT.EDU, v4) returned string '%s'\n", principalString);
            KLDisposeString (principalString);
        } else {
            printf ("KLGetStringFromPrincipal(lxs.root@ATHENA.MIT.EDU, v4) returned (err = %d)\n", err);
        }
        
        err = KLGetTripletFromPrincipal (lxsRootPrincipalV4, &user, &instance, &realm);
        if (err == klNoErr) {
            printf ("KLGetTripletFromPrincipal (lxs.root@ATHENA.MIT.EDU) returned triplet %s' '%s' '%s'\n",
                    user, instance, realm);
            KLDisposeString (user);
            KLDisposeString (instance);
            KLDisposeString (realm);
        } else {
            printf ("KLGetTripletFromPrincipal(lxs.root@ATHENA.MIT.EDU) returned (err = %d)\n", err);
        }
    }

    printf ("----------------------------------------------------------------\n");

    if (lxsRootPrincipalV4 != NULL && lxsRootPrincipalV5 != NULL) {
        Boolean equivalent;

        err = KLComparePrincipal (lxsRootPrincipalV5, lxsRootPrincipalV4, &equivalent);
        if (err == klNoErr) {
            printf ("KLComparePrincipal %s comparing lxs/root@ATHENA.MIT.EDU and lxs.root@ATHENA.MIT.EDU\n",
                    equivalent ? "passed" : "FAILED");
        } else {
            printf ("KLComparePrincipal returned (err = %d)\n", err);
        }
    }
    
    if (lxsPrincipal != NULL && lxsRootPrincipalV5 != NULL) {
        Boolean equivalent;

        err = KLComparePrincipal (lxsPrincipal, lxsRootPrincipalV4, &equivalent);
        if (err == klNoErr) {
            printf ("KLComparePrincipal %s comparing lxs@ATHENA.MIT.EDU and lxs.root@ATHENA.MIT.EDU\n",
                    equivalent ? "FAILED" : "passed");
        } else {
            printf ("KLComparePrincipal returned (err = %d)\n", err);
        }
    }

    if (lxsPrincipal != NULL && lxsRootPrincipalV5 != NULL) {
        Boolean equivalent;

        err = KLComparePrincipal (lxsPrincipal, lxsRootPrincipalV5, &equivalent);
        if (err == klNoErr) {
            printf ("KLComparePrincipal %s comparing lxs@ATHENA.MIT.EDU and lxs/root@ATHENA.MIT.EDU\n",
                    equivalent ? "FAILED" : "passed");
        } else {
            printf ("KLComparePrincipal returned (err = %d)\n", err);
        }
    }

    if (lxsDialupPrincipal != NULL && lxsRootPrincipalV5 != NULL) {
        Boolean equivalent;

        err = KLComparePrincipal (lxsDialupPrincipal, lxsRootPrincipalV5, &equivalent);
        if (err == klNoErr) {
            printf ("KLComparePrincipal %s comparing lxs/dialup@ATHENA.MIT.EDU and lxs/root@ATHENA.MIT.EDU\n",
                    equivalent ? "FAILED" : "passed");
        } else {
            printf ("KLComparePrincipal returned (err = %d)\n", err);
        }
    }

    printf ("----------------------------------------------------------------\n\n");

    if (extraLongPrincipal != NULL) KLDisposePrincipal (extraLongPrincipal);
    if (lxsRootPrincipalV5 != NULL) KLDisposePrincipal (lxsRootPrincipalV5);
    if (lxsRootPrincipalV4 != NULL) KLDisposePrincipal (lxsRootPrincipalV4);
    if (lxsDialupPrincipal != NULL) KLDisposePrincipal (lxsDialupPrincipal);
    if (lxsPrincipal       != NULL) KLDisposePrincipal (lxsPrincipal);
}


void TestApplicationOptions (void)
{
    KLSetIdleCallback (MyKerberosLoginIdleCallback, 101);
}

void TestKerberosRealms (void)
{
    printf ("About to test Kerberos realms\n");
    KLRemoveAllKerberosRealms ();
    KLAcquireNewInitialTickets (NULL, NULL, NULL, NULL);

    KLInsertKerberosRealm (realmList_End, "FOO");
    KLInsertKerberosRealm (realmList_End, "BAR");
    KLInsertKerberosRealm (realmList_End, "BAZ");
    KLAcquireNewInitialTickets (NULL, NULL, NULL, NULL);

    KLInsertKerberosRealm (realmList_End, "FOO");
    KLAcquireNewInitialTickets (NULL, NULL, NULL, NULL);
    
    KLSetKerberosRealm (0, "QUUX");
    KLAcquireNewInitialTickets (NULL, NULL, NULL, NULL);

    KLRemoveKerberosRealm (0);
    KLAcquireNewInitialTickets (NULL, NULL, NULL, NULL);
    
    KLSetKerberosRealm (2, "ATHENA.MIT.EDU");
    KLAcquireNewInitialTickets (NULL, NULL, NULL, NULL);

    KLRemoveAllKerberosRealms ();
    KLInsertKerberosRealm (realmList_End, "ATHENA.MIT.EDU");
    KLInsertKerberosRealm (realmList_End, "TEST-KERBEROS-1.0.6");
    KLInsertKerberosRealm (realmList_End, "TESTV5-KERBEROS-1.0.6");
    KLInsertKerberosRealm (realmList_End, "TEST-KERBEROS-1.1.1");
    KLInsertKerberosRealm (realmList_End, "TESTV5-KERBEROS-1.1.1");
    KLInsertKerberosRealm (realmList_End, "TEST-KERBEROS-1.2.0");
    KLInsertKerberosRealm (realmList_End, "TESTV5-KERBEROS-1.2.0");
    KLInsertKerberosRealm (realmList_End, "TEST-HEIMDAL-0.3D");
    KLInsertKerberosRealm (realmList_End, "TESTV5-HEIMDAL-0.3D");
    KLInsertKerberosRealm (realmList_End, "TEST-KTH-KRB-1.1");
}	


void TestLoginOptions (void)
{
    KLBoolean optionSetting;
    KLStatus err = klNoErr;
    KLLifetime lifetime;

    lifetime = 10*60;
    KLSetDefaultLoginOption(loginOption_MinimalTicketLifetime, &lifetime, sizeof(UInt32));

    lifetime = 8*60*60;
    KLSetDefaultLoginOption(loginOption_MaximalTicketLifetime, &lifetime, sizeof(UInt32));

    lifetime = 8*60*60;
    KLSetDefaultLoginOption(loginOption_DefaultTicketLifetime, &lifetime, sizeof(UInt32));
    
    optionSetting = false;
    KLSetDefaultLoginOption(loginOption_LongTicketLifetimeDisplay, &optionSetting, sizeof(optionSetting));
    
    optionSetting = false;
    KLSetDefaultLoginOption(loginOption_DefaultForwardableTicket, &optionSetting, sizeof(optionSetting));
    
    optionSetting = true;
    KLSetDefaultLoginOption(loginOption_RememberPrincipal, &optionSetting, sizeof(optionSetting));
    
    optionSetting = true;
    err = KLSetDefaultLoginOption(loginOption_RememberExtras, &optionSetting, sizeof(optionSetting));

    if (err == klNoErr) {
        KLAcquireNewInitialTickets (NULL, NULL, NULL, NULL);
        optionSetting = true;
        KLAcquireNewInitialTickets (NULL, NULL, NULL, NULL);
    }
}


/* Lame date formatting stolen from CCacheDump, like ctime but with no \n */

static const char *day_name[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

static const char *month_name[] = {"January", "February", "March","April","May","June",
	   "July", "August",  "September", "October", "November","December"};

char* TimeToString (char* timeString, long time)
{
    /* we come in in 1970 time */
    time_t timer = (time_t) time;
    struct tm tm;

    tm = *localtime (&timer);

    sprintf(timeString, "%.3s %.3s%3d %.2d:%.2d:%.2d %d",
            day_name[tm.tm_wday],
            month_name[tm.tm_mon],
            tm.tm_mday,
            tm.tm_hour,
            tm.tm_min,
            tm.tm_sec,
            tm.tm_year + 1900);

    return timeString;
}


void MyKerberosLoginIdleCallback (KLRefCon inAppData)
{
    syslog (LOG_ALERT, "App got callback while waiting for Mach IPC (appData == %d)\n", inAppData);
//    KLCancelAllDialogs ();
}

