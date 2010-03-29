/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*****************************************************************************
 * Cortex-A8 implementation                                                  *
 *****************************************************************************/
 
// Cortex-A8 implementations of memcpy( ), memmove( ) and bcopy( ).
//
// Our tests have shown that NEON is always a performance win for memcpy( ).
// However, for the specific case of copies from a warm source to a cold
// destination when the buffer size is between 1k and 32k, it is not enough
// of a performance win to offset the increased power footprint, resulting
// in an energy usage regression.  Thus, we detect that particular case, and
// pass those copies through the ARM core registers.  All other copies larger
// than 8 bytes are handled on NEON.
//
// Stephen Canon, August 2009

.text
.code 16
.syntax unified

// void bcopy(const void * source,
//            void * destination,
//            size_t length);
//
// void *memmove(void * destination,
//               const void * source,
//               size_t n);
//
// void *memcpy(void * restrict destination,
//              const void * restrict source,
//              size_t n);
//
// all copy n successive bytes from source to destination. memmove and memcpy
// returns destination, whereas bcopy has no return value. copying takes place
// as if it were through a temporary buffer -- after return destination contains
// exactly the bytes from source, even if the buffers overlap.

.thumb_func _bcopy
.globl _bcopy    
.thumb_func _memmove
.globl _memmove
.thumb_func _memcpy
.globl _memcpy

.align 2
_bcopy:
	mov       r3,      r0           // swap the first and second arguments
	mov       r0,      r1           // and fall through into memmove
	mov       r1,      r3           //

.align 2
_memmove:
_memcpy:
    subs      r3,      r0,  r1      // offset = destination addr - source addr
    it        eq
    bxeq      lr                    // if source == destination, early out

//  Our preference is for using a (faster) front-to-back copy.  However, if
//  0 < offset < length, it is necessary to copy back-to-front for correctness.
//  We have already ruled out offset == 0, so we can use an unsigned compare
//  with length -- if offset is higher, offset is either greater than length
//  or negative.

    cmp       r3,      r2
    bhs       L_copyFrontToBack
                             
/*****************************************************************************
 *  back to front copy                                                       *
 *****************************************************************************/

    mov       ip,      r0           // copy destination pointer.
    add       r1,           r2      // move source pointer to end of source array
    add       ip,           r2      // move destination pointer to end of dest array
    
    subs      r2,           $8      // if length - 8 is negative (i.e. length
    blt       L_scalarReverseCopy   // is less than 8), jump to cleanup path.
    tst       ip,           $7      // if (destination + length) is doubleword
    beq       L_vectorReverseCopy   // aligned, jump to fast path.
    
0:  ldrb      r3,     [r1, $-1]!    // load byte
    sub       r2,           $1      // decrement length
    strb      r3,     [ip, $-1]!    // store byte
    tst       ip,           $7      // test alignment
    bne       0b
    
    cmp       r2,           $0      // if length - 8 is negative,
    blt       L_scalarReverseCopy   // jump to the cleanup code
                                    
/*****************************************************************************
 *  destination is doubleword aligned                                        *
 *****************************************************************************/

L_vectorReverseCopy:
    ands      r3,      r1,  $3      // Extract the alignment of the source
    bic       r1,           $3
    tbh      [pc, r3, lsl $1]       // Dispatch table on source alignment
0:  
.short (L_reverseAligned0-0b)/2     // The NEON alignment hardware does not work
.short (L_reverseAligned1-0b)/2     // properly with sub 4-byte alignment and
.short (L_reverseAligned2-0b)/2     // buffers that are uncacheable, so we need
.short (L_reverseAligned3-0b)/2     // to have a software workaround.

/*****************************************************************************
 *  source is also at least word aligned                                     *
 *****************************************************************************/
    
L_reverseAligned0:
    subs      r2,           $0x38   // if length - 64 is negative, jump to
    blt       L_reverseVectorCleanup// the cleanup path.
    tst       ip,           $0x38   // if (destination + length) is cacheline
    beq       L_reverseCachelineAligned // aligned, jump to the fast path.
    
0:  sub       r1,           $8      // copy eight bytes at a time until the
    vld1.32  {d0},    [r1]          // destination is 8 byte aligned.
    sub       ip,           $8      //
    sub       r2,           $8      //
    tst       ip,           $0x38   //
    vst1.64  {d0},    [ip, :64]     //
    bne       0b                    //
    
    cmp       r2,           $0      // if length - 64 is negative,
    blt       L_reverseVectorCleanup// jump to the cleanup code
    
