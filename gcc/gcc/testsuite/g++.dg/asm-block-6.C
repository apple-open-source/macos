/* APPLE LOCAL begin radar 4150131 */
/* { dg-do compile { target powerpc*-*-darwin* } } */
/* { dg-options "-fasm-blocks" } */

main()
{
        asm {
                add r0,r0,r0
        }
}

#pragma options align=natural
#pragma options align=reset
/* APPLE LOCAL end radar 4150131 */
