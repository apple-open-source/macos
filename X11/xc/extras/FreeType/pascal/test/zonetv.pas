{****************************************************************************}
{*                                                                          *}
{*  ZoneTV.PAS                                                              *}
{*                                                                          *}
{*  This unit implements a simple TrueType zone points viewer for the       *}
{*  FREETYPE project debugger.                                              *}
{*                                                                          *}
{****************************************************************************}

Unit ZoneTV;

interface

uses Objects, Views, Drivers, FreeType, TTTypes, TTTables, TTObjs, TTDebug;

{$I DEBUGGER.INC}

type

  { TZoneViewer }

  { This TView is a simple point array viewer }

  PZoneViewer = ^TZoneViewer;
  TZoneViewer = object( TListViewer )

                  constructor Init( var Bounds : TRect;
                                    AZone      : PGlyph_Zone );

                  procedure Draw; virtual;
                  procedure HandleEvent( var Event : TEvent  ); virtual;

                private
                  Zone : PGlyph_Zone; { Pointer to the zone being displayed }
                  Save : TGlyph_Zone; { A copy of the zone to highlight     }
                                      { changes                             }
                  procedure Copy_Zone;

                end;

  { TCodeWindow }

  PZoneWindow = ^TZoneWindow;
  TZoneWindow = object( TWindow )
                  ZoneView : PZoneViewer;
                  constructor Init( var Bounds : TRect;
                                    AZone      : PGlyph_Zone );
                end;

implementation

{ TZoneViewer }

constructor TZoneViewer.Init;
var
  n : Int;
begin
  inherited Init( Bounds, 1, nil, nil );

  GrowMode  := gfGrowHiX or gfGrowHiY;
  DragMode  := dmDragGrow or dmLimitLoX or dmLimitLoY;
  Options   := Options or ofSelectable;
  EventMask := EventMask or evWave;

  Zone := AZone;

  GetMem( Save.org,   zone^.n_points*2*sizeof(Long) );
  GetMem( Save.cur,   zone^.n_points*2*sizeof(Long) );
  GetMem( Save.flags, zone^.n_points*sizeof(Byte) );

  Save.n_points   := Zone^.n_points;
  Save.n_contours := Zone^.n_contours;

  Copy_Zone;

  SetRange( Save.n_points );
end;


procedure TZoneViewer.Copy_Zone;
var
  n : Int;
begin
  n := 2*zone^.n_points * sizeof(Long);

  (* Note that we save also the original coordinates, as we're not sure *)
  (* that the debugger is debugged !                                    *)

  move( Zone^.org^, Save.org^, n );
  move( Zone^.cur^, Save.cur^, n );
  move( Zone^.flags^, Save.flags^, zone^.n_points );
end;


procedure TZoneViewer.HandleEvent( var Event : TEvent );
var
  Limits     : TRect;
  Mini, Maxi : Objects.TPoint;
begin

  inherited HandleEvent(Event);

  Case Event.What of

    evWave : case Event.Command of

               cmNewExecution : Copy_Zone;

               cmRefocus : DrawView;

              end;

    evCommand : case Event.Command of

                 cmResize: begin
                             Owner^.GetExtent(Limits);
                             SizeLimits( Mini, Maxi );
                             DragView(Event, DragMode, Limits, Mini, Maxi );
                             ClearEvent(Event);
                           end;
                end;
  end;
end;


procedure TZoneViewer.Draw;
const
  Colors : array[0..3] of byte
         = ($30,$3F,$0B,$0E);
  Touchs : array[0..3] of Char
         = (' ','x','y','b');
  OnOff  : array[0..1] of Char
         = (' ',':');
var
  I, J, Item : Int;
  B          : TDrawBuffer;
  S          : String;
  Indent     : Int;
  Ligne      : Int;

  Changed : Boolean;

  Back_Color,
  Color       : word;

  On_BP : boolean;
  BP    : PBreakPoint;

begin

  if HScrollBar <> nil then Indent := HScrollBar^.Value
                       else Indent := 0;

  with Save do
  begin

    for I := 0 to Self.Size.Y-1 do
    begin

      MoveChar( B, ' ', Colors[0], Self.Size.X );

      Item := TopItem + I;

      if (Range > 0) and
       ( Focused = Item ) then Back_Color := 2
                          else Back_Color := 0;

      if Item < n_points then
        begin

          Color := Back_Color;
          if ( flags^[item] <> Zone^.flags^[item] ) then inc( Color );

          S    := Hex16( Item ) + ':  ';
          S[1] := OnOff[Zone^.flags^[item] and 1];
          S[7] := Touchs[(Zone^.flags^[item] and TT_Flag_Touched_Both) shr 1];

          MoveStr( B, S, Colors[Color] );

          Color := Back_Color;
          if ( org^[item].x <> Zone^.org^[item].x ) then inc( Color );

          MoveStr ( B[8], Hex32( Zone^.org^[item].x ), Colors[Color] );
          MoveChar( B[16], ',', Colors[0], 1 );

          Color := Back_Color;
          if ( org^[item].y <> Zone^.org^[item].y ) then inc( Color );

          MoveStr( B[17], Hex32( Zone^.org^[item].y ), Colors[Color] );
          MoveStr( B[25], ' : ', Colors[0] );

          Color := Back_Color;
          if ( cur^[item].x <> Zone^.cur^[item].x ) then inc( Color );

          MoveStr ( B[28], Hex32( Zone^.cur^[item].x ), Colors[Color] );
          MoveChar( B[36], ',', Colors[0], 1 );

          Color := Back_Color;
          if ( cur^[item].y <> Zone^.cur^[item].y ) then inc( Color );

          MoveStr( B[37], Hex32( Zone^.cur^[item].y ), Colors[Color] );

        end;

      WriteLine( 0, I, Self.Size.X, 1, B );
    end;
  end;
end;

{ TZoneWindow }

constructor TZoneWindow.Init;
begin
  inherited Init( Bounds,'Zone',wnNoNumber );
  GetExtent( Bounds );
  Bounds.Grow(-1,-1);
  New( ZoneView, Init( Bounds, AZone ) );
  Insert( ZoneView );
end;

end.