L_reverseCachelineAligned:
    sub       r3,      r2,  $0x3c0  // If 1024 < length < 32768, use core
    cmp       r3,          $0x7c00  // register copies instead of NEON to
    blo       L_useSTMDB            // control energy usage.
    
    sub       r1,           $32     // decrement source
    sub       ip,           $32     // decrement destination
    mov       r3,           $-32    // load address increment
    tst       r1,           $0x1f   // if source shares 32 byte alignment
    beq       L_reverseSourceAligned// jump to loop with more alignment hints
    
    vld1.32  {q2,q3}, [r1], r3      // This loop handles 4-byte aligned copies
    vld1.32  {q0,q1}, [r1], r3      // as generally as possible.
    subs      r2,           $64     // 
    vst1.64  {q2,q3}, [ip,:256], r3 // The Cortex-A8 NEON unit does not always
    blt       1f                    // properly handle misalignment in vld1
.align 3                            // with an element size of 8 or 16, so
0:  vld1.32  {q2,q3}, [r1], r3      // this is the best we can do without
    vst1.64  {q0,q1}, [ip,:256], r3 // handling alignment in software.
    vld1.32   {q0,q1}, [r1], r3     // 
    subs      r2,           $64     // 
    vst1.64  {q2,q3}, [ip,:256], r3 // 
    bge       0b                    // 
    b         1f                    // 
    
L_reverseSourceAligned:
    vld1.64  {q2,q3}, [r1,:256], r3 // Identical to loop above except for
    vld1.64  {q0,q1}, [r1,:256], r3 // additional alignment information; this
    subs      r2,           $64     // gets an additional .5 bytes per cycle
    vst1.64  {q2,q3}, [ip,:256], r3 // on Cortex-A8.
    blt       1f                    // 
.align 3                            // 
0:  vld1.64  {q2,q3}, [r1,:256], r3 //
    vst1.64  {q0,q1}, [ip,:256], r3 //
    vld1.64  {q0,q1}, [r1,:256], r3 //
    subs      r2,           $64     //
    vst1.64  {q2,q3}, [ip,:256], r3 //
    bge       0b                    //
1:  vst1.64  {q0,q1}, [ip,:256], r3 // loop cleanup: final 32 byte store
    add       r1,           $32     // point source at last element stored
    add       ip,           $32     // point destination at last element stored
    
L_reverseVectorCleanup:
    adds      r2,           $0x38   // If (length - 8) < 0, goto scalar cleanup
    blt       L_scalarReverseCopy   //

0:  sub       r1,           $8      // copy eight bytes at a time until
    vld1.32  {d0},    [r1]          // (length - 8) < 0.
    sub       ip,           $8      //
    subs      r2,           $8      //
    vst1.64  {d0},    [ip, :64]     //
    bge       0b                    //

/*****************************************************************************
 *  sub-doubleword cleanup copies                                            *
 *****************************************************************************/

L_scalarReverseCopy:
    adds      r2,           #0x8    // restore length
    it        eq                    // if this is zero
    bxeq      lr                    // early out
         
