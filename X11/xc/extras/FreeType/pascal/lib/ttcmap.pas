(*******************************************************************
 *
 *  ttcmap.pas                                                   1.0
 *
 *    Character Mappings unit.
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

unit TTCMap;

interface

uses FreeType, TTTypes;

type
  (********************************************************************)
  (*                                                                  *)
  (*  CHARACTER MAPPINGS SUBTABLES                                    *)
  (*                                                                  *)
  (********************************************************************)

  (* FORMAT 0 *)

  (* Apple standard character to glyph index mapping table *)
  (* the glyphIdArray for this format has 256 entries      *)

  TCMap0 = record
             glyphIdArray : PUShort;
           end;

  (* FORMAT 2 *)

  (* the format 2 table contains a variable-length array of subHeaders   *)
  (* (at most 256 entries) whose size must be determined algorithmically *)
  TCMap2SubHeader = record
                      firstCode     : UShort; (* first valid low byte       *)
                      entryCount    : UShort; (* number of valid low bytes  *)
                      idDelta       : Short;  (* delta value to glyphIndex  *)
                      idRangeOffset : UShort; (* offset fr. here to 1stCode *)
                    end;

  TCMap2SubHeaders = array[0..100] of TCMap2SubHeader;
  PCMap2SubHeaders = ^TCMap2SubHeaders;

  (* Format 2 is used for mixed 8/16bit encodings (usually CJK fonts) *)
  TCMap2 = record
             subHeaderKeys : PUShort;
             (* high byte mapping table            *)
             (* value = subHeader index * 8        *)
             subHeaders    : PCMap2SubHeaders;
             glyphIdArray  : PUShort;
             numGlyphId    : Int;
           end;

  (* FORMAT 4 *)

  (*The format 4 table contains segCount segments *)
  TCMap4Segment = record
                    endCount      : UShort;
                    startCount    : UShort;
                    idDelta       : UShort;
                    idRangeOffset : UShort;
                  end;
  TCMap4Segments = array[0..100] of TCMap4Segment;
  PCMap4Segments = ^TCMap4Segments;

  (* Microsoft standard character to glyph index mapping table *)
  TCMap4 = record
             segCountX2    : UShort;  (* segments number * 2          *)
             searchRange   : UShort;  (* these parameters can be used *)
             entrySelector : UShort;  (* for a binary search          *)
             rangeShift    : UShort;
             segments      : PCMap4Segments;
             glyphIdArray  : PUShort;
             numGlyphId    : Int;
           end;

  (* FORMAT 6 *)

  (* trimmed table mapping (for representing one subrange) *)
  TCMap6 = record
             firstCode    : UShort; (* first character code of subrange    *)
             entryCount   : UShort; (* num. of character codes in subrange *)

             glyphIdArray : PUShort;
           end;

  (* CHARMAP TABLE *)

  PCMapTable = ^TCMapTable;
  TCMapTable = record
                 platformID         : UShort;
                 platformEncodingID : UShort;

                 Format  : word;
                 Length  : word;
                 Version : word;
                 Loaded  : Boolean;
                 Offset  : Long;

                 case Byte of
                   0 : ( cmap0 : TCMap0 );
                   2 : ( cmap2 : TCMap2 );
                   4 : ( cmap4 : TCMap4 );
                   6 : ( cmap6 : TCMap6 );
               end;

  TCMapTables = array[0..9] of TCMapTable;
  PCMapTables = ^TCMapTables;


 function  CharMap_Load( var cmap : TCMapTable ) : TError;

 procedure CharMap_Free( var cmap : TCMapTable );

 function CharMap_Index( var cmap : TCMapTable; charCode : Long ) : UShort;

implementation

uses
  TTError, TTMemory, TTFile;

 function  CharMap_Load( var cmap : TCMapTable ) : TError;
 var
   num_SH, u  : UShort;
   i          : Int;
   numGlyphId : Int;
   num_segs   : Int;
 label
   Fail;
 begin
   CharMap_Load := Failure;

   if cmap.loaded then
   begin
     CharMap_Load := Success;
     exit;
   end;

   if TT_Seek_File( cmap.offset ) then exit;

   case cmap.format of

     0: with cmap.cmap0 do
          if Alloc( glyphIdArray, 256 ) or
             TT_Read_File( glyphIdArray^, 256 ) then goto Fail;

     2: begin
          num_SH := 0;
          with cmap.cmap2 do
            begin
              if Alloc( subHeaderKeys, 256*sizeof(UShort) ) or
                 TT_Access_Frame( 512 ) then goto Fail;

              for i := 0 to 255 do
              begin
                u := GET_UShort shr 3;
                subHeaderKeys^[i] := u;

                if num_SH < u then num_SH := u;
              end;

              TT_Forget_Frame;

              (* now load sub headers *)
              numGlyphId := ((cmap.length - 2*(256+3) - num_SH*8) and $FFFF) 
                             div 2;

              if Alloc( subHeaders, (num_SH+1)*sizeof(TCMap2SubHeader) ) or
                 TT_Access_Frame( (num_SH+1)*8 ) then goto Fail;

              for i := 0 to num_SH do with subHeaders^[i] do
              begin
                firstCode  := GET_UShort;
                entryCount := GET_UShort;
                idDelta    := GET_UShort;
                (* we apply the location offset immediately *)
                idRangeOffset := GET_UShort - (num_SH-i)*8 - 2;
              end;

              TT_Forget_Frame;

              (* load glyph ids *)
              if Alloc( glyphIdArray, numGlyphId*sizeof(UShort) ) or
                 TT_Access_Frame( numGlyphId*2 ) then goto Fail;

              for i := 0 to numGlyphId-1 do
                glyphIdArray^[i] := GET_UShort;

              TT_Forget_Frame;
            end;
        end;

     4: with cmap.cmap4 do
        begin
          if TT_Access_Frame(8) then goto Fail;

          segCountX2    := Get_UShort;
          searchRange   := Get_UShort;
          entrySelector := Get_UShort;
          rangeShift    := Get_UShort;

          num_segs := segCountX2 shr 1;

          TT_Forget_Frame;

          (* load segments *)
          if Alloc( segments, num_segs*sizeof(TCMap4Segment) ) or
             TT_Access_Frame( (num_segs*4+1)*2 ) then goto Fail;

          for i := 0 to num_segs-1 do
            segments^[i].endCount := Get_UShort;

           Get_UShort;

          for i := 0 to num_segs-1 do
            segments^[i].startCount := Get_UShort;

          for i := 0 to num_segs-1 do
            segments^[i].idDelta := GET_Short;

          for i := 0 to num_segs-1 do
            segments^[i].idRangeOffset := GET_UShort;

          TT_Forget_Frame;

          numGlyphId := (( cmap.length - (16+8*num_segs) ) and $FFFF)
                          div 2;

          (* load glyph ids *)
          if Alloc( glyphIdArray, numGlyphId*sizeof(UShort) ) or
             TT_Access_Frame( numGlyphId*2 ) then goto Fail;

          for i := 0 to numGlyphId-1 do
            glyphIdArray^[i] := Get_UShort;

          TT_Forget_Frame;
        end;

     6: with cmap.cmap6 do
        begin
          if TT_Access_Frame(4) then goto Fail;

          firstCode  := GET_UShort;
          entryCount := GET_UShort;

          TT_Forget_Frame;

          if Alloc( glyphIdArray, entryCount*sizeof(Short) ) or
             TT_Access_Frame( entryCount*2 ) then goto Fail;

          for i := 0 to entryCount-1 do
            glyphIdArray^[i] := GET_UShort;

          TT_Forget_Frame;
        end;

     else
       error := TT_Err_Invalid_Charmap_Format;
       exit;
   end;

   CharMap_Load := success;
   exit;

 Fail:
   CharMap_Free( cmap );
 end;


 procedure CharMap_Free( var cmap : TCMapTable );
 begin
   with cmap do
     case format of

       0 : begin
             Free( cmap.cmap0.glyphIdArray );
           end;

       2 : begin
             Free( cmap.cmap2.glyphIdArray );
             Free( cmap.cmap2.subHeaders );
             Free( cmap.cmap2.glyphIdArray );
           end;

       4 : begin
             Free( cmap.cmap4.segments );
             Free( cmap.cmap4.glyphIdArray );
             cmap.cmap4.segCountX2 := 0;
           end;

       6 : begin
             Free( cmap.cmap6.glyphIdArray );
             cmap.cmap6.entryCount := 0;
           end;
     end;

   cmap.loaded  := False;
   cmap.format  := 0;
   cmap.length  := 0;
   cmap.version := 0;
 end;


 function code_to_index0( charCode : UShort; var cmap0 : TCMap0 ) : UShort;
 begin
   code_to_index0 := 0;
   if charCode < 256 then
     code_to_index0 := cmap0.glyphIdArray^[charCode]
 end;



 function code_to_index2( charCode : UShort; var cmap2 : TCMap2 ) : UShort;
 var
   index1, idx, offset : UShort;
 begin
   code_to_index2 := 0;

   if charCode < 256 then  idx := charCode
                     else  idx := charCode shr 8;

   index1 := cmap2.subHeaderKeys^[idx];

   if index1 = 0 then
     begin
       if charCode < 256 then
         code_to_index2 := cmap2.glyphIdArray^[charCode];  (* 8Bit charcode *)
     end
   else
     begin
       if charCode < 256 then
         exit;

       idx := charCode and 255;
       with cmap2.subHeaders^[index1] do
       begin
         if ( idx <  firstCode              ) or
            ( idx >= firstCode + entryCount ) then exit;

         offset := idRangeOffset div 2 + idx - firstCode;
  
         if offset >= cmap2.numGlyphId then exit;

         idx := cmap2.glyphIdArray^[offset];
         if idx <> 0 then
           code_to_index2 := (idx + idDelta) and $FFFF;
       end
     end;
 end;



 function code_to_index4( charCode : UShort; var cmap4 : TCMap4 ) : UShort;
 var
   i, index1, num_segs : Int;
 label
   Found;
 begin
   code_to_index4 := 0;
   num_segs       := cmap4.segCountX2 div 2;
   i              := 0;

   while ( i < num_segs ) do with cmap4.segments^[i] do
   begin
     if charCode <= endCount then goto Found;
     inc(i);
   end;

   exit;

  Found:
    with cmap4.segments^[i] do
    begin

      if charCode < startCount then
        exit;

      if idRangeOffset = 0 then
        code_to_index4 := (charCode + idDelta) and $FFFF
      else
      begin
        index1 := idRangeOffset div 2 + (charCode - startCount) -
                    -(num_segs-i);

        if ( index1 < cmap4.numGlyphId ) and
           ( cmap4.glyphIdArray^[index1] <> 0 ) then

          code_to_index4 := (cmap4.glyphIdArray^[index1] + idDelta) and $FFFF;
      end;
    end;
 end;


 function code_to_index6( charCode : UShort; var cmap6 : TCMap6 ) : UShort;
 begin
   code_to_index6 := 0;
   with cmap6 do
   begin

     if ( charCode <  firstCode              ) or
        ( charCode >= firstCode + entryCount ) then exit;
  
     code_to_index6 := glyphIdArray^[charCode-firstCode];
   end
 end;


  function CharMap_Index( var cmap : TCMapTable;
                          charCode : Long ) : UShort;
  begin
    CharMap_Index := 0;

    case cmap.format of
      0: CharMap_Index := code_to_index0( charCode, cmap.cmap0 );
      2: CharMap_Index := code_to_index2( charCode, cmap.cmap2 );
      4: CharMap_Index := code_to_index4( charCode, cmap.cmap4 );
      6: CharMap_Index := code_to_index6( charCode, cmap.cmap6 );
    end;
  end;

end.
