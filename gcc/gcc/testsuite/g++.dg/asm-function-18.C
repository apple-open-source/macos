/* APPLE LOCAL begin CW asm blocks 4258924 */
/* { dg-do assemble { target powerpc*-*-* } } */
/* { dg-options "-fasm-blocks" } */

struct AsmAlignCodeSample
{
        void AsmAlignCode();
        static void NextLabelFunction();

};

asm void AsmAlignCodeSample::AsmAlignCode()
{
        b NextLabelFunction
        .align 4
}
/* APPLE LOCAL end CW asm blocks 4258924 */
