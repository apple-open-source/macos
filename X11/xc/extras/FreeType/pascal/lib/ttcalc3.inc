(*******************************************************************
 *
 *  TTCalc3.Inc                                                 1.2
 *
 *    Arithmetic and Vectorial Computations (inline assembly)
 *    This version is used for Delphi 2
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

(* NOTE : Delphi seems to use the eax, edx then ecx registers to pass *)
(*        the first three parameters                                  *)

(**********************************************************)
(* 64 Bit Addition                                        *)

procedure Add64( var X, Y, Z : Int64 ); assembler;
asm
  push ebx
  push esi
  mov  ebx, [ eax ]
  mov  esi, [eax+4]
  add  ebx, [ edx ]
  adc  esi, [edx+4]
  mov  [ ecx ], ebx
  mov  [ecx+4], esi
  pop  esi
  pop  ebx
end;


(**********************************************************)
(* 64 Bit Substraction                                    *)

procedure Sub64( var X, Y, Z : Int64 ); assembler;
asm
  push ebx
  push esi
  mov  ebx, [ eax ]
  mov  esi, [eax+4]
  sub  ebx, [ edx ]
  sbb  esi, [edx+4]
  mov  [ ecx ], ebx
  mov  [ecx+4], esi
  pop  esi
  pop  ebx
end;


(**********************************************************)
(* Multiply two Int32 to an Int64                         *)

procedure MulTo64( X, Y : Int32; var Z : Int64 ); assembler;
asm
  imul edx
  mov  [ ecx ],eax
  mov  [ecx+4],edx
end;

(**********************************************************)
(* Divide an Int64 by an Int32                            *)

function Div64by32( var X : Int64; Y : Int32 ) : Int32; assembler;
asm
  mov ecx, edx
  mov edx, [eax+4].dword
  mov eax, [ eax ].dword
  idiv ecx
end;

procedure DivMod64by32( var X : Int64; Y : Int32; var Q, R : Int32 );
          assembler;
asm
  push  ebx
  mov  ebx, edx
  mov  edx, [eax+4].dword
  mov  eax, [ eax ].dword
  idiv ebx
  mov  [ecx], eax
  mov  ebx, R
  mov  [ebx], edx
  pop  ebx
end;

