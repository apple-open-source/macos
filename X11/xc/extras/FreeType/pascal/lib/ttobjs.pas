(*******************************************************************
 *
 *  ttobjs.pas                                                   2.0
 *
 *    Objects definition unit.
 *
 *  Copyright 1996, 1997 by
 *  David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 ******************************************************************)

(*                                                                       *)
(* The four important objects managed by the library are :               *)
(*                                                                       *)
(*   Face     : the object for a given typeface                          *)
(*   Instance : the object for a face's given pointsize/transform        *)
(*   Context  : the object for a given glyph loading/hinting execution   *)
(*   Glyph    : the object for a given glyph ( outline and metrics )     *)
(*                                                                       *)
(* A Face object is described by a TFace record, and its      *)
(* associated sub-tables. It is created through a call to the            *)
(* 'TT_Open_Face' API.                                                   *)
(*                                                                       *)
(* An Instance object is described by a TInstance record, and     *)
(* sub-tables. It is created for a given face through a call to the      *)
(* 'TT_Open_Instance' API. Several instances can share the same face     *)
(*                                                                       *)
(* The pointsize and/or transform of a given instance object can be      *)
(* changed on the fly through a call to the 'TT_Reset_Instance' API.     *)
(*                                                                       *)
(* A Glyph object is used to describe a glyph to the client application  *)
(* It is made of a TGlyph_Record header, with several sub-tables used    *)
(* to store, for example, point coordinates or outline info..            *)
(* It can hold metrics information and other attributes, as well as      *)
(* the glyph's outline. A client application can request any kind of     *)
(* info to the library on a given glyph through the 'TT_Get_Glyph'       *)
(* call.                                                                 *)
(*                                                                       *)
(*                                                                       *)
(* A Context is described by a TExec_Context record, and sub-tables *)
(* Execution contexts are created on demand during the following         *)
(* operations :                                                          *)
(*                                                                       *)
(*  - creating a new instance ( to read and execute the font program )   *)
(*  - setting/resetting the pointsize ( to execute the CVT program )     *)
(*  - during glyph loading ( when hinting is on )                        *)
(*                                                                       *)
(* They are used to run TrueType instructions and load/store             *)
(* glyph data that are not part of the Glyph object ( as they're of      *)
(* no meaning to a client application ).                                 *)
(*                                                                       *)
(* The library keeps track of all objects related to a given face :      *)
(*                                                                       *)
(* A face's instances are kept in two linked lists : one is the 'active' *)
(* list, which tracks the face's current opened instances, while the     *)
(* other is the 'idle' list used to collect/recycle instance objects     *)
(* when they become unuseful after a 'TT_Close_Instance' call.           *)
(*                                                                       *)
(* In the same way, a face's execution contexts are kept in two          *)
(* similar lists. Note that, as contexts are created on demand,          *)
(* the active and idle contexts lists should always contain few          *)
(* elements.                                                             *)
(*                                                                       *)
(* Look also for the following files :                                   *)
(*                                                                       *)
(*   Face manager     : TTFace.pas                                       *)
(*   Instance manager : TTInst.pas                                       *)
(*   Context  manager : TTExec.pas                                       *)
(*   Glyph    manager : TTGlyph.pas                                      *)
(*                                                                       *)

unit TTObjs;

interface

{$I TTCONFIG.INC}

uses FreeType,
     TTTypes,
     TTError,
     TTCache,
     TTTables,
     TTCMap;

type
  (* Graphics State                            *)
  (*                                           *)
  (* The Graphics State (GS) is managed by the *)
  (* instruction field, but does not come from *)
  (* the font file. Thus, we can use 'int's    *)
  (* where needed.                             *)
  (*                                           *)

  PGraphicsState = ^TGraphicsState;
  TGraphicsState = record
                     rp0,
                     rp1,
                     rp2                     : int;

                     dualVector,
                     projVector,
                     freeVector              : TT_UnitVector;

                     loop                    : longint;
                     minimum_distance        : TT_F26dot6;
                     round_state             : int;

                     auto_flip               : boolean;
                     control_value_cutin     : TT_F26dot6;
                     single_width_cutin      : TT_F26dot6;
                     single_width_value      : TT_F26dot6;
                     delta_base              : int;
                     delta_shift             : int;

                     instruct_control        : byte;
                     scan_control            : Boolean;
                     scan_type               : Int;

                     gep0,
                     gep1,
                     gep2                    : int;
                   end;


const
  Default_GraphicsState : TGraphicsState
                        = (
                            rp0                 : 0;
                            rp1                 : 0;
                            rp2                 : 0;
                            dualVector          : ( x:$4000; y:0 );
                            projVector          : ( x:$4000; y:0 );
                            freeVector          : ( x:$4000; y:0 );
                            loop                : 1;
                            minimum_distance    : 64;
                            round_state         : 1;
                            auto_flip           : True;
                            control_value_cutin : 4*17;
                            single_width_cutin  : 0;
                            single_width_value  : 0;
                            delta_Base          : 9;
                            delta_Shift         : 3;
                            instruct_control    : 0;
                            scan_control        : True;
                            scan_type           : 0;
                            gep0                : 1;
                            gep1                : 1;
                            gep2                : 1
                          );

  (**********************************************************************)
  (*                                                                    *)
  (*  Execution Subtables :                                             *)
  (*                                                                    *)
  (**********************************************************************)

const
  MaxCodeRanges = 3;
  (* There can only be 3 active code ranges at once :  *)
  (*   - the Font Program                              *)
  (*   - the CVT  Program                              *)
  (*   - a glyph's instructions set                    *)

  TT_CodeRange_Font  = 1;
  TT_CodeRange_Cvt   = 2;
  TT_CodeRange_Glyph = 3;

  CvtFlag_None = 0;
  CvtFlag_X    = 1;
  CvtFlag_Y    = 2;
  CvtFlag_Both = 3;

type
  TCodeRange = record
                 Base : PByte;
                 Size : Int;
               end;
  PCodeRange = ^TCodeRange;

  (* defines a code range                                            *)
  (*                                                                 *)
  (* code ranges can be resident to a glyph ( i.e. the Font Program) *)
  (* while some others are volatile ( Glyph instructions )           *)
  (* tracking the state and presence of code ranges allows function  *)
  (* and instruction definitions within a code range to be forgotten *)
  (* when the range is discarded                                     *)

  TCodeRangeTable = array[1..MaxCodeRanges] of TCodeRange;

  (* defines a function/instruction definition record *)
  PDefRecord = ^TDefRecord;
  TDefRecord = record
                 Range  : Int;     (* in which code range is it located ? *)
                 Start  : Int;     (* where does it start ?               *)
                 Opc    : Byte;    (* function #, or instruction code     *)
                 Active : boolean; (* is the entry active ?               *)
               end;

  PDefArray = ^TDefArray;
  TDefArray = array[0..99] of TDefRecord;

  (* defines a call record, used to manage function calls *)
  TCallRecord = record
                  Caller_Range : Int;
                  Caller_IP    : Int;
                  Cur_Count    : Int;
                  Cur_Restart  : Int;
                end;

  (* defines a simple call stack *)
  TCallStack = array[0..99] of TCallRecord;
  PCallStack = ^TCallStack;

  PGlyph_Zone = ^TGlyph_Zone;
  TGlyph_Zone = record
                  n_points   : Int;
                  n_contours : Int;

                  org        : TT_Points;  (* original (scaled) coords *)
                  cur        : TT_Points;  (* current coordinates      *)
                  flags      : TT_PTouchTable;

                  conEnds    : PUShort;
                end;

  TRound_Function = function( distance, compensation : TT_F26dot6 )
                     : TT_F26dot6;
  (* Rounding function, as used by the interpreter *)

  TMove_Function = procedure( zone     : PGlyph_Zone;
                              point    : Int;
                              distance : TT_F26dot6 );
  (* Point displacement along the freedom vector routine, as *)
  (* used by the interpreter                                 *)

  TProject_Function = function( var P1, P2 : TT_Vector ) : TT_F26dot6;
  (* Distance projection along one of the proj. vectors, as used *)
  (* by the interpreter                                          *)

  TFunc_Get_CVT = function ( index : Int ) : TT_F26Dot6;
  (* Reading a cvt value. Take care of non-square pixels when *)
  (* needed                                                   *)

  TFunc_Set_CVT = procedure( index : Int; value : TT_F26Dot6 );
  (* Setting or Moving a cvt value. Take care of non-square   *)
  (* pixels when needed                                       *)


  (********************************************************************)
  (*                                                                  *)
  (*   Glyph Sub-Tables                                               *)
  (*                                                                  *)
  (********************************************************************)

  PGlyph_Transform = ^TGlyph_Transform;
  TGlyph_Transform = record
                       xx, xy : TT_Fixed;
                       yx, yy : TT_Fixed;
                       ox, oy : TT_F26Dot6;
                     end;

  PSubglyph_Record = ^TSubglyph_Record;
  TSubglyph_Record = record
                       index        : Int;
                       is_scaled    : boolean;
                       is_hinted    : boolean;
                       preserve_pps : boolean;

                       bbox         : TT_BBox;
                       zone         : TGlyph_Zone;

                       arg1, arg2   : Int;
                       element_flag : Int;
                       transform    : TGlyph_Transform;
                       file_offset  : Long;

                       pp1, pp2     : TT_Vector;

                       advanceWidth : Int;
                       leftBearing  : Int;
                     end;

  TSubglyph_Stack = array[0..10] of TSubglyph_Record;
  PSubglyph_Stack = ^TSubglyph_Stack;

  (* A note regarding non-squared pixels :                               *)
  (*                                                                     *)
  (* ( This text will probably go into some docs at some time, for       *)
  (*   now, it is kept there to explain some definitions in the          *)
  (*   TIns_Metrics record ).                                            *)
  (*                                                                     *)
  (* The CVT is a one-dimensional array containing values that           *)
  (* control certain important characteristics in a font, like           *)
  (* the height of all capitals, all lowercase letter, default           *)
  (* spacing or stem width/height.                                       *)
  (*                                                                     *)
  (* These values are found in FUnits in the font file, and must be      *)
  (* scaled to pixel coordinates before being used by the CVT and        *)
  (* glyph programs. Unfortunately, when using distinct x and y          *)
  (* resolutions ( or distinct x and y pointsizes ), there are two       *)
  (* possible scalings.                                                  *)
  (*                                                                     *)
  (* A first try was to implement a 'lazy' scheme were all values        *)
  (* were scaled when first used. However, some values are always        *)
  (* used in the same direction, and some other are used in many         *)
  (* different circumstances and orientations.                           *)
  (*                                                                     *)
  (* I have found a simpler way to do the same, and it even seems to     *)
  (* work in most of the cases :                                         *)
  (*                                                                     *)
  (* - all CVT values are scaled to the maximum ppem size                *)
  (*                                                                     *)
  (* - when performing a read or write in the CVT, a ratio factor        *)
  (*   is used to perform adequate scaling. Example :                    *)
  (*                                                                     *)
  (*    x_ppem = 14                                                      *)
  (*    y_ppem = 10                                                      *)
  (*                                                                     *)
  (*   we chose ppem = x_ppem = 14 as the CVT scaling size. All cvt      *)
  (*   entries are scaled to it.                                         *)
  (*                                                                     *)
  (*   x_ratio = 1.0                                                     *)
  (*   y_ratio = y_ppem/ppem ( < 1.0 )                                   *)
  (*                                                                     *)
  (*   we compute the current ratio like :                               *)
  (*                                                                     *)
  (*     - if projVector is horizontal, ratio = x_ratio = 1.0            *)
  (*     - if projVector is vertical,   ratop = y_ratio                  *)
  (*     - else, ratio = sqrt( (proj.x*x_ratio)ý+(proj.y*y_ratio)ý )     *)
  (*                                                                     *)
  (*   reading a cvt value returns      ratio*cvt[index]                 *)
  (*   writing a cvt value in pixels    cvt[index]/ratio                 *)
  (*                                                                     *)
  (*   the current ppem is simple       ratio*ppem                       *)
  (*                                                                     *)

  TIns_Metrics = record
                   pointsize     : TT_F26Dot6;
                   x_resolution  : Int;
                   y_resolution  : Int;
                   x_ppem        : Int;
                   y_ppem        : Int;

                   x_scale1      : Long;
                   x_scale2      : Long;
                   y_scale1      : Long;
                   y_scale2      : Long;

                   (* for non-square pixels *)
                   x_ratio : Long;
                   y_ratio : Long;

                   scale1  : Long;
                   scale2  : Long;
                   ppem    : Int;
                   ratio   : Long;

                   (* compensations *)
                   compensations : array[0..3] of TT_F26Dot6;

                   (* flags *)
                   rotated       : Boolean;
                   stretched     : Boolean;
                 end;

  (********************************************************************)
  (*                                                                  *)
  (*  FreeType Face Object                                            *)
  (*                                                                  *)
  (********************************************************************)

  PFace         = ^TFace;
  PInstance     = ^TInstance;
  PExec_Context = ^TExec_Context;

  TFace = record

            stream     : TT_Stream;
            (* i/o stream *)

            ttcHeader  : TTTCHeader;
            (* TrueType collection header, if any was found *)

            maxProfile : TMaxProfile;
            (* maximum profile table, as defined by the TT Spec *)

            (* Note :                                         *)
            (*  it seems that some maximum values cannot be   *)
            (*  taken directly from this table, but rather by *)
            (*  combining some of its fields ( e.g. the max.  *)
            (*  number of points seems to be given by         *)
            (*   MAX( maxPoints, maxCompositePoints )         *)
            (*                                                *)
            (*  For this reason, we define later our own      *)
            (*  max values that are used to load and allocate *)
            (*  further tables..                              *)

            fontHeader : TT_Header;
            (* the font header as defined by the TT Spec *)

            horizontalHeader : TT_Horizontal_Header;
            (* the horizontal header, as defined in the spec *)

            verticalInfo : Boolean;
            (* set to true when vertical data is in the font *)

            verticalHeader : TT_Vertical_Header;
            (* vertical header table *)

            os2 : TT_OS2;
            (* 'OS/2' table *)

            postscript : TT_Postscript;
            (* 'Post' table *)

            hdmx : THdmx;
            (* 'hdmx' = horizontal device metrics table *)

            nameTable : TName_Table;
            (* 'name' = name table *)

            numTables : Int;
            dirTables : PTableDirEntries;
            (* The directory of the TrueType tables found in *)
            (* this face's stream                            *)

            numCMaps  : Int;
            cMaps     : PCMapTables;
            (* the directory of character mappings tables found *)
            (* for this face..                                  *)

            numLocations   : Int;
            glyphLocations : PStorage;
            (* the glyph locations table *)

            (* the hmtx table is now within the horizontal header *)

            fontPgmSize  : Int;
            fontProgram  : PByte;
            (* the font program, if any.. *)

            cvtPgmSize  : Int;
            cvtProgram  : PByte;
            (* the cvt (or 'prep') program, if any.. *)

            cvtSize  : Int;
            cvt      : PShort;
            (* the original, unscaled, control value table *)

            gasp     : TGasp;

            (* the following values must be set by the *)
            (* maximum profile loader..                *)

            numGlyphs     : Int;
            (* the face's total number of glyphs *)

            maxPoints     : Int;
            (* max glyph points number, simple and composite *)

            maxContours   : Int;
            (* max glyph contours number, simple and composite *)

            maxComponents : Int;
            (* max components in a composite glyph *)

            (* the following lists are used to track active *)
            (* instance and context objects, as well as     *)
            (* to recycle them..                            *)

            (* see 'TTLists'..                              *)

            instances : TCache;
            glyphs    : TCache;
            (* various caches for this face's child objects *)

            extension : Pointer;
            (* a typeless pointer to the face object's extensions *)

            generic   : Pointer;
            (* generic pointer - see TT_Set/Get_Face_Pointer *)
          end;

  (********************************************************************)
  (*                                                                  *)
  (*  FreeType Instance Object                                        *)
  (*                                                                  *)
  (********************************************************************)

  TInstance = record

                owner     : PFace;

                valid     : Boolean;
                metrics   : TIns_Metrics;

                numFDefs  : Int;       (* number of function defs *)
                maxFDefs  : Int;
                FDefs     : PDefArray; (* table of FDefs entries  *)

                numIDefs  : Int;       (* number of instruction defs *)
                maxIDefs  : Int;
                IDefs     : PDefArray; (* table of IDefs entries     *)

                maxFunc   : Int;       (* maximum function number    *)
                maxIns    : Int;       (* maximum instruction number *)

                codeRangeTable : TCodeRangeTable;

                GS        : TGraphicsState;

                storeSize : Int;
                storage   : PStorage;
                (* the storage area *)

                cvtSize   : Int;
                cvt       : PLong;
                (* the scaled control value table *)

                twilight  : TGlyph_Zone;
                (* the instance's twilight zone *)

                (* debugging variables *)

                debug   : Boolean;
                context : PExec_Context;
                (* when using the debugger, we must keep the  *)
                (* execution context with the instance object *)
                (* rather than asking it on demand            *)

                generic : Pointer;
                (* generic pointer - see TT_Set/Get_Instance_Pointer *)
              end;

  (********************************************************************)
  (*                                                                  *)
  (*  FreeType Execution Context Object                               *)
  (*                                                                  *)
  (********************************************************************)

  TExec_Context = record

                    face      : PFace;
                    instance  : PInstance;
                    error     : Int;

                    stackSize : Int;      (* size of instance stack *)
                    top       : Int;      (* top of instance stack  *)
                    stack     : PStorage; (* current instance stack *)

                    args      : Int; (* number of arguments in opcode *)
                    new_top   : Int; (* new stack top after opc. exec *)

                    zp0,
                    zp1,
                    zp2,
                    twilight,
                    pts       : TGlyph_Zone;

                    GS        : TGraphicsState;

                    curRange  : Int;   (* current code range number   *)
                    code      : PByte; (* current code range          *)
                    IP        : Int;   (* current instruction pointer *)
                    codeSize  : Int;   (* size of current range       *)

                    opcode    : Byte;  (* current opcode              *)
                    length    : Int;   (* length of current opcode    *)

                    step_ins  : boolean; (* used by the interpreter *)
                                         (* if true, go to the next *)
                                         (* instruction..           *)

                    loadSize  : Int;
                    loadStack : PSubglyph_Stack;
                    (* the load stack used to load composite glyphs *)

                    glyphIns  : PByte; (* glyph instructions *)
                    glyphSize : Int;   (* glyph ins. size    *)

                    callTop   : Int;
                    callSize  : Int;
                    callStack : PCallStack; (* interpreter call stack *)

                    period,                    (* values used for the *)
                    phase,                     (* 'SuperRounding'     *)
                    threshold : TT_F26dot6;

                    maxPoints   : Int;
                    maxContours : Int;

                    (* the following are copies of the variables found *)
                    (* in an instance object                           *)

                    numFDefs : Int;       (* number of function defs *)
                    maxFDefs : Int;
                    FDefs    : PDefArray; (* table of FDefs entries  *)

                    numIDefs : Int;       (* number of instruction defs *)
                    maxIDefs : Int;
                    IDefs    : PDefArray; (* table of IDefs entries     *)

                    maxFunc   : Int;       (* maximum function number    *)
                    maxIns    : Int;       (* maximum instruction number *)

                    codeRangeTable : TCodeRangeTable;

                    storeSize : Int;       (* size of current storage *)
                    storage   : PStorage;  (* storage area            *)

                    metrics   : TIns_Metrics;

                    cur_ppem       : Int;
                    scale1         : Long;
                    scale2         : Long;
                    cached_metrics : Boolean;

(*
                    numContours : Int;
                    endContours : PUShort;
*)
                    Instruction_Trap : Boolean;
                    (* used by the full-screen debugger. If set, the *)
                    (* interpreter will exit after executing one     *)
                    (* opcode. Used to perform single-stepping..     *)

                    is_composite : Boolean;
                    (* this flag is true when the glyph is a composite  *)
                    (* one. In this case, we measure original distances *)
                    (* in the loaded coordinates (font units), then     *)
                    (* scale them appropriately. This get rids of       *)
                    (* transformation artifacts (like symetries..)      *)

                    cvtSize  : Int;
                    cvt      : PLong;

                    (* these variables are proper to the context *)

                    F_dot_P   : Long;
                    (* the dot product of the free and projection *)
                    (* vector is used in frequent operations..    *)

                    func_round    : TRound_Function;
                    func_project  : TProject_Function;
                    func_dualproj : TProject_Function;
                    func_freeProj : TProject_Function;
                    func_move     : TMove_Function;

                    func_read_cvt  : TFunc_Get_CVT;
                    func_write_cvt : TFunc_Set_CVT;
                    func_move_cvt  : TFunc_Set_CVT;
                    (* single width ? *)

                  end;

  (********************************************************************)
  (*                                                                  *)
  (*  FreeType Glyph Object                                           *)
  (*                                                                  *)
  (********************************************************************)

  PGlyph = ^TGlyph;
  TGlyph = record
             face     : PFace;
             metrics  : TT_Big_Glyph_Metrics;
             outline  : TT_Outline;

             (* temporary - debugging purposes *)
             computed_width : Int;
             precalc_width  : Int;
             is_composite   : Boolean;
           end;

  PFont_Input = ^TFont_Input;
  TFont_Input = record
                  stream    : TT_Stream;  (* inpute stream               *)
                  fontIndex : Int;        (* index of font in collection *)
                end;

 (****************************************************************)
 (*                                                              *)
 (*  Code Range Functions                                        *)
 (*                                                              *)
 (****************************************************************)

 function Goto_CodeRange( exec  : PExec_Context;
                          range : Int;
                          IP    : Int ) : TError;
 (* Go to a specified coderange *)

 function Get_CodeRange( exec  : PExec_Context;
                         range : Int ) : PCodeRange;
 (* return a pointer to a given coderange record *)
 (* used only by the debugger                    *)

 function Set_CodeRange( exec   : PExec_Context;
                         range  : Int;
                         base   : Pointer;
                         length : Int ) : TError;
 (* Set a given code range properties *)

 function Clear_CodeRange( exec  : PExec_Context;
                           range : Int ) : TError;
 (* Clear a given code range *)

 (****************************************************************)
 (*                                                              *)
 (*  Management Functions                                        *)
 (*                                                              *)
 (****************************************************************)

 function New_Context( instance : PInstance ) : PExec_Context;
 (* Get a new execution context, either fresh or recycled, for *)
 (* an instance of the face 'res'                              *)
 (*                                                            *)
 (* Notes : - called by 'New_Face_Context'                     *)
 (*         - assumes that the face mutex is acquired          *)

 procedure Done_Context( exec : PExec_Context );
 (* Releases an execution context. The context can be destroyed *)
 (* or recycled, depending on implementation                    *)
 (*                                                             *)
 (* Notes : - called by 'Done_Face_Context'                     *)
 (*         - assumes that the face mutex is acquired           *)

 (****************************************************************)
 (*                                                              *)
 (*  Instance Update Functions                                   *)
 (*                                                              *)
 (****************************************************************)

 procedure Context_Load( exec : PExec_Context;
                         ins  : PInstance );
 (* update exec's data with the one found in 'ins' *)
 (* typically before an execution                  *)

 procedure Context_Save( exec : PExec_Context;
                         ins  : PInstance );
 (* update ins's data with the one found in 'exec' *)
 (* typically after an execution                   *)

 function  Context_Run( exec  : PExec_Context;
                        debug : Boolean ) : TError;

 function  Instance_Init( ins : PInstance ) : TError;

 function  Instance_Reset( ins   : PInstance;
                           debug : boolean    ) : TError;


 function  Scale_X( var metrics : TIns_Metrics; x : TT_Pos ) : TT_Pos;

 function  Scale_Y( var metrics : TIns_Metrics; y : TT_Pos ) : TT_Pos;

 function TTObjs_Init : TError;
 (* Initialize object manager *)

 procedure TTObjs_Done;
 (* Finalize object manager *)

var
  face_cache : TCache;
  exec_cache : TCache;

implementation

uses TTMemory, TTFile, TTCalc, TTLoad, TTInterp;

  function Face_Create( _face  : Pointer;
                        _input : Pointer ) : TError; far; forward;

  function Face_Destroy( _face : Pointer ) : TError; far; forward;

  function Context_Create( _context : Pointer;
                           _face    : Pointer ) : TError; far; forward;

  function Context_Destroy( exec : Pointer ) : TError; far; forward;

  function Instance_Create( _ins  : Pointer;
                            _face : Pointer  ) : TError; far; forward;

  function Instance_Destroy( instance : Pointer ) : TError; far; forward;

  function Glyph_Create( _glyph : Pointer;
                         _face  : Pointer  ) : TError; far; forward;

  function Glyph_Destroy( _glyph : Pointer ) : TError; far; forward;



const
  objs_face_class     : TCache_Class
                      = (object_size: sizeof(TFace);
                         idle_limit : -1;
                         init       : Face_Create;
                         done       : Face_Destroy );

  objs_exec_class     : TCache_Class
                      = (object_size: sizeof(TExec_Context);
                         idle_limit : 1;
                         init       : Context_Create;
                         done       : Context_Destroy );

  objs_instance_class : TCache_Class
                      = (object_size: sizeof(TInstance);
                         idle_limit : -1;
                         init       : Instance_Create;
                         done       : Instance_Destroy );

  objs_glyph_class    : TCache_Class
                      = (object_size: sizeof(TGlyph);
                         idle_limit : -1;
                         init       : Glyph_Create;
                         done       : Glyph_Destroy );

(*******************************************************************
 *
 *  Function    :  New_Context
 *
 *  Description :  gets a new active execution context for a given
 *                 resident/face object.
 *
 *  Input  :  aResident
 *
 *  Output :  Returns new exec. context. Nil in case of failure
 *
 *  Notes  :  Don't forget to modify 'Free_Context' if you change
 *            the fields of a TExec_Context
 *
 ******************************************************************)

 function New_Context( instance : PInstance ) : PExec_Context;
 var
   exec : PExec_Context;
 begin
   if instance = nil then
     exec := nil
   else
     Cache_New( exec_cache, Pointer(exec), instance^.owner );

   New_Context := exec;
 end;

(*******************************************************************
 *
 *  Function    :  Done_Context
 *
 *  Description :
 *
 *  Input  :  aResident
 *
 *  Output :  Discards an active execution context when it
 *            becomes unuseful. It is putin its face's recycle
 *            list
 *
 ******************************************************************)

 procedure Done_Context( exec : PExec_Context );
 begin
   if exec <> nil then
     Cache_Done( exec_cache, Pointer(exec) );
 end;

(*******************************************************************
 *
 *  Function    :  New_Instance
 *
 *  Description :  gets a new active instance for a given
 *                 face object.
 *
 *  Input  :  face
 *
 *  Output :  Returns new instance. Nil in case of failure
 *
 ******************************************************************)

 function New_Instance( face : PFace ) : PInstance;
 var
   ins : PInstance;
 begin
   if face = nil then
     ins := nil
   else
     Cache_New( face^.instances, Pointer(ins), face );

   New_Instance := ins;
 end;

(*******************************************************************
 *
 *  Function    :  Done_Instance
 *
 *  Description :
 *
 *  Input  :  instance
 *
 *  Output :  Discards an active instance when it
 *            becomes unuseful. It is put in its face's recycle
 *            list
 *
 ******************************************************************)

 procedure Done_Instance( instance : PInstance );
 begin
   if instance <> nil then
     Cache_Done( instance^.owner^.instances, Pointer(instance) );
 end;

 (****************************************************************)
 (*                                                              *)
 (*  Code Range Functions                                        *)
 (*                                                              *)
 (****************************************************************)

(*******************************************************************
 *
 *  Function    :  Goto_CodeRange
 *
 *  Description :  Switch to a new code range (updates Code and IP).
 *
 *  Input  :  exec    target execution context
 *            range   new execution code range
 *            IP      new IP in new code range
 *
 *  Output :  SUCCESS on success. FAILURE on error (no code range).
 *
 *****************************************************************)

 function Goto_CodeRange( exec  : PExec_Context;
                          range : Int;
                          IP    : Int ) : TError;
 begin
   Goto_CodeRange := Failure;

   if (range < 1) or (range > 3) then
   begin
     error := TT_Err_Bad_Argument;
     exit;
   end;

   with exec^.codeRangeTable[range] do
   begin

     if base = nil then
     begin
       error := TT_Err_Invalid_CodeRange;
       exit;
     end;

     (* NOTE : Because the last instruction of a program may be a CALL *)
     (*        which will return to the first byte *after* the code    *)
     (*        range, we test for IP <= Size, instead of IP < Size.    *)

     if IP > size then
     begin
       error := TT_Err_Code_Overflow;
       exit;
     end;

     exec^.code     := base;
     exec^.codeSize := size;
     exec^.IP       := IP;
     exec^.currange := range;
   end;

   Goto_CodeRange := Success;
 end;

(*******************************************************************
 *
 *  Function    :  Get_CodeRange
 *
 *  Description :  Returns a pointer to a given code range. Should
 *                 be used only by the debugger. Returns NULL if
 *                 'range' is out of current bounds.
 *
 *  Input  :  exec    target execution context
 *            range   new execution code range
 *
 *  Output :  Pointer to the code range record. NULL on failure.
 *
 *****************************************************************)

 function Get_CodeRange( exec  : PExec_Context;
                         range : Int ) : PCodeRange;
 begin
   if (range < 1) or (range > 3) then
     Get_CodeRange := nil
   else
     Get_CodeRange := @exec^.codeRangeTable[range];
 end;

(*******************************************************************
 *
 *  Function    :  Set_CodeRange
 *
 *  Description :  Sets a code range.
 *
 *  Input  :  exec    target execution context
 *            range   code range index
 *            base    new code base
 *            length  sange size in bytes
 *
 *  Output :  SUCCESS on success. FAILURE on error.
 *
 *****************************************************************)

 function Set_CodeRange( exec   : PExec_Context;
                         range  : Int;
                         base   : Pointer;
                         length : Int ) : TError;
 begin
   Set_CodeRange := Failure;

   if (range < 1) or (range > 3) then
     begin
       error := TT_Err_Invalid_CodeRange;
       exit;
     end;

   exec^.codeRangeTable[range].base := base;
   exec^.codeRangeTable[range].size := length;

   Set_CodeRange := Success;
 end;

(*******************************************************************
 *
 *  Function    :  Clear_CodeRange
 *
 *  Description :  clears a code range.
 *
 *  Input  :  exec    target execution context
 *            range   code range index
 *
 *  Output :  SUCCESS on success. FAILURE on error.
 *
 *  Notes  : Does not set the Error variable.
 *
 *****************************************************************)

 function Clear_CodeRange( exec  : PExec_Context;
                           range : Int ) : TError;
 begin
   Clear_CodeRange := Failure;

   if (range < 1) or (range > 3) then
     begin
       error := TT_Err_Invalid_CodeRange;
       exit;
     end;

    exec^.codeRangeTable[range].base := nil;
    exec^.codeRangeTable[range].size := 0;

    Clear_CodeRange := Success;
 end;


 (****************************************************************)
 (*                                                              *)
 (*  Management Functions                                        *)
 (*                                                              *)
 (****************************************************************)

(*******************************************************************
 *
 *  Function    :  Context_Destroy
 *
 *  Description :  Frees an execution context
 *
 *  Input  :  context : execution context
 *
 *  Notes  :  Allocation is found in the 'New_Context' function
 *
 ******************************************************************)

 function Context_Destroy( exec : Pointer ) : TError;
 begin
   Context_Destroy := Success;

   if exec = nil then exit;

   with PExec_Context(exec)^ do
   begin
     (* Free contours array *)
     Free( pts.conEnds );
     pts.n_contours := 0;

     Free( pts.cur );
     Free( pts.org );

     Free( pts.flags );
     pts.n_points := 0;

     (* Free stack *)
     Free( stack );
     stackSize := 0;

     (* Free call stack *)
     Free( callStack );
     callSize := 0;
     callTop  := 0;

     (* Free composite load stack *)
     Free( loadStack );

     (* free glyph code range *)
     Free( glyphIns );
     glyphSize := 0;

     instance := nil;
     face     := nil;
   end;
 end;


(*******************************************************************
 *
 *  Function    :  Context_Create
 *
 *  Description :  Creates a new execution context
 *
 *  Input  :  _context     context record
 *            _face        face record
 *
 ******************************************************************)

 function Context_Create( _context : Pointer;
                          _face    : Pointer ) : TError;
 var
   n_points   : Int;
   n_twilight : Int;

   exec : PExec_Context;
 label
   Fail_Memory;
 begin
   Context_Create := Failure;

   exec       := PExec_Context(_context);
   exec^.face := PFace(_face);

   with exec^ do
   begin

     callSize   := 32;
     loadSize   := face^.maxComponents + 1;
     storeSize  := face^.MaxProfile.maxStorage;
     stackSize  := face^.MaxProfile.maxStackElements + 32;
     (* Allocate a little extra for broken fonts like Courbs.ttf *)
     (* and Timesbs.ttf                                          *)

     n_points   := face^.maxPoints + 2;

     (* Reserve glyph code range *)
     if Alloc( glyphIns, face^.MaxProfile.maxSizeOfInstructions )  or

     (* Reserve call stack *)
        Alloc( callStack, callSize*sizeof(TCallRecord) )           or

     (* Reserve stack *)
        Alloc( stack, stackSize*sizeof(Long) )                     then

     (* we don't reserve the points and contours arrays anymore   *)
     (* as this will be performed automatically by a Context_Load *)

     (* the same is true for the load stack *)

       goto Fail_Memory;

     maxPoints   := 0;
     maxContours := 0;

     loadSize    := 0;
     loadStack   := nil;

     pts.n_points      := 0;
     pts.n_contours    := 0;

     instance   := nil;
   end;

   Context_Create := Success;
   exit;

 Fail_Memory:
   Context_Destroy(_context);
   error := TT_Err_Out_Of_Memory;
   exit;
 end;

(*******************************************************************
 *
 *  Function    :  Context_Run
 *
 *  Description :  Run a glyph's bytecode stream
 *
 *  Input  :  exec     context record
 *
 ******************************************************************)

 function Context_Run( exec  : PExec_Context;
                       debug : Boolean ) : TError;
 begin
   Context_Run := Failure;

   if Goto_CodeRange( exec, TT_CodeRange_Glyph, 0 ) then
     exit;

   with exec^ do
   begin
     top     := 0;
     callTop := 0;
     zp0     := pts;
     zp1     := pts;
     zp2     := pts;
     GS.gep0 := 1;
     GS.gep1 := 1;
     GS.gep2 := 1;

     GS.projVector.x := $4000;
     GS.projVector.y := $0000;
     GS.freeVector   := GS.projVector;
     GS.dualVector   := GS.projVector;
     GS.round_state  := 1;
     GS.loop         := 1;
   end;

   if not debug and Run_Ins( @exec^ ) then
   begin
     error := exec^.error;
     exit;
   end;

   Context_Run := Success;
 end;

(****************************************************************)
(*                                                              *)
(*  Instance Update Functions                                   *)
(*                                                              *)
(****************************************************************)

(*******************************************************************
 *
 *  Function    :  Context_Load
 *
 *  Description :  loads instance data into a new execution context
 *
 *******************************************************************)

 procedure Context_Load( exec : PExec_Context;
                         ins  : PInstance );

   procedure Update_Max( var size : Int;
                         mult     : Int;
                         var buff;
                         new_max  : Int );
   begin
     if size*mult < new_max then
     begin
       Free(buff);
       Alloc( buff, new_max*mult );
       size := new_max;
     end;
   end;


   procedure Update_Points( max_points   : Int;
                            max_contours : Int;
                            exec         : PExec_Context );
   begin
     if exec^.maxPoints < max_points then
     begin
       Free( exec^.pts.org );
       Free( exec^.pts.cur );
       Free( exec^.pts.flags );

       Alloc( exec^.pts.org, 2*sizeof(TT_F26dot6)*max_points );
       Alloc( exec^.pts.cur, 2*sizeof(TT_F26dot6)*max_points );
       Alloc( exec^.pts.flags, sizeof(Byte)      *max_points );

       exec^.maxPoints := max_points;
     end;

     if exec^.maxContours < max_contours then
     begin
       Free( exec^.pts.conEnds );
       Alloc( exec^.pts.conEnds, sizeof(Short)*max_contours );
       exec^.maxContours := max_contours;
     end;
   end;


 begin
   with exec^ do
   begin

     instance := ins;
     face     := ins^.owner;

     numFDefs := ins^.numFDefs;
     numIDefs := ins^.numIDefs;
     maxFDefs := ins^.maxFDefs;
     maxIDefs := ins^.maxIDefs;
     FDefs    := ins^.FDefs;
     IDefs    := ins^.IDefs;
     maxFunc  := ins^.maxFunc;
     maxIns   := ins^.maxIns;

     metrics  := ins^.metrics;

     codeRangeTable := ins^.codeRangeTable;

     storeSize := ins^.storeSize;
     storage   := ins^.storage;

     twilight  := ins^.twilight;

     (* We reserve some extra space to deal with broken fonts *)
     (* like Arial BS, Courier BS, etc..                      *)
     Update_Max( stackSize,
                 sizeof(Long),
                 stack,
                 face^.maxProfile.maxStackElements+32 );

     Update_Max( loadSize,
                 sizeof(TSubglyph_Record),
                 loadStack,
                 face^.maxComponents+1 );

     Update_Max( glyphSize,
                 sizeof(Byte),
                 glyphIns,
                 face^.maxProfile.maxSizeOfInstructions );

     (* XXXX : Don't forget the phantom points !! *)
     Update_Points( face^.maxPoints+2, face^.maxContours, exec );

     pts.n_points   := 0;
     pts.n_contours := 0;

     instruction_trap := false;

     (* Set default graphics state *)
     GS := ins^.GS;

     cvtSize := ins^.cvtSize;
     cvt     := ins^.cvt;
   end;
 end;


 procedure Context_Save( exec : PExec_Context;
                         ins  : PInstance );
 begin
   with ins^ do
   begin
     error    := exec^.error;

     numFDefs := exec^.numFDefs;
     numIDefs := exec^.numIDefs;
     maxFunc  := exec^.maxFunc;
     maxIns   := exec^.maxIns;

     codeRangeTable := exec^.codeRangeTable;

     (* Set default graphics state *)

     GS := exec^.GS;
   end;
 end;

(*******************************************************************
 *
 *  Function    :  Instance_Destroy
 *
 *  Description :  The Instance Record destructor.
 *
 *****************************************************************)

 function Instance_Destroy( instance : Pointer ) : TError;
 var
   ins : PInstance;
 begin

   Instance_Destroy := Success;

   ins := PInstance(instance);
   if ins = nil then
     exit;

   with ins^ do
   begin

     if debug then
     begin
       context := nil;
       debug   := false;
     end;

     (* Free twilight zone *)
     Free( twilight.org );
     Free( twilight.cur );
     Free( twilight.flags );
     twilight.n_points := 0;

     Free( cvt );
     cvtSize := 0;

     Free( storage );
     storeSize := 0;

     Free( FDefs );
     Free( IDefs );
     numFDefs := 0;
     numIDefs := 0;
     maxFDefs := 0;
     maxIDefs := 0;

     owner := nil;
     valid := false;

   end;
 end;

(*******************************************************************
 *
 *  Function    :  Instance_Create
 *
 *  Description :  The Instance constructor.
 *
 *  This functions creates a new instance object for a given face
 *
 *****************************************************************)

 function Instance_Create( _ins  : Pointer;
                           _face : Pointer  ) : TError;
 label
   Fail_Memory;
 var
   l    : Longint;
   ins  : PInstance;
   face : PFace;

   n_twilight : Int;
 begin
   Instance_Create := Failure;

   {$IFDEF ASSERT}
   if (_face = nil) then
     Panic1('TTInst.Init_Instance : void argument' );
   {$ENDIF}

   face := PFace(_face);
   ins  := PInstance(_ins);

   ins^.owner := face;

   with face^, ins^ do
   begin

     (* Reserve function and instruction defs arrays *)
     maxFDefs   := maxProfile.maxFunctionDefs;
     maxIDefs   := maxProfile.maxInstructionDefs;
     storeSize  := maxProfile.maxStorage;
     n_twilight := maxProfile.maxTwilightPoints;

     if Alloc( FDefs,   maxFDefs  * sizeof(TDefRecord) ) or
        Alloc( IDefs,   maxIDefs  * sizeof(TDefRecord) ) or
        Alloc( storage, storeSize * sizeof(Long) )       or

        Alloc( twilight.org, 2* n_twilight * sizeof(TT_F26Dot6) )  or
        Alloc( twilight.cur, 2* n_twilight * sizeof(TT_F26Dot6) )  or
        Alloc( twilight.flags,  n_twilight )

        then goto Fail_Memory;

     twilight.n_points := n_twilight;

     metrics.x_resolution := 96;
     metrics.y_resolution := 96;
     metrics.pointSize    := 10;
     metrics.x_scale2     := 1;
     metrics.y_scale2     := 1;
     metrics.scale2       := 1;

     { Reserve Control Value Table }
     cvtSize := face^.cvtSize;

     if Alloc( cvt, cvtSize * sizeof(Long) ) then
       goto Fail_Memory;

   end;

   Instance_Create := Success;
   exit;

 Fail_Memory:
   Instance_Destroy(ins);
   (* free all partially allocated tables, including the instance record *)

   error := TT_Err_Out_Of_Memory;
   exit;
 end;


(*******************************************************************
 *
 *  Function    :  Instance_Init
 *
 *  Description :  Initializes a fresh new instance
 *                 Executes the font program if any is found
 *
 *  Input : ins    the instance object to initialise
 *
 *****************************************************************)

 function Instance_Init( ins : PInstance ) : TError;
 var
   exec : PExec_Context;
   face : PFace;
 label
   Fin;
 begin
   Instance_Init := Failure;

   face := ins^.owner;

   if ins^.debug then
     exec := ins^.context
   else
     exec := New_Context( ins );
   (* debugging instances have their own context *)

   if exec = nil then
   begin
     error := TT_Err_Could_Not_Find_Context;
     exit;
   end;

   with ins^ do begin
     GS         := Default_GraphicsState;
     numFDefs   := 0;
     numIDefs   := 0;
     maxFunc    := -1;
     maxIns     := -1;
   end;

   Context_Load( exec, ins );

   with exec^ do
   begin
     callTop   := 0;
     top       := 0;
     period    := 64;
     phase     := 0;
     threshold := 0;

     with metrics do
     begin
       x_ppem    := 10;
       y_ppem    := 10;
       pointSize := 10;
       x_scale1  := 0;
       x_scale2  := 1;
       y_scale1  := 0;
       y_scale2  := 1;

       scale1 := 0;
       scale2 := 1;
       ratio  := 1 shl 16;
     end;

     instruction_trap := false;

     cvtSize := ins^.cvtSize;
     cvt     := ins^.cvt;

     F_dot_P := $10000;
   end;

   Set_CodeRange( exec,
                  TT_CodeRange_Font,
                  face^.fontProgram,
                  face^.fontPgmSize );
   (* Allow font program execution *)

   Clear_CodeRange( exec, TT_CodeRange_Cvt );
   Clear_CodeRange( exec, TT_CodeRange_Glyph );
   (* disable CVT and glyph programs coderanges *)

   if face^.fontPgmSize > 0 then
   begin
     if Goto_CodeRange( exec, TT_CodeRange_Font, 0 ) then
       goto Fin;

     if Run_Ins( @exec^ ) then
     begin
       error := exec^.error;
       goto Fin;
     end;
   end;

   Instance_Init := Success;

 Fin:
   Context_Save( exec, ins );

   if not ins^.debug then
     Done_Context( exec );

   ins^.valid := False;
 end;

(*******************************************************************
 *
 *  Function    :  Instance_Reset
 *
 *  Description :  Reset an instance to a new pointsize
 *                 Executes the prep/cvt program if any is found
 *
 *  Input : ins    the instance object to initialise
 *
 *****************************************************************)

 function Instance_Reset( ins   : PInstance;
                          debug : boolean    ) : TError;
 var
   exec : PExec_Context;
   face : PFace;
   i    : Int;
 label
   Fin;
 begin
   Instance_Reset := Failure;

   if ins^.valid then
   begin
     Instance_Reset := Success;
     exit;
   end;

   face := ins^.owner;

   (* compute new transform *)

   with ins^.metrics do
   begin

     if x_ppem < 1 then x_ppem := 1;
     if y_ppem < 1 then y_ppem := 1;

     if x_ppem >= y_ppem then
       begin
         scale1  := x_scale1;
         scale2  := x_scale2;
         ppem    := x_ppem;
         x_ratio := 1 shl 16;
         y_ratio := MulDiv_Round( y_ppem, $10000, x_ppem );
       end
     else
       begin
         scale1  := y_scale1;
         scale2  := y_scale2;
         ppem    := y_ppem;
         x_ratio := MulDiv_Round( x_ppem, $10000, y_ppem );
         y_ratio := 1 shl 16
       end;
   end;

   (* scale the cvt values to the new ppem *)

   for i := 0 to ins^.cvtSize-1 do
     ins^.cvt^[i] := MulDiv_Round( ins^.owner^.cvt^[i],
                                   ins^.metrics.scale1,
                                   ins^.metrics.scale2 );

   (* Note that we use the y resolution by default to scale the cvt *)

   ins^.GS := Default_GraphicsState;

   if ins^.debug then
     exec := ins^.context
   else
     exec := New_Context(ins);

   if exec = nil then
   begin
     error := TT_Err_Could_Not_Find_Context;
     exit;
   end;

   Context_Load( exec, ins );

   Set_CodeRange( exec,
                  TT_CodeRange_CVT,
                  face^.cvtProgram,
                  face^.cvtPgmSize );

   Clear_CodeRange( exec, TT_CodeRange_Glyph );

   with exec^ do
   begin

     for i := 0 to storeSize-1 do
       storage^[i] := 0;

     instruction_trap := False;

     top     := 0;
     callTop := 0;

     (* all twilight points are originally zero *)
     for i := 0 to twilight.n_points-1 do
     begin
       twilight.org^[i].x := 0;
       twilight.org^[i].y := 0;
       twilight.cur^[i].x := 0;
       twilight.cur^[i].y := 0;
     end;
   end;

   if face^.cvtPgmSize > 0 then
     if Goto_CodeRange( exec, TT_CodeRange_CVT, 0 ) or
        ( (not debug) and Run_Ins( @exec^ ) ) then
       goto Fin;

   ins^.GS        := exec^.GS;
   Instance_Reset := Success;

 Fin:
   Context_Save( exec, ins );

   if not ins^.debug then
     Done_Context(exec);

   if error = 0 then
     ins^.valid := True;
 end;


(*******************************************************************
 *
 *  Function    :  Face_Destroy
 *
 *  Description :  The face object destructor
 *
 *****************************************************************)

  function Face_Destroy( _face : Pointer ) : TError;
  var
    face : PFace;
    n    : Int;
  begin
    Face_Destroy := Success;

    face := PFace(_face);
    if face = nil then exit;

    Cache_Destroy( face^.instances );
    Cache_Destroy( face^.glyphs    );

    (* freeing the tables directory *)
    Free( face^.dirTables );
    face^.numTables := 0;

    (* freeing the locations table *)
    Free( face^.glyphLocations );
    face^.numLocations := 0;

    (* freeing the character mapping tables *)
    for n := 0 to face^.numCMaps-1 do
      CharMap_Free( face^.cMaps^[n] );

    Free( face^.cMaps );
    face^.numCMaps := 0;

    (* freeing the CVT *)
    Free( face^.cvt );
    face^.cvtSize := 0;

    (* freeing the horizontal header *)
    Free( face^.horizontalHeader.short_metrics );
    Free( face^.horizontalHeader.long_metrics  );
    if face^.verticalInfo then
    begin
      Free( face^.verticalHeader.short_metrics );
      Free( face^.verticalHeader.long_metrics  );
      face^.verticalInfo := False;
    end;

    (* freeing the programs *)
    Free( face^.fontProgram );
    Free( face^.cvtProgram );
    face^.fontPgmSize := 0;
    face^.cvtPgmSize  := 0;

    (* freeing the gasp table - none yet *)
    Free( face^.gasp.gaspRanges );

    (* freeing the names table *)
    Free( face^.nameTable.names );
    Free( face^.nameTable.storage );
    face^.nameTable.numNameRecords := 0;
    face^.nameTable.format         := 0;

    (* freeing the hdmx table *)
    for n := 0 to face^.hdmx.num_records-1 do
      Free( face^.hdmx.records^[n].widths );

    Free( face^.hdmx.records );
    face^.hdmx.num_records := 0;

    TT_Close_Stream( face^.stream );
  end;

(*******************************************************************
 *
 *  Function    :  Face_Create
 *
 *  Description :  The face object constructor
 *
 *****************************************************************)

  function Face_Create( _face  : Pointer;
                        _input : Pointer ) : TError;
  var
    input : PFont_Input;
    face  : PFace;
  label
    Fail;
  begin
    Face_Create := Failure;

    face  := PFace(_face);
    input := PFont_Input(_input);

    face^.stream := input^.stream;

    if Cache_Create( objs_instance_class, face^.instances ) or
       Cache_Create( objs_glyph_class,    face^.glyphs    ) then exit;

    (* Load collection directory if present *)
    if Load_TrueType_Directory( face, input^.fontIndex ) then
      exit;

    if Load_TrueType_Header                      ( face ) or
       Load_TrueType_MaxProfile                  ( face ) or
       Load_TrueType_Locations                   ( face ) or
       Load_TrueType_CMap                        ( face ) or
       Load_TrueType_CVT                         ( face ) or
       Load_TrueType_Metrics_Header       ( face, false ) or
       Load_TrueType_Programs                    ( face ) or
       Load_TrueType_Gasp                        ( face ) or
       Load_TrueType_Names                       ( face ) or
       Load_TrueType_OS2                         ( face ) or
       Load_TrueType_Hdmx                        ( face ) or
       Load_TrueType_Postscript                  ( face ) or
       Load_TrueType_Metrics_Header       ( face, true  ) then
      goto Fail;

    Face_Create := Success;
    exit;

  Fail:
    Face_Destroy( face );
  end;


  function Glyph_Destroy( _glyph : Pointer ) : TError;
  var
    glyph : PGlyph;
  begin
    Glyph_Destroy := Success;

    glyph := PGlyph(_glyph);
    if glyph = nil then
      exit;

    glyph^.outline.owner := true;
    TT_Done_Outline( glyph^.outline );
  end;


  function Glyph_Create( _glyph : Pointer;
                         _face  : Pointer  ) : TError;
  var
    glyph : PGlyph;
  begin
    glyph := PGlyph(_glyph);

    glyph^.face := PFace(_face);
    error       := TT_New_Outline( glyph^.face^.maxPoints+2,
                                   glyph^.face^.maxContours,
                                   glyph^.outline );
    if error <> TT_Err_Ok then
      Glyph_Create := Failure
    else
      Glyph_Create := Success;
  end;



  function  Scale_X( var metrics : TIns_Metrics; x : TT_Pos ) : TT_Pos;
  begin
    Scale_X := MulDiv_Round( x, metrics.x_scale1, metrics.x_scale2 );
  end;



  function  Scale_Y( var metrics : TIns_Metrics; y : TT_Pos ) : TT_Pos;
  begin
    Scale_Y := MulDiv_Round( y, metrics.y_scale1, metrics.y_scale2 );
  end;



  function TTObjs_Init : TError;
  begin
    TTObjs_Init := Failure;

    Cache_Create( objs_face_class, face_cache );
    Cache_Create( objs_exec_class, exec_cache );

    TTObjs_Init := success;
  end;



  procedure TTObjs_Done;
  begin
    Cache_Destroy( face_cache );
    Cache_Destroy( exec_cache );
  end;

end.

