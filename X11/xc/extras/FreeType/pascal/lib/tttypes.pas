(*******************************************************************
 *
 *  TTTypes.pas                                                  1.0
 *
 *    Global internal types definitions
 *
 *  Copyright 1996, 1997 by
 *  David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 ******************************************************************)

unit TTTypes;

interface

uses FreeType;

type

  (*********************** SIMPLE PRIMITIVE TYPES *******************)

  (* BYTE is already defined in Pascal       *)
  (* They are equivalent to C unsigned chars *)

  UShort   = Word;          (* unsigned short integer, must be on 16 bits *)
  Short    = Integer;       (* signed short integer,   must be on 16 bits *)

  Long     = Longint;
  ULong    = LongInt;      (* unsigned long integer, must be on 32 bits *)
                           (* NOTE : There is no 'LongWord' in Pascal,  *)
                           (*        but the unsigned ops are all in    *)
                           (*        the inline assembly routines       *)

{$IFDEF USE32}
  Int = LongInt;      (* the 'int' type is used for loop counters and  *)
{$ELSE}               (* indexes.. Their size must be the one a given  *)
  Int = Integer;      (* system handles most easily ( 16 bits on Turbo *)
{$ENDIF}              (* and 32 on Virtual Pascals )                   *)

  TByteArray = array[0..1000] of Byte;
  PByte      = ^TByteArray;

  TShortArray = array[0..1000] of Short;
  PShort      = ^TShortArray;

  TUShortArray = array[0..1000] of UShort;
  PUShort      = ^TUShortArray;

  TStorage    = array[0..16000] of Long;
  PStorage    = ^TStorage;
  PLong       = PStorage;
  PULong      = PStorage;

  TError      = boolean;

  (***************** FreeType Internal Types *****************************)

  TCoordinates = array[0..1023] of TT_F26Dot6;
  PCoordinates = ^TCoordinates;

  PTouchTable  = PByte;

  TVecRecord = record
                 n     : Int;           (* number of points            *)
                 org_x : PCoordinates;  (* original coordinates arrays *)
                 org_y : PCoordinates;
                 cur_x : PCoordinates;  (* current coordinates arrays  *)
                 cur_y : PCoordinates;
                 touch : PTouchTable;   (* touch flags array           *)
               end;
  (* This type is used to describe each point zone in the interpreter  *)

const

  TT_Round_Off            = 5;
  TT_Round_To_Half_Grid   = 0;
  TT_Round_To_Grid        = 1;
  TT_Round_To_Double_Grid = 2;
  TT_Round_Up_To_Grid     = 4;
  TT_Round_Down_To_Grid   = 3;
  TT_Round_Super          = 6;
  TT_ROund_Super_45       = 7;

  Success = False;
  Failure = True;

  TT_Flag_Touched_X    = $02;  (* X touched flag *)
  TT_Flag_Touched_Y    = $04;  (* Y touched flag *)

  TT_Flag_Touched_Both = TT_Flag_Touched_X or TT_FLag_Touched_Y;

  TT_Flag_On_Curve  = $01;  (* Point is On curve *)

implementation

end.
