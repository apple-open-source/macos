
#include <stdio.h>
#include <stdint.h>

#define SIGN_BIT        0x8000000000000000LL
#define EXP_MASK        0x7FF0000000000000LL
#define MANTISSA_MASK   0x000FFFFFFFFFFFFFLL
#define BIAS            0x3FF0000000000000LL

int main ( int argc, char **argv )
{
    int64_t    inval = 0;
    char *string;
    int err;
    union
    {
        int64_t i;
        double  d;
    }u;

    if( argc != 2 )
    {
        printf( "usage:  h2p  <value in hex>\n" );
        return 0;
    }
    
    FILE * file = fopen( argv[1], "r" );
    err = fseek( file, 0L, SEEK_END );    if( 0 != err )  printf( "err %d on fseek\n", err );
    size_t size = ftell( file );
    err = fseek( file, 0L, 0 );    
    string = (char*) valloc( size );
    err = fread( string, size-1, 1, file );  if( 0 != err )  printf( "err %d on fread\n", err );
    fclose( file );

    //skip until 0x is found
    char *c = string;
    while( c[0] != '\0' )
    {
        if( c[0] == '0' && c[1] == 'x' )
        {
            int64_t exp;
            sscanf( c, "%llx", &inval );
            u.i = inval;

            if( inval & SIGN_BIT )
                fprintf( stdout, "-" );
            else
                fprintf( stdout, " " );
            inval &= ~SIGN_BIT;

            exp = inval & EXP_MASK;
            exp -= BIAS;
            exp >>= 13*4;
            
            if( exp == -1023 )
                fprintf( stdout, "0x0.%13.13llXp", inval & MANTISSA_MASK );
            else
                fprintf( stdout, "0x1.%13.13llXp", inval & MANTISSA_MASK );
            
            if( exp >= 0 )
                fprintf( stdout, "+" );
            fprintf( stdout, "%d, //%17.21g\n", (int) exp, u.d );
        }
        c++;         
    }
        
    
    
    return 0;

}
