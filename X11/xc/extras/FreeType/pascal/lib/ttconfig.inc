(*                                                                   *)
(* TTConfig.Inc                                                      *)
(*                                                                   *)
(* This file contains several definition pragmas that are used to    *)
(* build several versions of the library. Each constant is commented *)

(* Define the DEBUG constant if you want the library dumping trace *)
(* information to the standard error output.                       *)
{ $DEFINE DEBUG}


(* Define the ASSERT constant if you want to generate runtime integrity  *)
(* checks within the library. Most of the checks will panic and stop the *)
(* the program when failed..                                             *)
{ $DEFINE ASSERT}


(* Define the INLINE constant if you want to use inlining when provided *)
(* by your compiler. Currently, only Virtual Pascal does                *)
{$IFDEF VIRTUALPASCAL}
{$DEFINE INLINE}
{$ENDIF}


(* Define the USE32 constant on 32-bit systems. Virtual Pascal *)
(* always define it by default. Now set for Delphi 2 and 3     *)
{$IFDEF WIN32}
{$DEFINE USE32}
{$ENDIF}

(* FreeType doesn't compile on old Pascal compilers that do not allow *)
(* inline assembly like Turbo Pascal 5.5 and below                    *)
{$IFDEF VER50}
ERROR : FreeType cannot be compiled with something older than Turbo Pascal 6.0
{$ENDIF}
{$IFDEF VER55}
ERROR : FreeType cannot be compiled with something older than Turbo Pascal 6.0
{$ENDIF}

(* Define the BORLANDPASCAL constant whenever you're using a DOS-based *)
(* version of Turbo or Borland Pascal.                                 *)
{$IFDEF VER60}
{$DEFINE BORLANDPASCAL}
{$ENDIF}
{$IFDEF VER70}
{$DEFINE BORLANDPASCAL}
{$ENDIF}

(* Define DELPHI16 when compiled in the 16_bit version of Delphi *)
{$IFDEF VER80}
{$DEFINE DELPHI16}
{$ENDIF}

(* Define DELPHI32 when compiled in any 32-bit version of Delphi *)
{$IFDEF VER90}     (* for Delphi 2 *)
{$DEFINE DELPHI32}
{$ENDIF}
{$IFDEF VER100}    (* for Delphi 3 *)
{$DEFINE DELPHI32}
{$ENDIF}
{$IFDEF VER110}    (* for Borland C++ Builder 3 *)
{$DEFINE DELPHI32}
{$ENDIF}
{$IFDEF VER120}    (* for Delphi 4 *)
{$DEFINE DELPHI32}
{$ENDIF}
{$IFDEF VER125}    (* for Borland C++ Builder 4 *)
{$DEFINE DELPHI32}
{$ENDIF}

(* I don't have Delphi 5, I hope this will work *)
{$IFDEF VER130}
{$DEFINE DELPHI32}
{$ENDIF}

