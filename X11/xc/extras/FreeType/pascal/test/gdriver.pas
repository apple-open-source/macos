(*******************************************************************
 *
 *  gdriver   : Graphics utility driver generic interface       1.1
 *
 *  Generic interface for all drivers of the graphics utility used
 *  by the FreeType test programs.
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

Unit GDriver;

interface

uses GEvents, GMain;

  (* Note that we now support an event based model, even with  */
  /* full-screen modes. It is the responsability of the driver */
  /* to map its events to the TEvent structure when called     */
  /* through Get_Event                                         *)

type
  Event = record
            what  : GEvent; (* event class     *)
            info  : Int;    (* event parameter *)
          end;

  (* the event classes are defined in the file 'gevents.h' included *)
  (* by the test programs, not by the graphics utility              *)

  procedure Get_Event( var ev : Event );
  (* get last event. In full-screen modes, a key-stroke must be */
  /* translated to an event class with a parameter.             *)

  function  Driver_Set_Graphics( mode : Int ) : boolean;
  (* A call to this function must set the graphics mode, the Vio    *)
  (* variable, as well as the values vio_ScanLineWidth, vio_Width   *)
  (* and vio_Height                                                 *)

  function Driver_Restore_Mode : boolean;
  (* Restore previous mode or release display buffer/window *)

  procedure Driver_Display_Bitmap( var buff; line, col : Int );
  (* Display bitmap on screen *)

implementation

{$IFDEF OS2}

  uses Os2Base, CRT;
  {$I GDRV_OS2.INC}

{$ELSE}

  uses CRT;
  {$I GDRV_DOS.INC}

{$ENDIF}

type
  Translator = record
                 key      : char;
                 ev_class : GEvent;
                 ev_info  : Int;
               end;

const
  Num_Translators = 15;

  Translators : array[1..Num_Translators] of Translator
              = (
                  (key:#27; ev_class:event_Quit ;         ev_info:0),

                  (key:'x'; ev_class: event_Rotate_Glyph; ev_info:  -1),
                  (key:'c'; ev_class: event_Rotate_Glyph; ev_info:   1),
                  (key:'v'; ev_class: event_Rotate_Glyph; ev_info: -16),
                  (key:'b'; ev_class: event_Rotate_Glyph; ev_info:  16),

                  (key:'9'; ev_class: event_Change_Glyph; ev_info:-100),
                  (key:'0'; ev_class: event_Change_Glyph; ev_info: 100),
                  (key:'i'; ev_class: event_Change_Glyph; ev_info: -10),
                  (key:'o'; ev_class: event_Change_Glyph; ev_info:  10),
                  (key:'k'; ev_class: event_Change_Glyph; ev_info:  -1),
                  (key:'l'; ev_class: event_Change_Glyph; ev_info:   1),

                  (key:'+'; ev_class: event_Scale_Glyph; ev_info:  10),
                  (key:'-'; ev_class: event_Scale_Glyph; ev_info: -10),
                  (key:'u'; ev_class: event_Scale_Glyph; ev_info:   1),
                  (key:'j'; ev_class: event_Scale_Glyph; ev_info:  -1)
                );

  procedure Get_Event( var ev : Event );
  var
    i : Int;
    c : char;
  begin
    c := ReadKey;

    for i := 1 to Num_Translators do
    begin
      if c = translators[i].key then
      begin
        ev.what := translators[i].ev_class;
        ev.info := translators[i].ev_info;
        exit;
      end;
    end;

    (* unrecognized keystroke *)

    ev.what := event_Keyboard;
    ev.info := Int(c);
  end;

end.

