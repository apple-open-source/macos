/* APPLE LOCAL begin radar 4211430 */
/* { dg-do run { target powerpc*-*-darwin* } } */
/* { dg-options "-fasm-blocks" } */
#include <stdio.h>
extern void abort();

typedef unsigned int UInt32;

#define FUNCPTR_TO_UINT(f) ((UInt32)(&(f)))

/*
 *
 *  C versions
 *  
 */
static UInt32  DoAsmCodeTest_C();
static void  flush_cache_line_c(unsigned long inCacheLine);
static void  flush_cache_line_c_end(); /* { dg-warning "used but never defined" } */


asm void flush_cache_line_c(
 register unsigned long inCacheLine)
{
 dcbst 0, r3
 sync

 icbi 0, r3
 isync
entry static flush_cache_line_c_end;
}

UInt32
DoAsmCodeTest_C()
{
 UInt32 mw_size = 0;

 mw_size = FUNCPTR_TO_UINT(flush_cache_line_c);
 mw_size = FUNCPTR_TO_UINT(flush_cache_line_c_end) - mw_size;
 return mw_size;
}

int main()
{
	if (DoAsmCodeTest_C() != 16)
	  abort();
	return 0;
}
/* APPLE LOCAL end radar 4211430 */
