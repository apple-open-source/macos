(*******************************************************************
 *
 *  FreeType.Pas
 *
 *    High-level interface specification.
 *
 *  Copyright 1996 David Turner, Robert Wilhelm and Werner Lemberg
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 *  Notes :
 *
 *    This is the only file that should be included by client
 *    application sources for the final release. All other types
 *    and functions defined in the "tt*.h" files are library
 *    internals, and should not be included ( except of course
 *    during development, as now )
 *
 ******************************************************************)

unit FreeType;

interface

type

{$IFDEF OS2}
  TT_Int    = Longint;
{$ELSE}
  TT_Int    = Integer;
{$ENDIF}

  TT_Long   = longint;
  TT_ULong  = longint;  (* there are no unsigned longs in Pascal :-(        *)
                        (* it will probably be a good idea to use cardinals *)
                        (* with Delphi and Virtual a bit later..            *)
  TT_Short  = integer;
  TT_UShort = word;

  TT_Fixed  = LongInt;  (* Signed Fixed 16.16 Float *)

  TT_FWord  = Integer;  (* Distance in FUnits *)
  TT_UFWord = Word;     (* Unsigned Distance  *)

  TT_F2Dot14 = Integer; (* signed fixed float 2.14 used for *)
                        (* unary vectors, with layout :     *)
                        (*                                  *)
                        (*  s : 1  -- sign bit              *)
                        (*  m : 1  -- mantissa bit          *)
                        (*  f : 14 -- unsigned fractional   *)
                        (*                                  *)
                        (*  's:m' is the 2-bit signed int   *)
                        (*  value to which the *positive*   *)
                        (*  fractional part should be       *)
                        (*  added.                          *)
                        (*                                  *)

  TT_F26Dot6 = LongInt; (* 26.6 fixed float, used for pixel coordinates *)

  TT_Pos     = Longint; (* funits or 26.6, depending on context *)

  (******************************************************)
  (*  a simple unit vector type                         *)
  (*                                                    *)
  TT_UnitVector = record

      x : TT_F2Dot14;
      y : TT_F2Dot14;
  end;

  (******************************************************)
  (*  a simple vector type                              *)
  (*                                                    *)
  TT_Vector = record

      x : TT_Pos;
      y : TT_Pos;
  end;

  (******************************************************)
  (*  a simple 2x2 matrix type                          *)
  (*                                                    *)
  TT_Matrix = record

      xx, xy : TT_Fixed;
      yx, yy : TT_Fixed;
  end;

  (******************************************************)
  (*  a glyph's bounding box                            *)
  (*                                                    *)
  TT_BBox = record

    xMin, yMin : TT_Pos;
    xMax, yMax : TT_Pos;
  end;

  (******************************************************)
  (*  the engine's error condition type - 0 always      *)
  (*  means success.                                    *)
  (*                                                    *)
  TT_Error = TT_Int;

  TT_Points_Table = array[0..99] of TT_Vector;
  TT_Points       = ^TT_Points_Table;

  TT_Coordinates  = array[0..99] of TT_Pos;
  TT_PCoordinates = ^TT_Coordinates;

  TT_TouchTable   = array[0..9] of byte;
  TT_PTouchTable  = ^TT_TouchTable;

  TT_ConStarts  = array[0..9] of word;
  TT_PConStarts = ^TT_ConStarts;

  (******************************************************)
  (*  glyph outline description                         *)
  (*                                                    *)
  TT_Outline = record

      n_points   : integer;
      n_contours : integer;

      points     : TT_Points;       (* array of point coordinates *)
      flags      : TT_PTouchTable;  (* array of point flags       *)
      conEnds    : TT_PConStarts;    (* array of contours ends    *)

      owner     : Boolean;          (* this flag is set when the outline *)
                                    (* owns the arrays it uses.          *)

      high_precision : Boolean;
      second_pass    : Boolean;
      dropout_mode   : Byte;
  end;

  (******************************************************)
  (*  glyph metrics structure                           *)
  (*                                                    *)
  TT_Glyph_Metrics = record

      bbox     : TT_BBox;
      bearingX : TT_Pos;
      bearingY : TT_Pos;
      advance  : TT_Pos;
  end;

  (******************************************************)
  (*  big glyph metrics structure                       *)
  (*                                                    *)
  TT_Big_Glyph_Metrics = record

      bbox         : TT_BBox;
      horiBearingX : TT_Pos;
      horiBearingY : TT_Pos;
      horiAdvance  : TT_Pos;
      vertBearingX : TT_Pos;
      vertBearingY : TT_Pos;
      vertAdvance  : TT_Pos;
  end;

  (******************************************************)
  (*  instance metrics. used to return information to   *)
  (*  clients regarding an instance's important state   *)
  (*                                                    *)
  TT_Instance_Metrics = record

      pointsize    : integer;

      x_ppem       : integer;
      y_ppem       : integer;

      x_scale      : TT_Fixed;
      y_scale      : TT_Fixed;

      x_resolution : integer;
      y_resolution : integer;
  end;

const
  TT_Flow_Down = -1;
  TT_Flow_Up   = +1;

type

  (******************************************************)
  (*  a record used to describe a bitmap or pixmap to   *)
  (*  the rasterizer.                                   *)
  (*                                                    *)
  TT_Raster_Map  = record

      Rows   : TT_Int;      (* rows number of the bitmap    *)
      Cols   : TT_Int;      (* columns (bytes) per row      *)
      Width  : TT_Int;      (* pixels per row               *)
      Flow   : TT_Int;      (* bit/pixmap's flow            *)
      Buffer : pointer;      (* bit/pixmap data              *)
      Size   : longint;      (* bit/pixmap data size (bytes) *)
  end;

  (******************************************************)
  (*  The TrueType font header table structure          *)
  (*                                                    *)
  TT_Header = record

      table_version   : TT_Fixed;
      font_revision   : TT_Fixed;

      checksum_adjust : TT_Long;
      magic_number    : TT_Long;

      flags           : TT_UShort;
      units_per_EM    : TT_UShort;

      created         : array[0..1] of TT_Long;
      modified        : array[0..1] of TT_Long;

      xMin, yMin      : TT_FWord;
      xMax, yMax      : TT_FWord;

      mac_style       : TT_UShort;
      lowest_rec_PPEM : TT_UShort;
      font_direction  : TT_Short;

      index_to_loc_format : TT_Short;
      glyph_data_format   : TT_Short;
  end;

  (******************************************************)
  (*  The TrueType horizontal header table structure    *)
  (*                                                    *)
  TT_Horizontal_Header = record

      version   : TT_Fixed;
      ascender  : TT_FWord;
      descender : TT_FWord;
      line_gap  : TT_FWord;

      advance_Width_Max      : TT_UShort;
      min_left_side_bearing  : TT_Short;
      min_right_side_bearing : TT_Short;
      xMax_extent            : TT_Short;
      caret_slope_rise       : TT_Short;
      caret_slope_run        : TT_Short;

      reserved  : array[0..4] of TT_SHort;

      metric_data_format     : TT_Short;
      number_of_HMetrics     : TT_UShort;

      (* the following are not part of the header in the file *)

      short_metrics : Pointer;
      long_metrics  : Pointer;
  end;

  (******************************************************)
  (*  The TrueType vertical header table structure      *)
  (*                                                    *)
  TT_Vertical_Header = record

      version   : TT_Fixed;
      ascender  : TT_FWord;
      descender : TT_FWord;
      line_gap  : TT_FWord;

      advance_Height_Max      : TT_UShort;
      min_top_side_bearing    : TT_Short;
      min_bottom_side_bearing : TT_Short;
      yMax_extent             : TT_Short;
      caret_slope_rise        : TT_Short;
      caret_slope_run         : TT_Short;

      reserved  : array[0..4] of TT_SHort;

      metric_data_format     : TT_Short;
      number_of_VMetrics     : TT_UShort;

      (* the following are not part of the header in the file *)

      short_metrics : Pointer;
      long_metrics  : Pointer;
  end;

  (******************************************************)
  (*  The TrueType OS/2 table structure                 *)
  (*                                                    *)
  TT_OS2 = record
      version             : TT_UShort;   (* $0001 *)
      xAvgCharWidth       : TT_Short;
      usWeightClass       : TT_UShort;
      usWidthClass        : TT_UShort;
      fsType              : TT_Short;
      ySubscriptXSize     : TT_Short;
      ySubscriptYSize     : TT_Short;
      ySubScriptXOffset   : TT_Short;
      ySubscriptYOffset   : TT_Short;
      ySuperscriptXSize   : TT_Short;
      ySuperscriptYSize   : TT_Short;
      ySuperscriptXOffset : TT_Short;
      ySuperscriptYOffset : TT_Short;
      yStrikeoutSize      : TT_Short;
      yStrikeoutPosition  : TT_Short;
      sFamilyClass        : TT_Short;
      panose              : array[0..9] of Byte;
      ulUnicodeRange1     : TT_ULong;   (* bits  0-31  *)
      ulUnicodeRange2     : TT_ULong;   (* bits 32-63  *)
      ulUnicodeRange3     : TT_ULong;   (* bits 64-95  *)
      ulUnicodeRange4     : TT_ULong;   (* bits 96-127 *)
      achVendID           : array[0..3] of Byte;
      fsSelection         : TT_UShort;
      usFirstCharIndex    : TT_UShort;
      usLastCharIndex     : TT_UShort;
      sTypoAscender       : TT_Short;
      sTypoDescender      : TT_Short;
      sTypoLineGap        : TT_Short;
      usWinAscent         : TT_UShort;
      usWinDescent        : TT_UShort;

      (* only version 1 tables *)
      ulCodePageRange1    : TT_ULong;
      ulCodePageRange2    : TT_ULong;
  end;

  (******************************************************)
  (*  The TrueType Postscript table structure           *)
  (*                                                    *)
  TT_Postscript = record

      FormatType         : TT_Fixed;
      italicAngle        : TT_Fixed;
      underlinePosition  : TT_Short;
      underlineThickness : TT_Short;
      isFixedPitch       : TT_ULong;
      minMemType42       : TT_ULong;
      maxMemType42       : TT_ULong;
      minMemType1        : TT_ULong;
      maxMemType1        : TT_ULong;
  end;

  (******************************************************)
  (*  face properties. use to report important face     *)
  (*  data to clients                                   *)
  (*                                                    *)
  TT_Face_Properties = record

      num_glyphs   : integer;
      max_points   : integer;
      max_contours : integer;
      max_faces    : integer;

      header       : ^TT_Header;
      horizontal   : ^TT_Horizontal_Header;
      vertical     : ^TT_Vertical_Header;
      os2          : ^TT_OS2;
      postscript   : ^TT_Postscript;
  end;

  (******************************************************)
  (*  Objects handle types                              *)
  (*                                                    *)
  TT_Stream   = record z : Pointer; end;
  TT_Face     = record z : Pointer; end;
  TT_Instance = record z : Pointer; end;
  TT_Glyph    = record z : Pointer; end;
  TT_CharMap  = record z : Pointer; end;

  TT_Gray_Palette = array[0..4] of byte;

  (******************************************************************)
  (*                                                                *)
  (*                         ERROR CODES                            *)
  (*                                                                *)
  (******************************************************************)

const
  (* ------------------- Success is always 0 ---------------------- *)

  TT_Err_Ok                      =  0;

  (* -------------------------------------------------------------- *)

  TT_Err_Invalid_Face_Handle     = $0001;
  TT_Err_Invalid_Instance_Handle = $0002;
  TT_Err_Invalid_Glyph_Handle    = $0003;
  TT_Err_Invalid_CharMap_Handle  = $0004;
  TT_Err_Invalid_Result_Address  = $0005;
  TT_Err_Invalid_Glyph_Index     = $0006;
  TT_Err_Invalid_Argument        = $0007;
  TT_Err_Could_Not_Open_File     = $0008;
  TT_Err_File_Is_Not_Collection  = $0009;

  TT_Err_Table_Missing           = $000A;
  TT_Err_Invalid_Horiz_Metrics   = $000B;
  TT_Err_Invalid_Vert_Metrics    = $000B;
  TT_Err_Invalid_CharMap_Format  = $000C;

  TT_Err_Invalid_File_Format     = $0010;
  TT_Err_File_Error              = $0011;

  TT_Err_Invalid_Engine          = $0020;
  TT_Err_Too_Many_Extensions     = $0021;
  TT_Err_Extensions_Unsupported  = $0022;
  TT_Err_Invalid_Extension_Id    = $0023;

  TT_Err_No_Vertical_Data        = $0030;

  TT_Err_Max_Profile_Missing     = $0080;
  TT_Err_Header_Table_Missing    = $0081;
  TT_Err_Horiz_Header_Missing    = $0082;
  TT_Err_Locations_Missing       = $0083;
  TT_Err_Name_Table_Missing      = $0084;
  TT_Err_CMap_Table_Missing      = $0085;
  TT_Err_Hmtx_Table_Missing      = $0086;
  TT_Err_OS2_Table_Missing       = $0087;
  TT_Err_Post_Table_Missing      = $0088;

  (* -------------------------------------------------------------- *)

  TT_Err_Out_Of_Memory           = $0100;

  (* -------------------------------------------------------------- *)

  TT_Err_Invalid_File_Offset     = $0200;
  TT_Err_Invalid_File_Read       = $0201;
  TT_Err_Invalid_Frame_Access    = $0202;

  (* -------------------------------------------------------------- *)

  TT_Err_Too_Many_Points         = $0300;
  TT_Err_Too_Many_Contours       = $0301;
  TT_Err_Invalid_Composite       = $0302;
  TT_Err_Too_Many_Ins            = $0303;

  (* -------------------------------------------------------------- *)

  TT_Err_Invalid_Opcode          = $0400;
  TT_Err_Too_Few_Arguments       = $0401;
  TT_Err_Stack_Overflow          = $0402;
  TT_Err_Code_Overflow           = $0403;
  TT_Err_Bad_Argument            = $0404;
  TT_Err_Divide_By_Zero          = $0405;
  TT_Err_Storage_Overflow        = $0406;
  TT_Err_Cvt_Overflow            = $0407;
  TT_Err_Invalid_Reference       = $0408;
  TT_Err_Invalid_Distance        = $0409;
  TT_Err_Interpolate_Twilight    = $040A;
  TT_Err_Debug_Opcode            = $040B;
  TT_Err_ENDF_In_Exec_Stream     = $040C;
  TT_Err_Out_Of_CodeRanges       = $040D;
  TT_Err_Nested_DEFs             = $040E;
  TT_Err_Invalid_CodeRange       = $040F;
  TT_Err_Invalid_Displacement    = $0410;
  TT_Err_Execution_Too_Long      = $0411;
  TT_Err_Too_Many_FuncDefs       = $0412;
  TT_Err_Too_Many_InsDefs        = $0413;

  TT_Err_Nested_Frame_Access     = $0500;
  TT_Err_Invalid_Cache_List      = $0501;
  TT_Err_Could_Not_Find_Context  = $0502;
  TT_Err_UNlisted_Object         = $0503;

  TT_Err_Raster_Pool_Overflow    = $0600;
  TT_Err_Raster_Negative_Height  = $0601;
  TT_Err_Invalid_Value           = $0602;
  TT_Err_Raster_Not_Initialised  = $0603;

  (* -------------------------------------------------------------- *)

  (***********************************************************************)
  (*                                                                     *)
  (*                  Base Library Functions                             *)
  (*                                                                     *)
  (***********************************************************************)

  (*****************************************************************)
  (*  Initialise the engine                                        *)
  (*                                                               *)
  function  TT_Init_FreeType : TT_Error;

  (*****************************************************************)
  (*  Finalise the engine - discards all objects                   *)
  (*                                                               *)
  procedure TT_Done_FreeType;

  (*****************************************************************)
  (*  Set the gray-level palette used for font-smoothing           *)
  (*                                                               *)
  (*  it is an array of 5 bytes following this convention :        *)
  (*                                                               *)
  (*    palette[0] := background (white)                           *)
  (*    palette[1] := light                                        *)
  (*    palette[2] := medium                                       *)
  (*    palette[3] := dark                                         *)
  (*    palette[4] := foreground (black)                           *)
  (*                                                               *)
  function TT_Set_Raster_Palette( palette : TT_Gray_Palette ) : TT_Error;

  (***********************************************************************)
  (*                                                                     *)
  (*                    Face Management functions                        *)
  (*                                                                     *)
  (***********************************************************************)

  (*****************************************************************)
  (*  Open a new font file and returns a handle for it in '_face'  *)
  (*                                                               *)
  (*  The file can be a TrueType collection, in which case the     *)
  (*  first embedded font will be loaded.                          *)
  (*                                                               *)
  function TT_Open_Face( fontname  : string;
                         var _face : TT_Face ) : TT_Error;

  (*****************************************************************)
  (*  Open a font file embedded in a collection.                   *)
  (*                                                               *)
  function TT_Open_Collection( fontname  : string;
                               faceIndex : integer;
                               var _face : TT_Face ) : TT_Error;

  (*****************************************************************)
  (*  Return face properties in 'prop'                             *)
  (*                                                               *)
  function TT_Get_Face_Properties( _face    : TT_Face;
                                   var prop : TT_Face_Properties ) : TT_Error;

  (*****************************************************************)
  (*  Set face's generic pointer                                   *)
  (*                                                               *)
  function TT_Set_Face_Pointer( _face : TT_Face;
                                data  : Pointer ) : TT_Error;

  (*****************************************************************)
  (*  Get face's generic pointer                                   *)
  (*                                                               *)
  function TT_Get_Face_Pointer( _face : TT_Face ) : Pointer;

  (*****************************************************************)
  (*  close a given face object. This releases all child objects   *)
  (*  like instances and glyphs                                    *)
  (*                                                               *)
  function TT_Close_Face( _face : TT_Face ) : TT_Error;

  (***********************************************************************)
  (*                                                                     *)
  (*                  Instance management functions                      *)
  (*                                                                     *)
  (***********************************************************************)

  (*****************************************************************)
  (*  open a new face instance and return a handle in '_ins'       *)
  (*                                                               *)
  function TT_New_Instance( _face    : TT_Face;
                            var _ins : TT_Instance ) : TT_Error;

  (*****************************************************************)
  (*  set an instance's device resolutions, expressed in dpi       *)
  (*                                                               *)
  function TT_Set_Instance_Resolutions( _ins         : TT_Instance;
                                        x_resolution : Integer;
                                        y_resolution : Integer ) : TT_Error;

  (*****************************************************************)
  (*  set an instance's point size (assumes width==height)         *)
  (*                                                               *)
  function TT_Set_Instance_PointSize( _ins      : TT_Instance;
                                      pointsize : Integer ) : TT_Error;

  (*****************************************************************)
  (*  set an instance's point size (assumes width==height)         *)
  (*                                                               *)
  function TT_Set_Instance_CharSize( _ins     : TT_Instance;
                                     charsize : Integer ) : TT_Error;

  (*****************************************************************)
  (*  set an instance's point size (assumes width==height)         *)
  (*                                                               *)
  function TT_Set_Instance_CharSizes( _ins      : TT_Instance;
                                      charsizex : Integer;
                                      charsizey : Integer ) : TT_Error;

  (*****************************************************************)
  (*  set an instance's height and width, expressed in pixels      *)
  (*                                                               *)
  function TT_Set_Instance_PixelSizes( _ins      : TT_Instance;
                                       pixelX    : Integer;
                                       pixelY    : Integer;
                                       pointsize : Integer ) : TT_Error;

  (*****************************************************************)
  (*  set an instance's height and width, expressed in pixels      *)
  (*                                                               *)

  (*****************************************************************)
  (*  the core truetype engine doesn't provide _direct_ support    *)
  (*  for rotation or stretching. This means that the transforms   *)
  (*  must be applied on the glyph outline by a higher-level       *)
  (*  library or the client application. However, we use two flags *)
  (*  to notice the TrueType hinter that the glyphs will be        *)
  (*  transformed later.                                           *)
  (*                                                               *)
  (*    rotated   : set if the glyphs are to be rotated            *)
  (*    distorted : set if the glyphs are to be distorted          *)
  (*                                                               *)
  (*  an application is any transform that doesn't keep distances  *)
  (*  constants. skewing and stretching are examples of distorsion *)
  (*                                                               *)
  function TT_Set_Instance_Transforms( _ins      : TT_Instance;
                                       rotated   : Boolean;
                                       distorted : Boolean ) : TT_Error;

  (*****************************************************************)
  (*  Return instance metrics in 'm'                               *)
  (*                                                               *)
  function TT_Get_Instance_Metrics( _ins  : TT_Instance;
                                    var m : TT_Instance_Metrics ) : TT_Error;

  (*****************************************************************)
  (*  Set instance generic pointer                                 *)
  (*                                                               *)
  function TT_Set_Instance_Pointer( _ins : TT_Instance;
                                    data : Pointer ) : TT_Error;

  (*****************************************************************)
  (*  Get instance generic pointer                                 *)
  (*                                                               *)
  function TT_Get_Instance_Pointer( _ins : TT_Instance ) : Pointer;

  (*****************************************************************)
  (*  Close an instance                                            *)
  (*                                                               *)
  function TT_Done_Instance( _ins : TT_Instance ) : TT_Error;

  (***********************************************************************)
  (*                                                                     *)
  (*                  Glyph management functions                         *)
  (*                                                                     *)
  (***********************************************************************)

  (*****************************************************************)
  (*  Create a new glyph container, return a handle in '_glyph'    *)
  (*                                                               *)
  function TT_New_Glyph( _face      : TT_Face;
                         var _glyph : TT_Glyph ) : TT_Error;

  (*****************************************************************)
  (*  Releases a glyph container                                   *)
  (*                                                               *)
  function TT_Done_Glyph( _glyph : TT_Glyph ) : TT_Error;

  (*****************************************************************)
  (*  Load a glyph inside a container                              *)
  (*                                                               *)
  function  TT_Load_Glyph( _instance   : TT_Instance;
                           _glyph      : TT_Glyph;
                           glyph_index : Word;
                           load_flags  : Integer ) : TT_Error;

const
  TT_Load_Scale_Glyph = 1;  (* ask the loader to scale the glyph  *)
                            (* to the current pointsize/transform *)

  TT_Load_Hint_Glyph  = 2;  (* when scaling is on, ask the loader *)
                            (* to hint the glyph too..            *)

  TT_Load_Debug       = 16;

  TT_Load_Default     = TT_Load_Scale_Glyph or
                        TT_Load_Hint_Glyph;


  (*****************************************************************)
  (*  Get a glyph's outline                                        *)
  (*                                                               *)
  function TT_Get_Glyph_Outline( _glyph      : TT_Glyph;
                                 var outline : TT_Outline ) : TT_Error;

  (*****************************************************************)
  (*  Get a glyph's metrics                                        *)
  (*                                                               *)
  function TT_Get_Glyph_Metrics( _glyph       : TT_Glyph;
                                 var gmetrics : TT_Glyph_Metrics ) : TT_Error;

  (*****************************************************************)
  (*  Get a glyph's big metrics                                    *)
  (*                                                               *)
  function TT_Get_Glyph_Big_Metrics( _glyph       : TT_Glyph;
                                     var gmetrics : TT_Big_Glyph_Metrics
                                   ) : TT_Error;

  (*****************************************************************)
  (*  Render a glyph's bitmap                                      *)
  (*                                                               *)
  function TT_Get_Glyph_Bitmap( _glyph   : TT_Glyph;
                                var map  : TT_Raster_Map;
                                x_offset : TT_F26Dot6;
                                y_offset : TT_F26Dot6 ) : TT_Error;

  (*****************************************************************)
  (*  Render a glyph's pixmap (i.e. smoothed glyph )               *)
  (*                                                               *)
  function TT_Get_Glyph_Pixmap( _glyph   : TT_Glyph;
                                var map  : TT_Raster_Map;
                                x_offset : TT_F26Dot6;
                                y_offset : TT_F26Dot6 ) : TT_Error;

  (***********************************************************************)
  (*                                                                     *)
  (*                      Outline functions                              *)
  (*                                                                     *)
  (***********************************************************************)

  (*****************************************************************)
  (*  Apply translation to an outline                              *)
  (*                                                               *)
  function TT_Translate_Outline( var out : TT_Outline;
                                 x, y    : TT_F26Dot6 ) : TT_Error;

  (*****************************************************************)
  (*  Apply a 2x2 transform to an outline                          *)
  (*                                                               *)
  function TT_Transform_Outline( var out : TT_Outline;
                                 var mat : TT_Matrix ) : TT_Error;

  (*****************************************************************)
  (*  Apply a 2x2 transform to a vector                            *)
  (*                                                               *)
  function TT_Transform_Vector( var x, y : TT_F26Dot6;
                                var mat  : TT_Matrix ) : TT_Error;

  (*****************************************************************)
  (*  Render an outline into a bitmap                              *)
  (*                                                               *)
  function TT_Get_Outline_Bitmap( var out : TT_Outline;
                                  var map : TT_raster_Map ) : TT_Error;

  (*****************************************************************)
  (*  Render an outline into a pixmap                              *)
  (*                                                               *)
  function TT_Get_Outline_Pixmap( var out : TT_Outline;
                                  var map : TT_raster_Map ) : TT_Error;


  (*****************************************************************)
  (*  Get an outline's bounding box                                *)
  (*                                                               *)
  function TT_Get_Outline_BBox( var out  : TT_Outline;
                                var bbox : TT_Bbox     ) : TT_Error;

  (*****************************************************************)
  (*  Create a new glyph outline                                   *)
  (*                                                               *)
  function TT_New_Outline( n_points   : integer;
                           n_contours : integer;
                           var out    : TT_Outline ) : TT_Error;

  (*****************************************************************)
  (*  Copy a glyph outline into another one                        *)
  (*                                                               *)
  function TT_Copy_Outline( var source : TT_Outline;
                            var target : TT_Outline ) : TT_Error;

  (*****************************************************************)
  (*  Clone a given outline. This will create the outline, then    *)
  (*  copy the source in it                                        *)
  (*                                                               *)
  function TT_Clone_Outline( var source : TT_Outline;
                             var target : TT_Outline ) : TT_Error;

  (*****************************************************************)
  (*  Discards a glyph outline                                     *)
  (*                                                               *)
  function TT_Done_Outline( var out : TT_Outline ) : TT_Error;

  (***********************************************************************)
  (*                                                                     *)
  (*                     Character Mapping support                       *)
  (*                                                                     *)
  (***********************************************************************)

  (*****************************************************************)
  (*  Get a face's number of character maps                        *)
  (*                                                               *)
  function  TT_Get_CharMap_Count( face : TT_Face ) : integer;

  (*****************************************************************)
  (*  Get a given char. map's ID in a face                         *)
  (*                                                               *)
  function  TT_Get_CharMap_ID( face         : TT_Face;
                               charmapIndex : integer;
                               var platform : integer;
                               var encoding : integer ) : TT_Error;

  (*****************************************************************)
  (*  Get a handle to a given char. map                            *)
  (*                                                               *)
  function  TT_Get_CharMap( face         : TT_Face;
                            charmapIndex : integer;
                            var charMap  : TT_CharMap ) : TT_Error;

  (*****************************************************************)
  (*  Translate from char. code to glyph index                     *)
  (*                                                               *)
  function  TT_Char_Index( charmap  : TT_CharMap;
                           charCode : Longint ) : Word;

  (***********************************************************************)
  (*                                                                     *)
  (*                        Names Table support                          *)
  (*                                                                     *)
  (***********************************************************************)

  (*****************************************************************)
  (*  Return number of name table entries                          *)
  (*                                                               *)
  function  TT_Get_Name_Count( face : TT_Face ) : integer;

  (*****************************************************************)
  (*  Return the ID of a given name table entry                    *)
  (*                                                               *)
  function  TT_Get_Name_ID( face         : TT_Face;
                            nameIndex    : integer;
                            var platform : integer;
                            var encoding : integer;
                            var language : integer;
                            var nameid   : integer ) : TT_Error;

  (*****************************************************************)
  (*  Return a given name table string                             *)
  (*                                                               *)
  function  TT_Get_Name_String( face      : TT_Face;
                                nameIndex : integer;
                                var str   : Pointer;
                                var len   : integer ) : TT_Error;

  (***********************************************************************)
  (*                                                                     *)
  (*                        Font Storage Access                          *)
  (*                                                                     *)
  (***********************************************************************)

  (*****************************************************************)
  (*  Access font data and copies it into user block               *)
  (*                                                               *)
  function  TT_Get_Font_Data( face       : TT_Face;
                              tableTag   : Longint;
                              offset     : Longint;
                              var buffer;
                              var length : longint ) : TT_Error;

implementation

uses
  TTTypes,
  TTError,
  TTCalc,
  TTMemory,
  TTTables,
  TTCache,
  TTFile,
  TTCMap,
  TTObjs,
  TTLoad,
  TTGLoad,
  TTRaster,
  TTInterp;

var
  raster_palette : TT_Gray_Palette;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Init_FreeType : TT_Error;
  begin
    if TTMemory_Init or
       TTCache_Init  or
       TTFile_Init   or
       TTObjs_Init   or
       TTRaster_Init then
    begin
      TT_Init_FreeType := error;
      exit;
    end;

    TT_Init_FreeType := TT_Err_Ok;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  procedure TT_Done_FreeType;
  begin
    TTRaster_Done;
    TTObjs_Done;
    TTFile_Done;
    TTCache_Done;
    TTMemory_Done;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Set_Raster_Palette( palette : TT_Gray_Palette ) : TT_Error;
  begin
    raster_palette := palette;
    TT_Set_Raster_Palette := TT_Err_Ok;
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Open_Face( fontname  : string;
                         var _face : TT_Face ) : TT_Error;
  var
    input        : TFont_Input;
  begin
    input.fontIndex := 0;

    if TT_Open_Stream( fontname, input.stream ) then
    begin
      TT_Open_Face    := error;
      exit;
    end;

    Cache_New( face_cache, Pointer(_face), @input );

    TT_Open_Face := error;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Open_Collection( fontname  : string;
                               faceIndex : integer;
                               var _face : TT_Face ) : TT_Error;
  var
    input : TFont_Input;
  begin
    input.fontIndex := faceIndex;

    if TT_Open_Stream( fontname, input.stream ) then
    begin
      TT_Open_Collection := error;
      exit;
    end;

    Cache_New( face_cache, Pointer(_face), @input );

    TT_Open_Collection := error;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Get_Face_Properties( _face    : TT_Face;
                                   var prop : TT_Face_Properties ) : TT_Error;
  var
    face : PFace;
  begin
    face := _face.z;
    if face <> nil then
    begin
      with prop do
      begin
        num_Glyphs   := face^.numGlyphs;
        max_Points   := face^.maxPoints;
        max_Contours := face^.maxContours;
        max_Faces    := face^.ttcHeader.dirCount;
        header       := @face^.fontHeader;
        horizontal   := @face^.horizontalHeader;

        if face^.verticalInfo then
          vertical := @face^.verticalHeader
        else
          vertical := nil;

        os2          := @face^.os2;
        postscript   := @face^.postscript;
      end;
      TT_Get_Face_Properties := TT_Err_Ok;
    end
    else
      TT_Get_Face_Properties := TT_Err_Invalid_Face_Handle;
  end;


  (*****************************************************************)
  (*  Set face's generic pointer                                   *)
  (*                                                               *)
  function TT_Set_Face_Pointer( _face : TT_Face;
                                data  : Pointer ) : TT_Error;
  var
    face :PFace;
  begin
    face := PFace(_face.z);
    if face <> nil then
      begin
        face^.generic       := data;
        TT_Set_Face_Pointer := TT_Err_Ok;
      end
    else
      TT_Set_Face_Pointer := TT_Err_Invalid_Face_Handle;
  end;

  (*****************************************************************)
  (*  Get face's generic pointer                                   *)
  (*                                                               *)
  function TT_Get_Face_Pointer( _face : TT_Face ) : Pointer;
  var
    face : PFace;
  begin
    face := PFace(_face.z);
    if face <> nil then
      TT_Get_Face_Pointer := face^.generic
    else
      TT_get_Face_Pointer := nil;
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Close_Face( _face : TT_Face ) : TT_Error;
  var
    face : PFace;
  begin
    face := _face.z;
    if face <> nil then
      begin
        error := TT_Err_Ok;
        (* Note : the stream is closed by the face destructor !! *)
        Cache_Done( face_cache, _face.z );
        TT_Close_Face := error;
      end
    else
      TT_Close_Face := TT_Err_Invalid_Face_Handle;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_New_Instance( _face    : TT_Face;
                            var _ins : TT_Instance ) : TT_Error;
  var
    face : PFace;
    ins  : PInstance;
  begin
    face := _face.z;
    if face <> nil then
      begin
        error := TT_Err_Ok;
        if not Cache_New( face^.instances, _ins.z, face ) then
          Instance_Init( _ins.z );
        TT_New_Instance := error;
      end
    else
      TT_New_Instance := TT_Err_Invalid_Face_Handle;
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Set_Instance_Resolutions( _ins         : TT_Instance;
                                        x_resolution : Integer;
                                        y_resolution : Integer ) : TT_Error;
  var
    ins : PInstance;
  begin
    ins := _ins.z;
    if ins <> nil then
      begin
        ins^.metrics.x_resolution   := x_resolution;
        ins^.metrics.y_resolution   := y_resolution;
        ins^.valid                  := False;
        TT_Set_Instance_Resolutions := TT_Err_Ok;
      end
    else
        TT_Set_Instance_Resolutions := TT_Err_Invalid_Instance_Handle;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Set_Instance_CharSize( _ins     : TT_Instance;
                                     charsize : Integer ) : TT_Error;
  var
    ins : PInstance;
  begin
    TT_Set_Instance_CharSize :=
      TT_Set_Instance_CharSizes( _ins, charsize, charsize );
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Set_Instance_CharSizes( _ins      : TT_Instance;
                                      charsizex : Integer;
                                      charsizey : Integer ) : TT_Error;
  var
    ins : PInstance;
  begin
    if (charsizex < 1*64) or (charsizey < 1*64) then
      begin
        TT_Set_Instance_CharSizes := TT_Err_Bad_Argument;
        exit;
      end;

    ins := _ins.z;
    if ins <> nil then
      begin
        with ins^.metrics do
        begin
          x_scale1 := ( Long(charsizex) * x_resolution ) div 72;
          x_scale2 := ins^.owner^.fontHeader.units_per_EM;

          y_scale1 := ( Long(charsizey) * y_resolution ) div 72;
          y_scale2 := x_scale2;

          if ins^.owner^.fontHeader.flags and 8 <> 0 then
          begin
            x_scale1 := (x_scale1 + 32) and -64;
            y_scale1 := (y_scale1 + 32) and -64;
          end;

          x_ppem   := x_scale1 div 64;
          y_ppem   := y_scale1 div 64;
        end;

        if charsizex > charsizey then
          ins^.metrics.pointsize  := charsizex
        else
          ins^.metrics.pointsize  := charsizey;

        ins^.valid                := False;
        TT_Set_Instance_CharSizes := TT_Err_Ok;
      end
    else
      TT_Set_Instance_CharSizes := TT_Err_Invalid_Instance_Handle;
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Set_Instance_PointSize( _ins      : TT_Instance;
                                      pointsize : integer ) : TT_Error;
  begin
    TT_Set_Instance_PointSize :=
      TT_Set_Instance_CharSize( _ins, pointsize*64 );
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Set_Instance_PixelSizes( _ins      : TT_Instance;
                                       pixelX    : Integer;
                                       pixelY    : Integer;
                                       pointsize : Integer ) : TT_Error;
  var
    ins : PInstance;
  begin
    ins := _ins.z;
    if ins <> nil then
      begin
        ins^.metrics.x_ppem    := pixelX;
        ins^.metrics.y_ppem    := pixelY;
        ins^.metrics.pointSize := pointsize;

        ins^.metrics.x_scale1  := ins^.metrics.x_ppem * 64;
        ins^.metrics.x_scale2  := ins^.owner^.fontHeader.units_per_EM;
        ins^.metrics.y_scale1  := ins^.metrics.y_ppem * 64;
        ins^.metrics.y_scale2  := ins^.metrics.x_scale2;

        ins^.valid             := false;

        TT_Set_Instance_PixelSizes := TT_Err_Ok;
      end
    else
      TT_Set_Instance_PixelSizes := TT_Err_Invalid_Instance_Handle;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Set_Instance_Transforms( _ins      : TT_Instance;
                                       rotated   : Boolean;
                                       distorted : Boolean ) : TT_Error;
  var
    ins : PInstance;
  begin
    ins := _ins.z;
    if ins <> nil then
      begin
        ins^.metrics.rotated   := rotated;
        ins^.metrics.stretched := distorted;
        TT_Set_Instance_Transforms := TT_Err_Ok;
      end
    else
      TT_Set_Instance_Transforms := TT_Err_Invalid_Instance_Handle;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Get_Instance_Metrics( _ins  : TT_Instance;
                                    var m : TT_Instance_Metrics ) : TT_Error;
  var
    ins : PInstance;
  begin
    ins := _ins.z;
    if ins <> nil then
      begin
        if not ins^.valid then
          if Instance_Reset( ins, False ) then
          begin
            TT_Get_Instance_Metrics := error;
            exit;
          end;

        with m do
        begin
          pointSize    := ins^.metrics.pointSize;

          x_scale      := MulDiv_Round( $10000,
                                        ins^.metrics.x_scale1,
                                        ins^.metrics.x_scale2 );

          y_scale      := MulDiv_Round( $10000,
                                        ins^.metrics.y_scale1,
                                        ins^.metrics.y_scale2 );

          x_resolution := ins^.metrics.x_resolution;
          y_resolution := ins^.metrics.y_resolution;

          x_ppem       := ins^.metrics.x_ppem;
          y_ppem       := ins^.metrics.y_ppem;

          TT_Get_Instance_Metrics := TT_Err_Ok;
        end;
      end
    else
      TT_Get_Instance_Metrics := TT_Err_Invalid_Instance_Handle;
  end;


  (*****************************************************************)
  (*  Set instance generic pointer                                 *)
  (*                                                               *)
  function TT_Set_Instance_Pointer( _ins : TT_Instance;
                                    data : Pointer ) : TT_Error;
  var
    ins : PInstance;
  begin
    ins := PInstance(_ins.z);
    if ins <> nil then
      begin
        ins^.generic := data;
        TT_Set_Instance_Pointer := TT_Err_Ok;
      end
    else
      TT_Set_Instance_Pointer := TT_Err_Invalid_Instance_Handle;
  end;

  (*****************************************************************)
  (*  Get instance generic pointer                                 *)
  (*                                                               *)
  function TT_Get_Instance_Pointer( _ins : TT_Instance ) : Pointer;
  var
    ins : PInstance;
  begin
    ins := PInstance(_ins.z);
    if ins <> nil then
      TT_Get_Instance_Pointer := ins^.generic
    else
      TT_Get_Instance_Pointer := nil;
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Done_Instance( _ins : TT_Instance ) : TT_Error;
  var
    ins : PInstance;
  begin
    ins := PInstance(_ins.z);
    if ins <> nil then
      begin
        error := TT_Err_Ok;
        Cache_Done( ins^.owner^.instances, ins );
        TT_Done_Instance := error;
      end
    else
      TT_Done_Instance := TT_Err_Invalid_Instance_Handle;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_New_Glyph( _face      : TT_Face;
                         var _glyph : TT_Glyph ) : TT_Error;
  var
    face : PFace;
  begin
    face := PFace(_face.z);
    if face <> nil then
      begin
        error := TT_Err_Ok;
        Cache_New( face^.glyphs, _glyph.z, _face.z );
        TT_New_Glyph := error;
      end
    else
      TT_New_Glyph := TT_Err_Invalid_Face_Handle;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Done_Glyph( _glyph : TT_Glyph ) : TT_Error;
  var
    glyph : PGlyph;
  begin
    glyph := PGlyph(_glyph.z);
    if glyph <> nil then
      begin
        error := TT_Err_Ok;
        Cache_Done( glyph^.face^.glyphs, glyph );
        TT_Done_Glyph := error;
      end
    else
      TT_Done_Glyph := TT_Err_Invalid_Glyph_Handle;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function  TT_Load_Glyph( _instance   : TT_Instance;
                           _glyph      : TT_Glyph;
                           glyph_index : Word;
                           load_flags  : Integer ) : TT_Error;
  var
    ins   : PInstance;
    glyph : PGlyph;
  begin
    ins := PInstance(_instance.z);
    if ins = nil then
      begin
        TT_Load_Glyph := TT_Err_Invalid_Instance_Handle;
        exit;
      end;

    glyph := PGlyph(_glyph.z);
    if glyph = nil then
      begin
        TT_Load_Glyph := TT_Err_Invalid_Glyph_Handle;
        exit;
      end;

    if ins^.owner <> glyph^.face then
      begin
        TT_Load_Glyph := TT_Err_Invalid_Face_Handle;
        exit;
      end;

    if not ins^.valid then
      if Instance_Reset( ins, False ) then
      begin
        TT_Load_Glyph := error;
        exit;
      end;

    error := TT_Err_Ok;
    Load_TrueType_Glyph( ins, glyph, glyph_index, load_flags );
    TT_Load_Glyph := error;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Get_Glyph_Outline( _glyph      : TT_Glyph;
                                 var outline : TT_Outline ) : TT_Error;
  var
    glyph : PGlyph;
  begin
    glyph := PGlyph(_glyph.z);
    if glyph <> nil then
      begin
        outline       := glyph^.outline;
        outline.owner := false;
        TT_Get_Glyph_Outline := TT_Err_Ok;
      end
    else
      TT_Get_Glyph_Outline := TT_Err_Invalid_Glyph_Handle;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Get_Glyph_Metrics( _glyph       : TT_Glyph;
                                 var gmetrics : TT_Glyph_Metrics ) : TT_Error;
  var
    glyph : PGlyph;
  begin
    glyph := PGlyph(_glyph.z);
    if glyph <> nil then
      begin
        gmetrics.bbox     := glyph^.metrics.bbox;
        gmetrics.bearingX := glyph^.metrics.horiBearingX;
        gmetrics.bearingY := glyph^.metrics.horiBearingY;
        gmetrics.advance  := glyph^.metrics.horiAdvance;
        TT_Get_Glyph_Metrics := TT_Err_Ok;
      end
    else
      TT_Get_Glyph_Metrics := TT_Err_Invalid_Glyph_Handle;
  end;

  (*****************************************************************)
  (*  Get a glyph's big metrics                                    *)
  (*                                                               *)
  function TT_Get_Glyph_Big_Metrics( _glyph       : TT_Glyph;
                                     var gmetrics : TT_Big_Glyph_Metrics
                                   ) : TT_Error;
  var
    glyph : PGlyph;
  begin
    glyph := PGlyph(_glyph.z);
    if glyph <> nil then
      begin
        gmetrics := glyph^.metrics;
        TT_Get_Glyph_Big_Metrics := TT_Err_Ok;
      end
    else
      TT_Get_Glyph_Big_Metrics := TT_Err_Invalid_Glyph_Handle;
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Get_Glyph_Bitmap( _glyph   : TT_Glyph;
                                var map  : TT_raster_Map;
                                x_offset : TT_F26Dot6;
                                y_offset : TT_F26Dot6 ) : TT_Error;
  var
    glyph   : PGlyph;
    outline : TT_Outline;
    n       : Int;
  begin
    glyph := _glyph.z;
    if glyph <> nil then
      begin
        outline := glyph^.outline;
        (* XXX: for now, we only use dropout mode #2 *)
        outline.dropout_mode := 2;

        TT_Translate_Outline( outline, x_offset, y_offset );
        TT_Get_Glyph_Bitmap := TT_Get_Outline_Bitmap( outline, map );
        TT_Translate_Outline( outline, -x_offset, -y_offset );
      end
    else
      TT_Get_Glyph_Bitmap := TT_Err_Invalid_Glyph_Handle;
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Get_Glyph_Pixmap( _glyph   : TT_Glyph;
                                var map  : TT_raster_Map;
                                x_offset : TT_F26Dot6;
                                y_offset : TT_F26Dot6 ) : TT_Error;
  var
    glyph   : PGlyph;
    outline : TT_Outline;
    n       : Int;
  begin
    glyph := _glyph.z;
    if glyph <> nil then
      begin
        outline := glyph^.outline;
        (* XXX: for now, we only use dropout mode #2 *)
        outline.dropout_mode := 2;

        TT_translate_Outline( outline, x_offset, y_offset );
        TT_Get_Glyph_Pixmap := TT_Get_Outline_Pixmap( outline, map );
        TT_translate_Outline( outline, -x_offset, -y_offset );
      end
    else
      TT_Get_Glyph_Pixmap := TT_Err_Invalid_Glyph_Handle;
  end;

  (*****************************************************************)
  (*  Create a new glyph outline                                   *)
  (*                                                               *)
  function TT_New_Outline( n_points   : integer;
                           n_contours : integer;
                           var out    : TT_Outline ) : TT_Error;
  label
    Fail;
  begin
    out.n_points   := n_points;
    out.n_contours := n_contours;
    out.points     := nil;
    out.flags      := nil;
    out.conEnds    := nil;
    out.owner      := true;

    if Alloc( Pointer(out.points), 2*n_points*sizeof(TT_Pos) ) or
       Alloc( Pointer(out.flags),    n_points*sizeof(Byte)   ) or
       Alloc( Pointer(out.conEnds),  n_contours*sizeof(Short) ) then
      goto Fail;

    TT_New_Outline := TT_Err_Ok;
    exit;

  Fail:
    TT_Done_Outline( out );
    TT_New_Outline := error;
  end;

  (*****************************************************************)
  (*  Copy a glyph outline into another one                        *)
  (*                                                               *)
  function TT_Copy_Outline( var source : TT_Outline;
                            var target : TT_Outline ) : TT_Error;
  begin
    if (source.n_points   = target.n_points) and
       (source.n_contours = target.n_contours) then
      begin
        move( source.points^, target.points^,  2*source.n_points*4 );
        move( source.flags^,  target.flags^,   source.n_points );
        move( source.conEnds^,target.conEnds^, source.n_contours*2 );
      end
    else
      TT_Copy_Outline := TT_Err_Invalid_Argument;
  end;

  (*****************************************************************)
  (*  Clone a given outline. This will create the outline, then    *)
  (*  copy the source in it                                        *)
  (*                                                               *)
  function TT_Clone_Outline( var source : TT_Outline;
                             var target : TT_Outline ) : TT_Error;
  begin
    error := TT_New_Outline( source.n_points, source.n_contours, target );
    if error = TT_Err_Ok then
      TT_Copy_Outline( source, target );

    TT_Clone_Outline := error;
  end;

  (*****************************************************************)
  (*  Discards a glyph outline                                     *)
  (*                                                               *)
  function TT_Done_Outline( var out : TT_Outline ) : TT_Error;
  begin
    if out.owner then
      begin
        Free( Pointer(out.conEnds) );
        Free( Pointer(out.flags) );
        Free( Pointer(out.points) );
        out.n_points   := 0;
        out.n_contours := 0;
        TT_Done_Outline := TT_Err_Ok;
      end
    else
      TT_Done_Outline := TT_Err_Invalid_Argument;
  end;

  (*****************************************************************)
  (*  Render an outline into a bitmap                              *)
  (*                                                               *)
  function TT_Get_Outline_Bitmap( var out : TT_Outline;
                                  var map : TT_raster_Map ) : TT_Error;
  begin
    if Render_Glyph( out, map ) then
      TT_Get_Outline_Bitmap := error
    else
      TT_Get_Outline_Bitmap := TT_Err_Ok;
  end;

  (*****************************************************************)
  (*  Render an outline into a pixmap                              *)
  (*                                                               *)
  function TT_Get_Outline_Pixmap( var out : TT_Outline;
                                  var map : TT_raster_Map ) : TT_Error;
  begin
    if Render_Gray_Glyph( out, map ) then
      TT_Get_Outline_Pixmap := error
    else
      TT_Get_Outline_Pixmap := TT_Err_Ok;
  end;

  (*****************************************************************)
  (*  Compute an outline's bounding box                            *)
  (*                                                               *)
  function TT_Get_Outline_BBox( var out  : TT_Outline;
                                var bbox : TT_Bbox     ) : TT_Error;
  var
    x,    y,
    xMin, yMin,
    xMax, yMax : TT_Pos;
    n          : Int;
  begin

    with bbox do
    begin
      xMin := $7FFFFFFF;
      xMax := $80000000;
      yMin := $7FFFFFFF;
      yMax := $80000000;

      for n := 0 to out.n_points-1 do
      begin
        x := out.points^[n].x;
        if x < xMin then xMin := x;
        if x > xMax then xMax := x;
        y := out.points^[n].y;
        if y < yMin then yMin := y;
        if y > yMax then yMax := y;
      end;
    end;

    TT_Get_Outline_BBox := TT_Err_Ok;
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Translate_Outline( var out : TT_Outline;
                                 x, y    : TT_F26Dot6 ) : TT_Error;
  var
    n : integer;
  begin
    if x <> 0 then
      for n := 0 to out.n_points-1 do
        inc( out.points^[n].x, x );

    if y <> 0 then
      for n := 0 to out.n_points-1 do
        inc( out.points^[n].y, y );

    TT_Translate_Outline := TT_Err_Ok;
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Transform_Outline( var out : TT_Outline;
                                 var mat : TT_Matrix ) : TT_Error;
  var
    n            : integer;
    x, y, nx, ny : TT_F26Dot6;
  begin
    for n := 0 to out.n_points-1 do
    begin
      x  := out.points^[n].x;
      y  := out.points^[n].y;

      nx := MulDiv_Round( mat.xx, x, $10000 ) +
            MulDiv_Round( mat.xy, y, $10000 );

      ny := MulDiv_ROund( mat.yx, x, $10000 ) +
            MulDiv_Round( mat.yy, y, $10000 );

      out.points^[n].x := nx;
      out.points^[n].y := ny;
    end;

    TT_Transform_Outline := TT_Err_Ok;
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function TT_Transform_Vector( var x, y : TT_F26Dot6;
                                var mat  : TT_Matrix ) : TT_Error;
  var
    nx, ny : TT_F26Dot6;
  begin
    nx := MulDiv_Round( mat.xx, x, $10000 ) +
          MulDiv_Round( mat.xy, y, $10000 );

    ny := MulDiv_Round( mat.yx, x, $10000 ) +
          MulDiv_Round( mat.yy, y, $10000 );

    x := nx;
    y := ny;

    TT_Transform_Vector := TT_Err_Ok;
  end;

  (***********************************************************************)
  (*                                                                     *)
  (*                     Character Mapping support                       *)
  (*                                                                     *)
  (***********************************************************************)

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function  TT_Get_CharMap_Count( face : TT_Face ) : integer;
  var
    faze : PFace;
  begin
    faze := PFace(face.z);
    if faze = nil then
      TT_Get_CharMap_Count := -1
    else
      TT_Get_CharMap_Count := faze^.numCMaps;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function  TT_Get_CharMap_ID( face         : TT_Face;
                               charmapIndex : integer;
                               var platform : integer;
                               var encoding : integer ) : TT_Error;
  var
    faze : PFace;
    cmap : PCMapTable;
  begin
    faze := PFace(face.z);
    if faze = nil then
    begin
      TT_Get_CharMap_ID := TT_Err_Invalid_Face_Handle;
      exit;
    end;

    if (charmapIndex < 0) or (charmapIndex >= faze^.numCMaps) then
    begin
      TT_Get_CharMap_ID := TT_Err_Invalid_Argument;
      exit;
    end;

    cmap     := @faze^.cMaps^[charmapIndex];
    platform := cmap^.platformID;
    encoding := cmap^.platformEncodingID;

    TT_Get_CharMap_ID := TT_Err_Ok;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function  TT_Get_CharMap( face         : TT_Face;
                            charmapIndex : integer;
                            var charMap  : TT_CharMap ) : TT_Error;
  var
    faze : PFace;
  begin
    faze := PFace(face.z);
    if faze = nil then
    begin
      TT_Get_CharMap := TT_Err_Invalid_Face_Handle;
      exit;
    end;

    if (charmapIndex < 0) or (charmapIndex >= faze^.numCMaps) then
    begin
      TT_Get_CharMap := TT_Err_Invalid_Argument;
      exit;
    end;

    charmap.z := @faze^.cMaps^[charmapIndex];

    TT_Get_CharMap := TT_Err_Ok;
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function  TT_Char_Index( charmap  : TT_CharMap;
                           charCode : Longint ) : Word;
  begin
    TT_Char_Index := CharMap_Index( PCMapTable(charmap.z)^, charCode );
  end;

  (***********************************************************************)
  (*                                                                     *)
  (*                        Names Table support                          *)
  (*                                                                     *)
  (***********************************************************************)

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function  TT_Get_Name_Count( face : TT_Face ) : integer;
  var
    faze : PFace;
  begin
    TT_Get_Name_Count := 0;

    faze := PFace( face.z );
    if faze = nil then exit;

    TT_Get_Name_Count := faze^.nameTable.numNameRecords;
  end;


  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function  TT_Get_Name_ID( face         : TT_Face;
                            nameIndex    : integer;
                            var platform : integer;
                            var encoding : integer;
                            var language : integer;
                            var nameid   : integer ) : TT_Error;
  var
    faze  : PFace;
    table : PName_Table;
    rec   : PName_Record;
  label
    Fail;
  begin
    faze := PFace( face.z );
    if faze = nil then
    begin
      TT_Get_Name_Id := TT_Err_Invalid_Face_Handle;
      goto Fail;
    end;

    table := @faze^.nameTable;
    if (nameIndex < 0) or (nameIndex > table^.numNameRecords) then
    begin
      TT_Get_Name_Id := TT_Err_Bad_Argument;
      goto Fail;
    end;

    rec := @table^.names^[nameIndex];

    platform := rec^.platformID;
    encoding := rec^.encodingID;
    language := rec^.languageID;
    nameid   := rec^.nameID;

    TT_Get_Name_ID := TT_Err_Ok;
    exit;

  Fail:
    platform := -1;
    encoding := -1;
    language := -1;
    nameid   := -1;
  end;

  (*****************************************************************)
  (*                                                               *)
  (*                                                               *)
  function  TT_Get_Name_String( face      : TT_Face;
                                nameIndex : integer;
                                var str   : Pointer;
                                var len   : integer ) : TT_Error;
  var
    faze  : PFace;
    table : PName_Table;
    rec   : PName_Record;
  label
    Fail;
  begin
    faze := PFace( face.z );
    if faze = nil then
    begin
      TT_Get_Name_String := TT_Err_Invalid_Face_Handle;
      goto Fail;
    end;

    table := @faze^.nameTable;
    if (nameIndex < 0) or (nameIndex > table^.numNameRecords) then
    begin
      TT_Get_Name_String := TT_Err_Bad_Argument;
      goto Fail;
    end;

    rec := @table^.names^[nameIndex];

    str := @table^.storage^[rec^.offset];
    len := rec^.length;

    TT_Get_Name_String := TT_Err_Ok;
    exit;

  Fail:
    str := nil;
    len := 0;
  end;


  (*****************************************************************)
  (*  Access font data and copies it into user block               *)
  (*                                                               *)
  function  TT_Get_Font_Data( face       : TT_Face;
                              tableTag   : Longint;
                              offset     : Longint;
                              var buffer;
                              var length : longint ) : TT_Error;
  var
    faze : PFace;
  begin
    faze := PFace(face.z);
    if faze = nil then
      begin
        TT_Get_Font_Data := TT_Err_Invalid_Face_Handle;
        length := 0;
      end
    else
      begin
        TT_Get_Font_Data := TT_Err_Ok;
        if Load_TrueType_Any( faze, tableTag, offset, buffer, length ) then
          TT_Get_Font_Data := error;
      end;
  end;


end.

