(*******************************************************************
 *
 *  TTCalc4.Inc                                                 1.2
 *
 *    Arithmetic and Vectorial Computations (inline assembly)
 *    This version is used for i386 FreePascal
 *
 *  Copyright 1996 David Turner, Robert Wilhelm and Werner Lemberg
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 *  NOTES : All vector operations were moved to the interpreter
 *
 ******************************************************************)

(**********************************************************)
(*                                                        *)
(* The following routines are inline assembly, they are   *)
(* thus processor and bitness specific. Replace them      *)
(* with your own if you want to port the TrueType Engine  *)

(**********************************************************)
(* 64 Bit Addition                                        *)

procedure Add64( var X, Y, Z : Int64 ); assembler;
asm
  push %ebx
  push %edx

  mov  X,%ebx
  mov  (%ebx)  ,%eax
  mov  4(%ebx) ,%edx

  mov  Y,%ebx
  add  (%ebx)  ,%eax
  adc  4(%ebx) ,%edx

  mov  Z,%ebx
  mov  %eax, (%ebx)
  mov  %edx, 4(%ebx)

  pop  %edx
  pop  %ebx
end;


(**********************************************************)
(* 64 Bit Substraction                                    *)

procedure Sub64( var X, Y, Z : Int64 ); assembler;
asm
  push %ebx
  push %edx

  mov  X,%ebx
  mov  (%ebx)  ,%eax
  mov  4(%ebx) ,%edx

  mov  Y,%ebx
  sub  (%ebx)  ,%eax
  sbb  4(%ebx) ,%edx

  mov  Z,%ebx
  mov  %eax, (%ebx)
  mov  %edx, 4(%ebx)

  pop  %edx
  pop  %ebx
end;


(**********************************************************)
(* Multiply two Int32 to an Int64                         *)

procedure MulTo64( X, Y : Int32; var Z : Int64 ); assembler;
asm
  push  %ebx
  push  %edx

  mov   X,%eax
  imull Y

  mov   Z,%ebx
  mov   %eax, (%ebx)
  mov   %edx, 4(%ebx)

  pop   %edx
  pop   %ebx
end;


(**********************************************************)
(* Divide an Int64 by an Int32                            *)

function Div64by32( var X : Int64; Y : Int32 ) : Int32; assembler;
asm
  push  %ebx
  push  %edx

  mov  X,%ebx
  mov  (%ebx)  ,%eax
  mov  4(%ebx) ,%edx
  idivl Y

  pop  %edx
  pop  %ebx
end;


procedure DivMod64by32( var X : Int64; Y : Int32; var Q, R : Int32 );
          assembler;
asm
  push  %ebx
  push  %edx

  mov  X,%ebx
  mov  (%ebx)  ,%eax
  mov  4(%ebx) ,%edx
  idivl Y

  mov  Q, %ebx
  mov  %eax, (%ebx)

  mov  R, %ebx
  mov  %edx, (%ebx)

  pop  %edx
  pop  %ebx
end;

