(*******************************************************************
 *
 *  TTDebug.Pas                                                 1.2
 *
 *    This unit is only used by the debugger.                
 *
 *  Copyright 1996 David Turner, Robert Wilhelm and Werner Lemberg
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 ******************************************************************)

unit TTDebug;

interface

uses TTTypes, TTTables, TTObjs, TTInterp;

type

  ByteHexStr  = string[2];    (* hex representation of a byte  *)
  ShortHexStr = string[4];    (*  "         "         "  short *)
  LongHexStr  = string[8];    (*  "         "         "  long  *)
  DebugStr    = string[128];  (* disassembled line output      *)

  { TBreakPoint }

  { A simple record to hold breakpoint information   }
  { it may be completed later with pass count, etc.. }
  { They must be in a sorted linked list             }

  PBreakPoint = ^TBreakPoint;
  TBreakPoint = record
                  Next     : PBreakPoint;
                  Range    : Int;
                  Address  : Int;
                end;

  { TRangeRec }

  { a record to store line number information and breakpoints list }

  PRangeRec = ^TRangeRec;
  TRangeRec = record
                Code         : PByte;
                Size         : Int;
                index        : Int;
                NLines       : Int;
                Disassembled : PUShort;
                Breaks       : PBreakPoint;
              end;


{ Generate_Range : Generate Line Number information specific to }
{                  a given range                                }

procedure Generate_Range( CR     : PCodeRange;
                          index  : Int;
                          var RR : TRangeRec );

{ Throw_Range : Discard Line Number Information }

procedure Throw_Range( var RR : TRangeRec );

{ Toggle_Break : Toggle a breakpoint }

procedure Toggle_Break( var Head : PBreakPoint; Range, Adr : Int );

{ Set_Break : Set a breakpoint on a given address }

procedure Set_Break  ( var Head : PBreakPoint; Range, Adr : Int );

{ Clear_Break : Clear one specific breakpoint }

procedure Clear_Break( var Head : PBreakPoint; Bp : PBreakPoint );

{ Clear_All_Breaks : Clear breakpoint list }

procedure Clear_All_Breaks( var Head : PBreakPoint );

{ Find_Breakpoint : find one breakpoint at a given address }

function Find_BreakPoint( Head : PBreakPoint; Range, IP : Int ) : PBreakPoint;

{ Cur_U_Line : returns the current disassembled line at Code(IP) }

function Cur_U_Line( Code : PByte; IP : Int ) : DebugStr;

{ Get_Length : returns the length of the current opcode at Code(IP) }

function Get_Length( Code : PByte; IP : Int ) : Int;

function Get_Dis_Line( var cr : TRangeRec; addr : Int ) : Int;


{ Hex_N : returns an hexadecimal string }

function Hex8 ( B : Byte ) : ByteHexStr;
function Hex16( W : word ) : ShortHexStr;
function Hex32( L : Long ) : LongHexStr;


implementation

type
  PStorageLong = ^TStorageLong;
  TStorageLong = record           (* do-it-all union record type *)
                   case Byte of
                    0 : ( L      : LongInt );
                    1 : ( S1, S2 : Integer );
                    2 : ( W1, W2 : Word );
                    3 : ( B1, B2,
                          B3, B4 : Byte );
                    4 : ( P      : Pointer );
                  end;

var
  OpSize : int;

const
  OpStr : array[ 0..255 ] of String[10]
        = (
            'SVTCA y',       (* Set vectors to coordinate axis y    *)
            'SVTCA x',       (* Set vectors to coordinate axis x    *)
            'SPvTCA y',      (* Set Proj. vec. to coord. axis y     *)
            'SPvTCA x',      (* Set Proj. vec. to coord. axis x     *)
            'SFvTCA y',      (* Set Free. vec. to coord. axis y     *)
            'SFvTCA x',      (* Set Free. vec. to coord. axis x     *)
            'SPvTL //',      (* Set Proj. vec. parallel to segment  *)
            'SPvTL +',       (* Set Proj. vec. normal to segment    *)
            'SFvTL //',      (* Set Free. vec. parallel to segment  *)
            'SFvTL +',       (* Set Free. vec. normal to segment    *)
            'SPvFS',         (* Set Proj. vec. from stack           *)
            'SFvFS',         (* Set Free. vec. from stack           *)
            'GPV',           (* Get projection vector               *)
            'GFV',           (* Get freedom vector                  *)
            'SFvTPv',        (* Set free. vec. to proj. vec.        *)
            'ISECT',         (* compute intersection                *)

            'SRP0',          (* Set reference point 0               *)
            'SRP1',          (* Set reference point 1               *)
            'SRP2',          (* Set reference point 2               *)
            'SZP0',          (* Set Zone Pointer 0                  *)
            'SZP1',          (* Set Zone Pointer 1                  *)
            'SZP2',          (* Set Zone Pointer 2                  *)
            'SZPS',          (* Set all zone pointers               *)
            'SLOOP',         (* Set loop counter                    *)
            'RTG',           (* Round to Grid                       *)
            'RTHG',          (* Round to Half-Grid                  *)
            'SMD',           (* Set Minimum Distance                *)
            'ELSE',          (* Else                                *)
            'JMPR',          (* Jump Relative                       *)
            'SCvTCi',        (* Set CVT                             *)
            'SSwCi',         (*                                     *)
            'SSW',           (*                                     *)

            'DUP',
            'POP',
            'CLEAR',
            'SWAP',
            'DEPTH',
            'CINDEX',
            'MINDEX',
            'AlignPTS',
            'INS_$28',
            'UTP',
            'LOOPCALL',
            'CALL',
            'FDEF',
            'ENDF',
            'MDAP[-]',
            'MDAP[r]',

            'IUP[y]',
            'IUP[x]',
            'SHP[0]',
            'SHP[1]',
            'SHC[0]',
            'SHC[1]',
            'SHZ[0]',
            'SHZ[1]',
            'SHPIX',
            'IP',
            'MSIRP[0]',
            'MSIRP[1]',
            'AlignRP',
            'RTDG',
            'MIAP[-]',
            'MIAP[r]',

            'NPushB',
            'NPushW',
            'WS',
            'RS',
            'WCvtP',
            'RCvt',
            'GC[0]',
            'GC[1]',
            'SCFS',
            'MD[0]',
            'MD[1]',
            'MPPEM',
            'MPS',
            'FlipON',
            'FlipOFF',
            'DEBUG',

            'LT',
            'LTEQ',
            'GT',
            'GTEQ',
            'EQ',
            'NEQ',
            'ODD',
            'EVEN',
            'IF',
            'EIF',
            'AND',
            'OR',
            'NOT',
            'DeltaP1',
            'SDB',
            'SDS',

            'ADD',
            'SUB',
            'DIV',
            'MUL',
            'ABS',
            'NEG',
            'FLOOR',
            'CEILING',
            'ROUND[G]',
            'ROUND[B]',
            'ROUND[W]',
            'ROUND[?]',
            'NROUND[G]',
            'NROUND[B]',
            'NROUND[W]',
            'NROUND[?]',

            'WCvtF',
            'DeltaP2',
            'DeltaP3',
            'DeltaC1',
            'DeltaC2',
            'DeltaC3',
            'SROUND',
            'S45Round',
            'JROT',
            'JROF',
            'ROFF',
            'INS_$7B',
            'RUTG',
            'RDTG',
            'SANGW',
            'AA',

            'FlipPT',
            'FlipRgON',
            'FlipRgOFF',
            'INS_$83',
            'INS_$84',
            'ScanCTRL',
            'SDPVTL[0]',
            'SDPVTL[1]',
            'GetINFO',
            'IDEF',
            'ROLL',
            'MAX',
            'MIN',
            'ScanTYPE',
            'IntCTRL',
            'INS_$8F',

            'INS_$90',
            'INS_$91',
            'INS_$92',
            'INS_$93',
            'INS_$94',
            'INS_$95',
            'INS_$96',
            'INS_$97',
            'INS_$98',
            'INS_$99',
            'INS_$9A',
            'INS_$9B',
            'INS_$9C',
            'INS_$9D',
            'INS_$9E',
            'INS_$9F',

            'INS_$A0',
            'INS_$A1',
            'INS_$A2',
            'INS_$A3',
            'INS_$A4',
            'INS_$A5',
            'INS_$A6',
            'INS_$A7',
            'INS_$A8',
            'INS_$A9',
            'INS_$AA',
            'INS_$AB',
            'INS_$AC',
            'INS_$AD',
            'INS_$AE',
            'INS_$AF',

            'PushB[0]',
            'PushB[1]',
            'PushB[2]',
            'PushB[3]',
            'PushB[4]',
            'PushB[5]',
            'PushB[6]',
            'PushB[7]',
            'PushW[0]',
            'PushW[1]',
            'PushW[2]',
            'PushW[3]',
            'PushW[4]',
            'PushW[5]',
            'PushW[6]',
            'PushW[7]',

            'MDRP[G]',
            'MDRP[B]',
            'MDRP[W]',
            'MDRP[?]',
            'MDRP[rG]',
            'MDRP[rB]',
            'MDRP[rW]',
            'MDRP[r?]',
            'MDRP[mG]',
            'MDRP[mB]',
            'MDRP[mW]',
            'MDRP[m?]',
            'MDRP[mrG]',
            'MDRP[mrB]',
            'MDRP[mrW]',
            'MDRP[mr?]',
            'MDRP[pG]',
            'MDRP[pB]',

            'MDRP[pW]',
            'MDRP[p?]',
            'MDRP[prG]',
            'MDRP[prB]',
            'MDRP[prW]',
            'MDRP[pr?]',
            'MDRP[pmG]',
            'MDRP[pmB]',
            'MDRP[pmW]',
            'MDRP[pm?]',
            'MDRP[pmrG]',
            'MDRP[pmrB]',
            'MDRP[pmrW]',
            'MDRP[pmr?]',

            'MIRP[G]',
            'MIRP[B]',
            'MIRP[W]',
            'MIRP[?]',
            'MIRP[rG]',
            'MIRP[rB]',
            'MIRP[rW]',
            'MIRP[r?]',
            'MIRP[mG]',
            'MIRP[mB]',
            'MIRP[mW]',
            'MIRP[m?]',
            'MIRP[mrG]',
            'MIRP[mrB]',
            'MIRP[mrW]',
            'MIRP[mr?]',
            'MIRP[pG]',
            'MIRP[pB]',

            'MIRP[pW]',
            'MIRP[p?]',
            'MIRP[prG]',
            'MIRP[prB]',
            'MIRP[prW]',
            'MIRP[pr?]',
            'MIRP[pmG]',
            'MIRP[pmB]',
            'MIRP[pmW]',
            'MIRP[pm?]',
            'MIRP[pmrG]',
            'MIRP[pmrB]',
            'MIRP[pmrW]',
            'MIRP[pmr?]'
         );

const
  HexStr : string[16] = '0123456789abcdef';

(*******************************************************************
 *
 *  Function    :  Hex8
 *
 *  Description :  Returns the string hexadecimal representation
 *                 of a Byte.
 *
 *  Input  :  B  byte
 *
 *  Output :  two-chars string
 *
 *****************************************************************)

function Hex8( B : Byte ) : ByteHexStr;
var
  S : ByteHexStr;
begin
  S[0] :=#2;
  S[1] := HexStr[ 1+( B shr 4 ) ];
  S[2] := HexStr[ 1+( B and 15 )];
  Hex8 := S;
end;

(*******************************************************************
 *
 *  Function    :  Hex16
 *
 *  Description :  Returns the string hexadecimal representation
 *                 of a Short.
 *
 *  Input  :  W  word
 *
 *  Output :  four-chars string
 *
 *****************************************************************)

function Hex16( W : word ) : ShortHexStr;
begin
  Hex16 := Hex8( Hi(w) )+Hex8( Lo(w) );
end;

(*******************************************************************
 *
 *  Function    :  Hex32
 *
 *  Description :  Returns the string hexadecimal representation
 *                 of a Long.
 *
 *  Input  :  L  Long
 *
 *  Output :  eight-chars string
 *
 *****************************************************************)

function Hex32( L : Long ) : LongHexStr;
begin
  Hex32 := Hex16( TStorageLong(L).W2 )+Hex16( TStorageLong(L).W1 );
end;

(*******************************************************************
 *
 *  Function    :  Cur_U_Line
 *
 *  Description :  Returns a string of the current unassembled
 *                 line at Code^[IP].
 *
 *  Input  :  Code    base code range
 *            IP      current instruction pointer
 *
 *  Output :  line string
 *
 *****************************************************************)

function Cur_U_Line( Code : PByte; IP : Int ) : DebugStr;
var
  Op   : Byte;
  N, I : Int;
  S    : DebugStr;
begin

  Op := Code^[IP];
  S  := Hex16(IP)+': '+Hex8(Op)+'  '+OpStr[Op];

  case Op of

    $40 : begin
           n := Code^[IP+1];
           S := S+'('+Hex8(n)+')';
           for i := 1 to n do
             S := S+' $'+Hex8( Code^[Ip+i+1] );
          end;

    $41 : begin
           n := Code^[IP+1];
           S := S+'('+Hex8(n)+')';
           for i := 1 to n do
             S := S+' $'+Hex8( Code^[Ip+i*2+1] )+Hex8( Code^[Ip+i*2+2] );
          end;

    $B0..$B7 : begin
                 n := Op-$B0;
                 for i := 0 to N do
                   S := S+' $'+Hex8( Code^[Ip+i+1] );
               end;

    $B8..$BF : begin
                 n := Op-$B8;
                 for i := 0 to N do
                   S := S+' $'+Hex8( Code^[IP+i*2+1] )+Hex8( Code^[Ip+i*2+2] );
               end;

  end;

  Cur_U_Line := S;
end;

(*******************************************************************
 *
 *  Function    :  Get_Length
 *
 *  Description :  Returns the length in bytes of the instruction at
 *                 current instruction pointer.
 *
 *  Input  :  Code  base code range
 *            IP    current instruction pointer
 *
 *  Output :  Length in bytes
 *
 *****************************************************************)

function Get_Length( Code : PByte; IP : Int ) : Int;
var
  Op    : Byte;
  N     : Int;
begin

  Op := Code^[IP];

  case Op of

    $40 : N := 2 + Code^[IP+1];
    $41 : N := 2 + Code^[IP+1]*2;

    $B0..$B7 : N := 2 + ( Op-$B0 );
    $B8..$BF : N := 3 + ( Op-$B8 )*2

  else
    N := 1;
  end;

  Get_Length := N;

end;

(*******************************************************************
 *
 *  Function    :  Generate_Range
 *
 *  Description :  Create a list of unassembled lines for a
 *                 given code range
 *
 *  Input  :
 *
 *  Output :
 *
 *****************************************************************)

procedure Generate_Range( CR     : PCodeRange;
                          index  : Int;
                          var RR : TRangeRec );
var
  Adr, Line, N : Int;
  Code         : PByte;
begin

  N    := CR^.Size;

  RR.Code := PByte( CR^.Base );
  RR.Size := N;

  Line := 0;

  if N > 0 then
  begin
    Adr  := 0;
    GetMem( RR.Disassembled, sizeof(Short) * N );

    while Adr < N do
      begin
        RR.Disassembled^[Line] := Adr;
        inc( Line );
        inc( Adr, Get_Length( RR.Code, Adr ));
      end;
  end;

  RR.NLines := Line;
  RR.Index  := index;
  RR.Breaks := nil;
end;

(*******************************************************************
 *
 *  Function    :  Get_Dis_Line
 *
 *  Description :  Returns the line index of address 'addr'
 *                 in the coderange 'cr'
 *
 *****************************************************************)

 function Get_Dis_Line( var cr : TRangeRec; addr : Int ) : Int;
 var
   l, r, m : Int;
 begin
   if (cr.NLines = 0) or
      (addr > cr.Disassembled^[cr.Nlines-1] ) then
     begin
       Get_Dis_Line := -1;
       exit;
     end;

   l := 0;
   r := cr.NLines-1;

   while ( r-l > 1 ) do
   begin
     if cr.Disassembled^[l] = addr then
       begin
         Get_Dis_Line := l;
         exit;
       end;

     if cr.Disassembled^[r] = addr then
       begin
         Get_Dis_Line := r;
         exit;
       end;

     m := (l+r) shr 1;
     if cr.Disassembled^[m] = addr then
       begin
         Get_Dis_Line := m;
         exit;
       end
     else
       if cr.Disassembled^[m] < addr then
         l := m
       else
         r := m;
   end;

   if cr.Disassembled^[r] = addr then
     begin
       Get_Dis_Line := r;
       exit;
     end;

   Get_Dis_Line := l;

 end;

(*******************************************************************
 *
 *  Function    :  Throw_Range
 *
 *  Description :  Destroys a list of unassembled lines for a
 *                 given code range
 *
 *  Input  :
 *
 *  Output :
 *
 *****************************************************************)

procedure Throw_Range( var RR : TRangeRec );
var
  B, Bnext : PBreakPoint;
begin

  if RR.Size > 0 then
    FreeMem( RR.Disassembled, RR.Size * sizeof(Short) );

  RR.Disassembled := nil;
  RR.Size         := 0;
  RR.Code         := nil;
  RR.NLines       := 0;

  B := RR.Breaks;
  RR.Breaks := nil;

  while B<>nil do
    begin
      Bnext := B^.Next;
      Dispose( B );
      B := Bnext;
    end;
end;

(*******************************************************************
 *
 *  Function    :  Set_Break
 *
 *  Description :  Sets a Breakpoint ON
 *
 *  Input  :
 *
 *  Output :
 *
 *****************************************************************)

procedure Set_Break( var Head : PBreakPoint;
                     Range    : Int;
                     Adr      : Int );
var
  BP,
  Old,
  Cur  : PBreakPoint;
begin
  Old := nil;
  Cur := Head;

  while (Cur <> nil) and (Cur^.Address < Adr) do
    begin
      Old := Cur;
      Cur := Cur^.Next;
    end;

  { No duplicates }
  if Cur <> nil then
    if (Cur^.Address = Adr) and (Cur^.Range = Range) then exit;

  New( BP );
  BP^.Address := Adr;
  BP^.Range   := Range;
  BP^.Next    := Cur;

  if Old = nil then
    Head := BP
  else
    Old^.Next := BP;
end;

(*******************************************************************
 *
 *  Function    :  Clear_Break
 *
 *  Description :  Clears a breakpoint OFF
 *
 *  Input  :
 *
 *  Output :
 *
 *****************************************************************)

procedure Clear_Break( var Head : PBreakPoint; Bp : PBreakPoint );
var
  Old,
  Cur  : PBreakPoint;
begin
  Old := nil;
  Cur := Head;

  while (Cur <> nil) and (Cur <> Bp) do
    begin
      Old := Cur;
      Cur := Cur^.Next;
    end;

  if Cur = nil then exit;

  if Old = nil then
    Head := Cur^.Next
  else
    Old^.Next := Cur^.Next;
end;



procedure Toggle_Break( var Head : PBreakPoint; Range, Adr : Int );
var
  Bp : PBreakPoint;
begin
 Bp := Find_BreakPoint( Head, Range, Adr );
 if Bp <> nil then Clear_Break( Head, Bp )
              else Set_Break( Head, Range, Adr );
end;

(*******************************************************************
 *
 *  Function    :  Clear_All_Breaks
 *
 *  Description :  Clears all breakpoints
 *
 *  Input  :
 *
 *  Output :
 *
 *****************************************************************)

procedure Clear_All_Breaks( var Head : PBreakPoint );
var
  Old,
  Cur  : PBreakPoint;
begin
  Cur  := Head;
  Head := Nil;

  while Cur <> nil do
    begin
      Old := Cur;
      Cur := Cur^.Next;
      Dispose( Old );
    end;
end;

(*******************************************************************
 *
 *  Function    :  Find_BreakPoint
 *
 *  Description :  Find a breakpoint at address IP
 *
 *  Input  :   Head     break points sorted linked list
 *             IP       address of expected breakpoint
 *
 *  Output :   pointer to breakpoint if found
 *             nil otherwise.
 *
 *****************************************************************)

function Find_BreakPoint( Head : PBreakPoint; Range, IP : Int ) : PBreakPoint;
var
  Cur : PBreakPoint;
  Res : PBreakPoint;
begin
  Cur := Head;
  Res := nil;

  while Cur <> nil do
  begin
    if (Cur^.Address = IP   ) and
       (Cur^.Range   = Range) then Res := Cur;

    if (Cur^.Address >= IP) then Cur := nil
                            else Cur := Cur^.Next;
  end;

  Find_BreakPoint := Res;
end;

end.
