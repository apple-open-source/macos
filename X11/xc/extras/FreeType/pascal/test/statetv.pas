unit StateTV;

interface

uses Objects, Views, Drivers, TTTypes, TTObjs, TTDebug;

{$I DEBUGGER.INC}

type

  { State Viewer }

  { A simple TView to show the current graphics state }

  PStateViewer = ^TStateViewer;
  TStateViewer = object( TView )

                   constructor Init( var Bounds : TRect;
                                     aexec      : PExec_Context );

                   procedure HandleEvent( var Event : TEvent ); virtual;
                   procedure Draw; virtual;

                 private
                   exec : PExec_Context;
                 end;

  { PStateWindow }

  PStateWindow = ^TStateWindow;
  TStateWindow = object( TWindow )
                   stateView : PStateViewer;
                   constructor Init( var Bounds : TRect;
                                     exec       : PExec_Context );
                 end;

implementation

{ TStateViewer }

constructor TStateViewer.Init;
begin
  inherited Init( Bounds );
  exec      := aexec;
  Options   := Options or ofSelectable;
  EventMask := EventMask or evWave;
end;

procedure TStateViewer.Draw;
var
  B     : TDrawBuffer;
  S     : String;
  Color : Int;
  n     : Int;
begin
  Color := $1E;
  n     := 0;
  MoveChar( B, ' ', Color, Self.Size.X );

  S := ' Loop                 ' + Hex16( exec^.GS.loop );

  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Auto_flip            ';
  if exec^.GS.auto_flip then S := S + ' Yes'
                            else S := S + '  No';
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Dual          ('+Hex16(exec^.GS.dualVector.x)+','+
                     Hex16(exec^.GS.dualVector.y)+')';
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Projection    ('+Hex16(exec^.GS.projVector.x)+','+
                     Hex16(exec^.GS.projVector.y)+')';
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Freedom       ('+Hex16(exec^.GS.freeVector.x)+','+
                     Hex16(exec^.GS.freeVector.y)+')';
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Gep0                   ' + Hex8( exec^.GS.gep0 );
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Gep1                   ' + Hex8( exec^.GS.gep1 );
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Gep2                   ' + Hex8( exec^.GS.gep2 );
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Ins_Control            ' + Hex8( exec^.GS.instruct_control );
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Rounding               ' + Hex8( exec^.GS.round_state );
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Min_Distance     ' + Hex32( exec^.GS.minimum_distance );
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Rp0                    ' + Hex8( exec^.GS.rp0 );
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Rp1                    ' + Hex8( exec^.GS.rp1 );
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Rp2                    ' + Hex8( exec^.GS.rp2 );
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Ctrl_Val_Cutin   ' + Hex32( exec^.GS.control_value_cutin );
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Sngl_Width_Cutin ' + Hex32( exec^.GS.single_width_cutin );
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Sngl_Widht_Value ' + Hex32( exec^.GS.single_width_value );
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  S := ' Scan_type              ' + Hex8( exec^.GS.scan_type );
  MoveStr( B, S, Color );
  WriteLine( 0, n, Self.Size.X, 1, B );
  inc( n );

  MoveChar( B, ' ', Color, Self.Size.X );
  WriteLine( 0, n, Self.Size.X, Size.Y-n, B );

end;

procedure TStateViewer.HandleEvent;
var
  Limits     : TRect;
  Mini, Maxi : Objects.TPoint;
begin

  inherited HandleEvent( Event );

  case Event.What of

    evWave : case Event.Command of

               cmReFocus : DrawView;
(*
                  cmResize: begin
                              Owner^.GetExtent(Limits);
                              SizeLimits( Mini, Maxi );
                              DragView(Event, DragMode, Limits, Mini, Maxi );
                              ClearEvent(Event);
                            end;
*)
             end;
  end;
end;


constructor TStateWindow.Init;
begin
  inherited Init( Bounds, 'State', wnNoNumber );
  GetExtent( Bounds );
  Bounds.Grow(-1,-1);
  New( StateView, Init( Bounds, exec ) );
  Insert( StateView );
end;

end.
