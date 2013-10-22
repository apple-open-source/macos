
#include <Kerberos/KerberosLogin.h>
#include <stdio.h>
#include <err.h>

int
main(int argc, char **argv)
{
    int		error;
    char	*outName = NULL;
    char	*name, *instance, *realm;
    KLBoolean	foundTickets = 0;
    char	*cacheName;
    KLPrincipal	principal;
    uint32_t	version;
    
    error = KLCacheHasValidTickets(NULL, kerberosVersion_V5,
				   &foundTickets, &principal, &cacheName);
    if (error)
	errx(1, "no valid ticket");
    
    error = KLGetTripletFromPrincipal (principal, &name, &instance, &realm);
    KLDisposePrincipal (principal);
    if (error)
	errx(1, "failed to parse principal");
    
    printf("name: %s instance: %s realm: %s\n", name, instance, realm);
    
    KLDisposeString (name);
    KLDisposeString (instance);
    KLDisposeString (realm);

    return 0;
}
