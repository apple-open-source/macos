(*******************************************************************
 *
 *  TTTables.Pas                                              1.2
 *
 *    TrueType Tables declarations
 *
 *  Copyright 1996 David Turner, Robert Wilhelm and Werner Lemberg
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 *
 *  Difference between 1.1 and 1.2 :
 *
 *  - TTTables now only contains the declarations of the
 *    TrueType tables.
 *
 *  - Instance, Resident and Execution context declarations
 *    were moved to TTObjs
 *
 *  - Tables loaders were moved to the new TTLoad component
 *
 ******************************************************************)

Unit TTTables;

interface

uses FreeType, TTTypes;

(***************************************************************************)
(*                                                                         *)
(*                      TrueType Table Types                               *)
(*                                                                         *)
(***************************************************************************)

type
  (* TrueType collection header *)
  PTTCHeader = ^TTTCHeader;
  TTTCHeader = record
                 Tag            : Long;
                 version        : TT_Fixed;
                 DirCount       : ULong;
                 TableDirectory : PStorage;
               end;

  (* TrueType Table Directory type *)
  PTableDir = ^TTableDir;
  TTableDir = Record
                version        : TT_Fixed;  (* should be $10000 *)
                numTables      : UShort;    (* Tables number    *)

                searchRange,             (* These parameters are only used  *)
                entrySelector,           (* for a dichotomy search in the   *)
                rangeShift     : UShort; (* directory. We ignore them       *)
               end;

  (* The 'TableDir' is followed by 'numTables' TableDirEntries *)

  TTableDirEntry = Record
                     Tag      : Long;   (*        table type *)
                     CheckSum : Long;   (*    table Checksum *)
                     Offset   : Long;   (* Table file offset *)
                     Length   : Long;   (*      Table length *)
                    end;

  TTableDirEntries = array[0..100] of TTableDirEntry;
  PTableDirEntries = ^TTableDirEntries;

  (* 'cmap' tables *)

  TCMapDir = record
               tableVersionNumber : UShort;
               numCMaps           : UShort;
             end;

  TCMapDirEntry = record
                    platformID         : UShort;
                    platformEncodingID : UShort;
                    offset             : Long;
                  end;

  TCMapDirEntries = array[0..10] of TCMapDirEntry;
  PCMapDirEntries = ^TCMapDirEntries;

  (* table "maxp" of Maximum Profiles' *)

  TMaxProfile = Record
                  Version                 : TT_Fixed;
                  numGlyphs,
                  maxPoints,
                  maxContours,
                  maxCompositePoints,
                  maxCompositeContours,
                  maxZones,
                  maxTwilightPoints,
                  maxStorage,
                  maxFunctionDefs,
                  maxInstructionDefs,
                  maxStackElements,

                  maxSizeOfInstructions,
                  maxComponentElements,
                  maxComponentDepth       : UShort;
                end;

  (* table "gasp" *)

const
  Gasp_GridFit = 1;
  Gasp_DoGray  = 2;

type
  TGaspRange = record
                 maxPPEM  : UShort;
                 gaspFlag : UShort;
               end;

  TGaspRanges = array[0..9] of TGaspRange;
  PGaspRanges = ^TGaspRanges;

  TGasp = record
            version    : UShort;
            numRanges  : UShort;
            gaspRanges : PGaspRanges;
          end;

  (* table "HMTX" *)

  TLongMetrics = record
                  advance : UShort;
                  bearing : Short;
                 end;

  TTableLongMetrics = array[0..255] of TLongMetrics;
  PTableLongMetrics = ^TTableLongMetrics;

  TShortMetrics = Short;
  TTableShortMetrics = array[0..255] of TShortMetrics;
  PTableShortMetrics = ^TTableShortMetrics;

{
  (* table "OS/2" *)

  TOS2_Table = record
                 version             : UShort;   (* $0001 *)
                 xAvgCharWidth       : Short;
                 usWeightClass       : UShort;
                 usWidthClass        : UShort;
                 fsType              : Short;
                 ySubscriptXSize     : Short;
                 ySubscriptYSize     : Short;
                 ySubScriptXOffset   : Short;
                 ySubscriptYOffset   : Short;
                 ySuperscriptXSize   : Short;
                 ySuperscriptYSize   : Short;
                 ySuperscriptXOffset : Short;
                 ySuperscriptYOffset : Short;
                 yStrikeoutSize      : Short;
                 yStrikeoutPosition  : Short;
                 sFamilyClass        : Short;
                 panose              : array[0..9] of Byte;
                 ulUnicodeRange1     : ULong;   (* bits  0-31  *)
                 ulUnicodeRange2     : ULong;   (* bits 32-63  *)
                 ulUnicodeRange3     : ULong;   (* bits 64-95  *)
                 ulUnicodeRange4     : ULong;   (* bits 96-127 *)
                 achVendID           : array[0..3] of Byte;
                 fsSelection         : UShort;
                 usFirstCharIndex    : UShort;
                 usLastCharIndex     : UShort;
                 sTypoAscender       : UShort;
                 sTypoDescender      : UShort;
                 sTypoLineGap        : UShort;
                 usWinAscent         : UShort;
                 usWinDescent        : UShort;

                 (* only version 1 tables *)
                 ulCodePageRange1    : ULong;
                 ulCodePageRange2    : ULong;
               end;

  (* table "post" *)

  TPostscript = record
                  FormatType         : TT_Fixed;
                  italicAngle        : TT_Fixed;
                  underlinePosition  : Short;
                  underlineThickness : Short;
                  isFixedPitch       : ULong;
                  minMemType42       : ULong;
                  maxMemType42       : ULong;
                  minMemType1        : ULong;
                  maxMemType1        : ULong;
                end;
}
  (* table "name" *)

  (* table "name" *)

  TName_Record  = record

    platformID : UShort;
    encodingID : UShort;
    languageID : UShort;
    nameID     : UShort;
    length     : UShort;
    offset     : UShort;
  end;
  PName_Record  = ^TName_Record;
  TName_Records = array[0..0] of TName_Record;
  PName_Records = ^TName_Records;


  PName_Table = ^TName_Table;
  TName_Table = record

    format         : UShort;
    numNameRecords : UShort;
    storageOffset  : UShort;
    names          : PName_Records;
    storage        : PByte;
  end;


  PHdmx_Record = ^THdmx_Record;
  THdmx_Record = record
                   ppem      : Byte;
                   max_width : Byte;
                   widths    : PByte;
                 end;

  THdmx_Records = array[0..19] of THdmx_Record;
  PHdmx_Records = ^THdmx_Records;

  THdmx = record
            version     : UShort;
            num_records : Short;
            records     : PHdmx_Records;
          end;

implementation

end.

