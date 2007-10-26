#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <openssl/rand.h>

#define PACKAGE_NAME "Crypt::OpenSSL::RSA"    

MODULE = Crypt::OpenSSL::Random		PACKAGE = Crypt::OpenSSL::Random		
void
random_bytes(num_bytes_SV)
    SV * num_bytes_SV;
PPCODE:
{
    unsigned char *rand_bytes;
    int num_bytes = SvIV(num_bytes_SV);
    if(New(0,rand_bytes, num_bytes, unsigned char) == NULL)
    {
        croak ("unable to allocate buffer for random bytes in package "
            PACKAGE_NAME);
    }

    if(RAND_bytes(rand_bytes, num_bytes))
    {
        XPUSHs(sv_2mortal(newSVpv(rand_bytes, num_bytes)));
        Safefree(rand_bytes);
        XSRETURN(1);
    }
    else
    {
        XSRETURN_NO;
    }
}

void
random_pseudo_bytes(num_bytes_SV)
    SV * num_bytes_SV;
PPCODE:
{
    unsigned char *rand_bytes;
    int num_bytes = SvIV(num_bytes_SV);
    if(New(0,rand_bytes, num_bytes, unsigned char) == NULL)
    {
        croak ("unable to allocate buffer for random bytes in package "
            PACKAGE_NAME);
    }

    if(RAND_bytes(rand_bytes, num_bytes))
    {
        XPUSHs(sv_2mortal(newSVpv(rand_bytes, num_bytes)));
        Safefree(rand_bytes);
        XSRETURN(1);
    }
    else
    {
        XSRETURN_NO;
    }
}

 # Seed the PRNG with user-provided bytes; returns true if the 
 # seeding was sufficient.

void
random_seed(random_bytes_SV)
    SV * random_bytes_SV;
PPCODE:
{
    int random_bytes_length;
    char *random_bytes;
    random_bytes = SvPV(random_bytes_SV, random_bytes_length);
    RAND_seed(random_bytes, random_bytes_length);
    XPUSHs( sv_2mortal( newSViv( RAND_status() ) ) );
}

 # Seed the PRNG with data from the indicated EntropyGatheringDaemon;
 # returns the number of bytes gathered, or -1 if there was a
 # connection failure or if the PRNG is still insufficiently seeded.

void
random_egd(egd_SV)
    SV * egd_SV;
PPCODE:
{
    int random_bytes_length;
    char *random_bytes;
    int status;
    char *egd = SvPV(egd_SV, random_bytes_length);
    status = RAND_egd(egd);
    XPUSHs( sv_2mortal( newSViv( status ) ) );
}

 # Returns true if the PRNG has enough seed data

void
random_status()
PPCODE:
{
    XPUSHs( sv_2mortal( newSViv( RAND_status() ) ) );
}

