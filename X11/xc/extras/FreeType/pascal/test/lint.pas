{***************************************************************************}
{*                                                                         *}
{*  FreeType Font glyph programs checker                                   *}
{*                                                                         *}
{*                                                                         *}
{*    This small program will load a TrueType font file and try to         *}
{*    render it at a given point size.                                     *}
{*                                                                         *}
{*    This version will also catch differences between the engine's        *}
{*    computed advance widths, and the pre-calc values found in the        *}
{*    "hdmx" table                                                         *}
{*                                                                         *}
{*  This source code has been compiled and run under both Virtual Pascal   *}
{*  on OS/2 and Borland's BP7.                                             *}
{*                                                                         *}
{***************************************************************************}

program Abc;

uses Crt,
     Common,
{$IFDEF OS2}
     Use32,
{$ENDIF}
     FreeType,
     TTObjs;

const
  Precis  = 64;
  Precis2 = Precis div 2;

  PrecisAux = 1024;

  Screen_Width  = 640;
  Screen_Height = 480;
  Screen_Cols   = Screen_Width div 8;
  Screen_Size   = Screen_Cols * Screen_Height;

  Grid_Width  = Screen_Width div 8;
  Grid_Height = Screen_Height div 8;
  Grid_Cols   = Grid_Width div 8;
  Grid_Size   = Grid_Cols * Grid_Height;

  Screen_Center_X = Screen_Width div 2;
  Screen_Center_Y = Screen_Height div 2;

  Grid_Center_X = Grid_Width div 2;
  Grid_Center_Y = Grid_Height div 2;

  Profile_Buff_Size = 64000;

var

  res, old_res           : integer;

  numPoints, numContours : integer;

  Bitmap_small : TT_Raster_Map;
  Bitmap_big   : TT_Raster_Map;

  Rotation : integer;  (* Angle modulo 1024 *)

  num_glyphs : integer;

  face     : TT_Face;
  instance : TT_Instance;
  glyph    : TT_Glyph;

  metrics  : TT_Glyph_Metrics;
  imetrics : TT_Instance_Metrics;

  props    : TT_Face_Properties;

  point_size : integer;
  error      : TT_Error;

  display_outline : boolean;
  hint_glyph      : boolean;
  scan_type       : Byte;

  old_glyph : integer;

  FOut : Text;

(*******************************************************************
 *
 *  Function    :  LoadTrueTypeChar
 *
 *  Description :  Loads a single glyph into the xcoord, ycoord and
 *                 flag arrays, from the instance data.
 *
 *****************************************************************)

Function LoadTrueTypeChar( index : integer;
                           hint  : boolean ) : TT_Error;
var
  j, load_flag : integer;

  result : TT_Error;

begin
  if hint then load_flag := TT_Load_Scale_Glyph or TT_Load_Hint_Glyph
          else load_flag := TT_Load_Scale_Glyph;

  result := TT_Load_Glyph( instance,
                           glyph,
                           index,
                           load_flag );

  LoadTrueTypeChar := result;
end;


procedure Usage;
begin
    Writeln('Simple TrueType Glyphs viewer - part of the FreeType project' );
    Writeln;
    Writeln('Usage : ',paramStr(0),' size fontname[.ttf] [fontname.. ]');
    Writeln;
    Halt(1);
end;



var i: integer;
    heure,
    min1,
    min2,
    sec1,
    sec2,
    cent1,
    cent2  :
{$IFDEF OS2}
    longint;
{$ELSE}
    word;
{$ENDIF}

    C : Char;

    Filename : String;

label Fin;

var
  Fail     : Integer;
  PtSize   : Integer;
  Param    : Integer;
  code     : Integer;
  glyphStr : String[4];
  cur_file : Integer;
  valid    : Boolean;

  Mem0     : Longint;

label
  Lopo;

begin

  Mem0 := MemAvail;

  TT_Init_FreeType;

  if ParamCount < 2 then Usage;

  val( ParamStr(1), point_size, code );
  if code <> 0 then Usage;

  if ( point_size <= 0 ) then
  begin
    Writeln('Invalid argument : pointsize must be >= 1');
    Usage;
  end;

  Expand_WildCards( 2, '.ttf' );

  for cur_file := 0 to num_arguments-1 do
  begin

    FileName := arguments[cur_file]^;

    if Pos('.',FileName) = 0 then FileName:=FileName+'.TTF';

    Write( MemAvail:6, ' ' );

    error := TT_Open_Face( filename, face );

    i := length(FileName);
    while (i > 1) and (FileName[i] <> '\') do dec(i);
    FileName := Copy( FileName, i+1, length(FileName) );

    Write( cur_file:3,' ', filename:12, ': ' );

    if error <> TT_Err_Ok then
    begin
      Writeln( 'could not open file, error code = ', error );
      goto Lopo;
    end;

    TT_Get_Face_Properties( face, props );
    num_glyphs := props.num_Glyphs;

    error := TT_New_Glyph( face, glyph );
    if error <> TT_Err_Ok then
    begin
      Writeln( 'could not create glyph, error code = ',
               error );
      goto Lopo;
    end;

    error := TT_New_Instance( face, instance );
    if error <> TT_Err_Ok then
    begin
      Writeln( 'could not create instance, error code = ',
               error );
      goto Lopo;
    end;

    error := TT_Set_Instance_PointSize( instance, point_size );
    if error <> TT_Err_Ok then
    begin
      Writeln( 'could not set point size, error code = ', error );
      goto Lopo;
    end;

    Fail := 0;
    for i := 0 to num_glyphs-1 do
    begin
      error := LoadTrueTypeChar( i, true );
      if error <> TT_Err_Ok then
        begin
          inc( Fail );
          if Fail < 10 then
            Writeln( 'error hinting glyph ', i, ', code = ',
                     error );
        end;

{$IFDEF RIEN}
      with PGlyph(glyph.z)^ do
      begin
        if (precalc_width >= 0) and
           (precalc_width <> computed_width) then
          begin
            Write( i:5,' hdmx = ',precalc_width:3 );
            Write( ', engine = ',computed_width );
            if is_composite then Write( ' (composite)' );
            Writeln;
          end
      end;
{$ENDIF}

    end;

    if Fail >= 10 then
      Writeln( 'there were ',Fail,' failed glyphs' );

    if Fail = 0 then
      Writeln( 'ok' );

  Lopo:

    TT_Close_Face( face );

  end;

 Fin:

  Writeln('Memory consumed by lint = ', Mem0 - MemAvail );

  TT_Done_FreeType;

  Writeln('Memory leaked after engine termination = ', Mem0 - MemAvail );

end.


