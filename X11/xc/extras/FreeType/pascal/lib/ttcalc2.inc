(*******************************************************************
 *
 *  TTCalc2.Inc                                                 1.2
 *
 *    Arithmetic and Vectorial Computations (inline assembly)
 *    This version is used for the OS/2 Virtual Pascal compiler
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

(* We need unsigned longints to perform correctly our additions *)
(* we include inline assembly to get them, baaahhh ..           *)

(**********************************************************)
(* 64 Bit Addition                                        *)

procedure Add64( var X, Y, Z : Int64 ); assembler;
{&USES ebx, edx}
asm
  mov ebx,[X].dword
  mov eax,[ebx]
  mov edx,[ebx+4]

  mov ebx,[Y].dword
  add eax,[ebx]
  adc edx,[ebx+4]

  mov ebx,[Z].dword
  mov [ebx],eax
  mov [ebx+4],edx
end;


(**********************************************************)
(* 64 Bit Substraction                                    *)

procedure Sub64( var X, Y, Z : Int64 ); assembler;
{&USES ebx, edx}
asm
  mov ebx,[X].dword
  mov eax,[ebx]
  mov edx,[ebx+4]

  mov ebx,[Y].dword
  sub eax,[ebx]
  sbb edx,[ebx+4]

  mov ebx,[Z].dword
  mov [ebx],eax
  mov [ebx+4],edx
end;


(**********************************************************)
(* Multiply two Int32 to an Int64                         *)

procedure MulTo64( X, Y : Int32; var Z : Int64 ); assembler;
{&USES ebx, edx }
asm
  mov  ebx,[Z].dword
  mov  eax,[X]
  imul dword ptr [Y]
  mov  [ebx],eax
  mov  [ebx+4],edx
end;


(**********************************************************)
(* Divide an Int64 by an Int32                            *)

function Div64by32( var X : Int64; Y : Int32 ) : Int32; assembler;
{&USES ebx, edx}
asm
  mov ebx, [X].dword
  mov eax, [ebx]
  mov edx, [ebx+4]
  idiv dword ptr [Y]
end;

procedure DivMod64by32( var X : Int64; Y : Int32; var Q, R : Int32 );
          assembler; {&USES ebx, edx}
asm
  mov ebx, [X].dword
  mov eax, [ebx]
  mov edx, [ebx+4]
  idiv dword ptr [Y]
  mov ebx, [Q].dword
  mov [ebx], eax
  mov ebx, [R].dword
  mov [ebx], edx
end;

