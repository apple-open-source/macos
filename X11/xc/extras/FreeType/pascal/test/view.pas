{***************************************************************************}
{*                                                                         *}
{*  FreeType Glyph Viewer.                                                 *}
{*                                                                         *}
{*                                                                         *}
{*    This small program will load a TrueType font file and allow          *}
{*    you to view/scale/rotate its glyphs. Glyphs are in the order         *}
{*    found within the 'glyf' table.                                       *}
{*                                                                         *}
{*  NOTE : This version displays a magnified view of the glyph             *}
{*         along with the pixel grid.                                      *}
{*                                                                         *}
{*  This source code has been compiled and run under both Virtual Pascal   *}
{*  on OS/2 and Borland's BP7.                                             *}
{*                                                                         *}
{***************************************************************************}

program View;

uses Crt,
     Common,
{$IFDEF OS2}
     Use32,
{$ENDIF}
     GMain,
     GEvents,
     GDriver,
     FreeType;

{&PMTYPE NOVIO}

{$DEFINE DEBUG}

const
  Precis  = 64;

  Precis2 = Precis div 2;

  PrecisAux = 1024;

  Profile_Buff_Size = 32000;

  Max_Files = 1024;

var
  face     : TT_Face;
  instance : TT_Instance;
  glyph    : TT_Glyph;

  metrics  : TT_Glyph_Metrics;
  imetrics : TT_Instance_Metrics;

  props    : TT_Face_Properties;

  ymin, ymax, xmax, xmin, xsize : longint;
  res, old_res                  : int;

  numPoints, numContours : int;

  Bit : TT_Raster_Map;

  Rotation : int;  (* Angle modulo 1024 *)

  num_glyphs : int;

  error      : TT_Error;
  gray_level : Boolean;

  display_outline : boolean;
  hint_glyph      : boolean;
  scan_type       : Byte;

  old_glyph : int;
  cur_glyph : int;

  scale_shift : Int;

  grayLines : array[0..2048] of Byte;

(*******************************************************************
 *
 *  Function    : Set_Raster_Area
 *
 *****************************************************************)

 procedure Set_Raster_Area;
 begin
   Bit.rows  := vio_Height;
   Bit.width := vio_Width;
   Bit.flow  := TT_Flow_Up;

   if gray_level then
     Bit.cols := Bit.width
   else
     Bit.cols := (Bit.width+7) div 8;

   Bit.size := Bit.rows * Bit.cols;
 end;

(*******************************************************************
 *
 *  Function    : Clear_Data
 *
 *****************************************************************)

 procedure Clear_Data;
 begin
   if gray_level then
     fillchar( Bit.buffer^, Bit.size, gray_palette[0] )
   else
     fillchar( Bit.buffer^, Bit.size, 0 );
 end;

(*******************************************************************
 *
 *  Function    : Init_Engine
 *
 *****************************************************************)

 procedure Init_Engine( maxRes   : Int );
 begin
   Set_Raster_Area;
   GetMem( Bit.buffer, Bit.size );
   Clear_Data;
 end;

(*******************************************************************
 *
 *  Function    : Reset_Scale
 *
 *****************************************************************)

 function Reset_Scale( res : Int ) : Boolean;
 begin
   error := TT_Set_Instance_CharSize( instance, res*64 );
   Reset_Scale := (error = TT_Err_Ok);
 end;


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
  j, load_flag : int;

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


var
  Error_String : String;
  ine : Int;

function Render_ABC( glyph_index : integer ) : boolean;
var
  i, j : integer;

  x, y : longint;

  start_x,
  start_y,
  step_x,
  step_y  : longint;

  fail : Int;
begin

  Render_ABC := True;

  TT_Get_Instance_Metrics( instance, imetrics );

  start_x := 4;
  start_y := vio_Height - 30 - imetrics.y_ppem;

  step_x  := imetrics.x_ppem + 4;
  step_y  := imetrics.y_ppem + 10;

  x := start_x;
  y := start_y;

  fail := 0;

  ine := glyph_index;
  while ine < num_glyphs do
  begin

    if LoadTrueTypeChar( ine, hint_glyph ) = TT_Err_Ok then
    begin

      TT_Get_Glyph_Metrics( glyph, metrics );

      if gray_level then
        TT_Get_Glyph_Pixmap( glyph, Bit, x*64, y*64 )
      else
        TT_Get_Glyph_Bitmap( glyph, Bit, x*64, y*64 );

      inc( x, (metrics.advance div 64) + 1 );

      if x > vio_Width - 40 then
      begin
        x := start_x;
        dec( y, step_y );
        if y < 10 then
        begin
          Render_ABC := False;
          exit;
        end;
      end;
    end
    else
      inc( fail );

    inc(ine);
  end;

  Render_ABC := False;
end;



procedure Erreur( s : String );
begin
  Restore_Screen;
  Writeln( 'Error : ', s, ', error code = ', error );
  Halt(1);
end;


procedure Usage;
begin
    Writeln('Simple TrueType Glyphs viewer - part of the FreeType project' );
    Writeln;
    Writeln('Usage : ',paramStr(0),' FontName[.TTF]');
    Halt(1);
end;



var
 i: integer;
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
  Fail     : Int;
  glyphStr : String[4];
  ev       : Event;

  Code     : Int;

  init_memory, end_memory : LongInt;

  num_args   : Integer;
  point_size : Integer;
  cur_file   : Integer;
  first_arg  : Int;
  sortie     : Boolean;
  valid      : Boolean;
  errmsg     : String;

label
  Lopo;

begin
  TextMode( co80+Font8x8 );

  TT_Init_FreeType;

  num_args  := ParamCount;

  if num_args = 0 then
    Usage;

  first_arg := 1;

  gray_level := False;

  if ParamStr(first_arg) = '-g' then
    begin
      inc( first_arg );
      gray_level := True;
    end;

  if first_arg > num_args+1 then
    Usage;

  val( ParamStr(first_arg), point_size, Code );
  if Code <> 0 then
    point_size := 24
  else
    inc( first_arg );

  Expand_Wildcards( first_arg, '.ttf' );

  cur_file := 0;

  if num_arguments = 0 then
  begin
    Writeln('Could not find file(s)');
    Halt(3);
  end;

  if gray_level then
    begin
      if not Set_Graph_Screen( Graphics_Mode_Gray ) then
        Erreur( 'could not set grayscale graphics mode' );
    end
  else
    begin
      if not Set_Graph_Screen( Graphics_Mode_Mono ) then
        Erreur( 'could not set mono graphics mode' );
    end;

  Init_Engine( 24 );

  repeat

      valid := True;

      FileName := arguments[cur_file]^;

      if Pos('.',FileName) = 0 then FileName:=FileName+'.TTF';

      error := TT_Open_Face( filename, face );
      if error <> TT_Err_Ok then
      begin
        str( error, errmsg );
        errmsg := 'Could not open '+filename+', error code = '+errmsg;
        valid  := false;
        goto Lopo;
      end;

      TT_Get_Face_Properties( face, props );

      num_glyphs := props.num_Glyphs;

      i := length(FileName);
      while (i > 1) and (FileName[i] <> '\') do dec(i);

      FileName := Copy( FileName, i+1, length(FileName) );

      error := TT_New_Glyph( face, glyph );
      if error <> TT_Err_Ok then
        Erreur('Could not create glyph container');

      error := TT_New_Instance( face, instance );
      if error <> TT_Err_Ok then
      begin
        str( error, errmsg );
        errmsg := 'Could not create instance, error code = '+errmsg;
        valid  := false;
        goto Lopo;
      end;

      TT_Set_Instance_Resolutions( instance, 96, 96 );

      Rotation  := 0;
      Fail      := 0;
      res       := point_size;
      scan_type := 2;

      Reset_Scale( res );

  Lopo:

      display_outline := true;
      hint_glyph      := true;

      old_glyph := -1;
      old_res   := res;
      cur_glyph := 0;

      sortie := false;

      Repeat

        if valid then
        begin
          if Render_ABC( cur_glyph ) then
            inc( Fail )
          else
            Display_Bitmap_On_Screen( Bit.Buffer^, Bit.rows, Bit.cols  );

          Clear_Data;

          Print_XY( 0, 0, FileName );

          TT_Get_Instance_Metrics( instance, imetrics );

          Print_Str('  pt size = ');
          Str( imetrics.pointSize div 64:3, glyphStr );
          Print_Str( glyphStr );

          Print_Str('  ppem = ');
          Str( imetrics.y_ppem:3, glyphStr );
          Print_Str( glyphStr );

          Print_Str('  glyph = ');
          Str( cur_glyph, glyphStr );
          Print_Str( glyphStr );

          Print_XY( 0, 1, 'Hinting  (''z'')  : ' );
          if hint_glyph then Print_Str('on ')
                        else Print_Str('off');

          Print_XY( 0, 2, 'scan type(''e'')  : ' );
          case scan_type of
            0 : Print_Str('none   ');
            1 : Print_Str('level 1');
            2 : Print_Str('level 2');
            4 : Print_Str('level 4');
            5 : Print_Str('level 5');
          end;
        end
        else
        begin
          Clear_Data;
          Display_Bitmap_On_Screen( Bit.buffer^, Bit.rows, Bit.cols );
          Print_XY( 0, 0, errmsg );
        end;

        Get_Event(ev);

        case ev.what of

          event_Quit : goto Fin;

          event_Keyboard : case char(ev.info) of

                            'n' : begin
                                    sortie := true;
                                    if cur_file+1 < num_arguments then
                                      inc( cur_file );
                                  end;

                            'p' : begin
                                    sortie := true;
                                    if cur_file > 0 then
                                      dec( cur_file );
                                  end;

                            'z' : hint_glyph := not hint_glyph;


                            'e' : begin
                                    inc( scan_type );
                                    if scan_type  = 3 then scan_type := 4;
                                    if scan_type >= 6 then scan_type := 0;
                                  end;
                           end;

          event_Scale_Glyph : begin
                                inc( res, ev.info );
                                if res < 1 then    res := 1;
                                if res > 1400 then res := 1400;
                              end;

          event_Change_Glyph : begin
                                 inc( cur_glyph, ev.info );
                                 if cur_glyph < 0 then cur_glyph := 0;
                                 if cur_glyph >= num_glyphs
                                   then cur_glyph := num_glyphs-1;
                               end;
        end;

        if res <> old_res then
          begin
            if not Reset_Scale(res) then
              Erreur( 'Could not resize font' );
            old_res := res;
          end;

      Until sortie;

      TT_Done_Glyph( glyph );
      TT_Close_Face( face );

  until false;

 Fin:
  Restore_Screen;

  Writeln;
  Writeln('Fails           : ', Fail );

  TT_Done_FreeType;
end.

