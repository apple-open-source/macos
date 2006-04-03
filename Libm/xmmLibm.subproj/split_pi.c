
#include <stdint.h>
#include <stdio.h>
#include <math.h>

//the bits of 2/pi after the initial 1.
const char *pi = "45F306DC9C882A53F84EAFA3EA69BB81B6C52B3278872083FCA2C757BD778AC36E48DC74849BA5C00C925DD413A32439FC3BD63962534E7DD1046BEA5D768909D338E04D68BEFC827323AC7306A673E93908BF177BF250763FF12FFFBC0B301FDE5E2316B414DA3EDA6CFD9E4F96136E9E8C7ECD3CBFD45AEA4F758FD7CBE2F67A0E73EF14A525D4D7F6BF623F1ABA10AC06608DF8F6D757E19F784135E86C3B53C722C2BDCC3610CB330ABE2940D0811BFFB1009AE64E620C0C2AAD94E75192C1C4F78118D68F883386CF9BB9D0125506B388ED172C394DBB5E89A2AE320A7D4BFE0E0A7EFC67D06585BC9F3064FB77867A4DDED63CBDF13E74";


int get_bit( int offset )
{
    int position = offset / 4;
    int nibble = pi[ position ];
    int value;
    int shift = 3 - (offset % 4 );

    if( nibble >= '0' && nibble <= '9' )
        value = nibble - '0';
    else
    {
        if( nibble >= 'A' && nibble <= 'F' )
            value = nibble - 'A' + 10;
        else
            return -1;
    }

    return (value >> shift) & 1;
}


/*
int main( void )
{
    const int bits_per_float = 25;
    const int mantissa_size = 52;
    const int bias = 1023;
    int currentp = 255;          //-1 is 2/pi, -2 would be 1/pi
    int nextp = currentp - 1;
    int offset = 0;
    uint64_t    mantissa = 0;
    int i, bit;
    union
    {
        uint64_t    u;
        double      d;
    }value;
    long double v = 0;
    long double test = 2.0L/3.14159265358979323846264338327950288L;
    
    while( currentp + bias > 0 )
    {
        //set up exponent
        value.u = currentp + bias;
        for( i = 0; i < bits_per_float; i++ )
        {
            bit = get_bit( offset++ );
            if( bit < 0 )
                goto exit;

            value.u = value.u << 1;
            value.u |= bit;
            nextp--;
        }
        value.u = value.u << ( mantissa_size - bits_per_float );
        v += value.d;
        printf( "0x1.%13.13llxp%d, //%17.21g  (%17.21g, %17.21g, %17.21g)\n", value.u & 0x000FFFFFFFFFFFFFULL, currentp, value.d, (double) test, (double) v, (double) (test - v) );

        value.u = 0;    
        //loop until next 1 is found
        do
        {
            bit = get_bit( offset++ );
            if( bit < 0 )
                goto exit;
            nextp--;
        }while( 0 == bit );
        currentp = nextp + 1;
    }

exit:

    return 0;
}
*/


int main( void )
{
    const int bits_per_float = 26;
    const int mantissa_size = 52;
    const int bias = 1023;
    int currentp = 0;          //-1 is 2/pi, -2 would be 1/pi
;    int offset = 0;
    uint64_t    mantissa = 0;
    int i, bit;
    union
    {
        uint64_t    u;
        double      d;
    }value, exponent, startExp, expstep;
    long double v = 0;
    long double test = 2.0L/3.14159265358979323846264338327950288L;
    
    startExp.u = bias + mantissa_size + currentp - bits_per_float;          expstep.u = bias - bits_per_float;
    startExp.u <<= mantissa_size;                                           expstep.u <<= mantissa_size;
    startExp.u &= 0x7ff0000000000000ULL;                                    expstep.u &= 0x7ff0000000000000ULL;

    //Deal with first implicit 1 bit
    value.u = 1;
    i = 1;
    
    while( startExp.d > 0 )
    {
        //set up exponent
        for( ; i < bits_per_float; i++ )
        {
            bit = get_bit( offset++ );
            if( bit < 0 )
                goto exit;

            value.u = value.u << 1;
            value.u |= bit;
        }
        value.u |= startExp.u;
        value.d -= startExp.d;
        v += value.d;
        printf( "{ 0x1.%13.13llxp%d, 0x1.%13.13llxp%d}, //%17.21g  (%17.21g, %17.21g, %17.21g) %17.21g\n", value.u & 0x000FFFFFFFFFFFFFULL, ilogb(value.d),value.u & 0x000FFFFFFFFFFFFFULL, ilogb(value.d), value.d, (double) test, (double) v, (double) (test - v), expstep.d );

        startExp.d *= expstep.d;
        i = 0;
        value.u = 0;
    }

exit:

    return 0;
}