0:  ldrb      r3,     [r1, #-1]!    // load a byte from source
    strb      r3,     [ip, #-1]!    // store to destination
    subs      r2,           #0x1    // subtract one from length
    bne       0b                    // if non-zero, repeat
    bx        lr                    // return
         
/*****************************************************************************
 *  STMDB loop for 1k-32k buffers                                            *
 *****************************************************************************/

L_useSTMDB:
    push     {r4-r8,r10,r11}
.align 3
0:  ldmdb	  r1!,  {r3-r8,r10,r11}
    subs      r2,           #0x40
    stmdb     ip!,  {r3-r8,r10,r11}
    ldmdb	  r1!,  {r3-r8,r10,r11}
	pld		 [r1, #-0x40]
    stmdb     ip!,  {r3-r8,r10,r11}
    bge       0b
    pop      {r4-r8,r10,r11}
    b         L_reverseVectorCleanup
    
/*****************************************************************************
 *  Misaligned vld1 loop                                                     *
 *****************************************************************************/

// Software alignment fixup to handle source and dest that are relatively
// misaligned mod 4 bytes.  Load two 4-byte aligned double words from source, 
// use vext.8 to extract a double word to store, and perform an 8-byte aligned
// store to destination.

#define RCOPY_UNALIGNED(offset)      \
    subs      r2,          $8       ;\
    blt       2f                    ;\
    sub       r1,          $8       ;\
    sub       ip,          $8       ;\
    mov       r3,          $-8      ;\
    vld1.32  {d2,d3}, [r1], r3      ;\
    subs      r2,          $8       ;\
    blt       1f                    ;\
0:  vext.8    d0,  d2, d3, $(offset);\
    vmov      d3,      d2           ;\
    vld1.32  {d2},    [r1], r3      ;\
    subs      r2,          $8       ;\
    vst1.64  {d0},    [ip, :64], r3 ;\
    bge       0b                    ;\
1:  vext.8    d0,  d2, d3, $(offset);\
    add       r1,          $8       ;\
    vst1.64  {d0},    [ip, :64]     ;\
2:  add       r2,          $8       ;\
    add       r1,          $(offset);\
    b         L_scalarReverseCopy

L_reverseAligned1:
    RCOPY_UNALIGNED(1)
L_reverseAligned2:
    RCOPY_UNALIGNED(2)
L_reverseAligned3:
    RCOPY_UNALIGNED(3)

/*****************************************************************************
 *  front to back copy                                                       *
 *****************************************************************************/

L_copyFrontToBack:
    mov       ip,      r0           // copy destination pointer.
    subs      r2,           $8      // if length - 8 is negative (i.e. length
    blt       L_scalarCopy          // is less than 8), jump to cleanup path.
    tst       ip,           $7      // if the destination is doubleword
    beq       L_vectorCopy          // aligned, jump to fast path.
    
0:  ldrb      r3,     [r1], $1      // load byte
    sub       r2,           $1      // decrement length
    strb      r3,     [ip], $1      // store byte
    tst       ip,           $7      // test alignment
    bne       0b
    
    cmp       r2,           $0      // if length - 8 is negative,
    blt       L_scalarCopy          // jump to the cleanup code
    
/*****************************************************************************
 *  destination is doubleword aligned                                        *
 *****************************************************************************/

L_vectorCopy:
    ands      r3,      r1,  $3      // Extract the alignment of the source
    bic       r1,           $3
    tbh      [pc, r3, lsl $1]       // Dispatch table on source alignment
0:  
.short (L_sourceAligned0-0b)/2      // The NEON alignment hardware does not work
.short (L_sourceAligned1-0b)/2      // properly with sub 4-byte alignment and
.short (L_sourceAligned2-0b)/2      // buffers that are uncacheable, so we need
.short (L_sourceAligned3-0b)/2      // to have a software workaround.

/*****************************************************************************
 *  source is also at least word aligned                                     *
 *****************************************************************************/
    
L_sourceAligned0:
    subs      r2,           $0x38   // If (length - 64) < 0
    blt       L_vectorCleanup       //   jump to cleanup code
    tst       ip,           $0x38   // If destination is 64 byte aligned
    beq       L_cachelineAligned    //   jump to main loop
    
0:  vld1.32  {d0},    [r1]!         // Copy one double word at a time until
    sub       r2,           $8      // the destination is 64-byte aligned.
    vst1.64  {d0},    [ip, :64]!    //
    tst       ip,           $0x38   //
    bne       0b                    //
    
    cmp       r2,           $0      // If (length - 64) < 0, goto cleanup
    blt       L_vectorCleanup       //
    
L_cachelineAligned:
    sub       r3,      r2,  $0x3c0  // If 1024 < length < 32768, use core
    cmp       r3,          $0x7c00  // register copies instead of NEON to
    blo       L_useSTMIA            // control energy usage.
    tst       r1,           $0x1f   // If source has 32-byte alignment, use
    beq       L_sourceAligned32     // an optimized loop.
    
    vld1.32  {q2,q3}, [r1]!         // This is the most common path for small
    vld1.32  {q0,q1}, [r1]!         // copies, which are alarmingly frequent.
    subs      r2,           #0x40   // It requires 4-byte alignment on the
    vst1.64  {q2,q3}, [ip, :256]!   // source.  For ordinary malloc'd buffers,
    blt       1f                    // this path could handle only single-byte
.align 3                            // alignment at speed by using vld1.8
0:  vld1.32  {q2,q3}, [r1]!         // instead of vld1.32; however, the NEON
    vst1.64  {q0,q1}, [ip, :256]!   // alignment handler misbehaves for some
    vld1.32  {q0,q1}, [r1]!         // special copies if the element size is
    subs      r2,           #0x40   // 8 or 16, so we need to work around
    vst1.64  {q2,q3}, [ip, :256]!   // sub 4-byte alignment in software, in
    bge       0b                    // another code path.
    b         1f
    
L_sourceAligned32:
    vld1.64  {q2,q3}, [r1, :256]!   // When the source shares 32-byte alignment
    vld1.64  {q0,q1}, [r1, :256]!   // with the destination, we use this loop
    subs      r2,           #0x40   // instead, which specifies the maximum
    vst1.64  {q2,q3}, [ip, :256]!   // :256 alignment on all loads and stores.
    blt       1f                    // 
.align 3                            // This gets an additional .5 bytes per
0:  vld1.64  {q2,q3}, [r1, :256]!   // cycle for in-cache copies, which is not
    vst1.64  {q0,q1}, [ip, :256]!   // insignificant for this (rather common)
    vld1.64  {q0,q1}, [r1, :256]!   // case.
    subs      r2,           #0x40   // 
    vst1.64  {q2,q3}, [ip, :256]!   // This is identical to the above loop,
    bge       0b                    // except for the additional alignment.
1:  vst1.64  {q0,q1}, [ip, :256]!   // 

L_vectorCleanup:
    adds      r2,           $0x38   // If (length - 8) < 0, goto scalar cleanup
    blt       L_scalarCopy          //
    
0:  vld1.32  {d0},    [r1]!         // Copy one doubleword at a time until
    subs      r2,           $8      // (length - 8) < 0.
    vst1.64  {d0},    [ip, :64]!    //
    bge       0b                    //

/*****************************************************************************
 *  sub-doubleword cleanup copies                                            *
 *****************************************************************************/

L_scalarCopy:
    adds      r2,           #0x8    // restore length
    it        eq                    // if this is zero
    bxeq      lr                    // early out
         
0:  ldrb      r3,     [r1], #1      // load a byte from source
    strb      r3,     [ip], #1      // store to destination
    subs      r2,           #1      // subtract one from length
    bne       0b                    // if non-zero, repeat
    bx        lr                    // return
    
/*****************************************************************************
 *  STMIA loop for 1k-32k buffers                                            *
 *****************************************************************************/

L_useSTMIA:
    push     {r4-r8,r10,r11}
.align 3
0:  ldmia     r1!,  {r3-r8,r10,r11}
    subs      r2,      r2,  #64
    stmia     ip!,  {r3-r8,r10,r11}
    ldmia     r1!,  {r3-r8,r10,r11}
    pld      [r1, #64]
    stmia     ip!,  {r3-r8,r10,r11}
    bge       0b
    pop      {r4-r8,r10,r11}
    b         L_vectorCleanup
    
/*****************************************************************************
 *  Misaligned reverse vld1 loop                                             *
 *****************************************************************************/

// Software alignment fixup to handle source and dest that are relatively
// misaligned mod 4 bytes.  Load two 4-byte aligned double words from source, 
// use vext.8 to extract a double word to store, and perform an 8-byte aligned
// store to destination.

#define COPY_UNALIGNED(offset)       \
    subs      r2,          $8       ;\
    blt       2f                    ;\
    vld1.32  {d2,d3}, [r1]!         ;\
    subs      r2,          $8       ;\
    blt       1f                    ;\
0:  vext.8    d0,  d2, d3, $(offset);\
    vmov      d2,      d3           ;\
    vld1.32  {d3},    [r1]!         ;\
    subs      r2,          $8       ;\
    vst1.64  {d0},    [ip, :64]!    ;\
    bge       0b                    ;\
1:  vext.8    d0,  d2, d3, $(offset);\
    sub       r1,          $8       ;\
    vst1.64  {d0},    [ip, :64]!    ;\
2:  add       r1,          $(offset);\
    add       r2,          $8       ;\
    b         L_scalarCopy

L_sourceAligned1:
    COPY_UNALIGNED(1)
L_sourceAligned2:
    COPY_UNALIGNED(2)
L_sourceAligned3:
    COPY_UNALIGNED(3)
