/* APPLE LOCAL begin begin radar 4257049 */
/* { dg-do assemble { target powerpc*-*-* } } */
/* { dg-options "-fasm-blocks" } */

typedef unsigned long testAsmFuncType;

extern testAsmFuncType testAsmFunction();

asm testAsmFuncType

testAsmFunction()

{

        nofralloc

        
        li      r3, 1

        blr

}
/* APPLE LOCAL end radar 4257049 */
