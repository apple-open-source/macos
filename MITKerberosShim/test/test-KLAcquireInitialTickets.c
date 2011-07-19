
#include <stdio.h>
#include <err.h>

#include <Kerberos/Kerberos.h>
#include <Kerberos/KerberosLogin.h>
#include <Kerberos/KerberosLoginPrivate.h>

#include <err.h>


static KLStatus
AcquireTGT(char *inusername)
{
    KLStatus	status;
    KLPromptMechanism prompt;
    char *credCacheName = NULL;
    KLPrincipal klprincipal = NULL;
	
    prompt = __KLPromptMechanism ();
    __KLSetPromptMechanism (klPromptMechanism_GUI);

    if (inusername && inusername[0]) {
	// user specified, acquire tickets for this user, and make it default
	status = KLCreatePrincipalFromString(inusername, kerberosVersion_V5, &klprincipal);
	if (status == klNoErr) {
	    status = KLAcquireInitialTickets(klprincipal, NULL, NULL, &credCacheName);
	    if (status == klNoErr)
		status = KLSetSystemDefaultCache(klprincipal);
	}
    }
    else {
	// no user specified, use default configuration
	status = KLAcquireInitialTickets(NULL, NULL, NULL, &credCacheName);
    }
		
    if(klprincipal != NULL)
	KLDisposePrincipal(klprincipal);
    if(credCacheName != NULL)
	KLDisposeString(credCacheName);
    __KLSetPromptMechanism (prompt);

    return status;
}

int
main(int argc, char **argv)
{
	if (argc != 2)
		errx(1, "argc != 2");
	AcquireTGT(argv[1]);
}
