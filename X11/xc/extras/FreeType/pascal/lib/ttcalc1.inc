(*******************************************************************
 *
 *  TTCalc1.Inc                                                1.3
 *
 *    Arithmetic and Vectorial Computations (inline assembly)
 *    This version is used for 16-bit Turbo-Borland Pascal 6.0 & 7.0
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

{**********************************************************}
{* 64 Bit Addition                                        *}

procedure Add64( var X, Y, Z : Int64 ); assembler;
asm
  les si,[X]

  mov ax,es:[ si ].word
  mov dx,es:[si+2].word
  mov bx,es:[si+4].word
  mov cx,es:[si+6].word

  les si,[Y]
  add ax,es:[ si ].word
  adc dx,es:[si+2].word
  adc bx,es:[si+4].word
  adc cx,es:[si+6].word

  les si,[Z]
  mov es:[ si ].word,ax
  mov es:[si+2].word,dx
  mov es:[si+4].word,bx
  mov es:[si+6].word,cx
end;


{**********************************************************}
{* 64 Bit Substraction                                    *}

procedure Sub64( var X, Y, Z : Int64 ); assembler;
asm
  les si,[X]

  mov ax,es:[ si ].word
  mov dx,es:[si+2].word
  mov bx,es:[si+4].word
  mov cx,es:[si+6].word

  les si,[Y]
  sub ax,es:[ si ].word
  sbb dx,es:[si+2].word
  sbb bx,es:[si+4].word
  sbb cx,es:[si+6].word

  les si,[Z]
  mov es:[ si ].word,ax
  mov es:[si+2].word,dx
  mov es:[si+4].word,bx
  mov es:[si+6].word,cx
end;


{**********************************************************}
{* Multiply two Int32 to an Int64                         *}

procedure MulTo64( X, Y : Int32; var Z : Int64 ); assembler;
asm
  les si,[Z]
  db $66; mov ax,[X].word
  db $66; imul [Y].word
  db $66; mov es:[si],ax
  db $66; mov es:[si+4],dx
end;


{**********************************************************}
{* Divide an Int64 by an Int32                            *}

function Div64by32( var X : Int64; Y : Int32 ) : Int32; assembler;
asm
  les si,[X]

  db $66; mov ax,es:[si]
  db $66; mov dx,es:[si+4]
  db $66; idiv [Y].word

  db $66; mov dx, ax
  db $66; sar dx, 16
end;


procedure DivMod64by32( var X : Int64; Y : Int32; var Q, R : Int32 ); assembler;
asm
  les si,[X]

  db $66; mov ax,es:[si]
  db $66; mov dx,es:[si+4]
  db $66; idiv [Y].word

  les si, [Q]
  db $66; mov es:[si], ax

  les si, [R]
  db $66; mov es:[si], dx
end;

