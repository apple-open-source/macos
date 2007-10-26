/* APPLE LOCAL file radar 4228828 */
typedef short int16_t;
typedef unsigned char uint8_t;
/* { dg-do compile { target i?86-*-darwin* } } */
/* { dg-options "-O3" } */

void do_transfer(int16_t *in, uint8_t *out)
{
    int tmp;
    tmp = in[0];  out[0] = (uint8_t) (( (tmp) < 0 ? 0 : ((tmp) > 255 ? 255 : (tmp)) ));
    tmp = in[1];  out[1] = (uint8_t)   ( (tmp) < 0 ? 0 : ((tmp) > 255 ? 255 : (tmp)) );
    tmp = in[2];  out[2] = (uint8_t)   ( (tmp) < 0 ? 0 : ((tmp) > 255 ? 255 : (tmp)) );
    tmp = in[3];  out[3] = (uint8_t)   ( (tmp) < 0 ? 0 : ((tmp) > 255 ? 255 : (tmp)) );
}
/* { dg-final { scan-assembler-not "leal" } } */
