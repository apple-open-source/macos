{***************************************************************************}
{*                                                                         *}
{*  FreeType Performance Timer                                             *}
{*                                                                         *}
{*                                                                         *}
{*  This source code has been compiled and run under both Virtual Pascal   *}
{*  on OS/2 and Borland's BP7.                                             *}
{*                                                                         *}
{*                                                                         *}
{*  The C scan-line converter has been highly optimized, unlike the        *}
{*  Pascal one which is still 'aged'. Don't be surprised to see drastic    *}
{*  performance differences then..                                         *}
{*                                                                         *}
{***************************************************************************}

program Timer;

uses
{$IFDEF OS2}
     Use32,
{$ENDIF}
     Crt,
     Dos,     (* for GetTime *)
     GMain,
     GEvents,
     GDriver,
     FreeType,

     TTError,  (* for CheckError      *)
     TTTypes;  (* for commodity types *)

{$DEFINE VISUAL}

{ $DEFINE DEBUG}

{$IFDEF VISUAL}
{&PMTYPE NOVIO}
{$ENDIF}

const
  Precis  = 64;
  Precis2 = Precis div 2;

  PrecisAux = 1024;

  Centre_X : int = 320;
  Centre_Y : int = 225;

  Max_Glyphs = 512;

var
  xC : TT_PCoordinates;
  yC : TT_PCoordinates;
  Fl : TT_PTouchTable;

  cons : PUShort;

  outlines  : array[0..Max_Glyphs-1] of TT_Outline;

  lastp : int;
  lastc : int;

  res : int;

  numPoints, numContours : int;

  Bit : TT_Raster_Map;

  Rotation : int;  (* Angle modulo 1024 *)

  num_glyphs : int;

  gray_level : Boolean;

  face     : TT_Face;
  instance : TT_Instance;
  glyph    : TT_Glyph;

  metrics  : TT_Glyph_Metrics;
  imetrics : TT_Instance_Metrics;

  props    : TT_Face_Properties;

  old_glyph : int;
  cur_glyph : int;
  tot_glyph : int;

  grayLines : array[0..2048] of Byte;

  error : TT_Error;


Procedure InitRows;
var
  i: integer;
  P: Pointer;
begin

  if gray_level then
  begin
    Bit.rows  := 200;
    Bit.cols  := 320;
    Bit.width := 320*2;
    Bit.flow  := TT_Flow_Down;
    Bit.size  := 320*200;
  end
  else
  begin
    Bit.rows  := 450;
    Bit.cols  := 80;
    Bit.width := 640;
    Bit.flow  := TT_Flow_Down;
    Bit.size  := 80*450;
  end;

  GetMem( Bit.buffer, Bit.size );
  if Bit.buffer = NIL then
   begin
    Writeln('ERREUR:InitRows:Not enough memory to allocate BitMap');
    halt(1);
   end;

  FillChar( Bit.Buffer^, Bit.Size, 0 );
end;


Procedure ClearData;
var i: integer;
begin
  FillChar( Bit.Buffer^, Bit.Size, 0 );
end;


procedure Preload_Glyphs( var start : Int );
var
  i, j, fin, np, nc : integer;
  outline           : TT_Outline;

begin
  fin := start + Max_Glyphs;
  if fin > num_glyphs then fin := num_glyphs;

  tot_glyph := fin-start;

  cur_glyph := 0;
  lastp     := 0;
  lastc     := 0;

  {$IFNDEF VISUAL}
  Write('Loading ', fin-start,' glyphs ');
  {$ENDIF}

  for i := start to fin-1 do
  begin

    if TT_Load_Glyph( instance,
                      glyph,
                      i,
                      TT_Load_Default ) = TT_Err_Ok then
    begin
      TT_Get_Glyph_Outline( glyph, outline );

      TT_New_Outline( outline.n_points,
                      outline.n_contours,
                      outlines[cur_glyph] );

      outline.high_precision := false;
      outline.second_pass    := false;

      TT_Copy_Outline( outline, outlines[cur_glyph] );


      TT_Translate_Outline( outlines[cur_glyph],
                            vio_Width*16,
                            vio_Height*16 );
      inc( cur_glyph );
    end;

  end;

  start := fin;
end;



function ConvertRaster(index : integer) : boolean;
begin
  if gray_level then
    error := TT_Get_Outline_Pixmap( outlines[index], Bit )
  else
    error := TT_Get_Outline_Bitmap( outlines[index], Bit );

  ConvertRaster := (error <> TT_Err_Ok);
end;


procedure Usage;
begin
    Writeln('Simple TrueType Glyphs viewer - part of the FreeType project' );
    Writeln;
    Writeln('Usage : ',paramStr(0),' FontName[.TTF]');
    Halt(1);
end;


function Get_Time : LongInt;
var
  heure,
  min,
  sec,
  cent :
{$IFDEF OS2}
  longint;
{$ELSE}
  word;
{$ENDIF}
begin
  GetTime( heure, min, sec, cent );
  Get_Time := 6000*longint(min) + 100*longint(sec) + cent;
end;



var i         : integer;
    Filename  : String;
    Fail      : Int;
    T, T0, T1 : Long;

    start : Int;

begin
  xC := NIL;
  yC := NIL;
  Fl := NIL;

  TT_Init_FreeType;

  if ParamCount = 0 then Usage;

  gray_level := ParamStr(1)='-g';

  if gray_level then
    if ParamCount <> 2 then Usage else
  else
    if ParamCount <> 1 then Usage;

  if gray_level then Filename := ParamStr(2)
                else Filename := ParamStr(1);

  if Pos('.',FileName) = 0 then FileName:=FileName+'.TTF';

  error := TT_Open_Face( filename, face );

  if error <> TT_Err_Ok then
    begin
      Writeln('ERROR: Could not open ', FileName );
      Check_Error(error);
    end;

  TT_Get_Face_Properties( face, props );

  num_glyphs := props.num_Glyphs;

  i := length(FileName);
  while (i > 1) and (FileName[i] <> '\') do dec(i);

  FileName := Copy( FileName, i+1, length(FileName) );

  error := TT_New_Glyph( face, glyph );
  if error <> TT_Err_Ok then
    begin
      Writeln('ERROR : Could not get glyph' );
      Check_Error(error);
    end;

  i := props.max_Points * num_glyphs;

  GetMem( fl, i );
  i := i * sizeof(Long);

  GetMem( xC, i );
  GetMem( yC, i );

  i := props.max_Contours * num_glyphs;

  GetMem( cons, i*sizeof(UShort) );

  error := TT_New_Instance( face, instance );
  if error <> TT_Err_Ok then
    begin
      Writeln('ERROR: Could not open face instance from ', Filename );
      Check_Error(error);
    end;

  error := TT_Set_Instance_PointSize( instance, 400 );
  if error <> TT_Err_Ok then
    begin
      Writeln('ERROR: Could set pointsize' );
      Check_Error(error);
    end;

  Rotation := 0;
  Fail     := 0;

  InitRows;

  {$IFDEF VISUAL}
  if gray_level then
    begin
      if not Set_Graph_Screen( Graphics_Mode_Gray ) then
        Panic1( 'could not set grayscale graphics mode' );
    end
  else
    begin
      if not Set_Graph_Screen( Graphics_Mode_Mono ) then
        Panic1( 'could not set mono graphics mode' );
    end;

  {$ENDIF}

  start := 0;

  T  := Get_Time;
  T1 := 0;

  while start < num_glyphs do
  begin

    Preload_Glyphs(start);

    {$IFNDEF VISUAL}
    write('... ');
    {$ENDIF}

    T0 := Get_Time;

    for cur_glyph := 0 to tot_glyph-1 do
    begin
      if not ConvertRaster(cur_glyph) then
      {$IFDEF VISUAL}
      begin
        Display_Bitmap_On_Screen( Bit.Buffer^, Bit.rows, Bit.cols  );
        ClearData;
      end
      {$ELSE}
      begin
      end
      {$ENDIF}
      else
        inc( Fail );
    end;

    T0 := Get_Time - T0;
    writeln( T0/100:0:2,' s' );

    inc( T1, T0 );

    for cur_glyph := 0 to tot_glyph-1 do
      TT_Done_Outline( outlines[cur_glyph] );
  end;

  T := Get_Time - T;

  {$IFDEF VISUAL}
  Restore_Screen;
  {$ENDIF}

  writeln;
  writeln('Render time   : ', T1/100:0:2,' s' );
  writeln('Total time    : ', T /100:0:2,' s');
  writeln('Glyphs/second : ', Long(num_glyphs)*100/T1:0:1 );
  writeln('Fails         : ',Fail );
end.

begin
end.

