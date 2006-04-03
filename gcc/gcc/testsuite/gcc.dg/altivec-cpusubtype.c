/* APPLE LOCAL begin radar 4161346 */
/* { dg-do compile { target "powerpc*-*-darwin*" } } */
/* { dg-options "-faltivec" } */
/* { dg-final { scan-assembler-not "ppc7400" } } */

int main( int argc, char * argv[] )
{
    return 0;
}
/* APPLE LOCAL end radar 4161346 */
