program Debugger;

uses
{$IFDEF OS2}
     Use32,
{$ENDIF}

(* Turbo Vision units *)
     Drivers,
     Objects,
     Views,
     Menus,
     App,
     MsgBox,

     Crt,

(* FreeType units *)
     FreeType,
     TTInterp,
     TTTypes,
     TTMemory,
     TTError,
     TTTables,
     TTObjs,
     TTFile,
     TTCalc,
     TTDebug,
     TTRaster,

(* graphics system units *)
     GDriver,
     GMain,
     GEvents,

(* Debugger's Turbo Vision enhancements *)
     CodeTv,
     StackTv,
     StateTv,
     ZoneTv;

{$I DEBUGGER.INC}

(* define this variable if you want to debug the CVT rather than a *)
(* glyph's instruction set..                                       *)
{ $DEFINE DEBUG_CVT}

const
  Precis = 64;

  Screen_Width  = 640;
  Screen_Height = 480;
  Screen_Cols   = Screen_Width div 8;
  Screen_Size   = Screen_Cols * Screen_Height;

  Grid_Width  = Screen_Width div 16;
  Grid_Height = Screen_Height div 16;
  Grid_Cols   = Grid_Width div 8;
  Grid_Size   = Grid_Cols * Grid_Height;

  Screen_Center_X = Screen_Width div 2;
  Screen_Center_Y = Screen_Height div 2;

  Grid_Center_X = Grid_Width div 2;
  Grid_Center_Y = Grid_Height div 2;

  Profile_Buff_Size = 64000;


type
  TDebug_Mode = ( debug_code, view_glyph );

  TMyApp = object( TApplication )
             constructor Init;
             procedure NewWindow; virtual;
             procedure InitMenuBar; virtual;
             procedure HandleEvent( var Event : TEvent ); virtual;

             procedure Single_Step;
             procedure Execute_Loop;
             procedure New_Execution;
             procedure ReFocus;
           end;

  TEtat = ( etat_Termine, etat_Arret, etat_Execution );

  TVolatileBreakPoint = record
                          range   : Int;
                          address : Int;
                        end;

var
  CW : PCodeWindow;
  SW : PStackWindow;
  GW : PStateWindow;
  ZW : PZoneWindow;

  Code_Range : array[1..3] of PCodeRange;

  Gen_Range : array[1..3] of TRangeRec;

  old_Range : Int;

  stream : TT_Stream;

  the_face     : TT_Face;
  the_glyph    : TT_Glyph;
  the_instance : TT_Instance;

  face     : PFace;
  glyph    : PGlyph;
  glyph2   : PGlyph;
  instance : PInstance;
  exec     : PExec_Context;

  error    : TT_Error;

  Etat : TEtat;

  Volatiles : PBreakPoint;

  xCoord : TT_PCoordinates;
  yCoord : TT_PCoordinates;
  Flag   : TT_PTouchTable;

  Bitmap_small : TT_Raster_Map;
  Bitmap_big   : TT_Raster_Map;

  display_outline : boolean;
  hint_glyph      : boolean;

  debug_mode  : TDebug_Mode;
  MyApp       : TMyApp;

  Range       : Int;
  P           : PByteArray;
  FileName    : String;
  Font_Buffer : PStorage;
  Out_File    : Text;
  T, I        : int;

  glyph_number : Int;
  point_size   : Int;

procedure Initialize;
var
  i : int;
begin
  for i := 1 to 3 do Code_Range[i] := Get_CodeRange(exec,i);
  for i := 1 to 3 do Generate_Range( Code_Range[i], i, Gen_Range[i] );

  Volatiles := nil;

  display_outline := true;
  Debug_Mode      := debug_code;
end;

(*******************************************************************
 *
 *  Function    : InitRows
 *
 *  Description : Allocates the target bitmaps
 *
 *****************************************************************)

Procedure Init_Engine;
var
  P: Pointer;
begin

  (* The big bitmap will contain the grid, the glyph contours and *)
  (* the magnified bitmap                                         *)

  Bitmap_big.rows  := Screen_Height;
  Bitmap_big.cols  := Screen_Cols;
  Bitmap_big.width := Screen_Width;
  Bitmap_big.flow  := TT_Flow_Up;
  Bitmap_big.size  := Screen_Size;

  GetMem( Bitmap_big.buffer, Bitmap_big.size );
  if Bitmap_big.buffer = NIL then
   begin
    Writeln('ERREUR:InitRows:Not enough memory to allocate big BitMap');
    halt(1);
   end;

  (* The small bitmap contains the rendered glyph, and is then later *)
  (* magnified into the big bitmap                                   *)

  Bitmap_small.rows  := Grid_Height;
  Bitmap_small.cols  := Grid_Cols;
  Bitmap_small.width := Grid_Width;
  Bitmap_small.flow  := TT_Flow_Up;
  Bitmap_small.size  := Grid_Size;

  GetMem( Bitmap_small.buffer, Bitmap_small.size );
  if Bitmap_small.buffer = NIL then
   begin
    Writeln('ERREUR:InitRows:Not enough memory to allocate big BitMap');
    halt(1);
   end;

  FillChar( Bitmap_big.Buffer^, Bitmap_big.Size, 0 );
  FillChar( Bitmap_small.Buffer^, Bitmap_small.size, 0 );
end;

(*******************************************************************
 *
 *  Function    :  ClearData
 *
 *  Description :  Clears the bitmaps
 *
 *****************************************************************)

Procedure ClearData;
var i: integer;
begin
  FillChar( Bitmap_big.  Buffer^, Bitmap_big.  Size, 0 );
  FillChar( Bitmap_small.Buffer^, Bitmap_small.size, 0 );
end;


function Render_Magnified : boolean;
label
  Exit_1;
type
  TBlock = array[0..7] of Byte;
  PBlock = ^TBlock;
const
{
  Grid_Empty : TBlock
             = ( $10, $10, $10, $FF, $10, $10, $10, $10 );
}
  Grid_Pixel2 : TBlock
              = ( $FE, $FE, $FE, $FE, $FE, $FE, $FE, $00 );

  Pixel_Center_X = 3;
  Pixel_Center_Y = 3;

  Grid_Empty : TBlock
             = ( $00, $00, $00, $10, $00, $00, $00, $00 );

  Grid_Pixel1 : TBlock
              = ( $00, $00, $38, $38, $38, $00, $00, $00 );

  Big_Center_X = Grid_Center_X*16 + Pixel_Center_X;
  Big_Center_Y = Grid_Center_Y*16 + Pixel_Center_Y;

var
  r, w, w2, u, v, b, c : integer;

  x, y : Long;

  block : PBlock;
  G     : TT_Outline;

  pixel,
  empty : PBlock;

  numPoints : integer;
begin
  Render_Magnified := False;

  ClearData;

  numpoints := exec^.pts.n_points - 2; (* Remove phantom points *)

  for r := 0 to numPoints-1 do with exec^.pts do
  begin
    glyph2^.outline.points^[r].x := exec^.pts.cur^[r].x+64;
    glyph2^.outline.points^[r].y := exec^.pts.cur^[r].y+64;
  end;

  (* We begin rendering the glyph within the small bitmap *)

  G.n_contours := glyph^.outline.n_contours;
  G.conEnds    := glyph^.outline.conEnds;
  G.Points     := glyph^.outline.points;
  G.points     := glyph2^.outline.points;
  G.Flags      := glyph^.outline.flags;

  G.second_pass    := True;
  G.high_precision := True;
  G.dropout_mode   := 2;

  if Render_Glyph ( G, Bitmap_small ) then goto Exit_1;

  (* Then, we render the glyph outline in the bit bitmap *)

  for r := 0 to numPoints-1 do
  begin
    x := exec^.pts.cur^[r].x;
    y := exec^.pts.cur^[r].y;

    x := (x - Precis*Grid_Center_X)*16 + Big_Center_X*Precis;
    y := (y - Precis*Grid_Center_Y)*16 + Big_Center_Y*Precis;

    glyph2^.outline.points^[r].x := x + 8*64;
    glyph2^.outline.points^[r].y := y + 8*64;
  end;

   (* first compute the magnified coordinates *)

  G.n_contours := glyph^.outline.n_contours;
  G.conEnds    := glyph^.outline.conEnds;
  G.Points     := glyph^.outline.points;
  G.points     := glyph2^.outline.points;
  G.Flags      := glyph^.outline.flags;

  G.second_pass    := True;
  G.high_precision := True;
  G.dropout_mode   := 2;

  if display_outline then
    if Render_Glyph ( G, Bitmap_big ) then goto Exit_1;

  (* Now, magnify the small bitmap, XORing it to the big bitmap *)

  r := 0;
  w := 0;
  b := 0;

  empty := @Grid_Empty;

  if display_outline then pixel := @Grid_Pixel1
                     else pixel := @Grid_Pixel2;

  for y := 0 to Grid_Height-1 do
  begin

    for x := 0 to Grid_Width-1 do
    begin

      w2 := w;
      b  := b shr 1;

      if b = 0 then
      begin
        c := PByte(Bitmap_small.Buffer)^[r];
        b := $80;
        inc( r );
      end;

      if c and b <> 0 then block := pixel
                      else block := empty;

      for v := 0 to 7 do
      begin
        PByte(Bitmap_Big.Buffer)^[w2] := PByte(Bitmap_Big.Buffer)^[w2]
                                         xor block^[v];
        inc( w2, Bitmap_Big.cols );
      end;

      inc( w, 2 );

    end;

    inc( w, 15*Screen_Cols );

  end;


  (* Display the resulting big bitmap *)

  Display_BitMap_On_Screen( Bitmap_big.Buffer^, 450, 80  );

Exit_1:
  (* Clear the bitmaps *)

  Render_Magnified := True;
end;


function Render_Simple : boolean;
label
  Exit_1;
var
  r, w, w2, u, v, b, c : integer;

  x, y : Long;

  G     : TT_Outline;

  numPoints : integer;
begin
  Render_Simple := False;

  numpoints := exec^.pts.n_points - 2; (* Remove phantom points *)

  for r := 0 to numPoints-1 do with exec^.pts do
  begin
    glyph2^.outline.points^[r].x := exec^.pts.cur^[r].x + 32;
    glyph2^.outline.points^[r].y := exec^.pts.cur^[r].y + 32;
  end;

  (* We begin rendering the glyph within the small bitmap *)

  G.n_contours := glyph^.outline.n_contours;
  G.conEnds    := glyph^.outline.conEnds;
  G.Points     := glyph^.outline.points;
  G.points     := glyph2^.outline.points;
  G.Flags      := glyph^.outline.flags;

  G.second_pass    := True;
  G.high_precision := True;
  G.dropout_mode   := 2;


  if display_outline then
    if Render_Glyph ( G, Bitmap_big ) then goto Exit_1;

  (* Display the resulting big bitmap *)

  Display_BitMap_On_Screen( Bitmap_big.Buffer^, 450, 80  );

Exit_1:
  (* Clear the bitmaps *)

  ClearData;

  Render_Simple := True;
end;


procedure Exit_Viewer;
begin
  Restore_Screen;
  debug_mode := debug_code;
  MyApp.SetScreenMode( smCo80 + smFont8x8 );
  MyApp.Show;
  MyApp.ReDraw;
end;


procedure Enter_Viewer;
begin
  Set_Graph_Screen( Graphics_Mode_Mono );

  if not Render_Magnified then
    Exit_Viewer
  else
    debug_mode := view_glyph;
end;


procedure TMyApp.Execute_Loop;
var
  Out : Boolean;
  B   : PBreakPoint;

  Event : TEvent;
begin

  Out  := False;
  etat := etat_Execution;

  repeat

    Single_Step;

    B := Find_BreakPoint( Volatiles, exec^.curRange, exec^.IP );
    if B <> nil then
      begin
        Clear_Break( Volatiles, B );
        Out := True;
      end;

    if etat = etat_Execution then
      begin
        B := Find_BreakPoint( Gen_Range[exec^.curRange].Breaks,
                              exec^.curRange,
                              exec^.IP );
        if B <> nil then
          begin
            Out  := True;
            Etat := etat_Arret;
          end;
      end
    else
      Out := True;

  until Out;

end;


procedure TMyApp.New_Execution;
var
  Event : TEvent;
begin
  Event.What    := evWave;
  Event.Command := cmNewExecution;

  HandleEvent( Event );
end;


procedure TMyApp.Single_Step;
var
  tempStr : string[6];
begin

  if Run_Ins( exec ) then
  begin
    etat := etat_Termine;
    str( exec^.error, tempStr );
    MessageBox( 'Error : '+tempStr, nil, mfError+mfOkButton );
    exit;
  end;

  if exec^.IP >= exec^.codeSize then

    begin
      if (exec^.curRange <> TT_CodeRange_CVT) or
         Goto_CodeRange( exec, TT_CodeRange_Glyph, 0 ) then

        begin
          etat := etat_Termine;
          MessageBox( 'Completed', nil, mfInformation+mfOkButton );
          exit;
        end;
    end
end;


procedure TMyApp.ReFocus;
var
  Event  : TEvent;
begin
  Event.What := evCommand;

  if Old_Range <> exec^.curRange then
  begin
    Old_Range     := exec^.curRange;
    Event.Command := cmChangeRange;
    Event.InfoPtr := @Gen_Range[Old_Range];
    CW^.HandleEvent( Event );
  end;

  Event.What    := evWave;
  Event.Command := cmRefocus;

  if etat <> etat_Termine then
    Event.InfoInt := Get_Dis_Line( Gen_Range[Old_Range], exec^.IP )
  else
    Event.InfoInt := -1;

  HandleEvent( Event );
end;


procedure TMyApp.NewWindow;
var
  R  : TRect;
  RR : TRangeRec;
begin
  Desktop^.GetExtent(R);
  R.B.X := 32;

  Old_Range := exec^.curRange;

  New( CW, Init( R, @Gen_Range[Old_Range] ) );
  Desktop^.Insert(CW);

  Desktop^.GetExtent(R);
  R.A.X := 32;
  R.B.X := 50;
  R.B.Y := R.B.Y div 2;

  New( SW, Init( R, exec ) );
  Desktop^.Insert(SW);

  Desktop^.GetExtent(R);
  R.A.X := 50;
  R.B.Y := R.B.Y div 2;

  New( GW, Init( R, exec ) );
  Desktop^.Insert(GW);

  Desktop^.GetExtent(R);
  R.A.X := 32;
  R.A.Y := R.B.Y div 2;

{$IFDEF DEBUG_CVT}
  New( ZW, Init( R, @exec^.twilight ) );
{$ELSE}
  New( ZW, Init( R, @exec^.pts ) );
{$ENDIF}
  Desktop^.Insert(ZW);

  etat := etat_Arret;
end;


procedure TMyApp.InitMenuBar;
var
  R : TRect;
begin
  GetExtent(R);
  R.B.Y := R.A.Y + 1;
  MenuBar := New( PMenuBar, Init( R, NewMenu(
                  NewSubMenu( '~F~ile', hcNoContext, NewMenu(
                        NewItem( '~O~pen','F3', kbF3, cmFileOpen,
                                 hcNoContext,
                           nil )),
                   NewSubMenu( '~R~un', hcNoContext,
                       NewMenu(
                         NewItem( '~R~un','Ctrl-F9', kbCtrlF9,
                                  cmRun, hcNoContext,

                          NewItem( '~G~o to cursor','F4', kbF4,
                                   cmGoToCursor, hcNoContext,

                           NewItem( '~T~race into', 'F7', kbF7,
                                    cmTraceInto, hcNoContext,

                            NewItem( '~S~tep over', 'F8', kbF8,
                                     cmStepOver, hcNoContext,

                             NewItem( '~V~iew glyph', 'F9', kbF9,
                                       cmViewGlyph, hcNoContext,
                                       nil
                                    )
                                   )
                                  )
                                 )
                                )
                              ),
                  nil
                )))));
end;


procedure TMyApp.HandleEvent( var Event : TEvent );
var
  Adr : Long;
begin

  if debug_mode = view_glyph then
  begin

    case Event.What of

      evKeyDown : case Event.KeyCode of

                    kbF2  : begin
                              display_outline := not display_outline;

                              if not Render_Magnified then
                                Exit_Viewer;

                            end;

                    kbESC : Exit_Viewer;

                  end;
    end;

    ClearEvent( Event );
    exit;

  end;

  inherited HandleEvent(Event);

  case Event.What of

    evCommand : case Event.Command of

                  cmNewWin : NewWindow;

                  cmGoToCursor : begin
                                   if etat = etat_Termine then exit;

                                   Event.Command := cmQueryCursorAddr;
                                   Event.InfoPtr := @Adr;

                                   CW^.HandleEvent( Event );

                                   Set_Break( Volatiles,
                                              exec^.curRange,
                                              Adr );

                                   New_Execution;
                                   Execute_Loop;
                                   ReFocus;
                                 end;

                  cmTraceInto : begin
                                  if etat = etat_termine then exit;

                                  New_Execution;
                                  Single_Step;
                                  ReFocus;
                                end;

                  cmStepOver : begin
                                 if etat = etat_termine then exit;

                                 New_Execution;
                                 with exec^ do
                                 case code^[IP] of

                                   $2A,  (* LOOPCALL *)
                                   $2B : (* CALL     *)

                                   begin

                                     Set_Break( Volatiles,
                                                exec^.curRange,
                                                exec^.IP +
                                                Get_Length( exec^.Code,
                                                            exec^.IP ) );
                                     Execute_Loop;
                                   end;

                                 else

                                   Single_Step;
                                 end;

                                 ReFocus;
                               end;

                  cmViewGlyph :
                                Enter_Viewer;

                else
                  exit;
                end;

  else
    exit;
  end;

  ClearEvent(Event);
end;


constructor TMyApp.Init;
begin
  inherited Init;
  SetScreenMode( smCo80 + smFont8x8 );
  NewWindow;
end;



(*******************************************************************
 *
 *  Function    :  LoadTrueTypeChar
 *
 *  Description :
 *
 *  Notes  :
 *
 *****************************************************************)

Function LoadTrueTypeChar( index : integer ) : boolean;
var
  j, load_flag : int;

  rc : TT_Error;

begin
  LoadTrueTypeChar := FALSE;
(*
  if hint_glyph then load_flag := TT_Load_Scale_Glyph or TT_Load_Hint_Glyph
                else load_flag := TT_Load_Scale_Glyph;
*)

  load_flag := TT_Load_Scale_Glyph or TT_Load_Hint_Glyph or TT_Load_Debug;

  rc := TT_Load_Glyph( the_instance,
                           the_glyph,
                           index,
                           load_flag );
  if rc <> TT_Err_Ok then exit;

  LoadTrueTypeChar := TRUE;
end;


procedure Usage;
begin
  Writeln('Simple Library Debugger -- part of the FreeType project');
  Writeln('-----------------------------------------------------');
  Writeln;
  Writeln(' Usage :  debugger glyph_number point_size fontfile[.ttf]');
  Writeln;
  halt(2);
end;


var
  Code : Int;

begin

  if ParamCount <> 3 then
    Usage;

  val( ParamStr(1), glyph_number, Code );
  if Code <> 0 then
    Usage;

  val( ParamStr(2), point_size, Code );
  if Code <> 0 then
    Usage;

  filename := ParamStr(3);
  if Pos( '.', filename ) = 0 then filename := filename + '.ttf';

  TT_Init_FreeType;

  error := TT_Open_Face( filename, the_face );
  if error <> TT_Err_Ok then
  begin
    Writeln('Could not open file ',filename );
    halt(1);
  end;

  face := PFace(the_face.z);

  error := TT_New_Glyph( the_face, the_glyph );
  if error <> TT_Err_Ok then
    begin
      Writeln('ERROR : Could not get glyph' );
      Check_Error(error);
    end;

  glyph2 := PGlyph( the_glyph.z );

  error := TT_New_Glyph( the_face, the_glyph );
  if error <> TT_Err_Ok then
    begin
      Writeln('ERROR : Could not get glyph' );
      Check_Error(error);
    end;

  glyph := PGlyph( the_glyph.z );

  error := TT_New_Instance( the_face, the_instance );
  if error <> TT_Err_Ok then
    begin
      Writeln('ERROR: Could not create new instance' );
      Check_Error(error);
    end;

  instance := PInstance(the_instance.z);

  exec := New_Context( instance );
  if exec = nil then
    begin
      Writeln( 'could not create execution context' );
      halt(1);
    end;

  instance^.debug   := true;
  instance^.context := exec;

  TT_Set_Instance_Resolutions( the_instance, 96, 96 );

{$IFDEF DEBUG_CVT}
  exec^.curRange  := 1;

  (* code taken from freetype.pas *)

  with instance^.metrics do
  begin
    x_scale1 := ( Long(point_size*64) * x_resolution ) div 72;
    x_scale2 := instance^.owner^.fontHeader.units_per_EM;

    y_scale1 := ( Long(point_size*64) * y_resolution ) div 72;
    y_scale2 := x_scale2;

    if instance^.owner^.fontHeader.flags and 8 <> 0 then
    begin
      x_scale1 := (x_scale1 + 32) and -64;
      y_scale1 := (y_scale1 + 32) and -64;
    end;

    x_ppem   := x_scale1 div 64;
    y_ppem   := y_scale1 div 64;
  end;

  instance^.metrics.pointsize := point_size*64;
  instance^.valid := False;

  if Instance_Reset( instance, true ) then
    Panic1('Could not reset instance before executing CVT');
{$ELSE}
  error := TT_Set_Instance_PointSize( the_instance, point_size );
  if error <> TT_Err_Ok then
  begin
    Writeln('Could not execute CVT program' );
    Check_Error(error);
  end;
{$ENDIF}

  Init_Engine;

{$IFNDEF DEBUG_CVT}
  if not LoadTrueTypeChar( glyph_number )  then
  begin
    Writeln('Error while loading glyph' );
    halt(1);
  end;
{$ENDIF}

  exec^.instruction_trap := true;

{$IFNDEF DEBUG_CVT}
(*  Run_Context( exec, true ); *)
{$ENDIF}

  Initialize;

  MyApp.Init;
  MyApp.Run;
  MyApp.Done;

  TT_Done_FreeType;
end.
