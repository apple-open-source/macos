(*******************************************************************
 *
 *  TTRaster.Pas                                              v 1.2
 *
 *  The FreeType glyph rasterizer.
 *
 *  Copyright 1996 David Turner, Robert Wilhelm and Werner Lemberg
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 *  NOTES : This version supports the following :
 *
 *    - direct grayscaling
 *    - sub-banding
 *    - drop-out modes 4 and 5
 *    - second pass for complete drop-out control ( bitmap only )
 *    - variable precision
 *
 *   Re-entrancy is _not_ planned.
 *
 *   Changes between 1.1 and 1.2 :
 *
 *     - no more trace tables, now uses linked list to sort
 *       coordinates.
 *
 *     - reduced code size using function dispatch within a generic
 *       draw_sweep function.
 *
 *     - added variable precision for finer rendering at small ppems
 *
 *
 *   Note that its interface may change in the future.
 *
 ******************************************************************)

Unit TTRASTER;

interface

{$I TTCONFIG.INC}

{ $DEFINE TURNS}

uses
{$IFDEF VIRTUALPASCAL}
     Use32,
{$ENDIF}
     FreeType,
     TTTypes;

const

  Err_Ras_None             =  0;
  Err_Ras_NotIni           = -2;  (* Rasterizer not Initialized    *)
  Err_Ras_Overflow         = -3;  (* Profile Table Overflow        *)
  Err_Ras_Neg_H            = -4;  (* Negative Height encountered ! *)
  Err_Ras_Invalid          = -5;  (* Invalid value encountered !   *)
  Err_Ras_Invalid_Contours = -6;

  function Render_Glyph( var glyph  : TT_Outline;
                         var target : TT_Raster_Map ) : TError;

  (* Render one glyph in the target bitmap, using drop-out control *)
  (* mode 'scan'                                                   *)

  function Render_Gray_Glyph( var glyph   : TT_Outline;
                              var target  : TT_Raster_Map ) : TError;

  (* Render one gray-level glyph in the target pixmap              *)
  (* palette points to an array of 5 colors used for the rendering *)
  (* use nil to reuse the last palette. Default is VGA graylevels  *)

{$IFDEF SMOOTH}
  function Render_Smooth_Glyph( var glyph   : TGlyphRecord;
                                    target  : PRasterBlock;
                                    scan    : Byte;
                                    palette : pointer      ) : boolean;
{$ENDIF}

  procedure Set_High_Precision( High : boolean );
  (* Set rendering precision. Should be set to TRUE for small sizes only *)
  (* ( typically < 20 ppem )                                             *)

  procedure Set_Second_Pass( Pass : boolean );
  (* Set second pass flag *)

  function  TTRaster_Init : TError;
  procedure TTRaster_Done;

implementation

uses
     TTCalc,       { used for MulDiv }
     TTError
{$IFDEF DEBUG}
     ,GMain      { Used to access VRAM pointer VIO during DEBUG }
{$ENDIF}
     ;


{$DEFINE NO_ASM}

const
  Render_Pool_Size = 64000;
  Gray_Lines_Size  = 2048;

  MaxBezier  = 32;       (* Maximum number of stacked B‚ziers.    *)
                         (* Setting this constant to more than 32 *)
                         (* is a pure waste of space              *)

  Pixel_Bits = 6;        (* fractional bits of input coordinates  *)

  Cell_Bits  = 8;

