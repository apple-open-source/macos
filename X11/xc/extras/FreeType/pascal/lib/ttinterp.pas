(*******************************************************************
 *
 *  TTInterp.pas                                              2.0
 *
 *  TrueType bytecode intepreter.
 *
 *  Copyright 1996 David Turner, Robert Wilhelm and Werner Lemberg
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 *
 *  Changes between 2.0 and 1.2 :
 *
 *  - Lots, lots, of changes : This version is not re-entrant,
 *    but much faster.
 *
 *
 ******************************************************************)

unit TTInterp;

interface

uses FreeType,
     TTTypes,
     TTObjs;

  function Run_Ins( exec : PExec_Context ) : Boolean;
  (* Run the interpreter with the current code range and IP *)

implementation
uses
  TTError,
  TTMemory,
  TTCalc;

type
  TInstruction_Function = procedure( args : PStorage );

const
  Null_Vector : TT_Vector = (x:0;y:0);

var
  exc : TExec_Context;  (* static variable *)

const

  (*********************************************************************)
  (*                                                                   *)
  (*  Before an opcode is executed, the interpreter verifies that      *)
  (*  there are enough arguments on the stack, with the help of        *)
  (*  the Pop_Push_Count table.                                        *)
  (*                                                                   *)
  (*  Note that for opcodes with a varying numbre of parameters,       *)
  (*  either 0 or 1 arg is verified before execution, depending        *)
  (*  on the nature of the instruction :                               *)
  (*                                                                   *)
  (*   - if the number of arguments is given by the bytecode           *)
  (*     stream or the loop variable, 0 is chosen.                     *)
  (*                                                                   *)
  (*   - if the first argument is a count n that is followed           *)
  (*     by arguments a1..an, then 1 is chosen.                        *)
  (*                                                                   *)
  (*********************************************************************)

  Pop_Push_Count : array[0..511] of byte
                 = (
                     (* SVTCA  y *)  0, 0,
                     (* SVTCA  x *)  0, 0,
                     (* SPvTCA y *)  0, 0,
                     (* SPvTCA x *)  0, 0,
                     (* SFvTCA y *)  0, 0,
                     (* SFvTCA x *)  0, 0,
                     (* SPvTL // *)  2, 0,
                     (* SPvTL +  *)  2, 0,
                     (* SFvTL // *)  2, 0,
                     (* SFvTL +  *)  2, 0,
                     (* SPvFS    *)  2, 0,
                     (* SFvFS    *)  2, 0,
                     (* GPV      *)  0, 2,
                     (* GFV      *)  0, 2,
                     (* SFvTPv   *)  0, 0,
                     (* ISECT    *)  5, 0,

                     (* SRP0     *)  1, 0,
                     (* SRP1     *)  1, 0,
                     (* SRP2     *)  1, 0,
                     (* SZP0     *)  1, 0,
                     (* SZP1     *)  1, 0,
                     (* SZP2     *)  1, 0,
                     (* SZPS     *)  1, 0,
                     (* SLOOP    *)  1, 0,
                     (* RTG      *)  0, 0,
                     (* RTHG     *)  0, 0,
                     (* SMD      *)  1, 0,
                     (* ELSE     *)  0, 0,
                     (* JMPR     *)  1, 0,
                     (* SCvTCi   *)  1, 0,
                     (* SSwCi    *)  1, 0,
                     (* SSW      *)  1, 0,

                     (* DUP      *)  1, 2,
                     (* POP      *)  1, 0,
                     (* CLEAR    *)  0, 0,
                     (* SWAP     *)  2, 2,
                     (* DEPTH    *)  0, 1,
                     (* CINDEX   *)  1, 1,
                     (* MINDEX   *)  1, 0, (* first arg *)
                     (* AlignPTS *)  2, 0,
                     (* INS_$28  *)  0, 0,
                     (* UTP      *)  1, 0,
                     (* LOOPCALL *)  2, 0,
                     (* CALL     *)  1, 0,
                     (* FDEF     *)  1, 0,
                     (* ENDF     *)  0, 0,
                     (* MDAP[0]  *)  1, 0,
                     (* MDAP[1]  *)  1, 0,

                     (* IUP[0]   *)  0, 0,
                     (* IUP[1]   *)  0, 0,
                     (* SHP[0]   *)  0, 0,  (* no args *)
                     (* SHP[1]   *)  0, 0,  (* no args *)
                     (* SHC[0]   *)  1, 0,
                     (* SHC[1]   *)  1, 0,
                     (* SHZ[0]   *)  1, 0,
                     (* SHZ[1]   *)  1, 0,
                     (* SHPIX    *)  1, 0,  (* first arg *)
                     (* IP       *)  0, 0,  (* no args   *)
                     (* MSIRP[0] *)  2, 0,
                     (* MSIRP[1] *)  2, 0,
                     (* AlignRP  *)  0, 0,  (* no args *)
                     (* RTDG     *)  0, 0,
                     (* MIAP[0]  *)  2, 0,
                     (* MIAP[1]  *)  2, 0,

                     (* NPushB   *)  0, 0,
                     (* NPushW   *)  0, 0,
                     (* WS       *)  2, 0,
                     (* RS       *)  1, 1,
                     (* WCvtP    *)  2, 0,
                     (* RCvt     *)  1, 1,
                     (* GC[0]    *)  1, 1,
                     (* GC[1]    *)  1, 1,
                     (* SCFS     *)  2, 0,
                     (* MD[0]    *)  2, 1,
                     (* MD[1]    *)  2, 1,
                     (* MPPEM    *)  0, 1,
                     (* MPS      *)  0, 1,
                     (* FlipON   *)  0, 0,
                     (* FlipOFF  *)  0, 0,
                     (* DEBUG    *)  1, 0,

                     (* LT       *)  2, 1,
                     (* LTEQ     *)  2, 1,
                     (* GT       *)  2, 1,
                     (* GTEQ     *)  2, 1,
                     (* EQ       *)  2, 1,
                     (* NEQ      *)  2, 1,
                     (* ODD      *)  1, 1,
                     (* EVEN     *)  1, 1,
                     (* IF       *)  1, 0,
                     (* EIF      *)  0, 0,
                     (* AND      *)  2, 1,
                     (* OR       *)  2, 1,
                     (* NOT      *)  1, 1,
                     (* DeltaP1  *)  1, 0, (* first arg *)
                     (* SDB      *)  1, 0,
                     (* SDS      *)  1, 0,

                     (* ADD      *)  2, 1,
                     (* SUB      *)  2, 1,
                     (* DIV      *)  2, 1,
                     (* MUL      *)  2, 1,
                     (* ABS      *)  1, 1,
                     (* NEG      *)  1, 1,
                     (* FLOOR    *)  1, 1,
                     (* CEILING  *)  1, 1,
                     (* ROUND[0] *)  1, 1,
                     (* ROUND[1] *)  1, 1,
                     (* ROUND[2] *)  1, 1,
                     (* ROUND[3] *)  1, 1,
                     (* NROUND[0]*)  1, 1,
                     (* NROUND[1]*)  1, 1,
                     (* NROUND[2]*)  1, 1,
                     (* NROUND[3]*)  1, 1,

                     (* WCvtF    *)  2, 0,
                     (* DeltaP2  *)  1, 0,  (* first arg *)
                     (* DeltaP3  *)  1, 0,  (* first arg *)
                     (* DeltaCn[0]*) 1, 0,  (* first arg *)
                     (* DeltaCn[1]*) 1, 0,  (* first arg *)
                     (* DeltaCn[2]*) 1, 0,  (* first arg *)
                     (* SROUND   *)  1, 0,
                     (* S45Round *)  1, 0,
                     (* JROT     *)  2, 0,
                     (* JROF     *)  2, 0,
                     (* ROFF     *)  0, 0,
                     (* INS_$7B  *)  0, 0,
                     (* RUTG     *)  0, 0,
                     (* RDTG     *)  0, 0,
                     (* SANGW    *)  1, 0,
                     (* AA       *)  1, 0,

                     (* FlipPT   *)  0, 0,  (* no args *)
                     (* FlipRgON *)  2, 0,
                     (* FlipRgOFF*)  2, 0,
                     (* INS_$83  *)  0, 0,
                     (* INS_$84  *)  0, 0,
                     (* ScanCTRL *)  1, 0,
                     (* SDVPTL[0]*)  2, 0,
                     (* SDVPTL[1]*)  2, 0,
                     (* GetINFO  *)  1, 1,
                     (* IDEF     *)  1, 0,
                     (* ROLL     *)  3, 3,  (* pops 3 args/push 3 args *)
                     (* MAX      *)  2, 1,
                     (* MIN      *)  2, 1,
                     (* ScanTYPE *)  1, 0,
                     (* InstCTRL *)  2, 0,
                     (* INS_$8F  *)  0, 0,

                     (* INS_$90 *)   0, 0,
                     (* INS_$91 *)   0, 0,
                     (* INS_$92 *)   0, 0,
                     (* INS_$93 *)   0, 0,
                     (* INS_$94 *)   0, 0,
                     (* INS_$95 *)   0, 0,
                     (* INS_$96 *)   0, 0,
                     (* INS_$97 *)   0, 0,
                     (* INS_$98 *)   0, 0,
                     (* INS_$99 *)   0, 0,
                     (* INS_$9A *)   0, 0,
                     (* INS_$9B *)   0, 0,
                     (* INS_$9C *)   0, 0,
                     (* INS_$9D *)   0, 0,
                     (* INS_$9E *)   0, 0,
                     (* INS_$9F *)   0, 0,

                     (* INS_$A0 *)   0, 0,
                     (* INS_$A1 *)   0, 0,
                     (* INS_$A2 *)   0, 0,
                     (* INS_$A3 *)   0, 0,
                     (* INS_$A4 *)   0, 0,
                     (* INS_$A5 *)   0, 0,
                     (* INS_$A6 *)   0, 0,
                     (* INS_$A7 *)   0, 0,
                     (* INS_$A8 *)   0, 0,
                     (* INS_$A9 *)   0, 0,
                     (* INS_$AA *)   0, 0,
                     (* INS_$AB *)   0, 0,
                     (* INS_$AC *)   0, 0,
                     (* INS_$AD *)   0, 0,
                     (* INS_$AE *)   0, 0,
                     (* INS_$AF *)   0, 0,

                     (* PushB[0] *)  0, 1,
                     (* PushB[1] *)  0, 2,
                     (* PushB[2] *)  0, 3,
                     (* PushB[3] *)  0, 4,
                     (* PushB[4] *)  0, 5,
                     (* PushB[5] *)  0, 6,
                     (* PushB[6] *)  0, 7,
                     (* PushB[7] *)  0, 8,
                     (* PushW[0] *)  0, 1,
                     (* PushW[1] *)  0, 2,
                     (* PushW[2] *)  0, 3,
                     (* PushW[3] *)  0, 4,
                     (* PushW[4] *)  0, 5,
                     (* PushW[5] *)  0, 6,
                     (* PushW[6] *)  0, 7,
                     (* PushW[7] *)  0, 8,

                     (* MDRP[00] *)  1, 0,
                     (* MDRP[01] *)  1, 0,
                     (* MDRP[02] *)  1, 0,
                     (* MDRP[03] *)  1, 0,
                     (* MDRP[04] *)  1, 0,
                     (* MDRP[05] *)  1, 0,
                     (* MDRP[06] *)  1, 0,
                     (* MDRP[07] *)  1, 0,
                     (* MDRP[08] *)  1, 0,
                     (* MDRP[09] *)  1, 0,
                     (* MDRP[10] *)  1, 0,
                     (* MDRP[11] *)  1, 0,
                     (* MDRP[12] *)  1, 0,
                     (* MDRP[13] *)  1, 0,
                     (* MDRP[14] *)  1, 0,
                     (* MDRP[15] *)  1, 0,
                     (* MDRP[16] *)  1, 0,
                     (* MDRP[17] *)  1, 0,

                     (* MDRP[18] *)  1, 0,
                     (* MDRP[19] *)  1, 0,
                     (* MDRP[20] *)  1, 0,
                     (* MDRP[21] *)  1, 0,
                     (* MDRP[22] *)  1, 0,
                     (* MDRP[23] *)  1, 0,
                     (* MDRP[24] *)  1, 0,
                     (* MDRP[25] *)  1, 0,
                     (* MDRP[26] *)  1, 0,
                     (* MDRP[27] *)  1, 0,
                     (* MDRP[28] *)  1, 0,
                     (* MDRP[29] *)  1, 0,
                     (* MDRP[30] *)  1, 0,
                     (* MDRP[31] *)  1, 0,

                     (* MIRP[00] *)  2, 0,
                     (* MIRP[01] *)  2, 0,
                     (* MIRP[02] *)  2, 0,
                     (* MIRP[03] *)  2, 0,
                     (* MIRP[04] *)  2, 0,
                     (* MIRP[05] *)  2, 0,
                     (* MIRP[06] *)  2, 0,
                     (* MIRP[07] *)  2, 0,
                     (* MIRP[08] *)  2, 0,
                     (* MIRP[09] *)  2, 0,
                     (* MIRP[10] *)  2, 0,
                     (* MIRP[11] *)  2, 0,
                     (* MIRP[12] *)  2, 0,
                     (* MIRP[13] *)  2, 0,
                     (* MIRP[14] *)  2, 0,
                     (* MIRP[15] *)  2, 0,
                     (* MIRP[16] *)  2, 0,
                     (* MIRP[17] *)  2, 0,

                     (* MIRP[18] *)  2, 0,
                     (* MIRP[19] *)  2, 0,
                     (* MIRP[20] *)  2, 0,
                     (* MIRP[21] *)  2, 0,
                     (* MIRP[22] *)  2, 0,
                     (* MIRP[23] *)  2, 0,
                     (* MIRP[24] *)  2, 0,
                     (* MIRP[25] *)  2, 0,
                     (* MIRP[26] *)  2, 0,
                     (* MIRP[27] *)  2, 0,
                     (* MIRP[28] *)  2, 0,
                     (* MIRP[29] *)  2, 0,
                     (* MIRP[30] *)  2, 0,
                     (* MIRP[31] *)  2, 0
                   );


(*******************************************************************
 *
 *  Function    :  Norm
 *
 *  Description :  returns the norm (length) of a vector
 *
 *  Input  :  X, Y   vector
 *
 *  Output :  returns length in F26dot6
 *
 *****************************************************************)

 function Norm( X, Y : TT_F26dot6 ): TT_F26dot6;
 var
   T1, T2 : Int64;
 begin
   MulTo64( X, X, T1 );
   MulTo64( Y, Y, T2 );

   Add64( T1, T2, T1 );

   if ( (T1.lo or T1.Hi) = 0 ) then Norm := 0
                               else Norm := Sqrt64( T1 );
 end;

(*******************************************************************
 *
 *  Function    :  Scale_Pixels
 *
 *  Description :  Converts from FUnits to Fractional pixels
 *                 coordinates.
 *
 *****************************************************************)

  function Scale_Pixels( value : long ) : TT_F26Dot6;
  {$IFDEF INLINE} inline; {$ENDIF}
  begin
    Scale_Pixels := MulDiv_Round( value,
                                  exc.metrics.scale1,
                                  exc.metrics.scale2 );
  end;

  function Get_Current_Ratio : Long;
  var
    x, y : Long;
  begin
    if exc.metrics.ratio <> 0 then
      Get_Current_Ratio := exc.metrics.ratio
    else
    begin
      if exc.GS.projVector.y = 0 then
        exc.metrics.ratio := exc.metrics.x_ratio

      else if exc.GS.projVector.x = 0 then
        exc.metrics.ratio := exc.metrics.y_ratio

      else
        begin
          x := MulDiv_Round( exc.GS.projVector.x,
                             exc.metrics.x_ratio,
                             $4000 );

          y := MulDiv_Round( exc.GS.projVector.y,
                             exc.metrics.y_ratio,
                             $4000 );

          exc.metrics.ratio := Norm( x, y );
        end;

      Get_Current_Ratio := exc.metrics.ratio;
    end
  end;

  function Get_Ppem : Long;
  {$IFDEF INLINE} inline; {$ENDIF}
  begin
    Get_Ppem := MulDiv_Round( exc.metrics.ppem, Get_Current_Ratio, $10000 );
  end;


  function Read_CVT( index : Int ) : TT_F26Dot6;
  {$IFNDEF FPK} far; {$ENDIF}
  begin
    Read_CVT := exc.cvt^[index];
  end;

  function Read_CVT_Stretched( index : Int ) : TT_F26Dot6; far;
  begin
    Read_CVT_Stretched := MulDiv_Round( exc.cvt^[index],
                                        Get_Current_Ratio,
                                        $10000 );
  end;


  procedure Write_CVT( index : Int; value : TT_F26Dot6 ); far;
  begin
    exc.cvt^[index] := value;
  end;

  procedure Write_CVT_Stretched( index : Int; value : TT_F26Dot6 ); far;
  begin
    exc.cvt^[index] := MulDiv_Round( value,
                                     $10000,
                                     Get_Current_Ratio );
  end;


  procedure Move_CVT( index : Int; value : TT_F26Dot6 ); far;
  begin
    inc( exc.cvt^[index], value );
  end;

  procedure Move_CVT_Stretched( index : Int; value : TT_F26dot6 ); far;
  begin
    inc( exc.cvt^[index], MulDiv_Round( value,
                                        $10000,
                                        Get_Current_Ratio ));
  end;

(*******************************************************************
 *
 *  Function    :  Calc_Length
 *
 *  Description :  Computes the length in bytes of current opcode
 *
 *****************************************************************)

 function Calc_Length : boolean;
 begin
   Calc_Length := false;

   exc.opcode := exc.Code^[exc.IP];

   case exc.opcode of

     $40 : if exc.IP+1 >= exc.codeSize
             then exit
           else
             exc.length := exc.code^[exc.IP+1]   + 2;

     $41 : if exc.IP+1 >= exc.codeSize
             then exit
           else
             exc.length := exc.code^[exc.IP+1]*2 + 2;

     $B0..$B7 : exc.length :=  exc.opcode-$B0    + 2;
     $B8..$BF : exc.length := (exc.opcode-$B8)*2 + 3;
   else
     exc.length := 1;
   end;

   Calc_Length := exc.IP+exc.length <= exc.codeSize;
 end;

(*******************************************************************
 *
 *  Function    :  Get_Short
 *
 *  Description :  Return a short integer taken from the instruction
 *                 stream at address IP.
 *
 *  Input  :  None
 *
 *  Output :  Short read at Code^[IP..IP+1]
 *
 *  Notes  :  This one could become a Macro in the C version
 *
 *****************************************************************)

 function GetShort : Short;
 var
   L        : Array[0..1] of Byte;
   resultat : Short absolute L;  (* XXX : un-portable *)
 begin
   (* This is little-endian code *)

   L[1]     := exc.code^[exc.IP]; inc(exc.IP);
   L[0]     := exc.code^[exc.IP]; inc(exc.IP);
   GetShort := resultat;
 end;


 function Goto_CodeRange( aRange,
                          aIP     : Int ): boolean;
 begin

   Goto_CodeRange := False;

   with exc do
   begin
     if (aRange<1) or (aRange>3) then
       begin
         exc.error := TT_Err_Bad_Argument;
         exit;
       end;

     with CodeRangeTable[ARange] do
       begin

         if Base = nil then  (* invalid coderange *)
         begin
           error := TT_Err_Invalid_Coderange;
           exit;
         end;

         (* NOTE : Because the last instruction of a program may be a CALL *)
         (*        which will return to the first byte *after* the code    *)
         (*        range, we test for AIP <= Size, instead of AIP < Size   *)

         if AIP > Size then
           begin
             error          := TT_Err_Code_Overflow;
             Goto_CodeRange := False;
             exit;
           end;

         Code     := PByte(Base);
         CodeSize := Size;
         IP       := AIP;
       end;

     curRange := ARange;
   end;

   Goto_CodeRange := True;
 end;


(*******************************************************************
 *
 *  Function    :  Direct_Move
 *
 *  Description :  Moves a point by a given distance along the
 *                 freedom vector.
 *
 *  Input  : Vx, Vy      point coordinates to move
 *           touch       touch flag to modify
 *           distance
 *
 *  Output :  None
 *
 *****************************************************************)

 {$F+}
 procedure Direct_Move( zone     : PGlyph_Zone;
                        point    : Int;
                        distance : TT_F26dot6 );
 var
   v : TT_F26dot6;
 begin
   v := exc.GS.freeVector.x;
   if v <> 0 then
   begin
     inc( zone^.cur^[point].x, MulDiv_Round( distance,
                                             Long(v)*$10000,
                                             exc.F_dot_P ));

     zone^.flags^[point] := zone^.flags^[point] or TT_Flag_Touched_X;
   end;

   v := exc.GS.freeVector.y;
   if v <> 0 then
   begin
     inc( zone^.cur^[point].y, MulDiv_Round( distance,
                                             Long(v)*$10000,
                                             exc.F_dot_P ));

     zone^.flags^[point] := zone^.flags^[point] or TT_Flag_Touched_Y;
   end;
 end;

 (* The following versions are used whenever both vectors are both *)
 (* along one of the coordinate unit vectors, i.e. in 90% cases    *)

 procedure Direct_Move_X( zone     : PGlyph_Zone;
                          point    : Int;
                          distance : TT_F26dot6 );
 begin
   inc( zone^.cur^[point].x, distance );
   zone^.flags^[point] := zone^.flags^[point] or TT_Flag_Touched_X;
 end;

 procedure Direct_Move_Y( zone     : PGlyph_Zone;
                          point    : Int;
                          distance : TT_F26dot6 );
 begin
   inc( zone^.cur^[point].y, distance );
   zone^.flags^[point] := zone^.flags^[point] or TT_Flag_Touched_Y;
 end;

(*******************************************************************
 *
 *  Function    :  Round_None
 *
 *  Description :  Do not round, but add engine compensation
 *
 *  Input  :  distance      : distance to round
 *            compensation  : engine compensation
 *
 *  Output :  rounded distance
 *
 *  NOTE : The spec says very few about the relationship between
 *         rounding and engine compensation. However, it seems
 *         from the description of super round that we should
 *         should add the compensation before rounding
 *
 *****************************************************************)

 function Round_None( distance     : TT_F26dot6;
                      compensation : TT_F26dot6 ) : TT_F26dot6;
 var
   val : TT_F26dot6;
 begin
   if distance >= 0 then
     begin
       val := distance + compensation;
       if val < 0 then val := 0;
     end
   else
     begin
       val := distance - compensation;
       if val > 0 then val := 0;
     end;

   Round_None := val;
 end;

(*******************************************************************
 *
 *  Function    :  Round_To_Grid
 *
 *  Description :  round value to grid after adding engine
 *                 compensation
 *
 *  Input  :  distance      : distance to round
 *            compensation  : engine compensation
 *
 *  Output :  rounded distance
 *
 *****************************************************************)

 function Round_To_Grid( distance     : TT_F26dot6;
                         compensation : TT_F26dot6 ) : TT_F26dot6;
 var
   val : TT_F26dot6;
 begin
   if distance >= 0 then
     begin
       val := (distance + 32 + compensation) and -64;
       if val < 0 then val := 0;
     end
   else
     begin
       val := - ((compensation - distance + 32) and -64);
       if val > 0 then val := 0;
     end;

   Round_To_Grid := val;
 end;

(*******************************************************************
 *
 *  Function    :  Round_To_Half_Grid
 *
 *  Description :  round value to half grid after adding engine
 *                 compensation
 *
 *  Input  :  distance      : distance to round
 *            compensation  : engine compensation
 *
 *  Output :  rounded distance
 *
 *****************************************************************)

 function Round_To_Half_Grid( distance     : TT_F26dot6;
                         compensation : TT_F26dot6 ) : TT_F26dot6;
 var
   val : TT_F26dot6;
 begin
   if distance >= 0 then
     begin
       val := (distance + compensation) and -64 + 32;
       if val < 0 then val := 0;
     end
   else
     begin
       val := - ((-distance + compensation) and -64 + 32);
       if val > 0 then val := 0;
     end;

   Round_To_Half_Grid := val;
 end;


(*******************************************************************
 *
 *  Function    :  Round_Down_To_Grid
 *
 *  Description :  round value down to grid after adding engine
 *                 compensation
 *
 *  Input  :  distance      : distance to round
 *            compensation  : engine compensation
 *
 *  Output :  rounded distance
 *
 *****************************************************************)

 function Round_Down_To_Grid( distance     : TT_F26dot6;
                              compensation : TT_F26dot6 ) : TT_F26dot6;
 var
   val : TT_F26dot6;
 begin
   if distance >= 0 then
     begin
       val := (distance + compensation) and -64;
       if val < 0 then val := 0;
     end
   else
     begin
       val := - ((-distance + compensation) and -64);
       if val > 0 then val := 0;
     end;

   Round_Down_To_Grid := val;
 end;

(*******************************************************************
 *
 *  Function    :  Round_Up_To_Grid
 *
 *  Description :  round value up to grid after adding engine
 *                 compensation
 *
 *  Input  :  distance      : distance to round
 *            compensation  : engine compensation
 *
 *  Output :  rounded distance
 *
 *****************************************************************)

 function Round_Up_To_Grid( distance     : TT_F26dot6;
                            compensation : TT_F26dot6 ) : TT_F26dot6;
 var
   val : TT_F26dot6;
 begin
   if distance >= 0 then
     begin
       val := (distance + 63 + compensation) and -64;
       if val < 0 then val := 0;
     end
   else
     begin
       val := - ((-distance + 63 + compensation) and -64);
       if val > 0 then val := 0;
     end;

   Round_Up_To_Grid := val;
 end;

(*******************************************************************
 *
 *  Function    :  Round_To_Double_Grid
 *
 *  Description :  round value to double grid after adding engine
 *                 compensation
 *
 *  Input  :  distance      : distance to round
 *            compensation  : engine compensation
 *
 *  Output :  rounded distance
 *
 *****************************************************************)

 function Round_To_Double_Grid( distance     : TT_F26dot6;
                                compensation : TT_F26dot6 ) : TT_F26dot6;
 var
   val : TT_F26dot6;
 begin
   if distance >= 0 then
     begin
       val := (distance + 16 + compensation) and -32;
       if val < 0 then val := 0;
     end
   else
     begin
       val := - ((-distance + 16 + compensation) and -32);
       if val > 0 then val := 0;
     end;

   Round_To_Double_Grid := val;
 end;

(*******************************************************************
 *
 *  Function    :  Round_Super
 *
 *  Description :  super round value to grid after adding engine
 *                 compensation
 *
 *  Input  :  distance      : distance to round
 *            compensation  : engine compensation
 *
 *  Output :  rounded distance
 *
 *  NOTE : The spec says very few about the relationship between
 *         rounding and engine compensation. However, it seems
 *         from the description of super round that we should
 *         should add the compensation before rounding
 *
 *****************************************************************)

 function Round_Super( distance     : TT_F26dot6;
                       compensation : TT_F26dot6 ) : TT_F26dot6;
 var
   val : TT_F26dot6;
 begin
   with exc do

     if distance >= 0 then
       begin
         val := (distance - phase + threshold + compensation) and -period;
         if val < 0 then val := 0;
         val := val + phase;
       end
     else
       begin
         val := -((-distance - phase + threshold + compensation) and -period);
         if val > 0 then val := 0;
         val := val - phase;
       end;

   Round_Super := val;
 end;

(*******************************************************************
 *
 *  Function    :  Round_Super_45
 *
 *  Description :  super round value to grid after adding engine
 *                 compensation
 *
 *  Input  :  distance      : distance to round
 *            compensation  : engine compensation
 *
 *  Output :  rounded distance
 *
 *  NOTE : There is a separate function for Round_Super_45 as we
 *         may need a greater precision.
 *
 *****************************************************************)

 function Round_Super_45( distance     : TT_F26dot6;
                          compensation : TT_F26dot6 ) : TT_F26dot6;
 var
   val : TT_F26dot6;
 begin
   with exc do

     if distance >= 0 then
       begin
         val := ((distance - phase + threshold + compensation) div period)
                * period;
         if val < 0 then val := 0;
         val := val + phase;
       end
     else
       begin
         val := -((-distance - phase + threshold + compensation) div period
                   * period );
         if val > 0 then val := 0;
         val := val - phase;
       end;

   Round_Super_45 := val;
 end;
 {$F-}

 procedure Compute_Round( round_mode : Byte );
 begin
   case Round_Mode of

{$IFDEF FPK}
     TT_Round_Off            : exc.func_round := @Round_None;
     TT_Round_To_Grid        : exc.func_round := @Round_To_Grid;
     TT_Round_Up_To_Grid     : exc.func_round := @Round_Up_To_Grid;
     TT_Round_Down_To_Grid   : exc.func_round := @Round_Down_To_Grid;
     TT_Round_To_Half_Grid   : exc.func_round := @Round_To_Half_Grid;
     TT_Round_To_Double_Grid : exc.func_round := @Round_To_Double_Grid;
     TT_Round_Super          : exc.func_round := @Round_Super;
     TT_Round_Super_45       : exc.func_round := @Round_Super_45;
{$ELSE}
     TT_Round_Off            : exc.func_round := Round_None;
     TT_Round_To_Grid        : exc.func_round := Round_To_Grid;
     TT_Round_Up_To_Grid     : exc.func_round := Round_Up_To_Grid;
     TT_Round_Down_To_Grid   : exc.func_round := Round_Down_To_Grid;
     TT_Round_To_Half_Grid   : exc.func_round := Round_To_Half_Grid;
     TT_Round_To_Double_Grid : exc.func_round := Round_To_Double_Grid;
     TT_Round_Super          : exc.func_round := Round_Super;
     TT_Round_Super_45       : exc.func_round := Round_Super_45;
{$ENDIF}
   end;
 end;


(*******************************************************************
 *
 *  Function    :  SetSuperRound
 *
 *  Description :  Set Super Round parameters
 *
 *  Input  :  GridPeriod   Grid period
 *            OpCode       SROUND opcode
 *
 *  Output :  None
 *
 *  Notes  :
 *
 *****************************************************************)

 procedure SetSuperRound( GridPeriod : TT_F26dot6; selector : Long );

 begin
   with exc do
   begin

     Case selector and $C0 of

      $00 : period := GridPeriod div 2;
      $40 : period := GridPeriod;
      $80 : period := GridPeriod * 2;

      (* This opcode is reserved, but ... *)

      $C0 : period := GridPeriod;
     end;

     Case selector and $30 of

      $00 : phase := 0;
      $10 : phase := period div 4;
      $20 : phase := period div 2;
      $30 : phase := gridPeriod*3 div 4;
     end;

     if selector and $F = 0 then

        Threshold := Period-1
      else
        Threshold := (Integer( selector and $F )-4)*period div 8;

     period    := period div 256;
     phase     := phase div 256;
     threshold := threshold div 256;

   end
 end;

(*******************************************************************
 *
 *  Function    :  Project
 *
 *  Description :  Computes the projection of (Vx,Vy) along the
 *                 current projection vector
 *
 *  Input  :  Vx, Vy    input vector
 *
 *  Output :  return distance in F26dot6
 *
 *****************************************************************)

 {$F+}
 function Project( var P1, P2 : TT_Vector ) : TT_F26dot6;
 var
   T1, T2 : Int64;
 begin
   with exc.GS.projVector do
   begin
     MulTo64( P1.x - P2.x, x, T1 );
     MulTo64( P1.y - P2.y, y, T2 );
   end;

   Add64( T1, T2, T1 );

   Project := Div64by32( T1, $4000 );
 end;


 function Dual_Project( var P1, P2 : TT_Vector ) : TT_F26dot6;
 var
   T1, T2 : Int64;
 begin
   with exc.GS.dualVector do
   begin
     MulTo64( P1.x - P2.x, x, T1 );
     MulTo64( P1.y - P2.y, y, T2 );
   end;

   Add64( T1, T2, T1 );

   Dual_Project := Div64by32( T1, $4000 );
 end;


 function Free_Project( var P1, P2 : TT_Vector ) : TT_F26dot6;
 var
   T1, T2 : Int64;
 begin
   with exc.GS.freeVector do
   begin
     MulTo64( P1.x - P2.x, x, T1 );
     MulTo64( P1.y - P2.y, y, T2 );
   end;

   Add64( T1, T2, T1 );

   Free_Project := Div64by32( T1, $4000 );
 end;


 function Project_x( var P1, P2 : TT_Vector ) : TT_F26dot6;
 begin
   Project_x := P1.x - P2.x;
 end;

 function Project_y( var P1, P2 : TT_Vector ) : TT_F26dot6;
 begin
   Project_y := P1.y - P2.y;
 end;
 {$F-}

(*******************************************************************
 *
 *  Function    :  Compute_Funcs
 *
 *  Description :  Computes the projections and movement function
 *                 pointers according to the current graphics state
 *
 *  Input  :  None
 *
 *****************************************************************)

 procedure Compute_Funcs;
 begin
   with exc, GS do
   begin

     if (freeVector.x = $4000) then
       begin
{$IFDEF FPK}
         func_freeProj := @Project_x;
{$ELSE}
         func_freeProj := Project_x;
{$ENDIF}
         F_dot_P       := Long(projVector.x) * $10000;
       end
     else
     if (freeVector.y = $4000) then
       begin
{$IFDEF FPK}
         func_freeProj := @Project_y;
{$ELSE}
         func_freeProj := Project_y;
{$ENDIF}
         F_dot_P       := Long(projVector.y) * $10000;
       end
     else
       begin
{$IFDEF FPK}
         func_move     := @Direct_Move;
         func_freeProj := @Free_Project;
{$ELSE}
         func_move     := Direct_Move;
         func_freeProj := Free_Project;
{$ENDIF}
         F_dot_P       := Long(projVector.x) * freeVector.x * 4 +
                          Long(projVector.y) * freeVector.y * 4;
       end;

{$IFDEF FPK}
     if (projVector.x = $4000) then func_Project := @Project_x
     else
     if (projVector.y = $4000) then func_Project := @Project_y
     else
                                    func_Project := @Project;

     if (dualVector.x = $4000) then func_dualproj := @Project_x
     else
     if (dualVector.y = $4000) then func_dualproj := @Project_y
     else
                                    func_dualproj := @Dual_Project;

     func_move := @Direct_Move;

     if F_dot_P = $40000000 then

       if freeVector.x = $4000 then func_move := @Direct_Move_x
       else
       if freeVector.y = $4000 then func_move := @Direct_Move_y;
{$ELSE}
     if (projVector.x = $4000) then func_Project := Project_x
     else
     if (projVector.y = $4000) then func_Project := Project_y
     else
                                    func_Project := Project;

     if (dualVector.x = $4000) then func_dualproj := Project_x
     else
     if (dualVector.y = $4000) then func_dualproj := Project_y
     else
                                    func_dualproj := Dual_Project;

     func_move := Direct_Move;

     if F_dot_P = $40000000 then

       if freeVector.x = $4000 then func_move := Direct_Move_x
       else
       if freeVector.y = $4000 then func_move := Direct_Move_y;
{$ENDIF}

     (* at small sizes, F_dot_P can become too small, resulting *)
     (* in overflows and 'spikes' in a number of glyfs like 'w' *)

     if abs( F_dot_P ) < $4000000 then F_dot_P := $40000000;

     (* set aspect ratio to 0 to force recomputation by Get_Current_Ratio *)
     metrics.ratio := 0;
   end;
 end;


(**************************************************)
(*                                                *)
(* Normalize :  Normer un vecteur ( U, V )        *)
(*              r‚sultat dans     ( X, Y )        *)
(*              False si vecteur paramŠtre nul    *)
(*                                                *)
(**************************************************)

function Normalize( U, V : TT_F26dot6; var R : TT_UnitVector ): boolean;
var
  Vec     : TT_Vector;
  W       : TT_F26dot6;
  S1, S2  : Boolean;
  T       : Int64;
begin

  if (Abs(U) < $10000) and (Abs(V) < $10000) then
    begin
      U := U*$100;
      V := V*$100;

      W := Norm( U, V );
      if W = 0 then
        begin
          (* XXX : Undocumented. Apparently, it is possible to try *)
          (*       to normalize the vector (0,0). Return success   *)
          (*       in this case                                    *)
          Normalize := SUCCESS;
          exit;
        end;

      R.x := MulDiv( U, $4000, W );
      R.y := MulDiv( V, $4000, W );

    end
  else
    begin

      W := Norm( U, V );

      if W > 0 then
       begin
        U := MulDiv( U, $4000, W );
        V := MulDiv( V, $4000, W );

        W := U*U + V*V;

        (* Now, we want that Sqrt( W ) = $4000 *)
        (* Or $1000000 <= W < $1004000         *)

        if U < 0 then begin U := -U; S1 := True; end else S1 := False;
        if V < 0 then begin V := -V; S2 := True; end else S2 := False;

        while W < $1000000 do
         begin
           (* We need to increase W, by a minimal amount *)
           if U < V then inc( U )
                    else inc( V );
           W := U*U + V*V;
         end;

        while W >= $1004000 do
         begin
           (* We need to decrease W, by a minimal amount *)
           if U < V then dec( U )
                    else dec( V );
           W := U*U + V*V;
         end;

        (* Note that in various cases, we can only *)
        (* compute a Sqrt(W) of $3FFF, eg. U=V     *)

        if S1 then U := -U;
        if S2 then V := -V;

        R.x := U; (* Type conversion *)
        R.y := V; (* Type conversion *)

      end
     else
      begin
       Normalize := False;
       exc.error := TT_Err_Divide_By_Zero;
      end;
  end;

  Normalize := True;
end;

{$F+}

(****************************************************************)
(*                                                              *)
(* MANAGING THE STACK                                           *)
(*                                                              *)
(*  Instructions appear in the specs' order                     *)
(*                                                              *)
(****************************************************************)

(*******************************************)
(* DUP[]     : Duplicate top stack element *)
(* CodeRange : $20                         *)

   procedure Ins_DUP( args : PStorage );
   begin
     args^[1] := args^[0];
   end;

(*******************************************)
(* POP[]     : POPs the stack's top elt.   *)
(* CodeRange : $21                         *)

   procedure Ins_POP( args : PStorage );
   begin
     (* nothing to do *)
   end;

(*******************************************)
(* CLEAR[]   : Clear the entire stack      *)
(* CodeRange : $22                         *)

   procedure Ins_CLEAR( args : PStorage );
   begin
     exc.new_top := 0;
   end;

(*******************************************)
(* SWAP[]    : Swap the top two elements   *)
(* CodeRange : $23                         *)

   procedure Ins_SWAP( args : PStorage );
   var L : Long;
   begin
     L        := args^[0];
     args^[0] := args^[1];
     args^[1] := L;
   end;

(*******************************************)
(* DEPTH[]   : return the stack depth      *)
(* CodeRange : $24                         *)

   procedure Ins_DEPTH( args : PStorage );
   begin
     args^[0] := exc.top;
   end;

(*******************************************)
(* CINDEX[]  : copy indexed element        *)
(* CodeRange : $25                         *)

   procedure Ins_CINDEX( args : PStorage );
   var
     L : Long;
   begin
     L := args^[0];
     if (L <= 0) or (L > exc.args) then
       exc.error := TT_Err_Invalid_Reference
     else
       args^[0] := exc.stack^[exc.args-l];
   end;

(*******************************************)
(* MINDEX[]  : move indexed element        *)
(* CodeRange : $26                         *)

   procedure Ins_MINDEX( args : PStorage );
   var
     L, K : Long;
   begin
     L := args^[0];
     if (L <= 0) or (L > exc.args) then
       exc.Error := TT_Err_Invalid_Reference
     else
       begin
         K := exc.stack^[exc.args-L];

         move( exc.stack^[exc.args-L+1],
               exc.stack^[exc.args-L],
               (L-1)*sizeof(Long) );

         exc.stack^[exc.args-1] := K;
       end;
   end;

(*******************************************)
(* ROLL[]    : roll top three elements     *)
(* CodeRange : $8A                         *)

   procedure Ins_ROLL( args : PStorage );
   var
     A, B, C : Long;
   begin
     A := args^[2];
     B := args^[1];
     C := args^[0];

     args^[2] := C;
     args^[1] := A;
     args^[0] := B;
   end;

(****************************************************************)
(*                                                              *)
(* MANAGING THE FLOW OF CONTROL                                 *)
(*                                                              *)
(*  Instructions appear in the specs' order                     *)
(*                                                              *)
(****************************************************************)

   function SkipCode : boolean;
   var
     b : Boolean;
   begin
     b := False;

     inc( exc.IP, exc.length );

     b := exc.IP < exc.codeSize;

     if b then b := Calc_Length;

     if not b then
       exc.error := TT_Err_Code_Overflow;

     SkipCode := b;
   end;


(*******************************************)
(* IF[]      : IF test                     *)
(* CodeRange : $58                         *)

   procedure Ins_IF( args : PStorage );
   var
     nIfs : Int;
     Out  : Boolean;
   begin
     if args^[0] <> 0 then exit;

     nIfs := 1;
     Out  := False;

     Repeat

      if not SkipCode then exit;

      Case exc.opcode of

      (* IF *)
       $58 : inc( nIfs );

      (* ELSE *)
       $1B : out:= nIfs=1;

      (* EIF *)
       $59 : begin
              dec( nIfs );
              out:= nIfs=0;
             end;
      end;

     until Out;
   end;


(*******************************************)
(* ELSE[]    : ELSE                        *)
(* CodeRange : $1B                         *)

   procedure Ins_ELSE( args : PStorage );
   var
     nIfs : Int;
   begin
     nIfs     := 1;

     Repeat

      if not SkipCode then exit;

      case exc.opcode of

      (* IF *)
       $58 : inc( nIfs );

      (* EIF *)
       $59 : dec( nIfs );
      end;

     until nIfs=0;
   end;

(*******************************************)
(* EIF[]     : End IF                      *)
(* CodeRange : $59                         *)

   procedure Ins_EIF( args : PStorage );
   begin
     (* nothing to do *)
   end;

(*******************************************)
(* JROT[]    : Jump Relative On True       *)
(* CodeRange : $78                         *)

   procedure Ins_JROT( args : PStorage );
   begin
     if args^[1] <> 0 then
     begin
       inc( exc.IP, args^[0] );
       exc.step_ins := false;
     end;
   end;

(*******************************************)
(* JMPR[]    : JuMP Relative               *)
(* CodeRange : $1C                         *)

   procedure Ins_JMPR( args : PStorage );
   begin
     inc( exc.IP, args^[0] );
     exc.step_ins := false;
   end;

(*******************************************)
(* JROF[]    : Jump Relative On False      *)
(* CodeRange : $79                         *)

   procedure Ins_JROF( args : PStorage );
   begin
     if args^[1] = 0 then
     begin
       inc( exc.IP, args^[0] );
       exc.step_ins := false;
     end;
   end;

(****************************************************************)
(*                                                              *)
(* LOGICAL FUNCTIONS                                            *)
(*                                                              *)
(*  Instructions appear in the specs' order                     *)
(*                                                              *)
(****************************************************************)

(*******************************************)
(* LT[]      : Less Than                   *)
(* CodeRange : $50                         *)

   procedure Ins_LT( args : PStorage );
   begin
     if args^[0] < args^[1] then args^[0] := 1
                            else args^[0] := 0;
   end;

(*******************************************)
(* LTEQ[]    : Less Than or EQual          *)
(* CodeRange : $51                         *)

   procedure Ins_LTEQ( args : PStorage );
   begin
     if args^[0] <= args^[1] then args^[0] := 1
                             else args^[0] := 0;
   end;

(*******************************************)
(* GT[]      : Greater Than                *)
(* CodeRange : $52                         *)

   procedure Ins_GT( args : PStorage );
   begin
     if args^[0] > args^[1] then args^[0] := 1
                            else args^[0] := 0;
   end;

(*******************************************)
(* GTEQ[]    : Greater Than or EQual       *)
(* CodeRange : $53                         *)

   procedure Ins_GTEQ( args : PStorage );
   begin
     if args^[0] >= args^[1] then args^[0] := 1
                             else args^[0] := 0;
   end;

(*******************************************)
(* EQ[]      : EQual                       *)
(* CodeRange : $54                         *)

   procedure Ins_EQ( args : PStorage );
   begin
     if args^[0] = args^[1] then args^[0] := 1
                            else args^[0] := 0;
   end;

(*******************************************)
(* NEQ[]     : Not EQual                   *)
(* CodeRange : $55                         *)

   procedure Ins_NEQ( args : PStorage );
   begin
     if args^[0] <> args^[1] then args^[0] := 1
                             else args^[0] := 0;
   end;

(*******************************************)
(* ODD[]     : Odd                         *)
(* CodeRange : $56                         *)

   procedure Ins_ODD( args : PStorage );
   begin
     if exc.func_round( args^[0], 0 ) and 127 = 64 then args^[0] := 1
                                                   else args^[0] := 0;
   end;

(*******************************************)
(* EVEN[]    : Even                        *)
(* CodeRange : $57                         *)

   procedure Ins_EVEN( args : PStorage );
   begin
     if exc.func_round( args^[0], 0 ) and 127 = 0 then args^[0] := 1
                                                  else args^[0] := 0;
   end;

(*******************************************)
(* AND[]     : logical AND                 *)
(* CodeRange : $5A                         *)

   procedure Ins_AND( args : PStorage );
   begin
     if ( args^[0] <> 0 ) and
        ( args^[1] <> 0 ) then args^[0] := 1
                          else args^[0] := 0;
   end;

(*******************************************)
(* OR[]      : logical OR                  *)
(* CodeRange : $5B                         *)

   procedure Ins_OR( args : PStorage );
   begin
     if ( args^[0] <> 0 ) or
        ( args^[1] <> 0 ) then args^[0] := 1
                          else args^[0] := 0;
   end;

(*******************************************)
(* NOT[]     : logical NOT                 *)
(* CodeRange : $5C                         *)

   procedure Ins_NOT( args : PStorage );
   begin
     if args^[0] <> 0 then args^[0] := 0
                      else args^[0] := 1;
   end;

(****************************************************************)
(*                                                              *)
(* ARITHMETIC AND MATH INSTRUCTIONS                             *)
(*                                                              *)
(*  Instructions appear in the specs' order                     *)
(*                                                              *)
(****************************************************************)

(*******************************************)
(* ADD[]     : ADD                         *)
(* CodeRange : $60                         *)

   procedure Ins_ADD( args : PStorage );
   begin
     inc( args^[0], args^[1] );
   end;

(*******************************************)
(* SUB[]     : SUBstract                   *)
(* CodeRange : $61                         *)

   procedure Ins_SUB( args : PStorage );
   begin
     dec( args^[0], args^[1] );
   end;

(*******************************************)
(* DIV[]     : DIVide                      *)
(* CodeRange : $62                         *)

   procedure Ins_DIV( args : PStorage );
   begin
    if args^[1] = 0 then
    begin
      exc.error := TT_Err_Divide_By_Zero;
      exit;
    end;

    args^[0] := MulDiv_Round( args^[0], 64, args^[1] );
   end;

(*******************************************)
(* MUL[]     : MULtiply                    *)
(* CodeRange : $63                         *)

   procedure Ins_MUL( args : PStorage );
   begin
     args^[0] := MulDiv_Round( args^[0], args^[1], 64 );
   end;

(*******************************************)
(* ABS[]     : ABSolute value              *)
(* CodeRange : $64                         *)

   procedure Ins_ABS( args : PStorage );
   begin
     args^[0] := abs( args^[0] );
   end;

(*******************************************)
(* NEG[]     : NEGate                      *)
(* CodeRange : $65                         *)

   procedure Ins_NEG( args : PStorage );
   begin
     args^[0] := -args^[0];
   end;

(*******************************************)
(* FLOOR[]   : FLOOR                       *)
(* CodeRange : $66                         *)

   procedure Ins_FLOOR( args : PStorage );
   begin
     args^[0] := args^[0] and -64;
   end;

(*******************************************)
(* CEILING[] : CEILING                     *)
(* CodeRange : $67                         *)

   procedure Ins_CEILING( args : PStorage );
   begin
     args^[0] := ( args^[0]+63 ) and -64;
   end;

(*******************************************)
(* MAX[]     : MAXimum                     *)
(* CodeRange : $68                         *)

   procedure Ins_MAX( args : PStorage );
   begin
     if args^[1] > args^[0] then args^[0] := args^[1];
   end;

(*******************************************)
(* MIN[]     : MINimum                     *)
(* CodeRange : $69                         *)

   procedure Ins_MIN( args : PStorage );
   begin
     if args^[1] < args^[0] then args^[0] := args^[1];
   end;

(****************************************************************)
(*                                                              *)
(* COMPENSATING FOR THE ENGINE CHARACTERISTICS                  *)
(*                                                              *)
(*  Instructions appear in the specs' order                     *)
(*                                                              *)
(****************************************************************)

(*******************************************)
(* ROUND[ab] : ROUND value                 *)
(* CodeRange : $68-$6B                     *)

   procedure Ins_ROUND( args : PStorage );
   begin
     args^[0] := exc.func_round( args^[0],
                                 exc.metrics.compensations[ exc.opcode-$68 ] );
   end;

(*******************************************)
(* NROUND[ab]: No ROUNDing of value        *)
(* CodeRange : $6C-$6F                     *)

   procedure Ins_NROUND( args : PStorage );
   begin
     args^[0] := Round_None( args^[0],
                             exc.metrics.compensations[ exc.opcode-$6C ] );
   end;

(****************************************************************)
(*                                                              *)
(* DEFINING AND USING FUNCTIONS AND INSTRUCTIONS                *)
(*                                                              *)
(*  Instructions appear in the specs' order                     *)
(*                                                              *)
(****************************************************************)

(*******************************************)
(* FDEF[]    : Function DEFinition         *)
(* CodeRange : $2C                         *)

   procedure Ins_FDEF( args : PStorage );
   var
     func : int;
   label
     Suite;
   begin

     (* check space *)
     if exc.numFDefs >= exc.maxFDefs then begin
       exc.error := TT_Err_Too_Many_FuncDefs;
       exit;
     end;

     func := Int(args^[0]);
     with exc.FDefs^[exc.numFDefs] do
       begin
         Range  := exc.curRange;
         Opc    := func;
         Start  := exc.IP+1;
         Active := True;
       end;

     if func > exc.maxFunc then
       exc.maxFunc := func;

     inc(exc.numFDefs);

     (* now skip the whole function definition *)
     (* we don't allow nested IDEFS & FDEFs    *)

     while SkipCode do

       case exc.opcode of

         $89,  (* IDEF *)
         $2C : (* FDEF *)
               begin
                 exc.error := TT_Err_Nested_Defs;
                 exit;
               end;

         $2D : (* ENDF *)
               exit;
       end;
   end;

(*******************************************)
(* ENDF[]    : END Function definition     *)
(* CodeRange : $2D                         *)

   procedure Ins_ENDF( args : PStorage );
   begin

     if exc.callTop <= 0 then   (* We encountered an ENDF without a call *)
     begin
       exc.error := TT_Err_ENDF_in_Exec_Stream;
       exit;
     end;

     dec( exc.CallTop );

     with exc.Callstack^[exc.CallTop] do
      begin
       dec( Cur_Count );

       exc.step_ins := false;

       if Cur_Count > 0 then

         begin
           (* Loop the current function *)
           inc( exc.callTop );
           exc.IP := Cur_Restart;
         end

       else
         (* exit the current call frame                      *)
         (* NOTE : When the last intruction of a program     *)
         (*        is a CALL or LOOPCALL, the return address *)
         (*        is always out of the code range. This is  *)
         (*        valid address, and  is why we do not test *)
         (*        the result of Goto_CodeRange here !!      *)

         Goto_CodeRange( Caller_Range, Caller_IP )
      end;

    end;

(*******************************************)
(* CALL[]    : CALL function               *)
(* CodeRange : $2B                         *)

   procedure Ins_CALL( args : PStorage );
   var
     ii, nn : Int;
     def    : PDefRecord;
   label
     Fail;
   begin

     (* First of all, check index *)
     if (args^[0] < 0) or (args^[0] > exc.maxFunc) then
       goto Fail;

     (* Except for some old Apple fonts, all functions in a TrueType *)
     (* fonts are defined in increasing order, starting from 0.      *)
     (*                                                              *)
     (* This mean that, normally, we have :                          *)
     (*                                                              *)
     (*    exc.maxFunc+1 = exc.numFDefs                              *)
     (*    exc.FDefs[n].opc = n for n in 0..exc.maxFunc              *)
     (*                                                              *)

     nn  := Int(args^[0]);
     def := @exc.FDefs^[nn];

     if ( exc.maxFunc+1 <> exc.numFDefs ) or ( def^.opc <> nn ) then begin
       (* lookup the FDefs table *)
       ii  := 0;
       def := @exc.FDefs^[0];
       while (ii < exc.numFDefs) and (def^.opc <> nn) do begin
         inc(ii);
         inc(def);
       end;

       (* Fail if the function isn't listed *)
       if ii >= exc.numFDefs then
         goto Fail;
     end;

     (* check that the function is active *)
     if not def^.active then
       goto Fail;

     (* check call stack *)
     if exc.callTop >= exc.callSize then
       begin
         exc.error := TT_Err_Stack_Overflow;
         exit;
       end;

     with exc.callstack^[exc.callTop] do
       begin
         Caller_Range := exc.curRange;
         Caller_IP    := exc.IP+1;
         Cur_Count    := 1;
         Cur_Restart  := def^.Start;
       end;

     inc( exc.CallTop );

     with def^ do Goto_CodeRange( Range, Start );

     exc.step_ins := false;
     exit;

    Fail:
      exc.error := TT_Err_Invalid_Reference;
      exit;
    end;

(*******************************************)
(* LOOPCALL[]: LOOP and CALL function      *)
(* CodeRange : $2A                         *)

   procedure Ins_LOOPCALL( args : PStorage );
   begin

     if ( args^[1] < 0 ) or ( args^[1] >= exc.numFDefs ) or
        ( not exc.FDefs^[args^[1]].Active ) then
       begin
         exc.error := TT_Err_Invalid_Reference;
         exit;
       end;

     if exc.callTop >= exc.callSize then
       begin
         exc.error := TT_Err_Stack_Overflow;
         exit;
       end;

     if args^[0] > 0 then
       begin
         with exc.callstack^[exc.callTop] do
           begin
             Caller_Range := exc.curRange;
             Caller_IP    := exc.IP+1;
             Cur_Count    := args^[0];
             Cur_Restart  := exc.FDefs^[args^[1]].Start;
           end;

         inc( exc.CallTop );

         with exc.FDefs^[args^[1]] do Goto_CodeRange( Range, Start );

         exc.step_ins := false;
       end;

   end;

(*******************************************)
(* IDEF[]    : Instruction DEFinition      *)
(* CodeRange : $89                         *)

   procedure Ins_IDEF( args : PStorage );
   var
     i, A : Int;
   begin

     A := 0;

     while ( A < exc.numIDefs ) do
       with exc.IDefs^[A] do
         begin

           if not Active then
             begin
               Opc    := args^[0];
               Start  := exc.IP+1;
               Range  := exc.curRange;
               Active := True;

               A := exc.numIDefs;

                (* now skip the whole function definition *)
                (* we don't allow nested IDEFS & FDEFs    *)

               while SkipCode do
                 case exc.opcode of

                   $89,  (* IDEF *)
                   $2C : (* FDEF *)
                         begin
                           exc.error := TT_Err_Nested_Defs;
                           exit;
                         end;

                   $2D : (* ENDF *)
                         exit;
                 end;
             end
           else
             inc( A );
         end;
     end;

(****************************************************************)
(*                                                              *)
(* PUSHING DATA ONTO THE INTERPRETER STACK                      *)
(*                                                              *)
(*  Instructions appear in the specs' order                     *)
(*                                                              *)
(****************************************************************)

(*******************************************)
(* NPUSHB[]  : PUSH N Bytes                *)
(* CodeRange : $40                         *)

   procedure Ins_NPUSHB( args : PStorage );
   var
    L, K : Long;
   begin
     L := exc.code^[exc.IP+1];

     if exc.top + L > exc.stackSize then
     begin
       exc.error := TT_Err_Stack_Overflow;
       exit;
     end;

     for K := 1 to L do
       args^[k-1] := exc.code^[exc.IP+1+k];

     inc( exc.new_top, L );
   end;

(*******************************************)
(* NPUSHW[]  : PUSH N Words                *)
(* CodeRange : $41                         *)

   procedure Ins_NPUSHW( args : PStorage );
   var
    L, K : Long;
   begin
     L := exc.code^[exc.IP+1];

     if exc.top + L > exc.stackSize then
     begin
       exc.error := TT_Err_Stack_Overflow;
       exit;
     end;

     inc( exc.IP, 2 );

     for K := 1 to L do
       args^[k-1] := GetShort;

     exc.step_ins := false;

     inc( exc.new_top, L );
   end;

(*******************************************)
(* PUSHB[abc]: PUSH Bytes                  *)
(* CodeRange : $B0-$B7                     *)

   procedure Ins_PUSHB( args : PStorage );
   var
    L, K : Long;
   begin
     L := exc.opcode - $B0+1;

     if exc.top + L >= exc.stackSize then
     begin
       exc.error := TT_Err_Stack_Overflow;
       exit;
     end;

     for k := 1 to L do
       args^[k-1] := exc.code^[exc.ip+k];

   end;

(*******************************************)
(* PUSHW[abc]: PUSH Words                  *)
(* CodeRange : $B8-$BF                     *)

   procedure Ins_PUSHW( args : PStorage );
   var
     L, K : Long;
   begin
     L := exc.opcode - $B8+1;

     if exc.top + L >= exc.stackSize then
     begin
       exc.error := TT_Err_Stack_Overflow;
       exit;
     end;

     inc( exc.IP );

     for k := 1 to L do
       args^[k-1] := GetShort;

     exc.step_ins := false;

   end;

(****************************************************************)
(*                                                              *)
(* MANAGING THE STORAGE AREA                                    *)
(*                                                              *)
(*  Instructions appear in the specs' order                     *)
(*                                                              *)
(****************************************************************)

(*******************************************)
(* RS[]      : Read Store                  *)
(* CodeRange : $43                         *)

   procedure Ins_RS( args : PStorage );
   begin
     if (args^[0] < 0) or (args^[0] >= exc.storeSize) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     args^[0] := exc.storage^[args^[0]];
   end;

(*******************************************)
(* WS[]      : Write Store                 *)
(* CodeRange : $42                         *)

   procedure Ins_WS( args : PStorage );
   begin
     if (args^[0] < 0) or (args^[0] >= exc.storeSize) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     exc.storage^[args^[0]] := args^[1];
   end;

(*******************************************)
(* WCVTP[]   : Write CVT in Pixel units    *)
(* CodeRange : $44                         *)

   procedure Ins_WCVTP( args : PStorage );
   begin
     if (args^[0] < 0) or (args^[0] >= exc.cvtSize) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     exc.func_write_cvt( args^[0], args^[1] );
   end;

(*******************************************)
(* WCVTF[]   : Write CVT in FUnits         *)
(* CodeRange : $70                         *)

   procedure Ins_WCVTF( args : PStorage );
   begin
     if (args^[0] < 0) or (args^[0] >= exc.cvtSize) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     exc.cvt^[args^[0]] := Scale_Pixels(args^[1]);
   end;

(*******************************************)
(* RCVT[]    : Read CVT                    *)
(* CodeRange : $45                         *)

   procedure Ins_RCVT( args : PStorage );
   begin
     if (args^[0] < 0) or (args^[0] >= exc.cvtSize) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     args^[0] := exc.func_read_cvt(args^[0]);
   end;

(****************************************************************)
(*                                                              *)
(* MANAGING THE GRAPHICS STATE                                  *)
(*                                                              *)
(*  Instructions appear in the specs' order                     *)
(*                                                              *)
(****************************************************************)

(*******************************************)
(* SVTCA[a]  : Set F and P vectors to axis *)
(* CodeRange : $00-$01                     *)

   procedure Ins_SVTCA( args : PStorage );
   var A, B : Short;
   begin
     case (exc.opcode and 1) of
       0 : A := $0000;
       1 : A := $4000;
     end;
     B := A xor $4000;

     exc.GS.freeVector.x := A;
     exc.GS.projVector.x := A;
     exc.GS.dualVector.x := A;

     exc.GS.freeVector.y := B;
     exc.GS.projVector.y := B;
     exc.GS.dualVector.y := B;

     Compute_Funcs;
   end;

(*******************************************)
(* SPVTCA[a] : Set PVector to Axis         *)
(* CodeRange : $02-$03                     *)

   procedure Ins_SPVTCA( args : PStorage );
   var A, B : Short;
   begin
     case (exc.opcode and 1) of
       0 : A := $0000;
       1 : A := $4000;
     end;
     B := A xor $4000;

     exc.GS.projVector.x := A;
     exc.GS.dualVector.x := A;

     exc.GS.projVector.y := B;
     exc.GS.dualVector.y := B;

     Compute_Funcs;
   end;

(*******************************************)
(* SFVTCA[a] : Set FVector to Axis         *)
(* CodeRange : $04-$05                     *)

   procedure Ins_SFVTCA( args : PStorage );
   var A, B : Short;
   begin
     case (exc.opcode and 1) of
       0 : A := $0000;
       1 : A := $4000;
     end;
     B := A xor $4000;

     exc.GS.freeVector.x := A;
     exc.GS.freeVector.y := B;

     Compute_Funcs;
   end;



   function Ins_SxVTL( aIdx1     : Int;
                       aIdx2     : Int;
                       aOpc      : Int;
                       var Vec   : TT_UnitVector ) : boolean;
   var
     A, B, C : Long;
   begin
     Ins_SxVTL := False;

     with exc do
     begin

       if (aIdx2 >= zp1.n_points) or (aIdx1 >= zp2.n_points) then
         begin
           Error := TT_Err_Invalid_Reference;
           exit;
         end;

       with zp1.Cur^[aIdx2] do
       begin
         A := x;
         B := y;
       end;

       with zp2.Cur^[aIdx1] do
       begin
         dec( A, x );
         dec( B, y );
       end;

       if aOpc and 1 <> 0 then
        begin
         C :=  B;  (* CounterClockwise rotation *)
         B :=  A;
         A := -C;
        end;

       if not Normalize( A, B, Vec ) then
       begin
         exc.error := TT_Err_Ok;
         Vec.x     := $4000;
         Vec.y     := $0000;
       end;

       Ins_SxVTL := True;
     end;
   end;


(*******************************************)
(* SPVTL[a]  : Set PVector to Line         *)
(* CodeRange : $06-$07                     *)

   procedure Ins_SPVTL( args : PStorage );
   begin
     if not INS_SxVTL( args^[1],
                       args^[0],
                       exc.opcode,
                       exc.GS.projVector ) then exit;

     exc.GS.dualVector := exc.GS.projVector;
     Compute_Funcs;
   end;

(*******************************************)
(* SFVTL[a]  : Set FVector to Line         *)
(* CodeRange : $08-$09                     *)

   procedure Ins_SFVTL( args : PStorage );
   begin
     if not INS_SxVTL( args^[1],
                       args^[0],
                       exc.opcode,
                       exc.GS.freeVector ) then exit;

     Compute_Funcs;
   end;

(*******************************************)
(* SFVTPV[]  : Set FVector to PVector      *)
(* CodeRange : $0E                         *)

   procedure Ins_SFVTPV( args : PStorage );
   begin
     exc.GS.freeVector := exc.GS.projVector;
     Compute_Funcs;
   end;

(*******************************************)
(* SDPVTL[a] : Set Dual PVector to Line    *)
(* CodeRange : $86-$87                     *)

   procedure Ins_SDPVTL( args : PStorage );
   var
     A, B, C : Long;
     p1, p2  : Int;
   begin

     p1 := args^[1];
     p2 := args^[0];

     if (args^[0] < 0) or (args^[0] >= exc.zp1.n_points) or
        (args^[1] < 0) or (args^[1] >= exc.zp2.n_points) then
       begin
         exc.error := TT_Err_Invalid_Reference;
         exit;
       end;

     A := exc.zp1.org^[p2].x - exc.zp2.org^[p1].x;
     B := exc.zp1.org^[p2].y - exc.zp2.org^[p1].y;

     if exc.opcode and 1 <> 0 then
      begin
       C :=  B;  (* CounterClockwise rotation *)
       B :=  A;
       A := -C;
      end;

     Normalize( A, B, exc.GS.dualVector );

     A := exc.zp1.cur^[p2].x - exc.zp2.cur^[p1].x;
     B := exc.zp1.cur^[p2].y - exc.zp2.cur^[p1].y;

     if exc.opcode and 1 <> 0 then
      begin
       C :=  B;  (* CounterClockwise rotation *)
       B :=  A;
       A := -C;
      end;

     Normalize( A, B, exc.GS.projVector );

     Compute_Funcs;
     exc.error := TT_Err_Ok;
   end;

(*******************************************)
(* SPVFS[]   : Set PVector From Stack      *)
(* CodeRange : $0A                         *)

   procedure Ins_SPVFS( args : PStorage );
   var
     S    : Short;
     X, Y : Long;
   begin
     S := args^[1]; Y := S;  (* type conversion; extends sign *)
     S := args^[0]; X := S;  (* type conversion; extends sign *)

     if not Normalize( X, Y, exc.GS.projVector ) then exit;

     exc.GS.dualVector := exc.GS.projVector;

     Compute_Funcs;
   end;

(*******************************************)
(* SFVFS[]   : Set FVector From Stack      *)
(* CodeRange : $0B                         *)

   procedure Ins_SFVFS( args : PStorage );
   var
     S    : Short;
     X, Y : Long;
   begin
     S := args^[1]; Y := S;  (* type conversion; extends sign *)
     S := args^[0]; X := S;  (* type conversion; extends sign *)

     if not Normalize( X, Y, exc.GS.freeVector ) then exit;

     Compute_Funcs;
   end;

(*******************************************)
(* GPV[]     : Get Projection Vector       *)
(* CodeRange : $0C                         *)

   procedure Ins_GPV( args : PStorage );
   begin
     args^[0] := exc.GS.projVector.x;
     args^[1] := exc.GS.projVector.y;
   end;

(*******************************************)
(* GFV[]     : Get Freedom Vector          *)
(* CodeRange : $0D                         *)

   procedure Ins_GFV( args : PStorage );
   begin
     args^[0] := exc.GS.freeVector.x;
     args^[1] := exc.GS.freeVector.y;
   end;

(*******************************************)
(* SRP0[]    : Set Reference Point 0       *)
(* CodeRange : $10                         *)

   procedure Ins_SRP0( args : PStorage );
   begin
     exc.GS.rp0 := args^[0];
   end;

(*******************************************)
(* SRP1[]    : Set Reference Point 1       *)
(* CodeRange : $11                         *)

   procedure Ins_SRP1( args : PStorage );
   begin
     exc.GS.rp1 := args^[0];
   end;

(*******************************************)
(* SRP2[]    : Set Reference Point 2       *)
(* CodeRange : $12                         *)

   procedure Ins_SRP2( args : PStorage );
   begin
     exc.GS.rp2 := args^[0];
   end;

(*******************************************)
(* SZP0[]    : Set Zone Pointer 0          *)
(* CodeRange : $13                         *)

   procedure Ins_SZP0( args : PStorage );
   begin
     case args^[0] of

       0 : exc.zp0 := exc.Twilight;
       1 : exc.zp0 := exc.Pts;
     else
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     exc.GS.gep0 := args^[0];
   end;

(*******************************************)
(* SZP1[]    : Set Zone Pointer 1          *)
(* CodeRange : $14                         *)

   procedure Ins_SZP1( args : PStorage );
   begin
     case args^[0] of

       0 : exc.zp1 := exc.Twilight;
       1 : exc.zp1 := exc.Pts;
     else
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     exc.GS.gep1 := args^[0];
   end;

(*******************************************)
(* SZP2[]    : Set Zone Pointer 2          *)
(* CodeRange : $15                         *)

   procedure Ins_SZP2( args : PStorage );
   begin
     case args^[0] of

       0 : exc.zp2 := exc.Twilight;
       1 : exc.zp2 := exc.Pts;
     else
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     exc.GS.gep2 := args^[0];
   end;

(*******************************************)
(* SZPS[]    : Set Zone Pointers           *)
(* CodeRange : $16                         *)

   procedure Ins_SZPS( args : PStorage );
   begin
     case args^[0] of

       0 : exc.zp0 := exc.Twilight;
       1 : exc.zp0 := exc.Pts;
     else
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     exc.zp1 := exc.zp0;
     exc.zp2 := exc.zp0;

     exc.GS.gep0 := args^[0];
     exc.GS.gep1 := args^[0];
     exc.GS.gep2 := args^[0];
   end;

(*******************************************)
(* RTHG[]    : Round To Half Grid          *)
(* CodeRange : $19                         *)

   procedure Ins_RTHG( args : PStorage );
   begin
     exc.GS.round_state := TT_Round_To_Half_Grid;

{$IFDEF FPK}
     exc.func_round := @Round_To_Half_Grid;
{$ELSE}
     exc.func_round := Round_To_Half_Grid;
{$ENDIF}
   end;

(*******************************************)
(* RTG[]     : Round To Grid               *)
(* CodeRange : $18                         *)

   procedure Ins_RTG( args : PStorage );
   begin
     exc.GS.round_state := TT_Round_To_Grid;

{$IFDEF FPK}
     exc.func_round := @Round_To_Grid;
{$ELSE}
     exc.func_round := Round_To_Grid;
{$ENDIF}
   end;

(*******************************************)
(* RTDG[]    : Round To Double Grid        *)
(* CodeRange : $3D                         *)

   procedure Ins_RTDG( args : PStorage );
   begin
     exc.GS.round_state := TT_Round_To_Double_Grid;

{$IFDEF FPK}
     exc.func_round := @Round_To_Double_Grid;
{$ELSE}
     exc.func_round := Round_To_Double_Grid;
{$ENDIF}
   end;

(*******************************************)
(* RUTG[]    : Round Up To Grid            *)
(* CodeRange : $7C                         *)

   procedure Ins_RUTG( args : PStorage );
   begin
     exc.GS.round_state := TT_Round_Up_To_Grid;

{$IFDEF FPK}
     exc.func_round := @Round_Up_To_Grid;
{$ELSE}
     exc.func_round := Round_Up_To_Grid;
{$ENDIF}
   end;

(*******************************************)
(* RDTG[]    : Round Down To Grid          *)
(* CodeRange : $7D                         *)

   procedure Ins_RDTG( args : PStorage );
   begin
     exc.GS.round_state := TT_Round_Down_To_Grid;

{$IFDEF FPK}
     exc.func_round := @Round_Down_To_Grid;
{$ELSE}
     exc.func_round := Round_Down_To_Grid;
{$ENDIF}
   end;

(*******************************************)
(* ROFF[]    : Round OFF                   *)
(* CodeRange : $7A                         *)

   procedure Ins_ROFF( args : PStorage );
   begin
     exc.GS.round_state := TT_Round_Off;

{$IFDEF FPK}
     exc.func_round := @Round_None;
{$ELSE}
     exc.func_round := Round_None;
{$ENDIF}
   end;

(*******************************************)
(* SROUND[]  : Super ROUND                 *)
(* CodeRange : $76                         *)

   procedure Ins_SROUND( args : PStorage );
   begin
     SetSuperRound( $4000, args^[0] );
     exc.GS.round_state := TT_Round_Super;

{$IFDEF FPK}
     exc.func_round := @Round_Super;
{$ELSE}
     exc.func_round := Round_Super;
{$ENDIF}
   end;

(*******************************************)
(* S45ROUND[]: Super ROUND 45 degrees      *)
(* CodeRange : $77                         *)

   procedure Ins_S45ROUND( args : PStorage );
   begin
     SetSuperRound( $2D41, args^[0] );
     exc.GS.round_state := TT_Round_Super_45;

{$IFDEF FPK}
     exc.func_round := @Round_Super_45;
{$ELSE}
     exc.func_round := Round_Super_45;
{$ENDIF}
   end;


(*******************************************)
(* SLOOP[]   : Set LOOP variable           *)
(* CodeRange : $17                         *)

   procedure Ins_SLOOP( args : PStorage );
   begin
     exc.GS.Loop := args^[0];
   end;

(*******************************************)
(* SMD[]     : Set Minimum Distance        *)
(* CodeRange : $1A                         *)

   procedure Ins_SMD( args : PStorage );
   begin
     exc.GS.minimum_distance := args^[0];
   end;

(*******************************************)
(* INSTCTRL[]: INSTruction ConTRol         *)
(* CodeRange : $8e                         *)

   procedure Ins_INSTCTRL( args : PStorage );
   var
     K, L : Int;
   begin
     K := args^[1];
     L := args^[0];

     if ( K < 1 ) or ( K > 2 ) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     if L <> 0 then L := K;

     exc.GS.instruct_control := ( exc.GS.instruct_control and not K ) or L;
   end;

(*******************************************)
(* SCANCTRL[]: SCAN ConTRol                *)
(* CodeRange : $85                         *)

   procedure Ins_SCANCTRL( args : PStorage );
   var
     A : Int;
   begin

     (* Get Threshold *)
     A := args^[0] and $FF;

     if A = $FF then
       exc.GS.scan_Control := True
     else
       if A = 0 then
         exc.GS.scan_Control := False
     else
       begin

         A := A * 64;

         (* XXX TODO : Add rotation and stretch cases *)

         if ( args^[0] and $100 <> 0 ) and
            ( exc.metrics.pointSize <= A ) then exc.GS.scan_Control := True;

         if ( args^[0] and $200 <> 0 ) and
            ( false ) then exc.GS.scan_Control := True;

         if ( args^[0] and $400 <> 0 ) and
            ( false ) then exc.GS.scan_Control := True;

         if ( args^[0] and $800 <> 0 ) and
            ( exc.metrics.pointSize > A ) then exc.GS.scan_Control := False;

         if ( args^[0] and $1000 <> 0 ) and
            ( not False ) then exc.GS.scan_Control := False;

         if ( args^[0] and $2000 <> 0 ) and
            ( not False ) then exc.GS.scan_Control := False;
       end;
   end;

(*******************************************)
(* SCANTYPE[]: SCAN TYPE                   *)
(* CodeRange : $8D                         *)

   procedure Ins_SCANTYPE( args : PStorage );
   begin
     (* For compatibility with future enhancements, *)
     (* we must ignore new modes                    *)

     if (args^[0] >= 0 ) and (args^[0] <= 5) then
     begin
       if args^[0] = 3 then args^[0] := 2;

       exc.GS.scan_type := args^[0];
     end;
   end;

(**********************************************)
(* SCVTCI[]  : Set Control Value Table Cut In *)
(* CodeRange : $1D                            *)

   procedure Ins_SCVTCI( args : PStorage );
   begin
     exc.GS.control_value_cutin := args^[0];
   end;

(**********************************************)
(* SSWCI[]   : Set Single Width Cut In        *)
(* CodeRange : $1E                            *)

   procedure Ins_SSWCI( args : PStorage );
   begin
     exc.GS.single_width_cutin := args^[0];
   end;

(**********************************************)
(* SSW[]     : Set Single Width               *)
(* CodeRange : $1F                            *)

   procedure Ins_SSW( args : PStorage );
   begin
     exc.GS.single_width_value := args^[0] div $400;
   end;

(**********************************************)
(* FLIPON[]  : Set Auto_flip to On            *)
(* CodeRange : $4D                            *)

   procedure Ins_FLIPON( args : PStorage );
   begin
     exc.GS.auto_flip := True;
   end;

(**********************************************)
(* FLIPOFF[] : Set Auto_flip to Off           *)
(* CodeRange : $4E                            *)

   procedure Ins_FLIPOFF( args : PStorage );
   begin
     exc.GS.auto_flip := False;
   end;

(**********************************************)
(* SANGW[]   : Set Angle Weigth               *)
(* CodeRange : $7E                            *)

   procedure Ins_SANGW( args : PStorage );
   begin
     (* instruction not supported anymore *)
   end;

(**********************************************)
(* SDB[]     : Set Delta Base                 *)
(* CodeRange : $5E                            *)

   procedure Ins_SDB( args : PStorage );
   begin
     exc.GS.delta_base := args^[0]
   end;

(**********************************************)
(* SDS[]     : Set Delta Shift                *)
(* CodeRange : $5F                            *)

   procedure Ins_SDS( args : PStorage );
   begin
     exc.GS.delta_shift := args^[0]
   end;

(**********************************************)
(* GC[a]     : Get Coordinate projected onto  *)
(* CodeRange : $46-$47                        *)

(* BULLSHIT : Measures from the original glyph must to be taken *)
(*            along the dual projection vector !!               *)

   procedure Ins_GC( args : PStorage );
   var
     L : Int;
   begin
     L := args^[0];

     if (L < 0) or (L >= exc.zp2.n_points) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     case exc.opcode and 1 of

        0 : L := exc.func_project ( exc.zp2.cur^[L], Null_Vector );
        1 : L := exc.func_dualProj( exc.zp2.org^[L], Null_Vector );
       end;

     args^[0] := L;
    end;

(**********************************************)
(* SCFS[]    : Set Coordinate From Stack      *)
(* CodeRange : $48                            *)
(*                                            *)
(* Formule :                                  *)
(*                                            *)
(*   OA := OA + ( value - OA.p )/( f.p ) x f  *)
(*                                            *)

   procedure Ins_SCFS( args : PStorage );
   var
     K, L : Int;
   begin
     L := args^[0];

     if (args^[0] < 0) or (args^[0] >= exc.zp2.n_points) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     K := exc.func_project( exc.zp2.cur^[L], Null_Vector );

     exc.func_move( @exc.zp2, L, args^[1] - K );

     (* not part of the specs, but here for safety *)

     if exc.GS.gep2 = 0 then
       exc.zp2.org^[L] := exc.zp2.cur^[L];

   end;

(**********************************************)
(* MD[a]     : Measure Distance               *)
(* CodeRange : $49-$4A                        *)

(* BULLSHIT : Measure taken in the original glyph must be along *)
(*            the dual projection vector                        *)

(* Second BULLSHIT : Flag attributions are inverted !!            *)
(*                   0 => measure distance in original outline    *)
(*                   1 => measure distance in grid-fitted outline *)

   procedure Ins_MD( args : PStorage );
   var
     K, L : Int;
     D    : TT_F26dot6;
     vec1 : TT_Vector;
     vec2 : TT_Vector;
   begin
     K := args^[1];
     L := args^[0];

     if (args^[0] < 0) or (args^[0] >= exc.zp0.n_points) or
        (args^[1] < 0) or (args^[1] >= exc.zp1.n_points) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     case exc.opcode and 1 of

       0 : D := exc.func_dualProj( exc.zp0.org^[L], exc.zp1.org^[K] );
       1 : D := exc.func_project ( exc.zp0.cur^[L], exc.zp1.cur^[K] );
     end;

     args^[0] := D;
   end;

(**********************************************)
(* MPPEM[]   : Measure Pixel Per EM           *)
(* CodeRange : $4B                            *)

  procedure Ins_MPPEM( args : PStorage );
  begin
    args^[0] := Get_Ppem;
  end;

(**********************************************)
(* MPS[]     : Measure PointSize              *)
(* CodeRange : $4C                            *)

   procedure Ins_MPS( args : PStorage );
   begin
     args^[0] := exc.metrics.pointSize;
   end;

(****************************************************************)
(*                                                              *)
(* MANAGING OUTLINES                                            *)
(*                                                              *)
(*  Instructions appear in the specs' order                     *)
(*                                                              *)
(****************************************************************)


(**********************************************)
(* FLIPPT[]  : FLIP PoinT                     *)
(* CodeRange : $80                            *)

   procedure Ins_FLIPPT( args : PStorage );
   var
     point : Int;
   begin
     if exc.top < exc.GS.loop then
     begin
       exc.error := TT_Err_Too_Few_Arguments;
       exit;
     end;

     while exc.GS.loop > 0 do
     begin
       dec( exc.args );

       point := exc.stack^[ exc.args ];

       if (point < 0) or (point >= exc.pts.n_points) then
       begin
         exc.error := TT_Err_Invalid_Reference;
         exit;
       end;

       exc.pts.flags^[point] := exc.pts.flags^[point] xor TT_Flag_On_Curve;

       dec( exc.GS.loop );
     end;

     exc.GS.loop := 1;
     exc.new_top := exc.args;
   end;

(**********************************************)
(* FLIPRGON[]: FLIP RanGe ON                  *)
(* CodeRange : $81                            *)

   procedure Ins_FLIPRGON( args : PStorage );
   var
     I, K, L : Int;
   begin
     K := args^[1];
     L := args^[0];

     if (K < 0) or (K >= exc.pts.n_points) or
        (L < 0) or (L >= exc.pts.n_points) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     for I := L to K do
       exc.pts.flags^[I] := exc.pts.flags^[I] or TT_Flag_On_Curve;
   end;

(**********************************************)
(* FLIPRGOFF : FLIP RanGe OFF                 *)
(* CodeRange : $82                            *)

   procedure Ins_FLIPRGOFF( args : PStorage );
   var
     I, K, L : Int;
   begin
     K := args^[1];
     L := args^[0];

     if (K < 0) or (K >= exc.pts.n_points) or
        (L < 0) or (L >= exc.pts.n_points) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     for I := L to K do
       exc.pts.flags^[I] := exc.pts.flags^[I] and not TT_Flag_On_Curve;
   end;



  function Compute_Point_Displacement( var x    : TT_F26dot6;
                                       var y    : TT_F26dot6;
                                       var zone : PGlyph_Zone;
                                       var refp : Int ) : TError;
  var
    zp   : PGlyph_Zone;
    p    : Int;
    d    : TT_F26dot6;
    vec1 : TT_Vector;
    vec2 : TT_Vector;
  begin

    Compute_Point_Displacement := Success;

    case exc.opcode and 1 of
      0 : begin zp := @exc.zp1; p := exc.GS.rp2; end;
      1 : begin zp := @exc.zp0; p := exc.GS.rp1; end;
    end;

    if (p < 0) or (p >= zp^.n_points) then
    begin
      exc.error := TT_Err_Invalid_Displacement;
      Compute_Point_Displacement := Failure;
      exit;
    end;

    zone := zp;
    refp := p;

    d := exc.func_project( zp^.cur^[p], zp^.org^[p] );

    x := MulDiv_Round( d, Long(exc.GS.freeVector.x)*$10000, exc.F_dot_P );
    y := MulDiv_Round( d, Long(exc.GS.freeVector.y)*$10000, exc.F_dot_P );

  end;


  procedure Move_Zp2_Point( point : Int;
                            dx    : TT_F26dot6;
                            dy    : TT_F26dot6 );
  begin
    if exc.GS.freeVector.x <> 0 then
    begin
      inc( exc.zp2.cur^[point].x, dx );
      exc.zp2.flags^[point] := exc.zp2.flags^[point] or TT_Flag_Touched_X;
    end;

    if exc.GS.freeVector.y <> 0 then
    begin
      inc( exc.zp2.cur^[point].y, dy );
      exc.zp2.flags^[point] := exc.zp2.flags^[point] or TT_Flag_Touched_Y;
    end;
  end;

(**********************************************)
(* SHP[a]    : SHift Point by the last point  *)
(* CodeRange : $32-33                         *)

   procedure Ins_SHP( args : PStorage );
   var
     zp   : PGlyph_Zone;
     refp : Int;

     dx   : TT_F26dot6;
     dy   : TT_F26dot6;
     point: Int;
   begin

     if Compute_Point_Displacement( dx, dy, zp, refp ) then
       exit;

     if exc.top < exc.GS.loop then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     while exc.GS.loop > 0 do
     begin

       dec( exc.args );

       point := exc.stack^[ exc.args ];

       if (point < 0) or (point >= exc.zp2.n_points) then
       begin
         exc.error := TT_Err_Invalid_Reference;
         exit;
       end;

       Move_Zp2_Point( point, dx, dy );

       dec( exc.GS.loop );

     end;

     exc.GS.loop := 1;
     exc.new_top := exc.args;
   end;

(**********************************************)
(* SHC[a]    : SHift Contour                  *)
(* CodeRange : $34-35                         *)

   procedure Ins_SHC( args : PStorage );
   var
     zp   : PGlyph_Zone;
     refp : Int;
     dx   : TT_F26dot6;
     dy   : TT_F26dot6;

     contour, i : Int;

     first_point, last_point : Int;
   begin

     contour := args^[0];

     if (args^[0] < 0) or (args^[0] >= exc.pts.n_contours ) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     if Compute_Point_Displacement( dx, dy, zp, refp ) then
       exit;

     if contour = 0 then first_point := 0 else
                         first_point := exc.pts.conEnds^[contour-1]+1;

     last_point := exc.pts.conEnds^[contour];

     for i := first_point to last_point do
     begin
       if (zp^.cur <> exc.zp2.cur) or
          (refp <> i ) then

         Move_Zp2_Point( i, dx, dy );
     end;

   end;

(**********************************************)
(* SHZ[a]    : SHift Zone                     *)
(* CodeRange : $36-37                         *)

   procedure Ins_SHZ( args : PStorage );
   var
     zp   : PGlyph_Zone;
     refp : Int;
     dx   : TT_F26dot6;
     dy   : TT_F26dot6;

     zone, i : Int;

     last_point : Int;
   begin

     zone := args^[0];

     if (args^[0] < 0) or (args^[0] > 1) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     if Compute_Point_Displacement( dx, dy, zp, refp ) then
       exit;

     last_point := zp^.n_points-1;

     for i := 0 to last_point do
     begin
       if (zp^.cur <> exc.zp2.cur) or
          (refp <> i ) then

         Move_Zp2_Point( i, dx, dy );
     end;

   end;

(**********************************************)
(* SHPIX[]   : SHift points by a PIXel amount *)
(* CodeRange : $38                            *)

   procedure Ins_SHPIX( args : PStorage );
   var
     dx   : TT_F26dot6;
     dy   : TT_F26dot6;
     point: Int;
   begin

     if exc.top < exc.GS.loop then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     dx := MulDiv_Round( args^[0],
                         exc.GS.freeVector.x,
                         $4000 );

     dy := MulDiv_Round( args^[0],
                         exc.GS.freeVector.y,
                         $4000 );

     while exc.GS.loop > 0 do
     begin

       dec( exc.args );

       point := exc.stack^[ exc.args ];

       if (point < 0) or (point >= exc.zp2.n_points) then
       begin
         exc.error := TT_Err_Invalid_Reference;
         exit;
       end;

       Move_Zp2_Point( point, dx, dy );

       dec( exc.GS.loop );

     end;

     exc.GS.loop := 1;
     exc.new_top := exc.args;
   end;

(**********************************************)
(* MSIRP[a]  : Move Stack Indirect Relative   *)
(* CodeRange : $3A-$3B                        *)

   procedure Ins_MSIRP( args : PStorage );
   var
     point    : Int;
     distance : TT_F26dot6;
     vec1     : TT_Vector;
     vec2     : TT_Vector;
   begin

     point := args^[0];

     if (args^[0] < 0) or (args^[0] >= exc.zp1.n_points) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     (* XXX : UNDOCUMENTED - Twilight Zone *)

     (* Again, one stupid undocumented feature found in the *)
     (* twilight zone. What did these guys had in mind when *)
     (* they wrote the spec ? There _must_ be another       *)
     (* specification than the published one !! #@%$& !!    *)

     if exc.GS.gep0 = 0 then   (* if in twilight zone *)
     begin
       exc.zp1.org^[point] := exc.zp0.org^[exc.GS.rp0];
       exc.zp1.cur^[point] := exc.zp1.org^[point];
     end;

     distance := exc.func_project( exc.zp1.cur^[point],
                                   exc.zp0.cur^[exc.GS.rp0] );

     exc.func_move( @exc.zp1, point, args^[1] - distance );

     exc.GS.rp1 := exc.GS.rp0;
     exc.GS.rp2 := point;

     if exc.opcode and 1 <> 0 then exc.GS.rp0 := point;
   end;

(**********************************************)
(* MDAP[a]   : Move Direct Absolute Point     *)
(* CodeRange : $2E-$2F                        *)

   procedure Ins_MDAP( args : PStorage );
   var
     point    : Int;
     cur_dist : TT_F26dot6;
     distance : TT_F26dot6;
   begin
     point := args^[0];

     if (args^[0] < 0) or (args^[0] >= exc.zp0.n_points) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     (* XXXX Is there some undocumented feature while in the *)
     (*      twilight zone ??                                *)

     if exc.opcode and 1 <> 0 then
       begin

         cur_dist := exc.func_project( exc.zp0.cur^[point], Null_Vector );

         distance := exc.func_round( cur_dist,
                                     exc.metrics.compensations[0] ) -
                     cur_dist;
       end
     else
       distance := 0;

     exc.func_move( @exc.zp0, point, distance );

     exc.GS.rp0 := point;
     exc.GS.rp1 := point;
   end;

(**********************************************)
(* MIAP[a]   : Move Indirect Absolute Point   *)
(* CodeRange : $3E-$3F                        *)

   procedure Ins_MIAP( args : PStorage );
   var
     cvtEntry : Int;
     point    : Int;
     distance : TT_F26dot6;
     org_dist : TT_F26dot6;
   begin
     cvtEntry := args^[1];
     point    := args^[0];

     if (args^[0] < 0) or (args^[0] >= exc.zp0.n_points  ) or
        (args^[1] < 0) or (args^[1] >= exc.cvtSize) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     (* Undocumented :                                    *)
     (*                                                   *)
     (* The behaviour of an MIAP instruction is quite     *)
     (* different when used in the twilight zone^.        *)
     (*                                                   *)
     (* First, no control value cutin test is performed   *)
     (* as it would fail anyway. Second, the original     *)
     (* point, i.e. (org_x,org_y) of zp0.point, is set   *)
     (* to the absolute, unrounded, distance found in     *)
     (* the CVT.                                          *)
     (*                                                   *)
     (* This is used in the CVT programs of the Microsoft *)
     (* fonts Arial, Times, etc.., in order to re-adjust  *)
     (* some key font heights. It allows the use of the   *)
     (* IP instruction in the twilight zone, which        *)
     (* otherwise would be "illegal" per se the specs :)  *)
     (*                                                   *)
     (* We implement it with a special sequence for the   *)
     (* twilight zone. This is a bad hack, but it seems   *)
     (* to work..                                         *)
     (*                                         - David   *)

     distance := exc.func_read_cvt(cvtEntry);

     if exc.GS.gep0 = 0 then  (* If in twilight zone *)
     begin
       exc.zp0.org^[point].y := MulDiv_Round( exc.GS.freeVector.x,
                                              distance,
                                              $4000 );

       exc.zp0.org^[point].y := MulDiv_Round( exc.GS.freeVector.y,
                                              distance,
                                              $4000 );

       exc.zp0.cur^[point] := exc.zp0.org^[point];
     end;

     org_dist := exc.func_project( exc.zp0.cur^[point], Null_Vector );

     if exc.opcode and 1 <> 0 then  (* rounding and control cutin flag *)
     begin

       if abs( distance-org_dist ) > exc.GS.control_value_cutin then
         distance := org_dist;

       distance := exc.func_round( distance,
                                   exc.metrics.compensations[0] );
     end;

     exc.func_move( @exc.zp0, point, distance - org_dist );

     exc.GS.rp0 := point;
     exc.GS.rp1 := point;

   end;

(**********************************************)
(* MDRP[abcde] : Move Direct Relative Point   *)
(* CodeRange   : $C0-$DF                      *)

   procedure Ins_MDRP( args : PStorage );
   var
     point      : Int;
     distance   : TT_F26dot6;
     org_dist   : TT_F26dot6;
   begin
     point := args^[0];

     if (args^[0] < 0) or (args^[0] >= exc.zp1.n_points) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     (* XXXX Is there some undocumented feature while in the *)
     (*      twilight zone ??                                *)

     org_dist := exc.func_dualProj( exc.zp1.org^[point],
                                    exc.zp0.org^[exc.GS.rp0] );
     (* single width cutin test *)

     if abs(org_dist) < exc.GS.single_width_cutin then

       if org_dist >= 0 then org_dist :=  exc.GS.single_width_value
                        else org_dist := -exc.GS.single_width_value;

     (* round flag *)

     if exc.opcode and 4 <> 0 then

       distance := exc.func_round( org_dist,
                                   exc.metrics.compensations[ exc.opcode and 3 ] )
     else
       distance := Round_None( org_dist,
                               exc.metrics.compensations[ exc.opcode and 3 ] );

     (* minimum distance flag *)

     if exc.opcode and 8 <> 0 then
     begin

       if org_dist >= 0 then

         if distance < exc.GS.minimum_distance then
           distance := exc.GS.minimum_distance
         else
       else
         if distance > -exc.GS.minimum_distance then
           distance := -exc.GS.minimum_distance;
     end;

     (* now move the point *)

     org_dist := exc.func_project( exc.zp1.cur^[point],
                                   exc.zp0.cur^[exc.GS.rp0] );

     exc.func_move( @exc.zp1, point, distance - org_dist );

     exc.GS.rp1 := exc.GS.rp0;
     exc.GS.rp2 := point;

     if exc.opcode and 16 <> 0 then exc.GS.rp0 := point;
   end;

(**********************************************)
(* MIRP[abcde] : Move Indirect Relative Point *)
(* CodeRange   : $E0-$FF                      *)

   procedure Ins_MIRP( args : PStorage );
   var
     point    : Int;
     cvtEntry : Int;
     cvt_dist : TT_F26dot6;
     distance : TT_F26dot6;
     cur_dist : TT_F26dot6;
     org_dist : TT_F26dot6;
   begin

     point    := args^[0];
     cvtEntry := args^[1];

     (* XXX : UNDOCUMENTED => cvt[-1] = 0 ???? *)

     if (args^[0] < 0 ) or (args^[0] >= exc.zp1.n_points) or
        (args^[1] < -1) or (args^[1] >= exc.cvtSize) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     if cvtEntry < 0 then
       cvt_dist := 0
     else
       cvt_dist := exc.func_read_cvt(cvtEntry);

     (* single width test *)

     if abs(cvt_dist) < exc.GS.single_width_cutin then

       if cvt_dist >= 0 then cvt_dist :=  exc.GS.single_width_value
                        else cvt_dist := -exc.GS.single_width_value;

     (* XXX : Undocumented - twilight zone *)

     if exc.GS.gep1 = 0 then   (* if in twilight zone *)
     begin
       exc.zp1.org^[point].x := exc.zp0.org^[exc.GS.rp0].x +
                                MulDiv_Round( cvt_dist,
                                              exc.GS.freeVector.x,
                                              $4000 );

       exc.zp1.org^[point].x := exc.zp0.org^[exc.GS.rp0].y +
                                MulDiv_Round( cvt_dist,
                                              exc.GS.freeVector.y,
                                              $4000 );

       exc.zp1.cur^[point] := exc.zp1.org^[point];
     end;


     org_dist := exc.func_dualProj( exc.zp1.org^[point],
                                    exc.zp0.org^[exc.GS.rp0] );

     cur_dist := exc.func_Project( exc.zp1.cur^[point],
                                   exc.zp0.cur^[exc.GS.rp0] );

     (* auto-flip test *)

     if exc.GS.auto_flip then
       if (org_dist xor cvt_dist < 0) then
         cvt_dist := -cvt_dist;

     (* control value cutin and round *)

     if exc.opcode and 4 <> 0 then
       begin
         (* XXX : UNDOCUMENTED : only perform cut-in test when both *)
         (*       zone pointers refer to the points zone            *)

         if exc.GS.gep0 = exc.GS.gep1 then
           if abs( cvt_dist - org_dist ) >= exc.GS.control_value_cutin then
             cvt_dist := org_dist;

         distance := exc.func_round( cvt_dist,
                                     exc.metrics.compensations[ exc.opcode and 3 ] );
       end
     else
       distance := Round_None( cvt_dist,
                               exc.metrics.compensations[ exc.opcode and 3 ] );

     (* minimum distance test *)

     if exc.opcode and 8 <> 0 then
     begin
       if org_dist >= 0 then

         if distance < exc.GS.minimum_distance then
           distance := exc.GS.minimum_distance
         else
       else
         if distance > -exc.GS.minimum_distance then
           distance := -exc.GS.minimum_distance;
     end;

     exc.func_move( @exc.zp1, point, distance - cur_dist );

     exc.GS.rp1 := exc.GS.rp0;

     if exc.opcode and 16 <> 0 then exc.GS.rp0 := point;

    (* UNDOCUMENTED !! *)

     exc.GS.rp2 := point;
   end;

(**********************************************)
(* ALIGNRP[]   : ALIGN Relative Point         *)
(* CodeRange   : $3C                          *)

   procedure Ins_ALIGNRP( args : PStorage );
   var
     point    : Int;
     distance : TT_F26dot6;
   begin
     if exc.top < exc.GS.loop then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     while exc.GS.loop > 0 do
     begin

       dec( exc.args );

       point := exc.stack^[ exc.args ];

       if (point < 0) or (point >= exc.zp1.n_points) then
       begin
         exc.error := TT_Err_Invalid_Reference;
         exit;
       end;

       distance := exc.func_project( exc.zp1.cur^[point],
                                     exc.zp0.cur^[exc.GS.rp0] );

       exc.func_move( @exc.zp1, point, -distance );

       dec( exc.GS.loop );
     end;

     exc.GS.loop := 1;
     exc.new_top := exc.args;
   end;

(**********************************************)
(* AA[]        : Adjust Angle                 *)
(* CodeRange   : $7F                          *)

   procedure Ins_AA( args : PStorage );
   begin
     (* Intentional - no longer supported *)
   end;

(**********************************************)
(* ISECT[]     : moves point to InterSECTion  *)
(* CodeRange   : $0F                          *)

   procedure Ins_ISECT( args : PStorage );
   var
     point  : Int;
     a0, a1 : Int;
     b0, b1 : Int;

     discriminant : TT_F26dot6;
     dx,  dy,
     dax, day,
     dbx, dby     : TT_F26dot6;

     val : TT_F26dot6;

     R : TT_Vector;

     U, V   : TT_UnitVector;
     T1, T2 : Int64;
   begin

     point := args^[0];
     a0    := args^[1];
     a1    := args^[2];
     b0    := args^[3];
     b1    := args^[4];

     if (b0 >= exc.zp0.n_points) or (b1 >= exc.zp0.n_points) or
        (a0 >= exc.zp1.n_points) or (a1 >= exc.zp1.n_points) or
        (point >= exc.zp0.n_points) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;
(*
     if   Normalize( exc.zp1.cur_x^[a1] - exc.zp1.cur_x^[a0],
                     exc.zp1.cur_y^[a1] - exc.zp1.cur_y^[a0],
                     U )
        and
          Normalize( - exc.zp0.cur_x^[b1] - exc.zp0.cur_x^[b0],
                       exc.zp0.cur_y^[b1] - exc.zp0.cur_y^[b0],
                       V )
       then
         begin

           dx := MulDiv_Round( exc.zp0.cur_x^[b0] -
                               exc.zp1.cur_x^[a0],
                               V.x,
                               $4000 ) +

                 MulDiv_Round( exc.zp0.cur_y^[b0] -
                               exc.zp1.cur_y^[a0],
                               V.y,
                               $4000 );

           dy := MulDiv_Round( U.x, V.x, $4000 ) +
                 MulDiv_Round( U.y, V.y, $4000 );

           if dy <> 0 then
           begin
             dx := MulDiv_Round( dx, $4000, dy );

             exc.zp2.flags^[point] := exc.zp2.flags^[point] or
                                      TT_Flag_Touched_Both;

             exc.zp2.cur_x^[point] := exc.zp1.cur_x^[a0] +

                      MulDiv_Round( dx, U.x, $4000 );

             exc.zp2.cur_y^[point] := exc.zp1.cur_y^[a0] +

                      MulDiv_Round( dx, U.y, $4000 );

             exit;
           end;
        end;
 *)
     dbx := exc.zp0.cur^[b1].x - exc.zp0.cur^[b0].x;
     dby := exc.zp0.cur^[b1].y - exc.zp0.cur^[b0].y;

     dax := exc.zp1.cur^[a1].x - exc.zp1.cur^[a0].x;
     day := exc.zp1.cur^[a1].y - exc.zp1.cur^[a0].y;

     dx := exc.zp0.cur^[b0].x - exc.zp1.cur^[a0].x;
     dy := exc.zp0.cur^[b0].y - exc.zp1.cur^[a0].y;

     exc.zp2.flags^[point] := exc.zp2.flags^[point] or
                              TT_Flag_Touched_Both;

     discriminant := MulDiv( dax, -dby, $40 ) +
                     MulDiv( day,  dbx, $40 );

     if abs(discriminant) >= $40 then
       begin

         val := MulDiv( dx, -dby, $40 ) +
                MulDiv( dy,  dbx, $40 );

         R.x := MulDiv( val, dax, discriminant );
         R.y := MulDiv( val, day, discriminant );

         exc.zp2.cur^[point].x := exc.zp1.cur^[a0].x + R.x;
         exc.zp2.cur^[point].y := exc.zp1.cur^[a0].y + R.y;
       end
     else
       begin

         (* else, take the middle of the middles of A and B *)

         exc.zp2.cur^[point].x := ( exc.zp1.cur^[a0].x +
                                    exc.zp1.cur^[a1].x +
                                    exc.zp0.cur^[b0].x +
                                    exc.zp0.cur^[b1].x ) div 4;

         exc.zp2.cur^[point].y := ( exc.zp1.cur^[a0].y +
                                    exc.zp1.cur^[a1].y +
                                    exc.zp0.cur^[b0].y +
                                    exc.zp0.cur^[b1].y ) div 4;
       end;
   end;

(**********************************************)
(* ALIGNPTS[]  : ALIGN PoinTS                 *)
(* CodeRange   : $27                          *)

   procedure Ins_ALIGNPTS( args : PStorage );
   var
     p1, p2   : Int;
     distance : TT_F26dot6;
   begin
     p1 := args^[0];
     p2 := args^[1];

     if (args^[0] < 0) or (args^[0] >= exc.zp1.n_points) or
        (args^[1] < 0) or (args^[1] >= exc.zp0.n_points) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     distance := exc.func_project( exc.zp0.cur^[p2],
                                   exc.zp1.cur^[p1] ) div 2;

     exc.func_move( @exc.zp1, p1, distance );
     exc.func_move( @exc.zp0, p2, -distance );
   end;

(**********************************************)
(* IP[]        : Interpolate Point            *)
(* CodeRange   : $39                          *)

   procedure Ins_IP( args : PStorage );
   var
     org_a : TT_F26dot6;
     org_b : TT_F26dot6;
     org_x : TT_F26dot6;
     cur_a : TT_F26dot6;
     cur_b : TT_F26dot6;
     cur_x : TT_F26dot6;

     distance : TT_F26dot6;

     point     : Int;
   begin

     if exc.top < exc.GS.loop then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     org_a := exc.func_dualProj( exc.zp0.org^[exc.GS.rp1], Null_Vector );

     org_b := exc.func_dualProj( exc.zp1.org^[exc.GS.rp2], Null_Vector );

     cur_a := exc.func_project( exc.zp0.cur^[exc.GS.rp1], Null_Vector );

     cur_b := exc.func_project( exc.zp1.cur^[exc.GS.rp2], Null_Vector );

     while exc.GS.loop > 0 do
     begin

       dec( exc.args );

       point := exc.stack^[ exc.args ];

       org_x := exc.func_dualProj( exc.zp2.org^[point], Null_Vector );

       cur_x := exc.func_project( exc.zp2.cur^[point], Null_Vector );

       if (( org_a <= org_b ) and ( org_x <= org_a )) or
          (( org_a >  org_b ) and ( org_x >= org_a )) then
         begin
           distance := ( cur_a - org_a ) + ( org_x - cur_x );
         end
       else
       if (( org_a <= org_b ) and ( org_x >= org_b )) or
          (( org_a >  org_b ) and ( org_x <  org_b )) then
         begin
           distance := ( cur_b - org_b ) + ( org_x - cur_x );
         end
       else
         begin
           (* note : it seems that rounding this value isn't a good *)
           (*        idea ( width of capital 'S' in Times           *)

           distance := MulDiv( cur_b - cur_a,
                               org_x - org_a,
                               org_b - org_a ) + ( cur_a - cur_x );
         end;

       exc.func_move( @exc.zp2, point, distance );

       dec( exc.GS.loop );
     end;

     exc.GS.loop := 1;
     exc.new_top := exc.args;
   end;

(**********************************************)
(* UTP[a]      : UnTouch Point                *)
(* CodeRange   : $29                          *)

   procedure Ins_UTP( args : PStorage );
   var
     mask : Byte;
   begin
     if (args^[0] < 0) or (args^[0] >= exc.zp0.n_points) then
     begin
       exc.error := TT_Err_Invalid_Reference;
       exit;
     end;

     mask := $FF;

     if exc.GS.freeVector.x <> 0 then mask := mask and not TT_Flag_Touched_X;
     if exc.GS.freeVector.y <> 0 then mask := mask and not TT_Flag_Touched_Y;

     exc.zp0.flags^[args^[0]] := exc.zp0.flags^[args^[0]] and mask;
   end;

(**********************************************)
(* IUP[a]      : Interpolate Untouched Points *)
(* CodeRange   : $30-$31                      *)

   procedure Ins_IUP( args : PStorage );
   var
     mask : byte;

     first_point,    (* first point of contour        *)
     end_point,      (* end point (last+1) of contour *)

     first_touched,  (* first touched point in contour   *)
     cur_touched,    (* current touched point in contour *)

     point,          (* current point   *)
     contour : Int;  (* current contour *)

     orgs,              (* original and current coordinate *)
     curs  : TT_Points; (* arrays                          *)

     procedure Shift_X( p1, p2, p : Int );
     var
       i : Int;
       x : TT_F26dot6;
     begin
       x := curs^[p].x - orgs^[p].x;

       for i := p1 to p-1 do inc( curs^[i].x, x );
       for i := p+1 to p2 do inc( curs^[i].x, x );
     end;

     procedure Shift_Y( p1, p2, p : Int );
     var
       i : Int;
       y : TT_F26dot6;
     begin
       y := curs^[p].y - orgs^[p].y;

       for i := p1 to p-1 do inc( curs^[i].y, y );
       for i := p+1 to p2 do inc( curs^[i].y, y );
     end;


     procedure Interp_X( p1, p2, ref1, ref2 : Int );
     var
       i                 : Int;
       x, x1, x2, d1, d2 : TT_F26dot6;
     begin

       if p1 > p2 then exit;

       x1 := orgs^[ref1].x;  d1 := curs^[ref1].x - orgs^[ref1].x;
       x2 := orgs^[ref2].x;  d2 := curs^[ref2].x - orgs^[ref2].x;

       if x1 = x2 then
         for i := p1 to p2 do
         begin
           x := orgs^[i].x;
           if x <= x1 then x := x + d1
                      else x := x + d2;

           curs^[i].x := x;
         end

       else
       if x1 < x2 then

         for i := p1 to p2 do
         begin
           x := orgs^[i].x;

           if (x <= x1) then x := x + d1
           else
           if (x >= x2) then x := x + d2
           else
             x := curs^[ref1].x +
                  MulDiv( x-x1, curs^[ref2].x-curs^[ref1].x, x2-x1 );

           curs^[i].x := x;
         end
       else

         (* x2 < x1 *)

         for i := p1 to p2 do
         begin
           x := orgs^[i].x;

           if ( x <= x2 ) then x := x + d2
           else
           if ( x >= x1 ) then x := x + d1
           else
             x := curs^[ref1].x +
                  MulDiv( x-x1, curs^[ref2].x-curs^[ref1].x, x2-x1 );

           curs^[i].x := x;
         end;
     end;

     procedure Interp_Y( p1, p2, ref1, ref2 : Int );
     var
       i                 : Int;
       y, y1, y2, d1, d2 : TT_F26dot6;
     begin

       if p1 > p2 then exit;

       y1 := orgs^[ref1].y;  d1 := curs^[ref1].y - orgs^[ref1].y;
       y2 := orgs^[ref2].y;  d2 := curs^[ref2].y - orgs^[ref2].y;

       if y1 = y2 then
         for i := p1 to p2 do
         begin
           y := orgs^[i].y;
           if y <= y1 then y := y + d1
                      else y := y + d2;

           curs^[i].y := y;
         end

       else
       if y1 < y2 then

         for i := p1 to p2 do
         begin
           y := orgs^[i].y;

           if (y <= y1) then y := y + d1
           else
           if (y >= y2) then y := y + d2
           else
             y := curs^[ref1].y +
                  MulDiv( y-y1, curs^[ref2].y-curs^[ref1].y, y2-y1 );

           curs^[i].y := y;
         end
       else

         (* y2 < y1 *)

         for i := p1 to p2 do
         begin
           y := orgs^[i].y;

           if ( y <= y2 ) then y := y + d2
           else
           if ( y >= y1 ) then y := y + d1
           else
             y := curs^[ref1].y +
                  MulDiv( y-y1, curs^[ref2].y-curs^[ref1].y, y2-y1 );

           curs^[i].y := y;
         end;
     end;

   begin
     orgs := exc.pts.org;
     curs := exc.pts.cur;

     case exc.opcode and 1 of
       0 : mask := TT_Flag_Touched_Y;
       1 : mask := TT_Flag_Touched_X;
     end;

     with exc do
     begin

       contour := 0;
       point   := 0;

       repeat

         end_point   := pts.conEnds^[contour];
         first_point := point;

         while ( point <= end_point          ) and
               ( pts.flags^[point] and mask = 0 ) do  inc(point);

         if point <= end_point then
         begin

           first_touched := point;
           cur_touched   := point;

           inc( point );

           while ( point <= end_point ) do
           begin
             if pts.flags^[point] and mask <> 0 then
             begin
               if opcode and 1 <> 0 then
                 Interp_X( cur_touched+1, point-1, cur_touched, point )
               else
                 Interp_Y( cur_touched+1, point-1, cur_touched, point );

               cur_touched := point;
             end;

             inc( point );
           end;

           if cur_touched = first_touched then
             if opcode and 1 <> 0 then
               Shift_X( first_point, end_point, cur_touched )
             else
               Shift_Y( first_point, end_point, cur_touched )
           else
             begin
               if opcode and 1 <> 0 then
               begin
                 interp_x( cur_touched+1, end_point,   cur_touched, first_touched );
                 interp_x( first_point, first_touched-1, cur_touched, first_touched );
               end
               else
               begin
                 interp_y( cur_touched+1, end_point,   cur_touched, first_touched );
                 interp_y( first_point, first_touched-1, cur_touched, first_touched );
               end;
             end;

         end;

         inc( contour );

       until contour >= pts.n_contours;

     end;

   end;

(**********************************************)
(* DELTAPn[]   : DELTA Exceptions P1, P2, P3  *)
(* CodeRange   : $5D,$71,$72                  *)

   procedure Ins_DELTAP( args : PStorage );
   var
     nump    : Int;
     k       : Int;
     A, B, C :Int;
   begin

     nump := args^[0];

     for K := 1 to nump do
       begin
         if exc.args < 2 then
         begin
           exc.error := TT_Err_Too_Few_Arguments;
           exit;
         end;

         dec( exc.args, 2 );

         A := exc.stack^[exc.args+1];
         B := exc.stack^[ exc.args ];

         (* XXX :                                              *)
         (* some commonly fonts have broke programs where the  *)
         (* the point reference has an invalid value. Here, we *)
         (* simply ignore them, because a DeltaP won't change  *)
         (* a glyph shape dramatically..                       *)
         (*                                                    *)

         if A < exc.zp0.n_points then
         begin
           C := ( B and $F0 ) shr 4;

           Case exc.opcode of
             $5D : ;
             $71 : C := C+16;
             $72 : C := C+32;
            end;

           C := C + exc.GS.delta_Base;

           if GET_Ppem = C then
             begin
               B := (B and $F) - 8;
               if B >= 0 then B := B+1;
               B := ( B*64 ) div ( 1 shl exc.GS.delta_Shift );

               exc.func_move( @exc.zp0, A, B );
             end;
         end;

       end;

     exc.new_top := exc.args;
   end;


(**********************************************)
(* DELTACn[]   : DELTA Exceptions C1, C2, C3  *)
(* CodeRange   : $73,$74,$75                  *)

   procedure Ins_DELTAC( args : PStorage );
   var
     nump    : Int;
     k       : Int;
     A, B, C :Int;
   begin

     nump := args^[0];

     for K := 1 to nump do
       begin
         if exc.args < 2 then
         begin
           exc.error := TT_Err_Too_Few_Arguments;
           exit;
         end;

         dec( exc.args, 2 );

         A := exc.stack^[exc.args+1];
         B := exc.stack^[ exc.args ];

         if A >= exc.cvtSize then
         begin
           exc.error := TT_Err_Invalid_Reference;
           exit;
         end;

         C := ( B and $F0 ) shr 4;

         Case exc.opcode of
           $73 : ;
           $74 : C := C+16;
           $75 : C := C+32;
          end;

         C := C + exc.GS.delta_Base;

         if GET_Ppem = C then
           begin
             B := (B and $F) - 8;
             if B >= 0 then B := B+1;
             B := ( B*64 ) div ( 1 shl exc.GS.delta_Shift );

             exc.func_move_cvt( A, B );
           end;
       end;

     exc.new_top := exc.args;
   end;

(****************************************************************)
(*                                                              *)
(* MISC. INSTRUCTIONS                                           *)
(*                                                              *)
(****************************************************************)

(***********************************************************)
(* DEBUG[]     : DEBUG. Unsupported                        *)
(* CodeRange   : $4F                                       *)

(* NOTE : The original instruction pops a value from the stack *)

   procedure Ins_DEBUG( args : PStorage );
   begin
     exc.error := TT_Err_Debug_Opcode;
   end;

(**********************************************)
(* GETINFO[]   : GET INFOrmation              *)
(* CodeRange   : $88                          *)

   procedure Ins_GETINFO( args : PStorage );
   var
     K : Int;
   begin
     K := 0;

     if args^[0] and 1 <> 0 then K := 3;
     (* We return then Windows 3.1 version number *)
     (* for the font scaler                       *)

     if false then K := K or $80;
     (* Has the glyph been rotated ? *)
     (* XXXX TO DO *)

     if false then K := K or $100;
     (* Has the glyph been stretched ? *)
     (* XXXX TO DO *)

     args^[0] := K;
   end;


   procedure Ins_UNKNOWN( args : PStorage );
   begin
     exc.error := TT_Err_Invalid_Opcode;
   end;
   {$F-}



const
  Instruct_Dispatch : array[0..255] of TInstruction_Function
         = (
             (* SVTCA  y *)  Ins_SVTCA,
             (* SVTCA  x *)  Ins_SVTCA,
             (* SPvTCA y *)  Ins_SPVTCA,
             (* SPvTCA x *)  Ins_SPVTCA,
             (* SFvTCA y *)  Ins_SFVTCA,
             (* SFvTCA x *)  Ins_SFVTCA,
             (* SPvTL // *)  Ins_SPVTL,
             (* SPvTL +  *)  Ins_SPVTL,
             (* SFvTL // *)  Ins_SFVTL,
             (* SFvTL +  *)  Ins_SFVTL,
             (* SPvFS    *)  Ins_SPVFS,
             (* SFvFS    *)  Ins_SFVFS,
             (* GPV      *)  Ins_GPV,
             (* GFV      *)  Ins_GFV,
             (* SFvTPv   *)  Ins_SFVTPV,
             (* ISECT    *)  Ins_ISECT,

             (* SRP0     *)  Ins_SRP0,
             (* SRP1     *)  Ins_SRP1,
             (* SRP2     *)  Ins_SRP2,
             (* SZP0     *)  Ins_SZP0,
             (* SZP1     *)  Ins_SZP1,
             (* SZP2     *)  Ins_SZP2,
             (* SZPS     *)  Ins_SZPS,
             (* SLOOP    *)  Ins_SLOOP,
             (* RTG      *)  Ins_RTG,
             (* RTHG     *)  Ins_RTHG,
             (* SMD      *)  Ins_SMD,
             (* ELSE     *)  Ins_ELSE,
             (* JMPR     *)  Ins_JMPR,
             (* SCvTCi   *)  Ins_SCVTCI,
             (* SSwCi    *)  Ins_SSWCI,
             (* SSW      *)  Ins_SSW,

             (* DUP      *)  Ins_DUP,
             (* POP      *)  Ins_POP,
             (* CLEAR    *)  Ins_CLEAR,
             (* SWAP     *)  Ins_SWAP,
             (* DEPTH    *)  Ins_DEPTH,
             (* CINDEX   *)  Ins_CINDEX,
             (* MINDEX   *)  Ins_MINDEX,
             (* AlignPTS *)  Ins_ALIGNPTS,
             (* INS_$28  *)  Ins_UNKNOWN,
             (* UTP      *)  Ins_UTP,
             (* LOOPCALL *)  Ins_LOOPCALL,
             (* CALL     *)  Ins_CALL,
             (* FDEF     *)  Ins_FDEF,
             (* ENDF     *)  Ins_ENDF,
             (* MDAP[0]  *)  Ins_MDAP,
             (* MDAP[1]  *)  Ins_MDAP,

             (* IUP[0]   *)  Ins_IUP,
             (* IUP[1]   *)  Ins_IUP,
             (* SHP[0]   *)  Ins_SHP,
             (* SHP[1]   *)  Ins_SHP,
             (* SHC[0]   *)  Ins_SHC,
             (* SHC[1]   *)  Ins_SHC,
             (* SHZ[0]   *)  Ins_SHZ,
             (* SHZ[1]   *)  Ins_SHZ,
             (* SHPIX    *)  Ins_SHPIX,
             (* IP       *)  Ins_IP,
             (* MSIRP[0] *)  Ins_MSIRP,
             (* MSIRP[1] *)  Ins_MSIRP,
             (* AlignRP  *)  Ins_ALIGNRP,
             (* RTDG     *)  Ins_RTDG,
             (* MIAP[0]  *)  Ins_MIAP,
             (* MIAP[1]  *)  Ins_MIAP,

             (* NPushB   *)  Ins_NPUSHB,
             (* NPushW   *)  Ins_NPUSHW,
             (* WS       *)  Ins_WS,
             (* RS       *)  Ins_RS,
             (* WCvtP    *)  Ins_WCVTP,
             (* RCvt     *)  Ins_RCVT,
             (* GC[0]    *)  Ins_GC,
             (* GC[1]    *)  Ins_GC,
             (* SCFS     *)  Ins_SCFS,
             (* MD[0]    *)  Ins_MD,
             (* MD[1]    *)  Ins_MD,
             (* MPPEM    *)  Ins_MPPEM,
             (* MPS      *)  Ins_MPS,
             (* FlipON   *)  Ins_FLIPON,
             (* FlipOFF  *)  Ins_FLIPOFF,
             (* DEBUG    *)  Ins_DEBUG,

             (* LT       *)  Ins_LT,
             (* LTEQ     *)  Ins_LTEQ,
             (* GT       *)  Ins_GT,
             (* GTEQ     *)  Ins_GTEQ,
             (* EQ       *)  Ins_EQ,
             (* NEQ      *)  Ins_NEQ,
             (* ODD      *)  Ins_ODD,
             (* EVEN     *)  Ins_EVEN,
             (* IF       *)  Ins_IF,
             (* EIF      *)  Ins_EIF,
             (* AND      *)  Ins_AND,
             (* OR       *)  Ins_OR,
             (* NOT      *)  Ins_NOT,
             (* DeltaP1  *)  Ins_DELTAP,
             (* SDB      *)  Ins_SDB,
             (* SDS      *)  Ins_SDS,

             (* ADD      *)  Ins_ADD,
             (* SUB      *)  Ins_SUB,
             (* DIV      *)  Ins_DIV,
             (* MUL      *)  Ins_MUL,
             (* ABS      *)  Ins_ABS,
             (* NEG      *)  Ins_NEG,
             (* FLOOR    *)  Ins_FLOOR,
             (* CEILING  *)  Ins_CEILING,
             (* ROUND[0] *)  Ins_ROUND,
             (* ROUND[1] *)  Ins_ROUND,
             (* ROUND[2] *)  Ins_ROUND,
             (* ROUND[3] *)  Ins_ROUND,
             (* NROUND[0]*)  Ins_ROUND,
             (* NROUND[1]*)  Ins_ROUND,
             (* NROUND[2]*)  Ins_ROUND,
             (* NROUND[3]*)  Ins_ROUND,

             (* WCvtF    *)  Ins_WCVTF,
             (* DeltaP2  *)  Ins_DELTAP,
             (* DeltaP3  *)  Ins_DELTAP,
             (* DeltaCn[0]*) Ins_DELTAC,
             (* DeltaCn[1]*) Ins_DELTAC,
             (* DeltaCn[2]*) Ins_DELTAC,
             (* SROUND   *)  Ins_SROUND,
             (* S45Round *)  Ins_S45ROUND,
             (* JROT     *)  Ins_JROT,
             (* JROF     *)  Ins_JROF,
             (* ROFF     *)  Ins_ROFF,
             (* INS_$7B  *)  Ins_UNKNOWN,
             (* RUTG     *)  Ins_RUTG,
             (* RDTG     *)  Ins_RDTG,
             (* SANGW    *)  Ins_SANGW,
             (* AA       *)  Ins_AA,

             (* FlipPT   *)  Ins_FLIPPT,
             (* FlipRgON *)  Ins_FLIPRGON,
             (* FlipRgOFF*)  Ins_FLIPRGOFF,
             (* INS_$83  *)  Ins_UNKNOWN,
             (* INS_$84  *)  Ins_UNKNOWN,
             (* ScanCTRL *)  Ins_SCANCTRL,
             (* SDPVTL[0]*)  Ins_SDPVTL,
             (* SDPVTL[1]*)  Ins_SDPVTL,
             (* GetINFO  *)  Ins_GETINFO,
             (* IDEF     *)  Ins_IDEF,
             (* ROLL     *)  Ins_ROLL,
             (* MAX      *)  Ins_MAX,
             (* MIN      *)  Ins_MIN,
             (* ScanTYPE *)  Ins_SCANTYPE,
             (* InstCTRL *)  Ins_INSTCTRL,
             (* INS_$8F  *)  Ins_UNKNOWN,

             (* INS_$90 *)   Ins_UNKNOWN,
             (* INS_$91 *)   Ins_UNKNOWN,
             (* INS_$92 *)   Ins_UNKNOWN,
             (* INS_$93 *)   Ins_UNKNOWN,
             (* INS_$94 *)   Ins_UNKNOWN,
             (* INS_$95 *)   Ins_UNKNOWN,
             (* INS_$96 *)   Ins_UNKNOWN,
             (* INS_$97 *)   Ins_UNKNOWN,
             (* INS_$98 *)   Ins_UNKNOWN,
             (* INS_$99 *)   Ins_UNKNOWN,
             (* INS_$9A *)   Ins_UNKNOWN,
             (* INS_$9B *)   Ins_UNKNOWN,
             (* INS_$9C *)   Ins_UNKNOWN,
             (* INS_$9D *)   Ins_UNKNOWN,
             (* INS_$9E *)   Ins_UNKNOWN,
             (* INS_$9F *)   Ins_UNKNOWN,

             (* INS_$A0 *)   Ins_UNKNOWN,
             (* INS_$A1 *)   Ins_UNKNOWN,
             (* INS_$A2 *)   Ins_UNKNOWN,
             (* INS_$A3 *)   Ins_UNKNOWN,
             (* INS_$A4 *)   Ins_UNKNOWN,
             (* INS_$A5 *)   Ins_UNKNOWN,
             (* INS_$A6 *)   Ins_UNKNOWN,
             (* INS_$A7 *)   Ins_UNKNOWN,
             (* INS_$A8 *)   Ins_UNKNOWN,
             (* INS_$A9 *)   Ins_UNKNOWN,
             (* INS_$AA *)   Ins_UNKNOWN,
             (* INS_$AB *)   Ins_UNKNOWN,
             (* INS_$AC *)   Ins_UNKNOWN,
             (* INS_$AD *)   Ins_UNKNOWN,
             (* INS_$AE *)   Ins_UNKNOWN,
             (* INS_$AF *)   Ins_UNKNOWN,

             (* PushB[0] *)  Ins_PUSHB,
             (* PushB[1] *)  Ins_PUSHB,
             (* PushB[2] *)  Ins_PUSHB,
             (* PushB[3] *)  Ins_PUSHB,
             (* PushB[4] *)  Ins_PUSHB,
             (* PushB[5] *)  Ins_PUSHB,
             (* PushB[6] *)  Ins_PUSHB,
             (* PushB[7] *)  Ins_PUSHB,
             (* PushW[0] *)  Ins_PUSHW,
             (* PushW[1] *)  Ins_PUSHW,
             (* PushW[2] *)  Ins_PUSHW,
             (* PushW[3] *)  Ins_PUSHW,
             (* PushW[4] *)  Ins_PUSHW,
             (* PushW[5] *)  Ins_PUSHW,
             (* PushW[6] *)  Ins_PUSHW,
             (* PushW[7] *)  Ins_PUSHW,

             (* MDRP[00] *)  Ins_MDRP,
             (* MDRP[01] *)  Ins_MDRP,
             (* MDRP[02] *)  Ins_MDRP,
             (* MDRP[03] *)  Ins_MDRP,
             (* MDRP[04] *)  Ins_MDRP,
             (* MDRP[05] *)  Ins_MDRP,
             (* MDRP[06] *)  Ins_MDRP,
             (* MDRP[07] *)  Ins_MDRP,
             (* MDRP[08] *)  Ins_MDRP,
             (* MDRP[09] *)  Ins_MDRP,
             (* MDRP[10] *)  Ins_MDRP,
             (* MDRP[11] *)  Ins_MDRP,
             (* MDRP[12] *)  Ins_MDRP,
             (* MDRP[13] *)  Ins_MDRP,
             (* MDRP[14] *)  Ins_MDRP,
             (* MDRP[15] *)  Ins_MDRP,
             (* MDRP[16] *)  Ins_MDRP,
             (* MDRP[17] *)  Ins_MDRP,

             (* MDRP[18] *)  Ins_MDRP,
             (* MDRP[19] *)  Ins_MDRP,
             (* MDRP[20] *)  Ins_MDRP,
             (* MDRP[21] *)  Ins_MDRP,
             (* MDRP[22] *)  Ins_MDRP,
             (* MDRP[23] *)  Ins_MDRP,
             (* MDRP[24] *)  Ins_MDRP,
             (* MDRP[25] *)  Ins_MDRP,
             (* MDRP[26] *)  Ins_MDRP,
             (* MDRP[27] *)  Ins_MDRP,
             (* MDRP[28] *)  Ins_MDRP,
             (* MDRP[29] *)  Ins_MDRP,
             (* MDRP[30] *)  Ins_MDRP,
             (* MDRP[31] *)  Ins_MDRP,

             (* MIRP[00] *)  Ins_MIRP,
             (* MIRP[01] *)  Ins_MIRP,
             (* MIRP[02] *)  Ins_MIRP,
             (* MIRP[03] *)  Ins_MIRP,
             (* MIRP[04] *)  Ins_MIRP,
             (* MIRP[05] *)  Ins_MIRP,
             (* MIRP[06] *)  Ins_MIRP,
             (* MIRP[07] *)  Ins_MIRP,
             (* MIRP[08] *)  Ins_MIRP,
             (* MIRP[09] *)  Ins_MIRP,
             (* MIRP[10] *)  Ins_MIRP,
             (* MIRP[11] *)  Ins_MIRP,
             (* MIRP[12] *)  Ins_MIRP,
             (* MIRP[13] *)  Ins_MIRP,
             (* MIRP[14] *)  Ins_MIRP,
             (* MIRP[15] *)  Ins_MIRP,

             (* MIRP[16] *)  Ins_MIRP,
             (* MIRP[17] *)  Ins_MIRP,
             (* MIRP[18] *)  Ins_MIRP,
             (* MIRP[19] *)  Ins_MIRP,
             (* MIRP[20] *)  Ins_MIRP,
             (* MIRP[21] *)  Ins_MIRP,
             (* MIRP[22] *)  Ins_MIRP,
             (* MIRP[23] *)  Ins_MIRP,
             (* MIRP[24] *)  Ins_MIRP,
             (* MIRP[25] *)  Ins_MIRP,
             (* MIRP[26] *)  Ins_MIRP,
             (* MIRP[27] *)  Ins_MIRP,
             (* MIRP[28] *)  Ins_MIRP,
             (* MIRP[29] *)  Ins_MIRP,
             (* MIRP[30] *)  Ins_MIRP,
             (* MIRP[31] *)  Ins_MIRP

           );

(****************************************************************)
(*                                                              *)
(*                    RUN                                       *)
(*                                                              *)
(*  This function executes a run of opcodes. It will exit       *)
(*  in the following cases :                                    *)
(*                                                              *)
(*   - Errors ( in which case it returns FALSE )                *)
(*                                                              *)
(*   - Reaching the end of the main code range  (returns TRUE)  *)
(*      reaching the end of a code range within a function      *)
(*      call is an error.                                       *)
(*                                                              *)
(*   - After executing one single opcode, if the flag           *)
(*     'Instruction_Trap' is set to TRUE. (returns TRUE)        *)
(*                                                              *)
(*  On exit whith TRUE, test IP < CodeSize to know wether it    *)
(*  comes from a instruction trap or a normal termination       *)
(*                                                              *)
(*                                                              *)
(*     Note : The documented DEBUG opcode pops a value from     *)
(*            the stack. This behaviour is unsupported, here    *)
(*            a DEBUG opcode is always an error.                *)
(*                                                              *)
(*                                                              *)
(* THIS IS THE INTERPRETER'S MAIN LOOP                          *)
(*                                                              *)
(*  Instructions appear in the specs' order                     *)
(*                                                              *)
(****************************************************************)

  function Run_Ins( exec : PExec_Context ) : Boolean;
  label
    SuiteLabel, ErrorLabel, No_Error;
  var
    A : Int;
  begin

    exc := exec^;

    (* set cvt functions *)

    exc.metrics.ratio := 0;
    if exc.instance^.metrics.x_ppem <> exc.instance^.metrics.y_ppem then
{$IFDEF FPK}
      begin
        exc.func_read_cvt  := @Read_CVT_Stretched;
        exc.func_write_cvt := @Write_CVT_Stretched;
        exc.func_move_cvt  := @Move_CVT_Stretched;
      end
    else
      begin
        exc.func_read_cvt  := @Read_CVT;
        exc.func_write_cvt := @Write_CVT;
        exc.func_move_cvt  := @Move_CVT;
      end;
{$ELSE}
      begin
        exc.func_read_cvt  := Read_CVT_Stretched;
        exc.func_write_cvt := Write_CVT_Stretched;
        exc.func_move_cvt  := Move_CVT_Stretched;
      end
    else
      begin
        exc.func_read_cvt  := Read_CVT;
        exc.func_write_cvt := Write_CVT;
        exc.func_move_cvt  := Move_CVT;
      end;
{$ENDIF}
    Compute_Funcs;
    Compute_Round( exc.GS.round_state );

    repeat
      Calc_Length;

     (* First, let's check for empty stack and overflow *)

      exc.args := exc.top - Pop_Push_Count[ exc.opcode*2 ];

     (* args is the top of the stack once arguments have been popped *)
     (* one can also see it as the index of the last argument        *)

      if exc.args < 0 then
      begin
        exc.error := TT_Err_Too_Few_Arguments;
        goto ErrorLabel;
      end;

      exc.new_top := exc.args + Pop_Push_Count[ exc.opcode*2+1 ];

     (* new_top  is the new top of the stack, after the instruction's *)
     (* execution. top will be set to new_top after the 'case'        *)

      if exc.new_top > exc.stackSize then
        begin
          exc.error := TT_Err_Stack_Overflow;
          goto ErrorLabel;
        end;

      exc.step_ins := true;
      exc.error    := TT_Err_Ok;

      Instruct_Dispatch[ exc.opcode ]( PStorage(@exc.stack^[exc.args]) );

      if exc.error <> TT_Err_Ok then
      begin

        case exc.error of

          TT_Err_Invalid_Opcode:  (* looking for redefined instructions *)

            begin
              A := 0;
              while ( A < exc.numIDefs ) do
                with exc.IDefs^[A] do

                  if Active and ( exc.opcode = Opc ) then
                    begin
                      if exc.callTop >= exc.callSize then
                        begin
                          exc.error := TT_Err_Invalid_Reference;
                          goto ErrorLabel;
                        end;

                      with exc.callstack^[exc.callTop] do
                        begin
                          Caller_Range := exc.curRange;
                          Caller_IP    := exc.IP+1;
                          Cur_Count    := 1;
                          Cur_Restart  := Start;
                        end;

                      if not Goto_CodeRange( Range, Start ) then
                        goto ErrorLabel;

                      goto SuiteLabel;
                    end
                  else
                    inc(A);

                exc.error := TT_Err_Invalid_Opcode;
                goto ErrorLabel;

            end;
        else
          exc.error := exc.error;
          goto ErrorLabel;
        end;

      end;

      exc.top := exc.new_top;

      if exc.step_ins then inc( exc.IP, exc.length );

  SuiteLabel:

      if (exc.IP >= exc.codeSize) then

       if exc.callTop > 0 then
         begin
           exc.error := TT_Err_Code_Overflow;
           goto ErrorLabel;
         end
       else
         goto No_Error;

    until exc.instruction_trap;

  No_Error:
    Run_Ins := Success;
    exec^   := exc;
    exit;

  ErrorLabel:
    Run_Ins := Failure;
    exec^   := exc;

  end;

end.
