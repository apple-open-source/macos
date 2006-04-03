
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

int main( void )
{
    const int bits_per_float = 24;
    const int mantissa_size = 52;
    const int bias = 1023;
    int currentp = -1;          //-1 is 2/pi, -2 would be 1/pi
    int nextp = currentp - 1;
    int offset = 0;
    uint64_t    mantissa = 0;
    int i, bit;
    union
    {
        uint64_t    u;
        double      d;
    }value, exponent, startExp, expstep;
    long double v = 0;
    long double test = 2.0L/3.14159265358979323846264338327950288L;
    
    startExp.u = mantissa_size + currentp - bits_per_float;                 
    expstep.u = bias - bits_per_float;
    startExp.u += bias;
    startExp.u <<= mantissa_size;                                           
    expstep.u <<= mantissa_size;
//    startExp.u &= 0x7ff0000000000000ULL;        
    startExp.u = 5;  // stick a simple constant in there for gdb testing -- jsm
    expstep.u &= 0x7ff0000000000000ULL; /* put a breakpoint here */
    
    while( currentp + bias > 0 )
    {
        //set up exponent
        value.u = 0;
        for( i = 0; i < bits_per_float; i++ )
        {
            bit = get_bit( offset++ );
            if( bit < 0 )
                goto exit;

            value.u = value.u << 1;
            value.u |= bit;
            nextp--;
        }
        value.u |= startExp.u;
        value.d -= startExp.d;
        v += value.d;
        printf( "0x1.%13.13llxp%d, //%17.21g  (%17.21g, %17.21g, %17.21g) %17.21g\n", value.u & 0x000FFFFFFFFFFFFFULL, ilogb(value.d), value.d, (double) test, (double) v, (double) (test - v), expstep.d );

        startExp.d *= expstep.d;
    }

exit:

    return 0;
}