type

  TEtats  = ( Indetermine, Ascendant, Descendant, Plat );

  PProfile = ^TProfile;
  TProfile = record
               Flow    : Int;       (* ascending or descending Profile *)
               Height  : Int;       (* Profile's height in scanlines   *)
               Start   : Int;       (* Profile's starting scanline     *)
               Offset  : ULong;     (* offset of first coordinate in   *)
                                    (* render pool                     *)

               Link    : PProfile;  (* link used in several cases      *)

               X       : Longint;   (* current coordinate during sweep *)
               CountL  : Int;       (* number of lines to step before  *)
                                    (* this Profile becomes drawable   *)

               next    : PProfile; (* next Profile of the same contour *)
             end;

  TBand = record
            Y_Min : Int;
            Y_Max : Int;
          end;

  (* Simple record used to implement a stack of bands, required *)
  (* by the sub-banding mechanism                               *)

const
  AlignProfileSize = ( sizeOf(TProfile) + 3 ) div 4;
  (* You may need to compute this according to your prefered alignement *)

  LMask : array[0..7] of Byte
        = ($FF,$7F,$3F,$1F,$0F,$07,$03,$01);

  RMask : array[0..7] of Byte
        = ($80,$C0,$E0,$F0,$F8,$FC,$FE,$FF);

  (* left and right fill bitmasks *)

type
  Function_Sweep_Init = procedure( var min, max : Int );

  Function_Sweep_Span = procedure( y     : Int;
                                   x1    : TT_F26dot6;
                                   x2    : TT_F26dot6;
                                   Left  : PProfile;
                                   Right : PProfile );

  Function_Sweep_Step = procedure;

  (* prototypes used for sweep function dispatch *)

  TPoint = record x, y : long; end;

  TBezierStack = array[0..32*2] of TPoint;
  PBezierStack = ^TBezierStack;

{$IFNDEF CONST_PREC}

var
  Precision_Bits   : Int;       (* Fractional bits of Raster coordinates *)
  Precision        : Int;
  Precision_Half   : Int;
  Precision_Step   : Int;       (* Bezier subdivision minimal step       *)
  Precision_Shift  : Int;       (* Shift used to convert coordinates     *)
  Precision_Mask   : Longint;   (* integer truncatoin mask               *)
  Precision_Jitter : Int;

{$ELSE}

const
  Precision_Bits   = 6;
  Precision        = 1 shl Precision_Bits;
  Precision_Half   = Precision div 2;
  Precision_Step   = Precision_Half;
  Precision_Shift  = 0;
  Precision_Mask   = -Precision;
  Precision_Jitter = 2;

{$ENDIF}

var
  Scale_Shift : Int;

  cProfile  : PProfile;  (* current Profile                *)
  fProfile  : PProfile;  (* head of Profiles linked list   *)
  oProfile  : PProfile;  (* old Profile                    *)
  gProfile  : PProfile;  (* last Profile in case of impact *)

  nProfs   : Int;        (* current number of Profiles *)

  Etat     : TEtats;     (* State of current trace *)

  Fresh    : Boolean;    (* Indicates a new Profile which 'Start' field *)
                         (* must be set                                 *)

  Joint    : Boolean;    (* Indicates that the last arc stopped sharp *)
                         (* on a scan-line. Important to get rid of   *)
                         (* doublets                                  *)

  Buff     : PStorage;   (* Profiles buffer a.k.a. Render Pool *)
  SizeBuff : ULong;      (* current render pool's size         *)
  MaxBuff  : ULong;      (* current render pool's top          *)
  profCur  : ULong;      (* current render pool cursor         *)

  Cible      : TT_Raster_Map; (* Description of target map *)

  BWidth     : integer;
  BCible     : PByte;   (* target bitmap buffer *)
  GCible     : PByte;   (* target pixmap buffer *)

  TraceOfs   : Int;     (* current offset in target bitmap      *)
  TraceIncr  : Int;     (* increment to next line in target map *)
  TraceG     : Int;     (* current offset in targer pixmap      *)

  gray_min_x : Int;     (* current min x during gray rendering *)
  gray_max_x : Int;     (* current max x during gray rendering *)

  (* Dispatch variables : *)

  Proc_Sweep_Init : Function_Sweep_Init;  (* Sweep initialisation *)
  Proc_Sweep_Span : Function_Sweep_Span;  (* Span drawing         *)
  Proc_Sweep_Drop : Function_Sweep_Span;  (* Drop out control     *)
  Proc_Sweep_Step : Function_Sweep_Step;  (* Sweep line step      *)

  Arcs     : TBezierStack;
  CurArc   : Int;             (* stack's top                 *)

  Points   : TT_Points;

  Flags    : PByte;           (* current flags array     *)
  Outs     : TT_PConStarts;   (* current endpoints array *)

  nPoints,            (* current number of points   *)
  nContours : Int;    (* current number of contours *)

  LastX,              (* Last and extrema coordinates during *)
  LastY,              (* rendering                           *)
  MinY,
  MaxY     : LongInt;

{$IFDEF TURNS}
  numTurns : Int;
{$ENDIF}

  DropOutControl : Byte;  (* current drop-out control mode *)

  Count_Table : array[0..255] of Word;
  (* Look-up table used to quickly count set bits in a gray 2x2 cell *)

  Count_Table2 : array[0..255] of Word;
  (* Look-up table used to quickly count set bits in a gray 2x2 cell *)

  Grays : array[0..4] of Byte;
  (* gray palette used during gray-levels rendering *)
  (* 0 : background .. 4 : foreground               *)

  Gray_Lines  : PByte;   { 2 intermediate bitmap lines         }
  Gray_Width  : integer; { width of the 'gray' lines in pixels }

{$IFDEF SMOOTH}
  Smooth_Cols : integer;
  Smooths : array[0..16] of Byte;
  (* smooth palette used during smooth-levels rendering *)
  (* 0 : background...16 : foreground                   *)

  smooth_pass : integer;
{$ENDIF}

  Second_Pass : boolean;
  (* indicates wether an horizontal pass should be performed  *)
  (* to control drop-out accurately when calling Render_Glyph *)
  (* Note that there is no horizontal pass during gray render *)

  (* better set it off at ppem >= 18                          *)

  Band_Stack : array[1..16] of TBand;
  Band_Top   : Int;



{$IFDEF DEBUG3}
(****************************************************************************)
(*                                                                          *)
(* Function:     Pset                                                       *)
(*                                                                          *)
(* Description:  Used only in the "DEBUG3" state.                           *)
(*                                                                          *)
(*               This procedure simply plots a point on the video screen    *)
(*               Note that it relies on the value of cProfile->start,       *)
(*               which may sometimes not be set yet when Pset is called.    *)
(*               This will usually result in a dot plotted on the first     *)
(*               screen scanline ( far away its original position ).        *)
(*                                                                          *)
(*               This "bug" means not that the current implementation is    *)
(*               buggy, as the bitmap will be rendered correctly, so don't  *)
(*               panic if you see 'flying' dots in debugging mode           *)
(*                                                                          *)
(*                                                                          *)
(* Input:        None                                                       *)
(*                                                                          *)
(* Returns:      Nada                                                       *)
(*                                                                          *)
(****************************************************************************)

procedure PSet;
var c  : byte;
    o  : Int;
    xz : LongInt;
begin
  xz := Buff^[profCur] div Precision;

  with cProfile^ do
   begin

    case Flow of
      TT_Flow_Up   : o := 80 * (profCur-Offset+Start) + xz div 8;
      TT_Flow_Down : o := 80 * (Start-profCur+offset) + xz div 8;
     end;

    if o > 0 then
     begin
      c := Vio^[o] or ( $80 shr ( xz and 7 ));
      Vio^[o] := c;
     end
   end;
end;

(****************************************************************************)
(*                                                                          *)
(* Function:    Clear_Band                                                  *)
(*                                                                          *)
(* Description: Clears a Band on screen during DEBUG3 rendering             *)
(*                                                                          *)
(* Input:       y1, y2   top and bottom of screen-wide band                 *)
(*                                                                          *)
(* Returns:     Nada.                                                       *)
(*                                                                          *)
(****************************************************************************)

procedure ClearBand( y1, y2 : Int );
var
  Y : Int;
  K : Word;
begin
  K := y1*80;
  FillChar( Vio^[k], (y2-y1+1)*80, 0 );
end;
{$ENDIF}

{$IFNDEF CONST_PREC}

(****************************************************************************)
(*                                                                          *)
(* Function:    Set_High_Precision                                          *)
(*                                                                          *)
(* Description: Sets precision variables according to param flag            *)
(*                                                                          *)
(* Input:       High     set to True for high precision ( typically for     *)
(*                       ppem < 18 ), false otherwise.                      *)
(*                                                                          *)
(****************************************************************************)

procedure Set_High_Precision( High : boolean );
begin
  if High then
    begin
      Precision_Bits   := 10;
      Precision_Step   := 128;
      Precision_Jitter := 24;
    end
  else
    begin
      Precision_Bits   := 6;
      Precision_Step   := 32;
      Precision_Jitter := 2;
    end;

  Precision       := 1 shl Precision_Bits;
  Precision_Half  := Precision shr 1;
  Precision_Shift := Precision_Bits - Pixel_Bits;
  Precision_Mask  := -Precision;
end;

{$ENDIF}

procedure Set_Second_Pass( Pass : boolean );
begin
  second_pass := pass;
end;


function TRUNC( x : Long ) : Long; {$IFDEF INLINE} inline; {$ENDIF}
begin
  Trunc := (x and -Precision) div Precision;
end;

function FRAC( x : Long ) : Int; {$IFDEF INLINE} inline; {$ENDIF}
begin
  Frac := x and (Precision-1);
end;

function FLOOR( x : Long ) : Long; {$IFDEF INLINE} inline; {$ENDIF}
begin
  Floor := x and -Precision;
end;

function CEILING( x : Long ) : Long; {$IFDEF INLINE} inline; {$ENDIF}
begin
  Ceiling := (x + Precision-1) and -Precision;
end;

function SCALED( x : Long ) : Long; {$IFDEF INLINE} inline; {$ENDIF}
begin
  SCALED := (x shl scale_shift) - precision_half;
end;

{$IFDEF USE32} (* speed things a bit on 32-bit systems *)
function MulDiv( a, b, c : Long ) : Long; {$IFDEF INLINE} inline; {$ENDIF}
begin
  MulDiv := a*b div c;
end;
{$ENDIF}

(****************************************************************************)
(*                                                                          *)
(* Function:    New_Profile                                                 *)
(*                                                                          *)
(* Description: Creates a new Profile in the render pool                    *)
(*                                                                          *)
(* Input:       AEtat    state/orientation of the new Profile               *)
(*                                                                          *)
(* Returns:     True on sucess                                              *)
(*              False in case of overflow or of incoherent Profile          *)
(*                                                                          *)
(****************************************************************************)

function New_Profile( AEtat : TEtats ) : boolean;
begin

  if fProfile = NIL then
    begin
      cProfile := PProfile( @Buff^[profCur] );
      fProfile := cProfile;
      inc( profCur, AlignProfileSize );
    end;

  if profCur >= MaxBuff then
    begin
      Error       := Err_Ras_Overflow;
      New_Profile := False;
      exit;
    end;

  with cProfile^ do
    begin

      Case AEtat of

        Ascendant  : Flow := TT_Flow_Up;
        Descendant : Flow := TT_Flow_Down;
      else
{$IFDEF DEBUG}
        Writeln('ERROR : Incoherent Profile' );
        Halt(30);
{$ELSE}
        New_Profile := False;
        Error       := Err_Ras_Invalid;
        exit;
{$ENDIF}
      end;

      Start   := 0;
      Height  := 0;
      Offset  := profCur;
      Link    := nil;
      next   := nil;
    end;

  if gProfile = nil then gProfile := cProfile;

  Etat  := AEtat;
  Fresh := True;
  Joint := False;

  New_Profile := True;
end;

{$IFDEF TURNS}
(****************************************************************************)
(*                                                                          *)
(* Function:    Insert_Y_Turn                                               *)
(*                                                                          *)
(* Description: Insert a slaient into the sorted list                       *)
(*                                                                          *)
(* Input:       new y turn                                                  *)
(*                                                                          *)
(****************************************************************************)

procedure  Insert_Y_Turn( y : Int );
var
  y_turns  : PStorage;
  y2, n    : Int;
begin
  n       := numTurns-1;
  y_turns := @Buff^[SizeBuff-numTurns];

  (* look for first y value that is <= *)
  while (n >= 0) and (y < y_turns^[n]) do dec(n);

  (* if it is <, simply insert it, ignor if we found one == *)

  if (n >= 0) and (y > y_turns^[n]) then
    while (n >= 0) do
    begin
      y2 := y_turns^[n];
      y_turns^[n] := y;
      y := y2;
      dec( n );
    end;

  if (n < 0) then
    begin
      dec( MaxBuff  );
      inc( numTurns );
      Buff^[SizeBuff-numTurns] := y;
    end
end;
{$ENDIF}


(****************************************************************************)
(*                                                                          *)
(* Function:    End_Profile                                                 *)
(*                                                                          *)
(* Description: Finalizes the current Profile.                              *)
(*                                                                          *)
(* Input:       None                                                        *)
(*                                                                          *)
(* Returns:     True on success                                             *)
(*              False on overflow or incoherency.                           *)
(*                                                                          *)
(****************************************************************************)

function End_Profile : boolean;
var
  H          : Int;
  oldProfile : PProfile;
begin
  H := profCur - cProfile^.Offset;

  if H < 0 then
    begin
      End_Profile := False;
      Error       := Err_Ras_Neg_H;
      exit;
    end;

  if H > 0 then
    begin
      oldProfile       := cProfile;
      cProfile^.Height := H;
      cProfile         := PProfile( @Buff^[profCur] );

      inc( profCur, AlignProfileSize );

      cProfile^.Height  := 0;
      cProfile^.Offset  := profCur;
      oldProfile^.next := cProfile;
      inc( nProfs );
    end;

  if profCur >= MaxBuff then
    begin
      End_Profile := False;
      Error       := Err_Ras_Overflow;
      exit;
    end;

  Joint := False;

  End_Profile := True;
end;

(****************************************************************************)
(*                                                                          *)
(* Function:    Finalize_Profile_Table                                      *)
(*                                                                          *)
(* Description: Adjusts all links in the Profiles list                      *)
(*                                                                          *)
(* Input:       None                                                        *)
(*                                                                          *)
(* Returns:     Nada                                                        *)
(*                                                                          *)
(****************************************************************************)

procedure Finalize_Profile_Table;
var
  n : int;
  p : PProfile;

  Bottom, Top : Int;
begin

  n := nProfs;

  if n > 1 then
    begin

      P := fProfile;

      while n > 0 do with P^ do
        begin
          if n > 1 then
            Link := PProfile( @Buff^[ Offset + Height ] )
          else
            Link := nil;

          with P^ do
            Case Flow of

              TT_Flow_Up : begin
                            Bottom := Start;
                            Top    := Start+Height-1;
                           end;

              TT_Flow_Down : begin
                              Bottom := Start-Height+1;
                              Top    := Start;

                              Start  := Bottom;
                              Offset := Offset+Height-1;
                             end;
             end;
{$IFDEF TURNS}
          Insert_Y_Turn( Bottom );
          Insert_Y_Turn( Top+1 );
{$ENDIF}
          P := Link;

          dec( n );
        end;
    end
  else
    fProfile := nil;

end;

(****************************************************************************)
(*                                                                          *)
(* Function:    Split_Bezier                                                *)
(*                                                                          *)
(* Description: Subdivises one Bezier arc into two joint                    *)
(*              sub-arcs in the Bezier stack.                               *)
(*                                                                          *)
(* Input:       None ( subdivised bezier is taken from the top of the       *)
(*              stack )                                                     *)
(*                                                                          *)
(* Returns:     Nada                                                        *)
(*                                                                          *)
(****************************************************************************)

procedure Split_Bezier( base : PBezierStack );
var
  arc  : PBezierStack;
  a, b : Long;
begin
{$IFNDEF NO_ASM}
  asm
    push esi
    push ebx
    push ecx

    mov  esi, base

    mov  eax, [esi+2*8]       (* arc^[4].x := arc^[2].x *)
    mov  ebx, [esi+1*8]       (* b := arc^[1].x *)
    mov  ecx, [esi+0*8]       (* b := (arc^[0].x+b) div 2 *)

    mov  [esi+4*8], eax

    add  eax, ebx             (* a := (arc^[2].x+b) div 2 *)
    add  ebx, ecx
    mov  edx, eax
    mov  ecx, ebx
    sar  edx, 31
    sar  ecx, 31
    sub  eax, edx
    sub  ebx, ecx
    sar  eax, 1
    sar  ebx, 1

    mov  [esi+3*8], eax       (* arc^[3].x := a *)
    mov  [esi+1*8], ebx

    add  eax, ebx             (* arc[2].x := (a+b) div 2 *)
    mov  edx, eax
    sar  edx, 31
    sub  eax, edx
    sar  eax, 1
    mov  [esi+2*8], eax

    add  esi, 4

    mov  eax, [esi+2*8]       (* arc^[4].x := arc^[2].x *)
    mov  ebx, [esi+1*8]       (* b := arc^[1].x *)
    mov  ecx, [esi+0*8]       (* b := (arc^[0].x+b) div 2 *)

    mov  [esi+4*8], eax

    add  eax, ebx             (* a := (arc^[2].x+b) div 2 *)
    add  ebx, ecx
    mov  edx, eax
    mov  ecx, ebx
    sar  edx, 31
    sar  ecx, 31
    sub  eax, edx
    sub  ebx, ecx
    sar  eax, 1
    sar  ebx, 1

    mov  [esi+3*8], eax       (* arc^[3].x := a *)
    mov  [esi+1*8], ebx

    add  eax, ebx             (* arc[2].x := (a+b) div 2 *)
    mov  edx, eax
    sar  edx, 31
    sub  eax, edx
    sar  eax, 1
    mov  [esi+2*8], eax

    pop ecx
    pop ebx
    pop esi
  end;
{$ELSE}
  arc := base;

  arc^[4].x := arc^[2].x;
  b := arc^[1].x;
  a := (arc^[2].x + b) div 2; arc^[3].x := a;
  b := (arc^[0].x + b) div 2; arc^[1].x := b;
  arc^[2].x := (a+b) div 2;

  arc^[4].y := arc^[2].y;
  b := arc^[1].y;
  a := (arc^[2].y + b) div 2; arc^[3].y := a;
  b := (arc^[0].y + b) div 2; arc^[1].y := b;
  arc^[2].y := (a+b) div 2;
{$ENDIF}
end;

(****************************************************************************)
(*                                                                          *)
(* Function:    Push_Bezier                                                 *)
(*                                                                          *)
(* Description: Clears the Bezier stack and pushes a new Arc on top of it.  *)
(*                                                                          *)
(* Input:       x1,y1 x2,y2 x3,y3  new Bezier arc                           *)
(*                                                                          *)
(* Returns:     nada                                                        *)
(*                                                                          *)
(****************************************************************************)

procedure PushBezier( x1, y1, x2, y2, x3, y3 : LongInt );
begin
  curArc:=0;

  with Arcs[CurArc+2] do begin x:=x1; y:=y1; end;
  with Arcs[CurArc+1] do begin x:=x2; y:=y2; end;
  with Arcs[ CurArc ] do begin x:=x3; y:=y3; end;
end;

(****************************************************************************)
(*                                                                          *)
(* Function:    Line_Up                                                     *)
(*                                                                          *)
(* Description: Compute the x-coordinates of an ascending line segment      *)
(*              and stores them in the render pool.                         *)
(*                                                                          *)
(* Input:       x1,y1 x2,y2  Segment start (x1,y1) and end (x2,y2) points   *)
(*                                                                          *)
(* Returns:     True on success                                             *)
(*              False if Render Pool overflow.                              *)
(*                                                                          *)
(****************************************************************************)

function Line_Up( x1, y1, x2, y2, miny, maxy : LongInt ) : boolean;
var
  Dx, Dy               : LongInt;
  e1, e2, f1, f2, size : Int;
  Ix, Rx, Ax           : LongInt;
  top                  : PStorage;
begin
  Line_Up := True;

  Dx := x2-x1; Dy := y2-y1;

  if (Dy <= 0) or (y2 < MinY) or (y1 > MaxY) then exit;

  if y1 < MinY then
   begin
    x1 := x1 + MulDiv( Dx, MinY-y1, Dy );
    e1 := Trunc(MinY);
    f1 := 0;
   end
  else
   begin
    e1 := Trunc(y1);
    f1 := Frac(y1);
   end;

  if y2 > MaxY then
   begin
    (* x2 := x2 + MulDiv( Dx, MaxY-y2, Dy ); *)
    e2 := Trunc(MaxY);
    f2 := 0;
   end
  else
   begin
    e2 := Trunc(y2);
    f2 := Frac(y2);
   end;

  if f1 > 0 then
    if e1 = e2 then exit
    else
      begin
        inc( x1, MulDiv( Dx, precision-f1, Dy ) );
        inc( e1 );
      end
  else
    if Joint then
      dec( profCur );

  Joint := (f2 = 0);

  (* Indicates that the segment stopped sharp on a ScanLine *)

  if Fresh then
   begin
    cProfile^.Start := e1;
    Fresh           := False;
   end;

  size := ( e2-e1 )+1;
  if ( profCur + size >= MaxBuff ) then
   begin
     Line_Up := False;
     Error   := Err_Ras_Overflow;
     exit;
   end;

  if Dx > 0 then
    begin
      Ix := (Precision*Dx) div Dy;
      Rx := (Precision*Dx) mod Dy;
      Dx := 1;
    end
  else
    begin
      Ix := -((Precision*-Dx) div Dy);
      Rx :=   (Precision*-Dx) mod Dy;
      Dx := -1;
    end;

  Ax  := -Dy;
  {top := @Buff^[profCur];}

  while size > 0 do
  begin
    Buff^[profCur] := x1;
    {$IFDEF DEBUG3} Pset; {$ENDIF}
    inc( profCur );
    {top := @top^[1];}

    inc( x1, Ix );
    inc( ax, rx );

    if ax >= 0 then
    begin
      dec( ax, dy );
      inc( x1, dx );
    end;

    dec( size );
  end;

end;

(****************************************************************************)
(*                                                                          *)
(* Function:    Line_Down                                                   *)
(*                                                                          *)
(* Description: Compute the x-coordinates of a descending line segment      *)
(*              and stores them in the render pool.                         *)
(*                                                                          *)
(* Input:       x1,y1 x2,y2  Segment start (x1,y1) and end (x2,y2) points   *)
(*                                                                          *)
(* Returns:     True on success                                             *)
(*              False if Render Pool overflow.                              *)
(*                                                                          *)
(****************************************************************************)

function Line_Down( x1, y1, x2, y2, miny, maxy : LongInt ): boolean;
var
  _fresh : Boolean;
begin
  _fresh  := fresh;

  Line_Down := Line_Up( x1, -y1, x2, -y2, -maxy, -miny );

  if _fresh and not fresh then
    cProfile^.start := -cProfile^.start;
end;

(****************************************************************************)
(*                                                                          *)
(* Function:    Bezier_Up                                                   *)
(*                                                                          *)
(* Description: Compute the x-coordinates of an ascending bezier arc        *)
(*              and stores them in the render pool.                         *)
(*                                                                          *)
(* Input:       None.The arc is taken from the top of the Bezier stack.     *)
(*                                                                          *)
(* Returns:     True on success                                             *)
(*              False if Render Pool overflow.                              *)
(*                                                                          *)
(****************************************************************************)

function Bezier_Up( miny, maxy : Long ) : boolean;
var
  x1, y1, x2, y2, e, e2, e0 : LongInt;
  carc, debArc, f1          : Int;
  base                      : PBezierStack;
label
  Fin;
begin
  Bezier_Up := True;

  carc := curArc;
  base := @Arcs[cArc];
  y1   := base^[2].y;
  y2   := base^[0].y;

  if ( y2 < MinY ) or ( y1 > MaxY ) then
    goto Fin;

  e2 := FLOOR(y2);

  if e2 > MaxY then e2 := MaxY;

  e0 := MinY;

  if y1 < MinY then
    e := MinY
  else
   begin
    e  := CEILING(y1);
    f1 := FRAC(y1);
    e0 := e;

    if f1 = 0 then
     begin

      if Joint then begin dec(profCur); Joint:=False; end;
      (* ^ Ce test permet d'‚viter les doublons *)

      Buff^[profCur] := base^[2].x;
      {$IFDEF DEBUG3} Pset; {$ENDIF}
      inc( profCur );
      inc( e, Precision );
     end
   end;

  if Fresh then
   begin
    cProfile^.Start := TRUNC(e0);
    Fresh := False;
   end;

  if e2 < e then
    goto Fin;

  (* overflow ? *)
  if ( profCur + TRUNC(e2-e)+ 1 >= MaxBuff ) then
    begin
      Bezier_Up := False;
      Error     := Err_Ras_Overflow;
      exit;
    end;

  debArc := cArc;

  while ( cArc >= debArc ) and ( e <= e2 ) do
   begin
    Joint := False;
    y2    := base^[0].y;

    if y2 > e then
      begin
        y1 := base^[2].y;
        if ( y2-y1 >= precision_step ) then
          begin
            Split_Bezier( base );
            inc( cArc, 2 );
            base := @base^[2];
          end
        else
          begin
            Buff^[profCur] := base^[2].x +
                              MulDiv( base^[0].x - base^[2].x,
                                      e - y1,
                                      y2 - y1 );

            {$IFDEF DEBUG3} Pset; {$ENDIF}

            inc( profCur );
            dec( cArc, 2 );
            base := @Arcs[cArc];
            inc( e, Precision );
          end;
      end
    else
      begin
        if y2 = e then
        begin
          joint := True;
          Buff^[profCur] := Arcs[cArc].x;
          {$IFDEF DEBUG3} Pset; {$ENDIF}
          inc( profCur );
          inc( e, Precision );
        end;
        dec( cArc, 2 );
        base := @Arcs[cArc];
      end
  end;

Fin:
  dec( curArc, 2);
  exit;
end;


(****************************************************************************)
(*                                                                          *)
(* Function:    Bezier_Down                                                 *)
(*                                                                          *)
(* Description: Compute the x-coordinates of a descending bezier arc        *)
(*              and stores them in the render pool.                         *)
(*                                                                          *)
(* Input:       None. Arc is taken from the top of the Bezier stack.        *)
(*                                                                          *)
(* Returns:     True on success                                             *)
(*              False if Render Pool overflow.                              *)
(*                                                                          *)
(****************************************************************************)

function Bezier_Down( miny, maxy : Long ) : boolean;
var
  base   : PBezierStack;
  _fresh : Boolean;
begin
  _fresh := fresh;
  base   := @Arcs[curArc];

  base^[0].y := -base^[0].y;
  base^[1].y := -base^[1].y;
  base^[2].y := -base^[2].y;

  Bezier_Down := Bezier_Up( -maxy, -miny );

  if _fresh and not fresh then
    cProfile^.start := -cProfile^.start;

  base^[0].y := -base^[0].y;
end;

(****************************************************************************)
(*                                                                          *)
(* Function:    Line_To                                                     *)
(*                                                                          *)
(* Description: Injects a new line segment and adjust Profiles list.        *)
(*                                                                          *)
(* Input:       x, y : segment endpoint ( start point in LastX,LastY )      *)
(*                                                                          *)
(* Returns:     True on success                                             *)
(*              False if Render Pool overflow or Incorrect Profile          *)
(*                                                                          *)
(****************************************************************************)

function Line_To( x, y : LongInt ) : boolean;
begin
  Line_To := False;

  case Etat of

    Indetermine : if y > lastY then
                    if not New_Profile( Ascendant ) then exit else
                  else
                   if y < lastY then
                    if not New_Profile( Descendant ) then exit;

    Ascendant   : if y < lastY then
                    if not End_Profile or
                       not New_Profile( Descendant ) then exit;

    Descendant  : if y > LastY then
                    if not End_Profile or
                       not New_Profile( Ascendant ) then exit;
   end;

  Case Etat of
    Ascendant  : if not Line_Up  ( LastX, LastY, X, Y, miny, maxy ) then exit;
    Descendant : if not Line_Down( LastX, LastY, X, Y, miny, maxy ) then exit;
   end;

  LastX := x;
  LastY := y;

  Line_To := True;
end;

(****************************************************************************)
(*                                                                          *)
(* Function:    Bezier_State                                                *)
(*                                                                          *)
(* Description: Determines the state (ascending/descending/flat/undet)      *)
(*              of a Bezier arc, along one given axis.                      *)
(*                                                                          *)
(* Input:       y1, y2, y3 : coordinates of the Bezier arc.                 *)
(*                           along the concerned axis.                      *)
(*                                                                          *)
(* Returns:     State, i.e. Ascending, Descending, Flat or Undetermined     *)
(*                                                                          *)
(****************************************************************************)


function Bezier_State( y1, y2, y3 : TT_F26Dot6 ) : TEtats;
begin
  (* determine orientation of a Bezier arc *)
  if y1 = y2 then

    if y2 = y3 then Bezier_State := Plat
    else
    if y2 > y3 then Bezier_State := Descendant
    else
                    Bezier_State := Ascendant
  else
  if y1 > y2 then

    if y2 >= y3 then Bezier_State := Descendant
    else
                     Bezier_State := Indetermine
  else

    if y2 <= y3 then Bezier_State := Ascendant
    else
                     Bezier_State := Indetermine;
end;


(****************************************************************************)
(*                                                                          *)
(* Function:    Bezier_To                                                   *)
(*                                                                          *)
(* Description: Injects a new bezier arc and adjust Profiles list.          *)
(*                                                                          *)
(* Input:       x,   y : arc endpoint ( start point in LastX, LastY )       *)
(*              Cx, Cy : control point                                      *)
(*                                                                          *)
(* Returns:     True on success                                             *)
(*              False if Render Pool overflow or Incorrect Profile          *)
(*                                                                          *)
(****************************************************************************)


function Bezier_To( x, y, Cx, Cy : LongInt ) : boolean;
var
  y3, x3   : LongInt;
  Etat_Bez : TEtats;
begin
  Bezier_To := False;

  PushBezier( LastX, LastY, Cx, Cy, X, Y );

  while ( curArc >= 0 ) do
   begin
    y3 := Arcs[curArc].y;
    x3 := Arcs[curArc].x;

    Etat_Bez := Bezier_State( Arcs[curArc+2].y, Arcs[curArc+1].y, y3 );

    case Etat_Bez of

      Plat        : dec( curArc, 2 );

      Indetermine : begin
                      Split_Bezier( @Arcs[curArc] );
                      inc( curArc, 2 );
                    end;
    else
      if Etat <> Etat_Bez then
      begin
        if Etat <> Indetermine then
            if not End_Profile then exit;

        if not New_Profile( Etat_Bez ) then exit;
      end;

      case Etat of
        Ascendant  : if not Bezier_Up( miny, maxy ) then exit;
        Descendant : if not Bezier_Down( miny, maxy ) then exit;
      end;

    end;
  end;

  LastX := x3;
  LastY := y3;

  Bezier_To := True;
end;

(****************************************************************************)
(*                                                                          *)
(* Function:    DecomposeCurve                                              *)
(*                                                                          *)
(* Description: This functions scans the outline arrays in order to         *)
(*              emit individual segments and beziers by calling the         *)
(*              functions Line_To and Bezier_To. It handles all weird       *)
(*              cases, like when the first point is off the curve, or       *)
(*              when there are simply no "on" points in the contour !       *)
(*                                                                          *)
(* Input:       xCoord, yCoord : array coordinates to use.                  *)
(*              first,  last   : indexes of first and last point in         *)
(*                               contour.                                   *)
(*                                                                          *)
(* Returns:     True on success                                             *)
(*              False if case of error.                                     *)
(*                                                                          *)
(* Notes:       The function assumes that 'first' < 'last'                  *)
(*                                                                          *)
(****************************************************************************)

procedure swap( var x, y : Long );  {$IFDEF INLINE} inline; {$ENDIF}
var
  s : Long;
begin
  s := x; x := y; y := s;
end;

function DecomposeCurve( first,  last   : Int;
                         flipped        : Boolean        ) : boolean;
var
  index : Int;

  x, y   : Long;    (* current point                *)
  cx, cy : Long;    (* current Bezier control point *)
  mx, my : Long;    (* middle point                 *)

  x_first, y_first : Long;    (* first point coordinates *)
  x_last,  y_last  : Long;    (* last point coordinates  *)

  on_curve : Boolean;
begin

  DecomposeCurve := False;

(* the following code is miscompiled by Virtual Pascal 1.1 *)
(* although it works OK with 2.0, strange...               *)
(*
  with points^[first] do
  begin
    x_first := SCALED( x );
    y_first := SCALED( y );
  end;
*)
  x_first := SCALED( points^[first].x );
  y_first := SCALED( points^[first].y );

  if flipped then  swap( x_first, y_first );

  with points^[last] do
  begin
    x_last  := SCALED( x );
    y_last  := SCALED( y );
  end;

  if flipped then  swap( x_last, y_last );

  LastX := x_first;  cx := x_first;
  LastY := y_first;  cy := y_first;

  index    := first;
  on_curve := Flags^[first] and 1 <> 0;

  (* check first point, and set origin *)
  if not on_curve then
  begin
    (* first point is off the curve - yes, this happens !! *)

    if Flags^[last] and 1 <> 0 then
      begin
        LastX := x_last;  (* start at last point if it is *)
        LastY := y_last;  (* on the curve                 *)
      end
    else
      begin
        LastX := (LastX + x_last) div 2; (* if both first and last point    *)
        LastY := (LastY + y_last) div 2; (* are off the curve, start midway *)

        (* record midpoint in x_last,y_last *)
        x_last := LastX;
        y_last := LastY;
      end;
  end;

  (* now process each contour point *)
  while ( index < last ) do
  begin
    inc( index );

    x := SCALED( points^[index].x );
    y := SCALED( points^[index].y );

    if flipped then swap( x, y );

    if on_curve then
      begin
        (* the previous point was on the curve *)

        on_curve := Flags^[index] and 1 <> 0;
        if on_curve then
          begin
            (* two successive on points -> emit segment *)
            if not Line_To( x, y ) then exit;
          end
        else
          begin
            (* else, keep current point as control for next bezier *)
            cx := x;
            cy := y;
          end;
      end
    else
      begin
        (* the previous point was off the curve *)

        on_curve := Flags^[index] and 1 <> 0;
        if on_curve then
          begin
            (* reaching on point -> emit Bezier *)
            if not Bezier_To( x, y, cx, cy ) then exit;
          end
        else
          begin
            (* two successive off points -> create middle point *)
            (* then emit Bezier                                 *)
            mx := (cx + x) div 2;
            my := (cy + y) div 2;

            if not Bezier_To( mx, my, cx, cy ) then exit;

            cx := x;
            cy := y;
          end;
      end;
  end;

  (* end of contour, close curve cleanly *)
  if ( Flags^[first] and 1 <> 0 ) then

    if on_curve then
      if not Line_To( x_first, y_first ) then exit else
    else
      if not Bezier_To( x_first, y_first, cx, cy ) then exit else

  else
    if not on_curve then
      if not Bezier_To( x_last, y_last, cx, cy ) then exit;

  DecomposeCurve := True;
end;

(****************************************************************************)
(*                                                                          *)
(* Function:    Convert_Glyph                                               *)
(*                                                                          *)
(* Description: Converts a glyph into a series of segments and arcs         *)
(*              and make a Profiles list with them.                         *)
(*                                                                          *)
(* Input:       _xCoord, _yCoord : coordinates tables.                      *)
(*                                                                          *)
(*              Uses the 'Flag' table too.                                  *)
(*                                                                          *)
(* Returns:     True on success                                             *)
(*              False if any error was encountered during render.           *)
(*                                                                          *)
(****************************************************************************)

Function Convert_Glyph( flipped : Boolean ) : boolean;
var
  i, j, First, Last, Start : Int;

  y1, y2, y3 : LongInt;

  lastProfile : PProfile;

begin
  Convert_Glyph := False;

  j        := 0;
  fProfile := NIL;
  Joint    := False;
  Fresh    := False;

  MaxBuff  := SizeBuff - AlignProfileSize;

{$IFDEF TURNS}
  numTurns := 0;
{$ENDIF}

  cProfile         := PProfile( @Buff^[profCur] );
  cProfile^.Offset := profCur;
  nProfs           := 0;

  for i := 0 to nContours-1 do
  begin

    Etat     := Indetermine;
    gProfile := nil;

    (* decompose a single contour into individual segments and *)
    (* beziers                                                 *)

    if not DecomposeCurve( j, outs^[i], flipped ) then exit;
    j := outs^[i] + 1;

    (* We _must_ take care of the case when the first and last arcs join  *)
    (* while having the same orientation                                  *)

    if ( Frac(lastY) = 0 ) and
       ( lastY >= MinY ) and
       ( lastY <= MaxY ) then

      if ( gProfile <> nil ) and                   (* gProfile can be nil   *)
         ( gProfile^.Flow = cProfile^.Flow ) then  (* if the contour was    *)
                                                   (* too small to be drawn *)
           dec( profCur );

    lastProfile := cProfile;

    if not End_Profile then exit;

    if gProfile <> nil then lastProfile^.next := gProfile;

  end;

  Finalize_Profile_Table;

  Convert_Glyph := (profCur < MaxBuff);
end;


  (************************************************)
  (*                                              *)
  (*  Init_Linked                                 *)
  (*                                              *)
  (*    Init an empty linked list.                *)
  (*                                              *)
  (************************************************)

  procedure Init_Linked( var L : PProfile );
  begin
    L := nil;
  end;

  (************************************************)
  (*                                              *)
  (*  InsNew :                                    *)
  (*                                              *)
  (*    Inserts a new Profile in a linked list.   *)
  (*                                              *)
  (************************************************)

  procedure InsNew( var List : PProfile;
                    Profile  : PProfile  );
  var
    current : PProfile;
    old     : ^PProfile;
    x       : Long;
  label
    Place;
  begin

    old     := @list;
    current := old^;
    x       := profile^.x;

    while current <> nil do
    begin
      if x < current^.x then
        goto Place;

      old     := @current^.link;
      current := old^;
    end;

  Place:
    profile^.link := current;
    old^          := profile;
  end;

  (************************************************)
  (*                                              *)
  (*  DelOld :                                    *)
  (*                                              *)
  (*    Removes an old Profile from a linked list *)
  (*                                              *)
  (************************************************)


  procedure DelOld( var List : PProfile;
                    Profile  : PProfile );
  var
    current : PProfile;
    old     : ^PProfile;

  begin

    old     := @list;
    current := old^;

    while current <> nil do
    begin
      if current = profile then
        begin
          old^ := current^.link;
          exit;
        end;

      old     := @current^.link;
      current := old^;
    end;

    {$IFDEF ASSERT}
    Writeln('(Raster:DelOld) Incoherent deletion');
    halt(9);
    {$ENDIF}
  end;


{$IFDEF TURNS}
  (************************************************)
  (*                                              *)
  (*  Update:                                     *)
  (*                                              *)
  (*    Update all X offsets in a drawing list    *)
  (*                                              *)
  (************************************************)

  procedure Update( var List : PProfile );
  var
    current : PProfile;
  begin
    (* recompute coordinates *)
    current := list;

    while current <> nil do with current^ do
    begin
      X := Buff^[offset];
      inc( offset, flow );
      dec( height );
      current := link;
    end;
  end;
{$ENDIF}

  (************************************************)
  (*                                              *)
  (*  Sort :                                      *)
  (*                                              *)
  (*    Sorts 'quickly' (??) a trace list.        *)
  (*                                              *)
  (************************************************)

  procedure Sort( var List : PProfile );
  var
    current, next : PProfile;
    old           : ^PProfile;
  begin

    (* First, recompute coordinates *)

    current := list;

    while current <> nil do with current^ do
    begin
      X := Buff^[offset];
      inc( offset, flow );
      dec( height );
      current := link;
    end;

    (* Then, do the sort *)

    old     := @list;
    current := old^;

    if current = nil then
      exit;

    next := current^.link;

    while next <> nil do
    begin
      if current^.x <= next^.x then
        begin
          old     := @current^.link;
          current := old^;

          if current = nil then
            exit;
        end
      else
        begin
          old^          := next;
          current^.link := next^.link;
          next^.link    := current;

          old     := @list;
          current := old^;
        end;

      next := current^.link;
    end;

  end;

{$IFDEF TURNS}

(********************************************************************)
(*                                                                  *)
(*  Generic Sweep Drawing routine                                   *)
(*                                                                  *)
(*                                                                  *)
(*                                                                  *)
(********************************************************************)

function Draw_Sweep : boolean;

label
  Scan_DropOuts,
  Next_Line,
  Skip_To_Next;

var
  y, k,
  I, J   : Int;
  P, Q   : PProfile;

  Top,
  Bottom,
  y_height,
  y_change,
  min_Y,
  max_Y  : Int;

  x1, x2, xs, e1, e2 : LongInt;

  Wait  : PProfile;

  Draw_Left  : PProfile;
  Draw_Right : PProfile;

  Drop_Left  : PProfile;
  Drop_Right : PProfile;

  P_Left,  Q_Left  : PProfile;
  P_Right, Q_Right : PProfile;

  Phase     : Int;
  dropouts  : Int;

begin

  Draw_Sweep := False;

  (* Init the empty linked lists *)

  Init_Linked( Wait );

  Init_Linked( Draw_Left  );
  Init_Linked( Draw_Right );

  Init_Linked( Drop_Left  );
  Init_Linked( Drop_Right );

  (* First, compute min Y and max Y *)

  P     := fProfile;
  max_Y := TRUNC(MinY);
  min_Y := TRUNC(MaxY);

  while P <> nil do
   with P^ do
    begin
     Q := P^.Link;

     Bottom := P^.Start;
     Top    := Bottom + P^.Height-1;

     if min_Y > Bottom then min_Y := Bottom;
     if max_Y < Top    then max_Y := Top;

     X := 0;
     InsNew( Wait, P );

     P := Q;
   end;

  (* Check the y-turns *)
  if (numTurns = 0) then
    begin
      Error := Err_Ras_Invalid;
      exit;
    end;

  (* Now inits the sweeps *)

  Proc_Sweep_Init( min_Y, max_Y );

  (* Then compute the distance of each Profile to min Y *)

  P := Wait;
  while P <> nil do
  begin
    with P^ do CountL := (Start-min_Y);
    P := P^.link;;
  end;

  (* Let's go *)

  y        := min_y;
  y_height := 0;

  if ( numTurns > 0 ) and
     ( Buff^[sizeBuff-numTurns] = min_y ) then
    dec( numTurns );

  while numTurns > 0 do
  begin
    (* Look in the wait list for new activations *)

    P := Wait;
    while P <> nil do with P^ do
    begin
      Q := link;

      dec( CountL, y_height );
      if CountL = 0 then
        begin
          DelOld( Wait, P );
          case Flow of
            TT_Flow_Up   : InsNew( Draw_Left, P );
            TT_Flow_Down : InsNew( Draw_Right, P );
          end
        end;

      P := Q;
    end;

    (* Sort the drawing lists *)

    Sort( Draw_Left );
    Sort( Draw_Right );

    y_change := Buff^[sizebuff-numTurns];
    dec( numTurns );

    y_height := y_change - y;

    while y < y_change do
    begin

      (* Let's trace *)

      dropouts  := 0;

      P_Left  := Draw_Left;
      P_Right := Draw_Right;

      while ( P_Left <> nil ) do
      begin

        {$IFDEF ASSERT}
        if P_Right = nil then
          Halt(13);
        {$ENDIF}

        x1 := P_Left^ .X;
        x2 := P_Right^.X;

        if x1 > x2 then
          begin
            xs := x1;
            x1 := x2;
            x2 := xs;
          end;

        if ( x2-x1 <= Precision ) then
          begin
            e1 := ( x1+Precision-1 ) and Precision_Mask;
            e2 := x2 and Precision_Mask;

            if (dropOutControl <> 0) and
               ((e1 > e2) or (e2 = e1 + Precision)) then
            begin
              P_Left ^.x := x1;
              P_Right^.x := x2;

              inc( dropouts );

              (* mark profile for drop-out control *)
              P_Left^.CountL := 1;

              goto Skip_To_Next;
            end
          end;

        Proc_Sweep_Span( y, x1, x2, P_Left, P_Right );

    Skip_To_Next:

        P_Left  := P_Left ^.Link;
        P_Right := P_Right^.Link;
      end;

      {$IFDEF ASSERT}
      if P_Right <> nil then
        Halt(10);
      {$ENDIF}

      (* Now perform the dropouts only _after_ the span drawing *)
      if (dropouts > 0) then
        goto Scan_DropOuts;

Next_Line:

      (* Step to next line *)
      Proc_Sweep_Step;

      inc(y);

      if y < y_change then
      begin
        Update( Draw_Left  );
        Update( Draw_Right );
      end

    end;

    (* We finalize the Profiles that need it *)

    P := Draw_Left;
    while P <> nil do
    begin
      Q := P^.Link;
      if P^.height = 0 then
          DelOld( Draw_Left, P  );
      P := Q;
    end;

    P := Draw_Right;
    while P <> nil do
    begin
      Q := P^.Link;
      if P^.height = 0 then
          DelOld( Draw_Right, P  );
      P := Q;
    end;

  end;

  while y <= max_y do
  begin
    Proc_Sweep_Step;
    inc( y );
  end;

  Draw_Sweep := True;
  exit;

Scan_DropOuts :
  P_Left  := Draw_Left;
  P_Right := Draw_Right;

  while (P_Left <> nil) do
  begin
    if P_Left^.countL <> 0 then
      begin
        P_Left^.countL := 0;
        Proc_Sweep_Drop( y, P_Left^.x, P_Right^.x, P_Left, P_Right );
      end;

    P_Left  := P_Left^.link;
    P_Right := P_Right^.Link;
  end;

  goto Next_Line;
end;


{$ELSE}

(********************************************************************)
(*                                                                  *)
(*  Generic Sweep Drawing routine                                   *)
(*                                                                  *)
(*                                                                  *)
(*                                                                  *)
(********************************************************************)

function Draw_Sweep : boolean;

label
  Skip_To_Next;

var
  y, k,
  I, J   : Int;
  P, Q   : PProfile;

  Top,
  Bottom,
  min_Y,
  max_Y  : Int;

  x1, x2, xs, e1, e2 : LongInt;

  Wait  : PProfile;

  Draw_Left  : PProfile;
  Draw_Right : PProfile;

  Drop_Left  : PProfile;
  Drop_Right : PProfile;

  P_Left,  Q_Left  : PProfile;
  P_Right, Q_Right : PProfile;

  Phase     : Int;
  dropouts  : Int;

begin

  Draw_Sweep := False;

  (* Init the empty linked lists *)

  Init_Linked( Wait );

  Init_Linked( Draw_Left  );
  Init_Linked( Draw_Right );

  Init_Linked( Drop_Left  );
  Init_Linked( Drop_Right );

  (* First, compute min Y and max Y *)

  P     := fProfile;
  max_Y := TRUNC(MinY);
  min_Y := TRUNC(MaxY);

  while P <> nil do
   with P^ do
    begin
     Q := P^.Link;

     Bottom := P^.Start;
     Top    := Bottom + P^.Height-1;

     if min_Y > Bottom then min_Y := Bottom;
     if max_Y < Top    then max_Y := Top;

     X := 0;
     InsNew( Wait, P );

     P := Q;
   end;

  (* Now inits the sweeps *)

  Proc_Sweep_Init( min_Y, max_Y );

  (* Then compute the distance of each Profile to min Y *)

  P := Wait;
  while P <> nil do
  begin
    with P^ do CountL := (Start-min_Y);
    P := P^.link;;
  end;

  (* Let's go *)

  for y := min_Y to max_Y do
  begin

    (* Look in the wait list for new activations *)

    P := Wait;
    while P <> nil do with P^ do
    begin
      Q := link;

      if CountL = 0 then
        begin
          DelOld( Wait, P );
          case Flow of
            TT_Flow_Up   : InsNew( Draw_Left, P );
            TT_Flow_Down : InsNew( Draw_Right, P );
          end
        end
      else
        dec( CountL );

      P := Q;
    end;

    (* Sort the drawing lists *)

    Sort( Draw_Left );
    Sort( Draw_Right );

    (* Let's trace *)

    dropouts  := 0;

    P_Left  := Draw_Left;
    P_Right := Draw_Right;

    while ( P_Left <> nil ) do
    begin

      {$IFDEF ASSERT}
      if P_Right = nil then
        Halt(13);
      {$ENDIF}

      Q_Left  := P_Left^ .Link;
      Q_Right := P_Right^.Link;

      {$IFDEF ASSERT}
      if Q_Right = nil then
        Halt(11);
      {$ENDIF}

      x1 := P_Left^ .X;
      x2 := P_Right^.X;

      if x1 > x2 then
        begin
          xs := x1;
          x1 := x2;
          x2 := xs;
        end;

      if ( x2-x1 <= Precision ) then
        begin
          e1 := ( x1+Precision-1 ) and Precision_Mask;
          e2 := x2 and Precision_Mask;

          if (dropOutControl <> 0) and
             ((e1 > e2) or (e2 = e1 + Precision)) then
          begin
            P_Left^.x  := x1;
            P_Right^.x := x2;

            inc( dropouts );

            DelOld( Draw_Left,  P_Left );
            DelOld( Draw_Right, P_Right );

            InsNew( Drop_Left,  P_Left );
            InsNew( Drop_Right, P_Right );

            goto Skip_To_Next;
          end
        end;

      Proc_Sweep_Span( y, x1, x2, P_Left, P_Right );

      (* We finalize the Profile if needed *)

      if P_Left ^.height = 0 then
          DelOld( Draw_Left,  P_Left  );

      if P_Right^.height = 0 then
          DelOld( Draw_Right, P_Right );

  Skip_To_Next:

      P_Left  := Q_Left;
      P_Right := Q_Right;
    end;

    {$IFDEF ASSERT}
    if P_Right <> nil then
      Halt(10);
    {$ENDIF}

    (* Now perform the dropouts only _after_ the span drawing *)

    P_Left  := Drop_Left;
    P_Right := Drop_Right;

    while ( dropouts > 0 ) do
    begin

      Q_Left  := P_Left^. Link;
      Q_Right := P_Right^.Link;

      DelOld( Drop_Left, P_Left );
      DelOld( Drop_Right, P_Right );

      Proc_Sweep_Drop( y, P_Left^.x, P_Right^.x, P_Left, P_Right );

      if P_Left^.height > 0 then
        InsNew( Draw_Left, P_Left );

      if P_Right^.height > 0 then
        InsNew( Draw_Right, P_Right );

      P_Left  := Q_Left;
      P_Right := Q_Right;

      dec( dropouts );
    end;

    (* Step to next line *)

    Proc_Sweep_Step;

  end;

  Draw_Sweep := True;

end;

{$ENDIF}

{$F+ Far calls are necessary for function pointers under BP7}
{    This flag is currently ignored by the Virtual Compiler }

(***********************************************************************)
(*                                                                     *)
(*  Vertical Sweep Procedure Set :                                     *)
(*                                                                     *)
(*  These three routines are used during the vertical black/white      *)
(*  sweep phase by the generic Draw_Sweep function.                    *)
(*                                                                     *)
(***********************************************************************)

procedure Vertical_Sweep_Init( var min, max : Int );
begin
  case Cible.flow of

    TT_Flow_Up : begin
                   traceOfs  := min * Cible.cols;
                   traceIncr := Cible.cols;
                 end;
  else
    traceOfs  := (Cible.rows - 1 - min)*Cible.cols;
    traceIncr := -Cible.cols;
  end;

  gray_min_x := 0;
  gray_max_x := 0;
end;



procedure Vertical_Sweep_Span( y     : Int;
                               x1,
                               x2    : TT_F26dot6;
                               Left,
                               Right : PProfile );
var
  e1, e2  : Longint;
  c1, c2  : Int;
  f1, f2  : Int;
  base    : PByte;
begin
{$IFNDEF NO_ASM}
  asm
    push esi
    push ebx
    push ecx

    mov  eax, X1
    mov  ebx, X2
    mov  ecx, [Precision_Bits]

    sub  ebx, eax
    add  eax, [Precision]
    dec  eax

    sub  ebx, [Precision]
    cmp  ebx, [Precision_Jitter]
    jg @No_Jitter

  @Do_Jitter:
    mov  ebx, eax
    jmp @0

  @No_Jitter:
    mov  ebx, X2

  @0:
    sar  ebx, cl
    js   @Sortie

    sar  eax, cl
    mov  ecx, [BWidth]

    cmp  eax, ebx
    jg   @Sortie

    cmp  eax, ecx
    jge  @Sortie

    test eax, eax
    jns  @1
    xor  eax, eax
  @1:
    cmp  ebx, ecx
    jl   @2
    lea  ebx, [ecx-1]
  @2:

    mov  edx, eax
    mov  ecx, ebx
    and  edx, 7
    sar  eax, 3
    and  ecx, 7
    sar  ebx, 3

    cmp  eax, [gray_min_X]
    jge  @3
    mov  [gray_min_X], eax

  @3:
    cmp  ebx, [gray_max_X]
    jl   @4
    mov  [gray_max_X], ebx

  @4:
    mov esi, ebx

    mov ebx, [BCible]
    add ebx, [TraceOfs]
    add ebx, eax

    sub esi, eax
    jz @5

    mov  al, [LMask + edx].byte
    or   [ebx], al
    inc  ebx
    dec  esi
    jz @6
    mov  eax, -1
  @7:
    mov [ebx].byte, al
    dec esi
    lea ebx, [ebx+1]
    jnz @7

  @6:
    mov al, [RMask + ecx].byte
    or [ebx], al
    jmp @8

  @5:
    mov al, [LMask + edx].byte
    and al, [RMask + ecx].byte
    or  [ebx], al

  @8:
  @Sortie:
    pop  ecx
    pop  ebx
    pop  esi
  end;
{$ELSE}

  e1 := (( x1+Precision-1 ) and Precision_Mask) div Precision;

  if ( x2-x1-Precision <= Precision_Jitter ) then
    e2 := e1
  else
    e2 := ( x2 and Precision_Mask ) div Precision;

  if (e2 >= 0) and (e1 < BWidth) then

    begin
      if e1 <  0      then e1 := 0;
      if e2 >= BWidth then e2 := BWidth-1;

      c1 := e1 shr 3;
      c2 := e2 shr 3;

      f1 := e1 and 7;
      f2 := e2 and 7;

      if gray_min_X > c1 then gray_min_X := c1;
      if gray_max_X < c2 then gray_max_X := c2;

      base := @BCible^[TraceOfs + c1];

      if c1 = c2 then
        base^[0] := base^[0] or ( LMask[f1] and Rmask[f2] )
      else
       begin
         base^[0] := base^[0] or LMask[f1];

         if c2>c1+1 then
           FillChar( base^[1], c2-c1-1, $FF );

         base     := @base^[c2-c1];
         base^[0] := base^[0] or RMask[f2];
       end
    end;
{$ENDIF}
end;


procedure Vertical_Sweep_Drop( y     : Int;
                               x1,
                               x2    : TT_F26dot6;
                               Left,
                               Right : PProfile );
var
  e1, e2  : Longint;
  c1, c2  : Int;
  f1, f2  : Int;

  j : Int;
begin

  (* Drop-out control *)

  e1 := ( x1+Precision-1 ) and Precision_Mask;
  e2 := x2 and Precision_Mask;

  (* We are guaranteed that x2-x1 <= Precision here *)

  if e1 > e2 then
   if e1 = e2 + Precision then

    case DropOutControl of

      (* Drop-out Control Rule #3 *)
      1 : e1 := e2;

      4 : begin
            e1 := ((x1+x2+1) div 2 + Precision-1) and Precision_Mask;
            e2 := e1;
          end;

      (* Drop-out Control Rule #4 *)

      (* The spec is not very clear regarding rule #4. It       *)
      (* presents a method that is way too costly to implement  *)
      (* while the general idea seems to get rid of 'stubs'.    *)
      (*                                                        *)
      (* Here, we only get rid of stubs recognized when :       *)
      (*                                                        *)
      (*  upper stub :                                          *)
      (*                                                        *)
      (*   - P_Left and P_Right are in the same contour         *)
      (*   - P_Right is the successor of P_Left in that contour *)
      (*   - y is the top of P_Left and P_Right                 *)
      (*                                                        *)
      (*  lower stub :                                          *)
      (*                                                        *)
      (*   - P_Left and P_Right are in the same contour         *)
      (*   - P_Left is the successor of P_Right in that contour *)
      (*   - y is the bottom of P_Left                          *)
      (*                                                        *)

      2,5 : begin

            if ( x2-x1 < Precision_Half ) then
            begin
              (* upper stub test *)

              if ( Left^.next = Right ) and
                 ( Left^.Height <= 0 )  then exit;

              (* lower stub test *)

              if ( Right^.next = Left ) and
                 ( Left^.Start = y   ) then exit;
            end;

            (* Check that the rightmost pixel is not already set *)
            e1 := e1 div Precision;

            c1 := e1 shr 3;
            f1 := e1 and 7;

            if ( e1 >= 0 ) and ( e1 < BWidth )                and
               ( BCible^[TraceOfs+c1] and ($80 shr f1) <> 0 ) then
              exit;

            case DropOutControl of
              2 : e1 := e2;
              5 : e1 := ((x1+x2+1) div 2 + Precision-1) and Precision_Mask;
            end;

            e2 := e1;

          end;
    else
      exit;  (* unsupported mode *)
    end

   else
  else
    e2 := e1;   (* when x1 = e1, x2 = e2, e2 = e1 + 64 *)

  e1 := e1 div Precision;

  if (e1 >= 0) and (e1 < BWidth ) then
    begin
      c1 := e1 shr 3;
      f1 := e1 and 7;

      if gray_min_X > c1 then gray_min_X := c1;
      if gray_max_X < c1 then gray_max_X := c1;

      j := TraceOfs + c1;

      BCible^[j] := BCible^[j] or ($80 shr f1);
    end;
end;



procedure Vertical_Sweep_Step;
begin
  inc( TraceOfs, traceIncr );
end;


(***********************************************************************)
(*                                                                     *)
(*  Horizontal Sweep Procedure Set :                                   *)
(*                                                                     *)
(*  These three routines are used during the horizontal black/white    *)
(*  sweep phase by the generic Draw_Sweep function.                    *)
(*                                                                     *)
(***********************************************************************)

procedure Horizontal_Sweep_Init( var min, max : Int );
begin
  (* Nothing, really *)
end;


procedure Horizontal_Sweep_Span( y     : Int;
                                 x1,
                                 x2    : TT_F26dot6;
                                 Left,
                                 Right : PProfile );
var
  e1, e2  : Longint;
  c1, c2  : Int;
  f1, f2  : Int;

  j : Int;
begin

  if ( x2-x1 < Precision ) then
  begin
    e1 := ( x1+(Precision-1) ) and Precision_Mask;
    e2 := x2 and Precision_Mask;

    if e1 = e2 then
    begin
      c1 := y shr 3;
      f1 := y and 7;

      if (e1 >= 0) then
      begin
        e1 := e1 shr Precision_Bits;
        if Cible.flow = TT_Flow_Up then
          j := c1 + e1*Cible.cols
        else
          j := c1 + (Cible.rows-1-e1)*Cible.cols;
        if e1 < Cible.Rows then
          BCible^[j] := BCible^[j] or ($80 shr f1);
      end;
    end;
  end;

{$IFDEF RIEN}
  e1 := ( x1+(Precision-1) ) and Precision_Mask;
  e2 := x2 and Precision_Mask;

  (* We are here guaranteed that x2-x1 > Precision *)

   c1 := y shr 3;
   f1 := y and 7;

   if (e1 >= 0) then
   begin
     e1 := e1 shr Precision_Bits;
     if Cible.flow = TT_Flow_Up then
       j := c1 + e1*Cible.cols
     else
       j := c1 + (Cible.rows-1-e1)*Cible.cols;
     if e1 < Cible.Rows then
       BCible^[j] := BCible^[j] or ($80 shr f1);
   end;

   if (e2 >= 0) then
   begin
     e2 := e2 shr Precision_Bits;
     if Cible.flow = TT_Flow_Up then
       j := c1 + e1*Cible.cols
     else
       j := c1 + (Cible.rows-1-e2)*Cible.cols;
     if (e2 <> e1) and (e2 < Cible.Rows) then
       BCible^[j] := BCible^[j] or ($80 shr f1);
   end;
{$ENDIF}

end;



procedure Horizontal_Sweep_Drop( y     : Int;
                                 x1,
                                 x2    : TT_F26dot6;
                                 Left,
                                 Right : PProfile );
var
  e1, e2  : Longint;
  c1, c2  : Int;
  f1, f2  : Int;

  j : Int;
begin

  e1 := ( x1+(Precision-1) ) and Precision_Mask;
  e2 := x2 and Precision_Mask;

  (* During the horizontal sweep, we only take care of drop-outs *)

  if e1 > e2 then
   if e1 = e2 + Precision then

    case DropOutControl of

      0 : exit;

      (* Drop-out Control Rule #3 *)
      1 : e1 := e2;

      4 : begin
            e1 := ( (x1+x2) div 2 +Precision div 2 ) and Precision_Mask;
            e2 := e1;
          end;

      (* Drop-out Control Rule #4 *)

      (* The spec is not very clear regarding rule #4. It       *)
      (* presents a method that is way too costly to implement  *)
      (* while the general idea seems to get rid of 'stubs'.    *)
      (*                                                        *)

      2,5 : begin

              (* rightmost stub test *)

              if ( Left^.next = Right ) and
                 ( Left^.Height <= 0 )  then exit;

              (* leftmost stub test *)

              if ( Right^.next = Left ) and
                 ( Left^.Start = y   ) then exit;

              (* Check that the upmost pixel is not already set *)

              e1 := e1 div Precision;

              c1 := y shr 3;
              f1 := y and 7;

              if Cible.flow = TT_Flow_Up then
                j := c1 + e1*Cible.cols
              else
                j := c1 + (Cible.rows-1-e1)*Cible.cols;

              if ( e1 >= 0 ) and ( e1 < Cible.Rows ) and
                 ( BCible^[j] and ($80 shr f1) <> 0 ) then exit;

              case DropOutControl of
                2 : e1 := e2;
                5 : e1 := ((x1+x2) div 2 + Precision_Half) and Precision_Mask;
              end;

              e2 := e1;
            end;
    else
      exit;  (* Unsupported mode *)
    end;

   c1 := y shr 3;
   f1 := y and 7;

   if (e1 >= 0) then
   begin
     e1 := e1 shr Precision_Bits;
     if Cible.flow = TT_Flow_Up then
       j := c1 + e1*Cible.cols
     else
       j := c1 + (Cible.rows-1-e1)*Cible.cols;
     if e1 < Cible.Rows then BCible^[j] := BCible^[j] or ($80 shr f1);
   end;

end;



procedure Horizontal_Sweep_Step;
begin
  (* Nothing, really *)
end;

(***********************************************************************)
(*                                                                     *)
(*  Vertical Gray Sweep Procedure Set :                                *)
(*                                                                     *)
(*  These two   routines are used during the vertical gray-levels      *)
(*  sweep phase by the generic Draw_Sweep function.                    *)
(*                                                                     *)
(*                                                                     *)
(*  NOTES :                                                            *)
(*                                                                     *)
(*  - The target pixmap's width *must* be a multiple of 4              *)
(*                                                                     *)
(*  - you have to use the function Vertical_Sweep_Span for             *)
(*    the gray span call.                                              *)
(*                                                                     *)
(***********************************************************************)

procedure Vertical_Gray_Sweep_Init( var min, max : Int );
begin
  min        :=  min and -2;
  max        :=  (max+3) and -2;

  case Cible.flow of

    TT_Flow_Up : begin
                   traceG    := (min div 2)*Cible.cols;
                   traceIncr := Cible.cols;
                 end;
  else
    traceG    := (Cible.rows-1- (min div 2))*Cible.cols;
    traceIncr := -Cible.cols;
  end;

  TraceOfs   :=  0;
  gray_min_x :=  Cible.Cols;
  gray_max_x := -Cible.Cols;
end;


procedure Vertical_Gray_Sweep_Step;
var
  j, c1, c2 : Int;
begin
  inc( TraceOfs, Gray_Width );

  if TraceOfs > Gray_Width then
  begin

    if gray_max_X >= 0 then
    begin

      if gray_max_x > cible.cols-1 then gray_max_x := cible.cols-1;
      if gray_min_x < 0            then gray_min_x := 0;

      j := TraceG + gray_min_x*4;

      for c1 := gray_min_x to gray_max_x do
      begin

        c2 := Count_Table[ BCible^[c1           ] ] +
              Count_Table[ BCible^[c1+Gray_Width] ];

        if c2 <> 0 then
        begin
          BCible^[c1           ] := 0;
          BCible^[c1+Gray_Width] := 0;

          GCible^[j] := GCible^[j] or Grays[ (c2 and $F000) shr 12 ]; inc(j);
          GCible^[j] := GCible^[j] or Grays[ (c2 and $0F00) shr  8 ]; inc(j);
          GCible^[j] := GCible^[j] or Grays[ (c2 and $00F0) shr  4 ]; inc(j);
          GCible^[j] := GCible^[j] or Grays[ (c2 and $000F)        ]; inc(j);
        end
        else
          inc( j, 4 );

      end;
    end;

    TraceOfs   := 0;
    inc( TraceG, traceIncr );

    gray_min_x :=  Cible.Cols;
    gray_max_x := -Cible.Cols;
  end;
end;

(***********************************************************************)
(*                                                                     *)
(*  Horizontal Gray Sweep Procedure Set :                              *)
(*                                                                     *)
(*  These three routines are used during the horizontal gray-levels    *)
(*  sweep phase by the generic Draw_Sweep function.                    *)
(*                                                                     *)
(***********************************************************************)

procedure Horizontal_Gray_Sweep_Span( y     : Int;
                                      x1,
                                      x2    : TT_F26dot6;
                                      Left,
                                      Right : PProfile );
var
  e1, e2    : TT_F26Dot6;
  c1, f1, j : Int;
begin
  exit;
  y  := y div 2;

  e1 := ( x1+(Precision-1) ) and Precision_Mask;
  e2 := x2 and Precision_Mask;

  if (e1 >= 0) then
  begin
    e1 := e1 shr (Precision_Bits+1);
(*    if Cible.flow = TT_Flow_Up then *)
      j := y + e1*Cible.cols;
(*    else
//      j := c1 + (Cible.rows-1-e1)*Cible.cols;  *)
    if e1 < Cible.Rows then
      if GCible^[j] = Grays[0] then
        GCible^[j] := Grays[1];
  end;

  if (e2 >= 0) then
  begin
    e2 := e2 shr (Precision_Bits+1);
(*    if Cible.flow = TT_Flow_Up then *)
      j := y + e2*Cible.cols;
(*    else
//      j := c1 + (Cible.rows-1-e2)*Cible.cols; *)
    if (e2 <> e1) and (e2 < Cible.Rows) then
      if GCible^[j] = Grays[0] then
        GCible^[j] := Grays[1];
  end;

end;


procedure Horizontal_Gray_Sweep_Drop( y     : Int;
                                      x1,
                                      x2    : TT_F26dot6;
                                      Left,
                                      Right : PProfile );
var
  e1, e2  : Longint;
  f1, f2  : Int;
  color   : Byte;
  j : Int;
begin

  e1 := ( x1+(Precision-1) ) and Precision_Mask;
  e2 := x2 and Precision_Mask;

  (* During the horizontal sweep, we only take care of drop-outs *)

  if e1 > e2 then
   if e1 = e2 + Precision then

    case DropOutControl of

      0 : exit;

      (* Drop-out Control Rule #3 *)
      1 : e1 := e2;

      4 : begin
            e1 := ( (x1+x2) div 2 +Precision div 2 ) and Precision_Mask;
            e2 := e1;
          end;

      (* Drop-out Control Rule #4 *)

      (* The spec is not very clear regarding rule #4. It       *)
      (* presents a method that is way too costly to implement  *)
      (* while the general idea seems to get rid of 'stubs'.    *)
      (*                                                        *)

      2,5 : begin

              (* lowest stub test *)

              if ( Left^.next = Right ) and
                 ( Left^.Height <= 0 )  then exit;

              (* upper stub test *)

              if ( Right^.next = Left ) and
                 ( Left^.Start = y    ) then exit;

              case DropOutControl of
                2 : e1 := e2;
                5 : e1 := ((x1+x2) div 2 + Precision_Half) and Precision_Mask;
              end;

              e2 := e1;
            end;
    else
      exit;  (* Unsupported mode *)
    end;

   if (e1 >= 0) then
   begin
     (* A small trick to make 'average' thin line appear in *)
     (* medium gray..                                       *)

     if ( x2-x1 >= Precision_Half ) then
       color := Grays[2]
     else color := Grays[1];

     e1 := e1 shr (Precision_Bits+1);
     if Cible.flow = TT_Flow_Up then
       j := (y div 2) + e1*Cible.cols
     else
       j := (y div 2) + (Cible.rows-1-e1)*Cible.cols;
     if e1 < Cible.Rows then
       if GCible^[j] = Grays[0] then
         GCible^[j] := color;
   end;
end;

{$IFDEF SMOOTH}

(***********************************************************************)
(*                                                                     *)
(*  Vertical Smooth Sweep Procedure Set :                              *)
(*                                                                     *)
(*  These two   routines are used during the vertical smooth-levels    *)
(*  sweep phase by the generic Draw_Sweep function.                    *)
(*                                                                     *)
(*                                                                     *)
(*  NOTES :                                                            *)
(*                                                                     *)
(*  - The target pixmap's width *must* be a multiple of 2              *)
(*                                                                     *)
(*  - you have to use the function Vertical_Sweep_Span for             *)
(*    the smooth span call.                                            *)
(*                                                                     *)
(***********************************************************************)

procedure Smooth_Sweep_Init( var min, max : Int );
var
  i : integer;
begin
  min        :=  min and -4;
  max        :=  (max + 7) and -4;
  TraceOfs   :=  0;
  TraceG     :=  Cible.Cols * ( min div 4 );
  gray_min_x :=  Cible.Cols;
  gray_max_x := -Cible.Cols;

  smooth_pass := 0;
(*
  for i := 0 to Smooth_Cols-1 do
    GCible^[i] := 0;
*)
end;



procedure Smooth_Sweep_Step;
var
  j, c1, c2 : Int;
begin

  if gray_max_X >= 0 then
  begin

    if gray_max_x > cible.cols-1 then gray_max_x := cible.cols-1;

    if gray_min_x < 0 then gray_min_x := 0;

    j := TraceG + gray_min_x*2;

    for c1 := gray_min_x to gray_max_x do
    begin

     c2 := Count_Table2[ BCible^[c1] ];

      if c2 <> 0 then
      begin
        inc( GCible^[j], c2 shr 4  ); inc(j);
        inc( GCible^[j], c2 and 15 ); inc(j);

        BCible^[c1] := 0;
      end
      else
        inc( j, 2 );
    end;

  end;

  traceOfs := 0;
  inc( smooth_pass );

  if smooth_pass >= 4 then
  begin

    j := TraceG + gray_min_x*2;

    for c1 := gray_min_x to gray_max_x do
    begin
      c2 := GCible^[j]; GCible^[j] := Smooths[c2]; inc(j);
      c2 := GCible^[j]; GCible^[j] := Smooths[c2]; inc(j);
    end;

    smooth_pass := 0;
    inc( TraceG, Cible.Cols );

    gray_min_x :=  Cible.Cols;
    gray_max_x := -Cible.Cols;
  end;

end;

{$ENDIF}

{$F-  End of dispatching functions definitions }

(****************************************************************************)
(*                                                                          *)
(* Function:    Render_Single_Pass                                          *)
(*                                                                          *)
(* Description: Performs one sweep with sub-banding.                        *)
(*                                                                          *)
(* Input:       _XCoord, _YCoord : x and y coordinates arrays               *)
(*                                                                          *)
(* Returns:     True on success                                             *)
(*              False if any error was encountered during render.           *)
(*                                                                          *)
(****************************************************************************)

function Render_Single_Pass( vertical : Boolean ) : boolean;
var
  i, j, k : Int;
begin
  Render_Single_Pass := False;

  while Band_Top > 0 do

    begin

      with Band_Stack[ Band_Top ] do
        begin
          MaxY   := longint(Y_Max) * Precision;
          MinY   := longint(Y_Min) * Precision;
        end;

      profCur := 0;
      Error   := Err_Ras_None;

      if not Convert_Glyph( vertical ) then
        begin

          if Error <> Err_Ras_Overflow then exit;
          Error := Err_Ras_None;

          (* sub-banding *)

          {$IFDEF DEBUG3}
          ClearBand( MinY shr Precision_Bits, MaxY shr Precision_Bits );
          {$ENDIF}

          with Band_Stack[Band_Top] do
            begin
              I := Y_Min;
              J := Y_Max;
            end;

          K := ( I + J ) div 2;

          if ( Band_Top >= 8 ) or ( K <= I ) then
            begin
              Band_Top := 0;
              Error := Err_Ras_Invalid;
              exit;
            end
          else
            begin

              with Band_Stack[Band_Top+1] do
                begin
                  Y_Min := K;
                  Y_Max := J;
                end;

              Band_Stack[Band_Top].Y_Max := K-1;

              inc( Band_Top );
            end
        end
      else
        begin

          if ( fProfile <> nil ) then
            if not Draw_Sweep then exit;

          dec( Band_Top );
        end;

    end;

  Render_Single_Pass := true;
end;

(****************************************************************************)
(*                                                                          *)
(* Function:    Render_Glyph                                                *)
(*                                                                          *)
(* Description: Renders a glyph in a bitmap.      Sub-banding if needed     *)
(*                                                                          *)
(* Input:       AGlyph   Glyph record                                       *)
(*                                                                          *)
(* Returns:     True on success                                             *)
(*              False if any error was encountered during render.           *)
(*                                                                          *)
(****************************************************************************)

function Render_Glyph( var glyph  : TT_Outline;
                       var target : TT_Raster_Map ) : TError;
begin

 Render_Glyph := Failure;

 if Buff = nil then
   begin
     Error := Err_Ras_NotIni;
     exit;
   end;

 if glyph.conEnds^[glyph.n_contours-1] > glyph.n_points then
   begin
     Error := Err_Ras_Invalid_Contours;
     exit;
   end;

 Cible := target;

 Outs      := glyph.conEnds;
 Flags     := PByte(glyph.flags);
 nPoints   := Glyph.n_points;
 nContours := Glyph.n_contours;

 points := Glyph.points;

 Set_High_Precision( glyph.high_precision );
 scale_shift    := precision_shift;
 DropOutControl := glyph.dropout_mode;
 second_pass    := glyph.second_pass;

 Error := Err_Ras_None;

 (* Vertical Sweep *)

{$IFDEF FPK}
 Proc_Sweep_Init := @Vertical_Sweep_Init;
 Proc_Sweep_Span := @Vertical_Sweep_Span;
 Proc_Sweep_Drop := @Vertical_Sweep_Drop;
 Proc_Sweep_Step := @Vertical_Sweep_Step;
{$ELSE}
 Proc_Sweep_Init := Vertical_Sweep_Init;
 Proc_Sweep_Span := Vertical_Sweep_Span;
 Proc_Sweep_Drop := Vertical_Sweep_Drop;
 Proc_Sweep_Step := Vertical_Sweep_Step;
{$ENDIF}

 Band_Top            := 1;
 Band_Stack[1].Y_Min := 0;
 Band_Stack[1].Y_Max := Cible.Rows-1;

 BWidth := Cible.width;
 BCible := PByte( Cible.Buffer );

 if not Render_Single_Pass( False ) then exit;

 (* Horizontal Sweep *)

 if Second_Pass then
 begin

{$IFDEF FPK}
   Proc_Sweep_Init := @Horizontal_Sweep_Init;
   Proc_Sweep_Span := @Horizontal_Sweep_Span;
   Proc_Sweep_Drop := @Horizontal_Sweep_Drop;
   Proc_Sweep_Step := @Horizontal_Sweep_Step;
{$ELSE}
   Proc_Sweep_Init := Horizontal_Sweep_Init;
   Proc_Sweep_Span := Horizontal_Sweep_Span;
   Proc_Sweep_Drop := Horizontal_Sweep_Drop;
   Proc_Sweep_Step := Horizontal_Sweep_Step;
{$ENDIF}

   Band_Top            := 1;
   Band_Stack[1].Y_Min := 0;
   Band_Stack[1].Y_Max := Cible.Width-1;

   BWidth := Cible.rows;
   BCible := PByte( Cible.Buffer );

   if not Render_Single_Pass( True ) then exit;

 end;

 Render_Glyph := Success;
end;

(****************************************************************************)
(*                                                                          *)
(* Function:    Render_Gray_Glyph                                           *)
(*                                                                          *)
(* Description: Renders a glyph with grayscaling. Sub-banding if needed     *)
(*                                                                          *)
(* Input:       AGlyph   Glyph record                                       *)
(*                                                                          *)
(* Returns:     True on success                                             *)
(*              False if any error was encountered during render.           *)
(*                                                                          *)
(****************************************************************************)

  function Render_Gray_Glyph( var glyph   : TT_Outline;
                              var target  : TT_Raster_Map ) : TError;
begin

 Render_Gray_Glyph := Failure;

 cible := target;

 Outs      := Glyph.conEnds;
 Flags     := PByte(glyph.flags);
 nPoints   := Glyph.n_points;
 nContours := Glyph.n_contours;

 points := Glyph.points;

 Set_High_Precision( glyph.high_precision );
 scale_shift    := precision_shift+1;
 DropOutControl := glyph.dropout_mode;
 second_pass    := glyph.high_precision;

 Error := Err_Ras_None;

 Band_Top            := 1;
 Band_Stack[1].Y_Min := 0;
 Band_Stack[1].Y_Max := 2*Cible.Rows - 1;

 BWidth := Gray_Width;

 if BWidth > Cible.cols div 4 then BWidth := Cible.cols div 4;

 BWidth := BWidth*8;
 BCible := PByte( Gray_Lines   );
 GCible := PByte( Cible.Buffer );

{$IFDEF FPK}
 Proc_Sweep_Init := @Vertical_Gray_Sweep_Init;
 Proc_Sweep_Span := @Vertical_Sweep_Span;
 Proc_Sweep_Drop := @Vertical_Sweep_Drop;
 Proc_Sweep_Step := @Vertical_Gray_Sweep_Step;
{$ELSE}
 Proc_Sweep_Init := Vertical_Gray_Sweep_Init;
 Proc_Sweep_Span := Vertical_Sweep_Span;
 Proc_Sweep_Drop := Vertical_Sweep_Drop;
 Proc_Sweep_Step := Vertical_Gray_Sweep_Step;
{$ENDIF}

 if not Render_Single_Pass( False ) then exit;

 (* Horizontal Sweep *)

 if Second_Pass then
 begin

{$IFDEF FPK}
   Proc_Sweep_Init := @Horizontal_Sweep_Init;
   Proc_Sweep_Span := @Horizontal_Gray_Sweep_Span;
   Proc_Sweep_Drop := @Horizontal_Gray_Sweep_Drop;
   Proc_Sweep_Step := @Horizontal_Sweep_Step;
{$ELSE}
   Proc_Sweep_Init := Horizontal_Sweep_Init;
   Proc_Sweep_Span := Horizontal_Gray_Sweep_Span;
   Proc_Sweep_Drop := Horizontal_Gray_Sweep_Drop;
   Proc_Sweep_Step := Horizontal_Sweep_Step;
{$ENDIF}

   Band_Top            := 1;
   Band_Stack[1].Y_Min := 0;
   Band_Stack[1].Y_Max := Cible.Width*2-1;

   BWidth := Cible.rows;
   GCible := PByte( Cible.Buffer );

   if not Render_Single_Pass( True ) then exit;

 end;

 Render_Gray_Glyph := Success;
 exit;

end;

{$IFDEF SMOOTH}
(****************************************************************************)
(*                                                                          *)
(* Function:    Render_Smooth_Glyph                                         *)
(*                                                                          *)
(* Description: Renders a glyph with grayscaling. Sub-banding if needed     *)
(*                                                                          *)
(* Input:       AGlyph   Glyph record                                       *)
(*                                                                          *)
(* Returns:     True on success                                             *)
(*              False if any error was encountered during render.           *)
(*                                                                          *)
(****************************************************************************)

function Render_Smooth_Glyph( var glyph   : TGlyphRecord;
                                  target  : PRasterBlock;
                                  scan    : Byte;
                                  palette : pointer      ) : boolean;
begin

 Render_Smooth_Glyph := Failure;

 if target <> nil then
   cible := target^;
(*
 if palette <> nil then
   move( palette^, Grays, 5 );
*)
 Outs      := Glyph.endPoints;
 Flags     := PByte(glyph.Flag);
 nPoints   := Glyph.Points;
 nContours := Glyph.numConts;

 scale_shift    := precision_shift+2;
 DropOutControl := scan;

 Raster_Error := Err_Ras_None;

 Band_Top            := 1;
 Band_Stack[1].Y_Min := 0;
 Band_Stack[1].Y_Max := 4*Cible.Rows - 1;

 BWidth := Smooth_Cols;

 if BWidth > Cible.cols then BWidth := Cible.cols;

 BWidth := BWidth*8;
 BCible := PByte( Gray_Lines   );
 GCible := PByte( Cible.Buffer );

{$IFDEF FPK}
 Proc_Sweep_Init := @Smooth_Sweep_Init;
 Proc_Sweep_Span := @Vertical_Sweep_Span;
 Proc_Sweep_Drop := @Vertical_Sweep_Drop;
 Proc_Sweep_Step := @Smooth_Sweep_Step;
{$ELSE}
 Proc_Sweep_Init := Smooth_Sweep_Init;
 Proc_Sweep_Span := Vertical_Sweep_Span;
 Proc_Sweep_Drop := Vertical_Sweep_Drop;
 Proc_Sweep_Step := Smooth_Sweep_Step;
{$ENDIF}

 if not Render_Single_Pass( Glyph.XCoord, Glyph.YCoord ) then exit;

 Render_Smooth_Glyph := Success;

end;

{$ENDIF}

(****************************************************************************)
(*                                                                          *)
(* Function:    Init_Rasterizer                                             *)
(*                                                                          *)
(* Description: Initializes the rasterizer.                                 *)
(*                                                                          *)
(* Input:       rasterBlock   target bitmap/pixmap description              *)
(*              profBuffer    pointer to the render pool                    *)
(*              profSize      size in bytes of the render pool              *)
(*                                                                          *)
(* Returns:     1 ( always, but we should check parameters )                *)
(*                                                                          *)
(****************************************************************************)

function TTRaster_Init : TError;
var
  i, j, c, l : integer;
const
  Default_Grays : array[0..4] of Byte
                = ( 0, 23, 27, 29, 31 );

  Default_Smooths : array[0..16] of Byte
                  = ( 0,  20, 20, 21, 22, 23, 24, 25,
                      26, 27, 28, 29, 30, 31, 31, 31, 31 );
begin
  GetMem( Buff, Render_Pool_Size );
  SizeBuff := (Render_Pool_Size div 4);

  GetMem( Gray_Lines, Gray_Lines_Size );
  Gray_Width := Gray_Lines_Size div 2;

{$IFDEF SMOOTH}
  Smooth_Cols := Gray_Lines_Size div 4;
{$ENDIF}

  { Initialisation of Count_Table }

  for i := 0 to 255 do
  begin
    l := 0;
    j := i;
    for c := 0 to 3 do
    begin
      l := l shl 4;
      if ( j and $80 <> 0 ) then inc(l);
      if ( j and $40 <> 0 ) then inc(l);
      j := (j shl 2) and $FF;
    end;
    Count_table[i] := l;
  end;

  (* default Grays takes the gray levels of the standard VGA *)
  (* 256 colors mode                                                *)

  Grays[0] := 0;
  Grays[1] := 23;
  Grays[2] := 27;
  Grays[3] := 29;
  Grays[4] := 31;


{$IFDEF SMOOTH}

  { Initialisation of Count_Table2 }
  for i := 0 to 255 do
  begin
    l := 0;
    j := i;
    for c := 0 to 1 do
    begin
      l := l shl 4;
      if ( j and $80 <> 0 ) then inc(l);
      if ( j and $40 <> 0 ) then inc(l);
      if ( j and $20 <> 0 ) then inc(l);
      if ( j and $10 <> 0 ) then inc(l);
      j := (j shl 4) and $FF;
    end;
    Count_table2[i] := l;
  end;
  move( Default_Smooths, Smooths, 17 );
{$ENDIF}

  Set_High_Precision(False);
  Set_Second_Pass(False);

  DropOutControl := 2;
  Error          := Err_Ras_None;

  TTRaster_Init := Success;
end;

procedure Cycle_DropOut;
begin
  case DropOutControl of

    0 : DropOutControl := 1;
    1 : DropOutControl := 2;
    2 : DropOutControl := 4;
    4 : DropOutControl := 5;
  else
    DropOutControl := 0;
  end;
end;

procedure TTRaster_Done;
begin
  FreeMem( Buff, Render_Pool_Size );
  FreeMem( Gray_Lines, Gray_Lines_Size );
end;


end.
