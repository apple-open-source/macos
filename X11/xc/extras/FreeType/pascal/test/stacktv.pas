(* The Turbo Vision Stack Component. Part of the FreeType Debugger *)

unit StackTV;

interface

uses Objects, Views, Drivers, TTTypes, TTObjs, TTDebug;

type

  { TStackView }

  { A Simple stack display }

  PStackView = ^TStackView;
  TStackView = object( TListViewer )
                 constructor Init( var Bounds  : TRect;
                                   aexec       : PExec_Context;
                                   AVScrollBar : PScrollBar );

                 procedure HandleEvent( var Event : TEvent ); virtual;
                 procedure Draw; virtual;
                 procedure Update;

               private
                 exec : PExec_Context;
               end;

  { TStackWindow }

  PStackWindow = ^TStackWindow;
  TStackWindow = object( TWindow )
                   V : PScrollBar;
                   S : PStackView;
                   constructor Init( var Bounds : TRect;
                                     exec       : PExec_Context );
                 end;

implementation

{$I DEBUGGER.INC}

{ TStackView }

constructor TStackView.Init;
begin
  inherited Init( Bounds, 1, nil, AVScrollBar );
  exec := aexec;

  GrowMode  := gfGrowHiX or gfGrowHiY;
  DragMode  := dmDragGrow or dmLimitLoX or dmLimitLoY;
  EventMask := EventMask or evWave;

  SetRange( exec^.stackSize );
end;

procedure TStackView.Draw;
const
  Colors : array[0..1] of Byte = ($1E,$3E);
var
  B       : TDrawBuffer;
  Color   : Byte;
  I, Item : Int;
  S       : String[16];
begin
  Color := Colors[0];

  if exec^.top <= Size.Y then Item := Size.Y-1
                         else Item := exec^.top-1-TopItem;

  for I := 0 to Size.Y-1 do
  begin

    MoveChar( B, ' ', Color, Size.X );

    if Item < exec^.top then
     begin
       S :=  ' ' + Hex16( Item ) + ': ' + Hex32( exec^.stack^[Item] );
       MoveStr( B, S, Color );
     end;

    WriteLine( 0, I, Size.X, 1, B );
    dec( Item );
  end;

end;


procedure TStackView.Update;
begin
  FocusItem( 0 );
  DrawView;
end;

procedure TStackView.HandleEvent;
var
  Limits     : TRect;
  Mini, Maxi : Objects.TPoint;
begin
  case Event.What of

    evWave : case Event.Command of

               cmReFocus : Update;

             end;
  end;

  inherited HandleEvent( Event );

  case Event.Command of

    cmResize: begin
                Owner^.GetExtent(Limits);
                SizeLimits( Mini, Maxi );
                DragView(Event, DragMode, Limits, Mini, Maxi );
                ClearEvent(Event);
              end;
  end;

end;



{ TStackWindow }

constructor TStackWindow.Init;
var
  R : TRect;
begin
  inherited Init( Bounds, 'Pile', wnNoNumber );

  GetExtent( Bounds );
  R     := Bounds;
  R.A.X := R.B.X-1;
  inc( R.A.Y );
  dec( R.B.Y );
  New( V, Init(R) );
  Insert( V );

  R := Bounds;
  R.Grow(-1,-1);
  New( S, Init( R, exec, V ));

  Insert( S );
end;

end.
