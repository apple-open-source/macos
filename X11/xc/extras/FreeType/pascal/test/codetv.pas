{****************************************************************************}
{*                                                                          *}
{*  CodeView.PAS                                                            *}
{*                                                                          *}
{*  This unit implements a simple TrueType bytecode viewer for the          *}
{*  FREETYPE project debugger.                                              *}
{*                                                                          *}
{****************************************************************************}

Unit CodeTV;

interface

uses Objects, Views, Drivers, TTTypes, TTDebug;

{$I DEBUGGER.INC}

type

  { TCodeViewer }

  { This TView is a simple code list viewer ( IP + focused + breaks ) }

  PCodeViewer = ^TCodeViewer;
  TCodeViewer = object( TListViewer )

                  constructor Init( var Bounds : TRect;
                                    ARange     : PRangeRec );

                  procedure Draw; virtual;
                  procedure HandleEvent( var Event : TEvent  ); virtual;

                  procedure Change_Range( ARange : PRangeRec );
                  procedure Change_Focus( ALine  : integer );

                  procedure Get_Cursor_Addr( P : PLong );

                private
                  CodeRange : PRangeRec;
                  IP        : Int;
                end;

  { TCodeWindow }

  PCodeWindow = ^TCodeWindow;
  TCodeWindow = object( TWindow )
                  CodeView : PCodeViewer;
                  constructor Init( var Bounds : TRect;
                                    ARange     : PRangeRec );
                end;

implementation

{ TCodeViewer }

constructor TCodeViewer.Init;
begin
  inherited Init( Bounds, 1, nil, nil );

  GrowMode  := gfGrowHiX or gfGrowHiY;
  DragMode  := dmDragGrow or dmLimitLoX or dmLimitLoY;
  EventMask := EventMask or evWave;

  IP := 0;

  Change_Range( ARange );
end;


procedure TCodeViewer.Change_Range;
begin
  codeRange := ARange;

  if codeRange <> nil then
    SetRange( codeRange^.NLines )
  else
    SetRange( 0 );
end;

procedure TCodeViewer.Change_Focus;
begin

  if ALine < 0 then
  begin
    IP := -1;
    DrawView;
    exit;
  end;

  if ALine >= TopItem + Size.Y then TopItem := ALine;

  if codeRange <> nil then
  begin
    FocusItem( ALine );
    IP := codeRange^.Disassembled^[ALine];
  end;
  DrawView;
end;


procedure TCodeViewer.Get_Cursor_Addr( P : PLong );
begin
  with codeRange^ do
  begin
    if (Focused < 0) or (Focused >= NLines) then
      P^[0] := -1
    else
      P^[0] := disassembled^[Focused];
  end;
end;


procedure TCodeViewer.HandleEvent( var Event : TEvent );
var
  Limits     : TRect;
  Mini, Maxi : Objects.TPoint;
begin

  inherited HandleEvent(Event);

  case Event.What of

    evCommand : case Event.Command of

                  cmChangeRange : Change_Range( Event.InfoPtr );

                  cmQueryCursorAddr : Get_Cursor_Addr( Event.InfoPtr );

                  cmResize: begin
                              Owner^.GetExtent(Limits);
                              SizeLimits( Mini, Maxi );
                              DragView(Event, DragMode, Limits, Mini, Maxi );
                              ClearEvent(Event);
                            end;

                 end;

    evWave : case Event.Command of

               cmReFocus : Change_Focus( Event.InfoInt );

             end;
  end;
end;


procedure TCodeViewer.Draw;
const
  Colors : array[0..3] of byte
         = ($1E,$40,$0E,$30);
  Prefix : array[1..3] of Char
         = ( 'f', 'c', 'g' );
var
  I, J, Item : Int;
  B          : TDrawBuffer;
  S          : String;
  Indent     : Int;
  Ligne      : Int;

  Color  : word;

  On_BP : boolean;
  BP    : PBreakPoint;

begin

{
  Colors[0] := GetColor(1);  (* Normal line *)
  Colors[1] := GetColor(2);  (* Normal breakpoint *)
  Colors[2] := GetColor(3);  (* Focused line *)
  Colors[3] := GetColor(4);  (* Focused breakpoint *)
}
  if HScrollBar <> nil then Indent := HScrollBar^.Value
                       else Indent := 0;

  with CodeRange^ do
  begin

    BP := Breaks;

    if (BP <> nil) and (NLines > TopItem) then
      while (BP <> nil) and (BP^.Address < Disassembled^[TopItem]) do
        BP := BP^.Next;

    for I := 0 to Self.Size.Y-1 do
    begin

      Item := TopItem + I;

      Color := 0;

      if Item < NLines then
        begin

          Ligne := Disassembled^[Item];

          if (BP <> nil) and (BP^.Address = Ligne) then
            begin
              Color := 1;
              Repeat
                BP := BP^.Next
              until (BP = nil) or (BP^.Address > Ligne);
            end;

          if (Range > 0) and
             ( Focused = Item ) then

            Color := Color or 2;

          S := ' ' + Cur_U_Line( Code, Ligne );

          S[2] := Prefix[index];

          S := copy( S, 1 + Indent, Self.Size.X );

          if Ligne = IP then
           begin
             S[1] := '=';
             S[7] := '>';
           end
        end
      else
        begin
          S := '';
        end;

      Color := Colors[Color];

      MoveChar( B, ' ', Color, Self.Size.X );
      MoveStr( B, S, Color );

      WriteLine( 0, I, Self.Size.X, 1, B );
    end;
  end;
end;

{ TCodeWindow }

constructor TCodeWindow.Init;
begin
  inherited Init( Bounds,'Code',wnNoNumber );
  GetExtent( Bounds );
  Bounds.Grow(-1,-1);
  New( CodeView, Init( Bounds, ARange ) );
  Insert( CodeView );
end;

end.
