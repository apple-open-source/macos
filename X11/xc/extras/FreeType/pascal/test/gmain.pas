(*******************************************************************
 *
 *  gmain     graphics utility main interface                   1.1
 *
 *  This file defines a common interface, implemented in the body
 *  file 'gmain.c'. It relies on system dependent driver files,
 *  like 'gfs_os.c', whose interface is described in 'gdriver.h'.
 *
 *  Copyright 1996 David Turner, Robert Wilhelm and Werner Lemberg.
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 ******************************************************************)

Unit GMain;

interface

const
  Graphics_Mode_Mono = 1;
  Graphics_Mode_Gray = 2;

type
  TVioScreenBuffer = array[0..0] of Byte;
  PVioScreenBuffer = ^TVioScreenBuffer;

{$IFDEF OS2}
  Int = LongInt;
{$ELSE}
  Int = Integer;
{$ENDIF}

var

  Vio : PVioScreenBuffer;
  (* pointer to VRAM or display buffer *)

  vio_ScanLineWidth : Int;
  vio_Width         : Int;
  vio_Height        : Int;

  gray_palette : array[0..4] of Byte;  (* gray palette *)

  gcursor_x : int;
  gcursor_y : int;

  gwindow_width  : int;
  gwindow_height : int;

  function Set_Graph_Screen( mode : int ) : boolean;
  (* Set a Graphics Mode, chosen from the Graphics_Mode_xxx list *)

  function Restore_Screen : boolean;
  (* Restore a previous ( or text ) video mode *)

  procedure Display_Bitmap_On_Screen( var buff; line, col : Int );
  (* display bitmap of 'line' line, and 'col' columns ( each *)
  (* column mode of 1 byte                                   *)

  procedure Goto_XY( x, y : int );

  procedure Print_Str( str : string );

  procedure Print_XY ( x, y : int; str : string );

implementation

uses GDriver;

  type
    TFunction_8x8 = procedure( x, y : Int; c : char );

    TByte = array[0..0] of Byte;
    PByte = ^TByte;

  var
    Current_Mode : Byte;
    Print_8x8    : TFunction_8x8;

  const
    Font_8x8 : array[0..2047] of Byte
             = (
                 $00, $00, $00, $00, $00, $00, $00, $00,
                 $7E, $81, $A5, $81, $BD, $99, $81, $7E,
                 $7E, $FF, $DB, $FF, $C3, $E7, $FF, $7E,
                 $6C, $FE, $FE, $FE, $7C, $38, $10, $00,
                 $10, $38, $7C, $FE, $7C, $38, $10, $00,
                 $38, $7C, $38, $FE, $FE, $92, $10, $7C,
                 $00, $10, $38, $7C, $FE, $7C, $38, $7C,
                 $00, $00, $18, $3C, $3C, $18, $00, $00,
                 $FF, $FF, $E7, $C3, $C3, $E7, $FF, $FF,
                 $00, $3C, $66, $42, $42, $66, $3C, $00,
                 $FF, $C3, $99, $BD, $BD, $99, $C3, $FF,
                 $0F, $07, $0F, $7D, $CC, $CC, $CC, $78,
                 $3C, $66, $66, $66, $3C, $18, $7E, $18,
                 $3F, $33, $3F, $30, $30, $70, $F0, $E0,
                 $7F, $63, $7F, $63, $63, $67, $E6, $C0,
                 $99, $5A, $3C, $E7, $E7, $3C, $5A, $99,
                 $80, $E0, $F8, $FE, $F8, $E0, $80, $00,
                 $02, $0E, $3E, $FE, $3E, $0E, $02, $00,
                 $18, $3C, $7E, $18, $18, $7E, $3C, $18,
                 $66, $66, $66, $66, $66, $00, $66, $00,
                 $7F, $DB, $DB, $7B, $1B, $1B, $1B, $00,
                 $3E, $63, $38, $6C, $6C, $38, $86, $FC,
                 $00, $00, $00, $00, $7E, $7E, $7E, $00,
                 $18, $3C, $7E, $18, $7E, $3C, $18, $FF,
                 $18, $3C, $7E, $18, $18, $18, $18, $00,
                 $18, $18, $18, $18, $7E, $3C, $18, $00,
                 $00, $18, $0C, $FE, $0C, $18, $00, $00,
                 $00, $30, $60, $FE, $60, $30, $00, $00,
                 $00, $00, $C0, $C0, $C0, $FE, $00, $00,
                 $00, $24, $66, $FF, $66, $24, $00, $00,
                 $00, $18, $3C, $7E, $FF, $FF, $00, $00,
                 $00, $FF, $FF, $7E, $3C, $18, $00, $00,
                 $00, $00, $00, $00, $00, $00, $00, $00,
                 $18, $3C, $3C, $18, $18, $00, $18, $00,
                 $6C, $6C, $6C, $00, $00, $00, $00, $00,
                 $6C, $6C, $FE, $6C, $FE, $6C, $6C, $00,
                 $18, $7E, $C0, $7C, $06, $FC, $18, $00,
                 $00, $C6, $CC, $18, $30, $66, $C6, $00,
                 $38, $6C, $38, $76, $DC, $CC, $76, $00,
                 $30, $30, $60, $00, $00, $00, $00, $00,
                 $18, $30, $60, $60, $60, $30, $18, $00,
                 $60, $30, $18, $18, $18, $30, $60, $00,
                 $00, $66, $3C, $FF, $3C, $66, $00, $00,
                 $00, $18, $18, $7E, $18, $18, $00, $00,
                 $00, $00, $00, $00, $00, $18, $18, $30,
                 $00, $00, $00, $7E, $00, $00, $00, $00,
                 $00, $00, $00, $00, $00, $18, $18, $00,
                 $06, $0C, $18, $30, $60, $C0, $80, $00,
                 $7C, $CE, $DE, $F6, $E6, $C6, $7C, $00,
                 $30, $70, $30, $30, $30, $30, $FC, $00,
                 $78, $CC, $0C, $38, $60, $CC, $FC, $00,
                 $78, $CC, $0C, $38, $0C, $CC, $78, $00,
                 $1C, $3C, $6C, $CC, $FE, $0C, $1E, $00,
                 $FC, $C0, $F8, $0C, $0C, $CC, $78, $00,
                 $38, $60, $C0, $F8, $CC, $CC, $78, $00,
                 $FC, $CC, $0C, $18, $30, $30, $30, $00,
                 $78, $CC, $CC, $78, $CC, $CC, $78, $00,
                 $78, $CC, $CC, $7C, $0C, $18, $70, $00,
                 $00, $18, $18, $00, $00, $18, $18, $00,
                 $00, $18, $18, $00, $00, $18, $18, $30,
                 $18, $30, $60, $C0, $60, $30, $18, $00,
                 $00, $00, $7E, $00, $7E, $00, $00, $00,
                 $60, $30, $18, $0C, $18, $30, $60, $00,
                 $3C, $66, $0C, $18, $18, $00, $18, $00,
                 $7C, $C6, $DE, $DE, $DC, $C0, $7C, $00,
                 $30, $78, $CC, $CC, $FC, $CC, $CC, $00,
                 $FC, $66, $66, $7C, $66, $66, $FC, $00,
                 $3C, $66, $C0, $C0, $C0, $66, $3C, $00,
                 $F8, $6C, $66, $66, $66, $6C, $F8, $00,
                 $FE, $62, $68, $78, $68, $62, $FE, $00,
                 $FE, $62, $68, $78, $68, $60, $F0, $00,
                 $3C, $66, $C0, $C0, $CE, $66, $3A, $00,
                 $CC, $CC, $CC, $FC, $CC, $CC, $CC, $00,
                 $78, $30, $30, $30, $30, $30, $78, $00,
                 $1E, $0C, $0C, $0C, $CC, $CC, $78, $00,
                 $E6, $66, $6C, $78, $6C, $66, $E6, $00,
                 $F0, $60, $60, $60, $62, $66, $FE, $00,
                 $C6, $EE, $FE, $FE, $D6, $C6, $C6, $00,
                 $C6, $E6, $F6, $DE, $CE, $C6, $C6, $00,
                 $38, $6C, $C6, $C6, $C6, $6C, $38, $00,
                 $FC, $66, $66, $7C, $60, $60, $F0, $00,
                 $7C, $C6, $C6, $C6, $D6, $7C, $0E, $00,
                 $FC, $66, $66, $7C, $6C, $66, $E6, $00,
                 $7C, $C6, $E0, $78, $0E, $C6, $7C, $00,
                 $FC, $B4, $30, $30, $30, $30, $78, $00,
                 $CC, $CC, $CC, $CC, $CC, $CC, $FC, $00,
                 $CC, $CC, $CC, $CC, $CC, $78, $30, $00,
                 $C6, $C6, $C6, $C6, $D6, $FE, $6C, $00,
                 $C6, $C6, $6C, $38, $6C, $C6, $C6, $00,
                 $CC, $CC, $CC, $78, $30, $30, $78, $00,
                 $FE, $C6, $8C, $18, $32, $66, $FE, $00,
                 $78, $60, $60, $60, $60, $60, $78, $00,
                 $C0, $60, $30, $18, $0C, $06, $02, $00,
                 $78, $18, $18, $18, $18, $18, $78, $00,
                 $10, $38, $6C, $C6, $00, $00, $00, $00,
                 $00, $00, $00, $00, $00, $00, $00, $FF,
                 $30, $30, $18, $00, $00, $00, $00, $00,
                 $00, $00, $78, $0C, $7C, $CC, $76, $00,
                 $E0, $60, $60, $7C, $66, $66, $DC, $00,
                 $00, $00, $78, $CC, $C0, $CC, $78, $00,
                 $1C, $0C, $0C, $7C, $CC, $CC, $76, $00,
                 $00, $00, $78, $CC, $FC, $C0, $78, $00,
                 $38, $6C, $64, $F0, $60, $60, $F0, $00,
                 $00, $00, $76, $CC, $CC, $7C, $0C, $F8,
                 $E0, $60, $6C, $76, $66, $66, $E6, $00,
                 $30, $00, $70, $30, $30, $30, $78, $00,
                 $0C, $00, $1C, $0C, $0C, $CC, $CC, $78,
                 $E0, $60, $66, $6C, $78, $6C, $E6, $00,
                 $70, $30, $30, $30, $30, $30, $78, $00,
                 $00, $00, $CC, $FE, $FE, $D6, $D6, $00,
                 $00, $00, $B8, $CC, $CC, $CC, $CC, $00,
                 $00, $00, $78, $CC, $CC, $CC, $78, $00,
                 $00, $00, $DC, $66, $66, $7C, $60, $F0,
                 $00, $00, $76, $CC, $CC, $7C, $0C, $1E,
                 $00, $00, $DC, $76, $62, $60, $F0, $00,
                 $00, $00, $7C, $C0, $70, $1C, $F8, $00,
                 $10, $30, $FC, $30, $30, $34, $18, $00,
                 $00, $00, $CC, $CC, $CC, $CC, $76, $00,
                 $00, $00, $CC, $CC, $CC, $78, $30, $00,
                 $00, $00, $C6, $C6, $D6, $FE, $6C, $00,
                 $00, $00, $C6, $6C, $38, $6C, $C6, $00,
                 $00, $00, $CC, $CC, $CC, $7C, $0C, $F8,
                 $00, $00, $FC, $98, $30, $64, $FC, $00,
                 $1C, $30, $30, $E0, $30, $30, $1C, $00,
                 $18, $18, $18, $00, $18, $18, $18, $00,
                 $E0, $30, $30, $1C, $30, $30, $E0, $00,
                 $76, $DC, $00, $00, $00, $00, $00, $00,
                 $00, $10, $38, $6C, $C6, $C6, $FE, $00,
                 $7C, $C6, $C0, $C6, $7C, $0C, $06, $7C,
                 $00, $CC, $00, $CC, $CC, $CC, $76, $00,
                 $1C, $00, $78, $CC, $FC, $C0, $78, $00,
                 $7E, $81, $3C, $06, $3E, $66, $3B, $00,
                 $CC, $00, $78, $0C, $7C, $CC, $76, $00,
                 $E0, $00, $78, $0C, $7C, $CC, $76, $00,
                 $30, $30, $78, $0C, $7C, $CC, $76, $00,
                 $00, $00, $7C, $C6, $C0, $78, $0C, $38,
                 $7E, $81, $3C, $66, $7E, $60, $3C, $00,
                 $CC, $00, $78, $CC, $FC, $C0, $78, $00,
                 $E0, $00, $78, $CC, $FC, $C0, $78, $00,
                 $CC, $00, $70, $30, $30, $30, $78, $00,
                 $7C, $82, $38, $18, $18, $18, $3C, $00,
                 $E0, $00, $70, $30, $30, $30, $78, $00,
                 $C6, $10, $7C, $C6, $FE, $C6, $C6, $00,
                 $30, $30, $00, $78, $CC, $FC, $CC, $00,
                 $1C, $00, $FC, $60, $78, $60, $FC, $00,
                 $00, $00, $7F, $0C, $7F, $CC, $7F, $00,
                 $3E, $6C, $CC, $FE, $CC, $CC, $CE, $00,
                 $78, $84, $00, $78, $CC, $CC, $78, $00,
                 $00, $CC, $00, $78, $CC, $CC, $78, $00,
                 $00, $E0, $00, $78, $CC, $CC, $78, $00,
                 $78, $84, $00, $CC, $CC, $CC, $76, $00,
                 $00, $E0, $00, $CC, $CC, $CC, $76, $00,
                 $00, $CC, $00, $CC, $CC, $7C, $0C, $F8,
                 $C3, $18, $3C, $66, $66, $3C, $18, $00,
                 $CC, $00, $CC, $CC, $CC, $CC, $78, $00,
                 $18, $18, $7E, $C0, $C0, $7E, $18, $18,
                 $38, $6C, $64, $F0, $60, $E6, $FC, $00,
                 $CC, $CC, $78, $30, $FC, $30, $FC, $30,
                 $F8, $CC, $CC, $FA, $C6, $CF, $C6, $C3,
                 $0E, $1B, $18, $3C, $18, $18, $D8, $70,
                 $1C, $00, $78, $0C, $7C, $CC, $76, $00,
                 $38, $00, $70, $30, $30, $30, $78, $00,
                 $00, $1C, $00, $78, $CC, $CC, $78, $00,
                 $00, $1C, $00, $CC, $CC, $CC, $76, $00,
                 $00, $F8, $00, $B8, $CC, $CC, $CC, $00,
                 $FC, $00, $CC, $EC, $FC, $DC, $CC, $00,
                 $3C, $6C, $6C, $3E, $00, $7E, $00, $00,
                 $38, $6C, $6C, $38, $00, $7C, $00, $00,
                 $18, $00, $18, $18, $30, $66, $3C, $00,
                 $00, $00, $00, $FC, $C0, $C0, $00, $00,
                 $00, $00, $00, $FC, $0C, $0C, $00, $00,
                 $C6, $CC, $D8, $36, $6B, $C2, $84, $0F,
                 $C3, $C6, $CC, $DB, $37, $6D, $CF, $03,
                 $18, $00, $18, $18, $3C, $3C, $18, $00,
                 $00, $33, $66, $CC, $66, $33, $00, $00,
                 $00, $CC, $66, $33, $66, $CC, $00, $00,
                 $22, $88, $22, $88, $22, $88, $22, $88,
                 $55, $AA, $55, $AA, $55, $AA, $55, $AA,
                 $DB, $F6, $DB, $6F, $DB, $7E, $D7, $ED,
                 $18, $18, $18, $18, $18, $18, $18, $18,
                 $18, $18, $18, $18, $F8, $18, $18, $18,
                 $18, $18, $F8, $18, $F8, $18, $18, $18,
                 $36, $36, $36, $36, $F6, $36, $36, $36,
                 $00, $00, $00, $00, $FE, $36, $36, $36,
                 $00, $00, $F8, $18, $F8, $18, $18, $18,
                 $36, $36, $F6, $06, $F6, $36, $36, $36,
                 $36, $36, $36, $36, $36, $36, $36, $36,
                 $00, $00, $FE, $06, $F6, $36, $36, $36,
                 $36, $36, $F6, $06, $FE, $00, $00, $00,
                 $36, $36, $36, $36, $FE, $00, $00, $00,
                 $18, $18, $F8, $18, $F8, $00, $00, $00,
                 $00, $00, $00, $00, $F8, $18, $18, $18,
                 $18, $18, $18, $18, $1F, $00, $00, $00,
                 $18, $18, $18, $18, $FF, $00, $00, $00,
                 $00, $00, $00, $00, $FF, $18, $18, $18,
                 $18, $18, $18, $18, $1F, $18, $18, $18,
                 $00, $00, $00, $00, $FF, $00, $00, $00,
                 $18, $18, $18, $18, $FF, $18, $18, $18,
                 $18, $18, $1F, $18, $1F, $18, $18, $18,
                 $36, $36, $36, $36, $37, $36, $36, $36,
                 $36, $36, $37, $30, $3F, $00, $00, $00,
                 $00, $00, $3F, $30, $37, $36, $36, $36,
                 $36, $36, $F7, $00, $FF, $00, $00, $00,
                 $00, $00, $FF, $00, $F7, $36, $36, $36,
                 $36, $36, $37, $30, $37, $36, $36, $36,
                 $00, $00, $FF, $00, $FF, $00, $00, $00,
                 $36, $36, $F7, $00, $F7, $36, $36, $36,
                 $18, $18, $FF, $00, $FF, $00, $00, $00,
                 $36, $36, $36, $36, $FF, $00, $00, $00,
                 $00, $00, $FF, $00, $FF, $18, $18, $18,
                 $00, $00, $00, $00, $FF, $36, $36, $36,
                 $36, $36, $36, $36, $3F, $00, $00, $00,
                 $18, $18, $1F, $18, $1F, $00, $00, $00,
                 $00, $00, $1F, $18, $1F, $18, $18, $18,
                 $00, $00, $00, $00, $3F, $36, $36, $36,
                 $36, $36, $36, $36, $FF, $36, $36, $36,
                 $18, $18, $FF, $18, $FF, $18, $18, $18,
                 $18, $18, $18, $18, $F8, $00, $00, $00,
                 $00, $00, $00, $00, $1F, $18, $18, $18,
                 $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF,
                 $00, $00, $00, $00, $FF, $FF, $FF, $FF,
                 $F0, $F0, $F0, $F0, $F0, $F0, $F0, $F0,
                 $0F, $0F, $0F, $0F, $0F, $0F, $0F, $0F,
                 $FF, $FF, $FF, $FF, $00, $00, $00, $00,
                 $00, $00, $76, $DC, $C8, $DC, $76, $00,
                 $00, $78, $CC, $F8, $CC, $F8, $C0, $C0,
                 $00, $FC, $CC, $C0, $C0, $C0, $C0, $00,
                 $00, $00, $FE, $6C, $6C, $6C, $6C, $00,
                 $FC, $CC, $60, $30, $60, $CC, $FC, $00,
                 $00, $00, $7E, $D8, $D8, $D8, $70, $00,
                 $00, $66, $66, $66, $66, $7C, $60, $C0,
                 $00, $76, $DC, $18, $18, $18, $18, $00,
                 $FC, $30, $78, $CC, $CC, $78, $30, $FC,
                 $38, $6C, $C6, $FE, $C6, $6C, $38, $00,
                 $38, $6C, $C6, $C6, $6C, $6C, $EE, $00,
                 $1C, $30, $18, $7C, $CC, $CC, $78, $00,
                 $00, $00, $7E, $DB, $DB, $7E, $00, $00,
                 $06, $0C, $7E, $DB, $DB, $7E, $60, $C0,
                 $38, $60, $C0, $F8, $C0, $60, $38, $00,
                 $78, $CC, $CC, $CC, $CC, $CC, $CC, $00,
                 $00, $7E, $00, $7E, $00, $7E, $00, $00,
                 $18, $18, $7E, $18, $18, $00, $7E, $00,
                 $60, $30, $18, $30, $60, $00, $FC, $00,
                 $18, $30, $60, $30, $18, $00, $FC, $00,
                 $0E, $1B, $1B, $18, $18, $18, $18, $18,
                 $18, $18, $18, $18, $18, $D8, $D8, $70,
                 $18, $18, $00, $7E, $00, $18, $18, $00,
                 $00, $76, $DC, $00, $76, $DC, $00, $00,
                 $38, $6C, $6C, $38, $00, $00, $00, $00,
                 $00, $00, $00, $18, $18, $00, $00, $00,
                 $00, $00, $00, $00, $18, $00, $00, $00,
                 $0F, $0C, $0C, $0C, $EC, $6C, $3C, $1C,
                 $58, $6C, $6C, $6C, $6C, $00, $00, $00,
                 $70, $98, $30, $60, $F8, $00, $00, $00,
                 $00, $00, $3C, $3C, $3C, $3C, $00, $00,
                 $00, $00, $00, $00, $00, $00, $00, $00
               );

{$F+}
  procedure Print_8x8_Mono( x, y : Int; c : char );
  var
    offset, i : Int;
    bitm      : PByte;
  begin
    offset := x + y*Vio_ScanLineWidth*8;
    bitm   := @Font_8x8[ ord(c)*8 ];

    for i := 0 to 7 do
    begin
      Vio^[offset] := bitm^[i];
      inc( offset, Vio_ScanLineWidth );
    end;
  end;

  procedure Print_8x8_Gray( x, y : Int; c : char );
  var
    offset, i, bit : Int;

    bitm : PByte;
  begin
    offset := ( x + y*Vio_ScanLineWidth )*8;
    bitm   := @font_8x8[ ord(c)*8 ];

    for i := 0 to 7 do
    begin
      bit := $80;
      while bit > 0 do
      begin
        if ( bit and bitm^[i] <> 0 ) then Vio^[offset] := $FF
                                     else Vio^[offset] := $00;
        bit := bit shr 1;
        inc( offset );
      end;

      inc( offset, Vio_ScanLineWidth-8 );
    end;

  end;
{$F-}

  function Set_Graph_Screen( mode : Int ): boolean;
  begin
    Set_Graph_Screen := False;

    gcursor_x := 0;
    gcursor_y := 0;

    case mode of

      Graphics_Mode_Mono : begin
                             if not Driver_Set_Graphics(mode) then exit;
                             gwindow_width  := vio_ScanLineWidth;
                             gwindow_height := vio_Height div 8;

                             Print_8x8 := Print_8x8_Mono;
                           end;

      Graphics_Mode_Gray : begin
                             if not Driver_Set_Graphics(mode) then exit;
                             gwindow_width  := vio_ScanLineWidth div 8;
                             gwindow_height := vio_Height div 8;

                             Print_8x8 := Print_8x8_Gray;
                           end;
    else
      exit;
    end;

    Set_Graph_Screen := True;
  end;


  function Restore_Screen : boolean;
  begin
    gcursor_x := 0;
    gcursor_y := 0;

    gwindow_height := 0;
    gwindow_width  := 0;

    Restore_Screen := Driver_Restore_Mode;
  end;

  procedure Display_Bitmap_On_Screen;
  begin
    Driver_Display_Bitmap( buff, line, col );
  end;

  procedure Goto_XY( x, y : Int );
  begin
    gcursor_x := x;
    gcursor_y := y;
  end;

  procedure Print_Str( str : string );
  var
    i : Int;
  begin
    for i := 1 to length(str) do
    begin
      case str[i] of

        #13 : begin
                gcursor_x := 0;
                inc( gcursor_y );
                if gcursor_y > gwindow_height then gcursor_y := 0;
              end;
      else
        Print_8x8( gcursor_x, gcursor_y, str[i] );
        inc( gcursor_x );
        if gcursor_x >= gwindow_width then
        begin
          gcursor_x := 0;
          inc( gcursor_y );
          if gcursor_y >= gwindow_height then gcursor_y := 0;
        end
      end
    end
  end;

  procedure Print_XY( x, y : Int; str : string );
  begin
    Goto_XY( x, y );
    Print_Str( str );
  end;

end.

