(*******************************************************************
 *
 *  TTCalc.Pas                                                  1.2
 *
 *    Arithmetic and Vectorial Computations (specification)
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

unit TTCalc;

interface

{$I TTCONFIG.INC}

type
  (* IntN types :                                                       *)
  (*                                                                    *)
  (*  These types are used as a way to garantee the size of some        *)
  (*  specific integers.                                                *)
  (*                                                                    *)
  (*  Of course, they are equivalent to Short, UShort, Long, etc ..     *)
  (*  but parts of this unit could be used by different programs.       *)
  (*                                                                    *)

  (* Define the 16-bit type *)
{$IFDEF BORLANDPASCAL}
  Int16  = Integer;
  Word16 = Word;               (* 16-bits unsigned *)
{$ELSE}
{$IFDEF DELPHI16}
  Int16  = Integer;
  Word16 = Word;               (* 16-bits unsigned *)
{$ELSE}
{$IFDEF DELPHI32}
  Int16  = SmallInt;
  Word16 = Word;               (* 16-bits unsigned *)
{$ELSE}
  Int16  = SmallInt;
  Word16 = SmallWord;          (* 16-bits unsigned *)
{$ENDIF}
{$ENDIF}
{$ENDIF}

  Int32  = LongInt;            (* 32 bits integer  *)

  Word32 = LongInt;            (* 32 bits 'unsigned'. Note that there's *)
                               (* no unsigned long in Pascal..          *)
                               (* As cardinals are only 31 bits !!      *)

  Int64  = record              (* 64 ""            *)
            Lo,
            Hi  : LongInt;
          end;

function MulDiv( A, B, C : Int32 ): Int32;

function MulDiv_Round( A, B, C : Int32 ): Int32;

procedure Add64( var X, Y, Z : Int64 );
procedure Sub64( var X, Y, Z : Int64 );

procedure MulTo64( X, Y : Int32; var Z : Int64 );

function  Div64by32( var X : Int64; Y : Int32 ) : Int32;

function Order64( var Z : Int64 ) : integer;
function Order32(     Z : Int32 ) : integer;

function Sqrt32( L : Int32 ): LongInt;
function Sqrt64( L : Int64 ): LongInt;

{$IFDEF TEST}
  procedure Neg64( var x : Int64 );
  procedure DivMod64by32( var X : Int64; Y : Int32; var Q, R : Int32 );
{$ENDIF}

implementation

(* add support for Virtual Pascal inline assembly *)
{$IFDEF VIRTUALPASCAL}
{$I TTCALC2.INC}
{$ENDIF}

(* add support for Delphi 2 and 3 inline assembly *)
{$IFDEF DELPHI32}
{$I TTCALC3.INC}
{$ENDIF}

(* add support for Borland Pascal and Turbo Pascal inline assembly *)
{$IFDEF BORLANDPASCAL}
{$I TTCALC1.INC}
{$ENDIF}

(* Delphi 16 uses the same inline assembly than Borland Pascal *)
{$IFDEF DELPHI16}
{$I TTCALC1.INC}
{$ENDIF}

(* add support for Free Pascal inline assembly *)
{$IFDEF FPK}
{$I TTCALC4.INC}
{$ENDIF}

 (*****************************************************************)
 (*                                                               *)
 (*  MulDiv : computes A*B/C with an intermediate 64 bits         *)
 (*           precision.                                          *)
 (*                                                               *)
 (*****************************************************************)

 function MulDiv( a, b, c : Int32 ) : Int32;
 var
   s    : Int32;
   temp : Int64;
 begin
   s := a;       a := abs(a);
   s := s xor b; b := abs(b);
   s := s xor c; c := abs(c);

   MulTo64( a, b, temp );
   c := Div64by32( temp, c );

   if s < 0 then c := -c;

   MulDiv := c;
 end;

 (*****************************************************************)
 (*                                                               *)
 (*  MulDiv : computes A*B/C with an intermediate 64 bits         *)
 (*  _Round   precision and rounding.                             *)
 (*                                                               *)
 (*****************************************************************)

 function MulDiv_Round( a, b, c : Int32 ) : Int32;
 var
   s : Int32;

   temp, temp2 : Int64;
 begin
   s := a;       a := abs(a);
   s := s xor b; b := abs(b);
   s := s xor c; c := abs(c);

   MulTo64( a, b, temp );

   temp2.hi := 0;
   temp2.lo := c div 2;

   Add64( temp, temp2, temp );

   c := Div64by32( temp, c );

   if s < 0 then c := -c;

   MulDiv_Round := c;
 end;


(**********************************************************)
(* Negation                                               *)

 procedure Neg64( var x : Int64 );
 begin
   (* Remember that -(0x80000000) == 0x80000000 with 2-complement! *)
   (* We take care of that here.                                   *)

   x.hi := x.hi xor $FFFFFFFF;
   x.lo := x.lo xor $FFFFFFFF;
   inc( x.lo );

   if x.lo = 0 then
   begin
     inc( x.hi );
     if x.hi = $80000000 then  (* check -MaxInt32-1 *)
     begin
       dec( x.lo );  (* we return $7FFFFFFF *)
       dec( x.hi );
     end;
   end;
 end;


(**********************************************************)
(* MSB index ( return -1 for 0 )                          *)

function Order64( var Z : Int64 ) : integer;
begin
  if Z.Hi <> 0 then Order64 := 32 + Order32( Z.Hi )
               else Order64 := Order32( Z.Lo );
end;


(**********************************************************)
(* MSB index ( return -1 for 0 )                          *)

function Order32( Z : Int32 ) : integer;
var b : integer;
begin
  b := 0;
  while Z <> 0 do begin Z := Z shr 1; inc( b ); end;
  Order32 := b-1;
end;


const
  Roots : array[0..62] of LongInt
        = (
               1,    1,    2,     3,     4,     5,     8,    11,
              16,   22,   32,    45,    64,    90,   128,   181,
             256,  362,  512,   724,  1024,  1448,  2048,  2896,
            4096, 5892, 8192, 11585, 16384, 23170, 32768, 46340,

              65536,   92681,  131072,   185363,   262144,   370727,
             524288,  741455, 1048576,  1482910,  2097152,  2965820,
            4194304, 5931641, 8388608, 11863283, 16777216, 23726566,

              33554432,   47453132,   67108864,   94906265,
             134217728,  189812531,  268435456,  379625062,
             536870912,  759250125, 1073741824, 1518500250,
            2147483647
          );


(**************************************************)
(* Integer Square Root                            *)

function Sqrt32( L : Int32 ): LongInt;
var
  R, S : LongInt;
begin
  if L<=0 then Sqrt32:=0 else
  if L=1 then Sqrt32:=1 else
   begin
    R:=Roots[ Order32(L) ];

    Repeat
     S:=R;
     R:=( R+ L div R ) shr 1;
    until ( R <= S ) and ( R*R <= L );

    Sqrt32:=R;
   end;
end;


(**************************************************)
(* Integer Square Root                            *)

function Sqrt64( L : Int64 ): LongInt;
var
  L2   : Int64;
  R, S : LongInt;
begin
   if L.Hi < 0 then Sqrt64:=0 else
   begin
    S := Order64(L);
    if S = 0 then Sqrt64:=1 else
     begin
      R := Roots[S];

      Repeat

       S := R;
       R := ( R+Div64by32(L,R) ) shr 1;

       if ( R > S ) then continue;

       MulTo64( R,  R, L2 );
       Sub64  ( L, L2, L2 );

      until ( L2.Hi >= 0 );

      Sqrt64 := R;
     end
   end
end;

end.
