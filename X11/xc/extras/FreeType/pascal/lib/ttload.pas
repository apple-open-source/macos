(*******************************************************************
 *
 *  TTLoad.Pas                                                 1.0
 *
 *    TrueType Tables loaders
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
 *  Difference between 1.0 and 1.1 : HUGE !!
 *
 *  - Changed the load model to get in touch with TTFile 1.1
 *  - Now loads one whole resident table in one call
 *  - defined resident and instance records/data
 *
 ******************************************************************)

Unit TTLoad;

interface

uses FreeType, TTTypes, TTTables, TTCMap, TTObjs;

 function LookUp_TrueType_Table( face : PFace;
                                 aTag : string ) : int;

 function Load_TrueType_Directory( face      : PFace;
                                   faceIndex : Int ) : TError;

 function Load_TrueType_MaxProfile( face : PFace ) : TError;
 function Load_TrueType_Header    ( face : PFace ) : TError;
 function Load_TrueType_Locations ( face : PFace ) : TError;
 function Load_TrueType_CVT       ( face : PFace ) : TError;
 function Load_TrueType_CMap      ( face : PFace ) : TError;
 function Load_TrueType_Gasp      ( face : PFace ) : TError;
 function Load_TrueType_Names     ( face : PFace ) : TError;
 function Load_TrueType_Programs  ( face : PFace ) : TError;
 function Load_trueType_Postscript( face : PFace ) : TError;
 function Load_TrueType_OS2       ( face : PFace ) : TError;
 function Load_TrueType_HDMX      ( face : PFace ) : TError;

 function Load_TrueType_Metrics_Header( face     : PFace;
                                        vertical : Boolean ) : TError;

 function Load_TrueType_Any( face        : PFace;
                             tag         : longint;
                             offset      : longint;
                             var buffer;
                             var length  : longint ) : TError;

implementation

uses TTError, TTMemory, TTFile, TTCalc;

  (* Composite glyph decoding flags *)

(*******************************************************************
 *
 *  Function    :  LookUp_TrueType_Table
 *
 *  Description :  Looks for a TrueType table by name
 *
 *  Input  :  face   resident table to look for
 *            aTag        searched tag
 *
 *  Output :  index of table if found, -1 otherwise.
 *
 ******************************************************************)

 function LookUp_TrueType_Table( face : PFace;
                                 aTag : string ) : int;
 var
   ltag : Long;
   i   : int;
 begin
   ltag := (Long(ord(aTag[1])) shl 24) +  (Long(ord(aTag[2])) shl 16) +
           (Long(ord(aTag[3])) shl 8 ) +   Long(ord(aTag[4]));

   for i := 0 to face^.numTables-1 do
     begin

       if face^.dirTables^[i].Tag = lTag then
         begin
           LookUp_TrueType_Table := i;
           exit;
         end
     end;

   (* couldn't find the table *)
   LookUp_TrueType_Table := -1;
 end;


 function LookUp_Mandatory_Table( face : PFace;
                                  aTag : string ) : int;
 var
   table : int;
 begin
   table := LookUp_TrueType_Table( face, aTag );
   if table < 0 then
     error := TT_Err_Table_Missing;

   LookUp_Mandatory_Table := table;
 end;

(*******************************************************************
 *
 *  Function    :  Load_TrueType_Collection
 *
 *  Description :
 *
 *  Input  :  face
 *
 *  Output :  True on success. False on failure
 *
 *  Notes : A table directory doesn't own subttables. There is no
 *          constructor or destructor for it.
 *
 ******************************************************************)

 function Load_TrueType_Collection( face : PFace ) : TError;
 var
   n : Int;
 const
   TTC_Tag = ( ord('t') shl 24 ) +
             ( ord('t') shl 16 ) +
             ( ord('c') shl 8  ) +
             ( ord(' ')        );
 begin
   Load_TrueType_Collection := Failure;

   with face^.ttcHeader do
   begin

     if TT_Seek_File( 0 )     or
        TT_Access_Frame( 12 ) then exit;

     Tag      := Get_ULong;
     version  := Get_Long;
     dirCount := Get_Long;

     TT_Forget_Frame;

     if Tag <> TTC_Tag then
     begin
       Tag            := 0;
       version        := 0;
       dirCount       := 0;
       tableDirectory := nil;

       error := TT_Err_File_Is_Not_Collection;
       exit;
     end;

     if Alloc( tableDirectory, dirCount * sizeof(ULong) ) or
        TT_Access_Frame( dirCount*4 ) then exit;

     for n := 0 to dirCount-1 do
       tableDirectory^[n] := Get_ULong;

     TT_Forget_Frame;
   end;

   Load_TrueType_Collection := Success;
 end;

(*******************************************************************
 *
 *  Function    :  Load_TrueType_Directory
 *
 *  Description :
 *
 *  Input  :  face
 *
 *  Output :  True on success. False on failure
 *
 *  Notes : A table directory doesn't own subttables. There is no
 *          constructor or destructor for it.
 *
 ******************************************************************)

 function Load_TrueType_Directory( face      : PFace;
                                   faceIndex : Int ) : TError;
 var
   n        : Int;
   tableDir : TTableDir;
 begin
    Load_TrueType_Directory := Failure;

    {$IFDEF DEBUG} Write('Directory '); {$ENDIF}

    if Load_TrueType_Collection(face) then
      begin
        if error <> TT_Err_File_Is_Not_Collection then
          exit;

        (* The file isn't a collection, exit if index isn't 0 *)
        if faceIndex <> 0 then
          exit;

        error := TT_Err_Ok;

        (* Now skip to the beginning of the file *)
        if TT_Seek_File(0) then
          exit;
      end
    else
      begin
        (* file is a collection. Check the index *)
        if ( faceIndex < 0 ) or
           ( faceIndex >= face^.ttcHeader.dirCount ) then
          begin
            error := TT_Err_Bad_Argument;
            exit;
          end;

        (* select a TT Font within the ttc file *)
        if TT_Seek_File( face^.ttcHeader.tableDirectory^[faceIndex] ) then
          exit;
      end;

    if TT_Access_Frame( 12 ) then
      exit;

    tableDir.version   := GET_Long;
    tableDir.numTables := GET_UShort;

    tableDir.searchRange   := GET_UShort;
    tableDir.entrySelector := GET_UShort;
    tableDir.rangeShift    := GET_UShort;

    {$IFDEF DEBUG} Writeln('Tables number : ', tableDir.numTables ); {$ENDIF}

    TT_Forget_Frame;

    (* Check that we have a 'sfnt' format there *)
    if (tableDir.version <> $10000   ) and     (* MS fonts  *)
       (tableDir.version <> $74727565) then    (* Mac fonts *)
    begin
      {$IFDEF DEBUG} Writeln('Invalid font format'); {$ENDIF}
      error := TT_Err_Invalid_File_Format;
      exit;
    end;

    with face^ do
    begin

      numTables := tableDir.numTables;

      if Alloc( dirTables, numTables * sizeof( TTableDirEntry ) ) or
         TT_Access_Frame( 16 * numTables ) then exit;

      for n := 0 to numTables-1 do with dirTables^[n] do
      begin
        Tag        := GET_ULong;
        Checksum   := GET_ULong;
        Offset     := GET_Long;
        Length     := Get_Long;
      end;

      TT_Forget_Frame;

   end;

   {$IFDEF DEBUG} Writeln('loaded'); {$ENDIF}

   Load_TrueType_Directory := Success;
 end;

(*******************************************************************
 *
 *  Function    :  Load_TrueType_MaxProfile
 *
 *  Description :
 *
 *  Input  :  face
 *
 *  Output :  True on success. False on failure
 *
 *  Notes : A maximum profile is a static table that owns no
 *          subttable. It has then no constructor nor destructor
 *
 ******************************************************************)

 function Load_TrueType_MaxProfile( face : PFace ) : TError;
 var
   table : int;
 begin

   Load_TrueType_MaxProfile := Failure;

   {$IFDEF DEBUG} Write('MaxProfile '); {$ENDIF}

   table := LookUp_Mandatory_Table( face, 'maxp');
   if table < 0 then exit;

   with face^ do
   begin

     if TT_Seek_File( dirTables^[table].Offset ) or
        TT_Access_Frame( 32 ) then exit;

     with MaxProfile do
      begin

        ULong(Version) := GET_ULong;

        numGlyphs   := GET_UShort;
        maxPoints   := GET_UShort;
        maxContours := GET_UShort;

        maxCompositePoints   := GET_UShort;
        maxCompositeContours := GET_UShort;
        maxZones             := GET_UShort;
        maxTwilightPoints    := GET_UShort;
        maxStorage           := GET_UShort;
        maxFunctionDefs      := GET_UShort;
        maxINstructionDefs   := GET_UShort;
        maxStackElements     := GET_UShort;

        maxSizeOfInstructions := GET_UShort;
        maxComponentElements  := GET_UShort;
        maxComponentDepth     := GET_UShort;
      end;

     TT_Forget_Frame;

    (* XXX : an adjustement that is necessary to load certain */
    /*       broken fonts like "Keystrokes MT" :-(            */
    /*                                                        */
    /*   We allocate 64 function entries by default when      */
    /*   the maxFunctionDefs field is null.                   *)

    (*   otherwise, we increment this field by one, in order  *)
    (*   to load some old Apple fonts..                       *)

     if maxProfile.maxFunctionDefs = 0 then
       maxProfile.maxFunctionDefs := 64;

     numGlyphs := MaxProfile.numGlyphs;
     (* compute number of glyphs *)

     maxPoints := MaxProfile.maxCompositePoints;
     if (maxPoints < MaxProfile.maxPoints) then
       maxPoints := MaxProfile.maxPoints;
     (* compute max number of points *)

     maxContours := MaxProfile.maxCompositeContours;
     if maxContours < MaxProfile.maxContours then
       maxContours := MaxProfile.maxContours;
     (* compute max number of contours *)

     maxComponents := MaxProfile.maxComponentElements +
                      MaxProfile.maxComponentDepth;
     (* compute max number of components for glyph loading *)

     (* XXX: some fonts have maxComponents set to 0; we will *)
     (*      then use 16 of them by default                  *)
     if maxComponents = 0 then maxComponents := 16;

     (* We also increase maxPoints and maxContours in order to support *)
     (* some broken fonts                                              *)
     inc( maxPoints,   8 );
     inc( maxContours, 4 );
   end;

   {$IFDEF DEBUG} Writeln('loaded'); {$ENDIF}

   Load_TrueType_MaxProfile := Success;
 end;

(*******************************************************************
 *
 *  Function    :  Load_TrueType_Gasp
 *
 *  Description :
 *
 *  Input  :  face
 *
 ******************************************************************)

 function Load_TrueType_Gasp( face : PFace ) : TError;
 var
   gRanges  : PGaspRanges;
   table, i : Int;
 label
   Fail;
 begin
   Load_TrueType_Gasp := Failure;

   with face^.gasp do
   begin
     version    := 0;
     numRanges  := 0;
     gaspRanges := nil;
   end;

   table := Lookup_TrueType_Table( face, 'gasp' );
   if ( table < 0 ) then
   begin
     Load_TrueType_Gasp := Success;
     exit;
   end;

   if TT_Seek_File( face^.dirTables^[table].Offset ) or
      TT_Access_Frame( 4 ) then exit;

   with face^.gasp do
   begin
     version    := Get_UShort;
     numRanges  := Get_UShort;
     gaspRanges := nil;
   end;

   TT_Forget_Frame;

   if Alloc( gRanges, face^.gasp.numRanges * sizeof(TGaspRange) ) or
      TT_Access_Frame( face^.gasp.numRanges * 4 ) then
     goto Fail;

   face^.gasp.gaspRanges := gRanges;

   for i := 0 to face^.gasp.numRanges-1 do
     with gRanges^[i] do
     begin
       maxPPEM  := Get_UShort;
       gaspFlag := Get_UShort;
     end;

   TT_Forget_Frame;

   Load_TrueType_Gasp := Success;
   exit;

 Fail:
   Free( gRanges );
   face^.gasp.numRanges := 0;
 end;


(*******************************************************************
 *
 *  Function    :  Load_TrueType_Header
 *
 *  Description :  Load the TrueType header table in the resident
 *                 table
 *
 *  Input  :  face   current leading segment.
 *
 *  Output :  True on success. False on failure
 *
 *  Notes : A font header is a static table that owns no
 *          subttable. It has then no constructor nor destructor
 *
 ******************************************************************)

 function  Load_TrueType_Header( face : PFace ) : TError;
 var
   i : int;
 begin
   Load_TrueType_Header := Failure;

   {$IFDEF DEBUG} Write('Header '); {$ENDIF}

   i := LookUp_Mandatory_Table(face, 'head');
   if i <= 0 then exit;

   with face^ do
   begin

     if TT_Seek_File( dirTables^[i].offset ) or
        TT_Access_Frame( 54 ) then exit;

     with FontHeader do
     begin

       ULong(Table_Version) := GET_ULong;
       ULong(Font_Revision) := GET_ULong;

       Checksum_Adjust := GET_Long;
       Magic_Number    := GET_Long;

       Flags        := GET_UShort;
       Units_Per_EM := GET_UShort;

       Created [0] := GET_Long; Created [1] := GET_Long;
       Modified[0] := GET_Long; Modified[1] := GET_Long;

       xMin := GET_Short;
       yMin := GET_SHort;
       xMax := GET_SHort;
       yMax := GET_Short;

       Mac_Style       := GET_UShort;
       Lowest_Rec_PPEM := GET_UShort;

       Font_Direction      := GET_Short;
       Index_To_Loc_Format := GET_Short;
       Glyph_Data_Format   := GET_Short;

       {$IFDEF DEBUG} Writeln('Units per EM : ',Units_Per_EM ); {$ENDIF}

     end;

     TT_Forget_Frame;

   end;

   {$IFDEF DEBUG} Writeln('loaded'); {$ENDIF}

   Load_TrueType_Header := Success;
 end;

(*******************************************************************
 *
 *  Function    : Load_TrueType_Metrics
 *
 *  Description : Load TrueType metrics either from the "hmtx" or
 *                "vmtx" table.
 *
 *  Input  :  face      current resident leading segment
 *            vertical  boolean. When set, try to load the vertical
 *                      header.
 *
 *  Output :  True on success. False on failure
 *
 ******************************************************************)

 function Load_TrueType_Metrics( face     : PFace;
                                 vertical : Boolean ) : TError;
 var
   table, n           : int;
   num_longs          : int;
   num_shorts         : int;
   num_shorts_checked : int;
   temp               : Short;

   header     : ^TT_Horizontal_Header;

   shorts     : ^PTableShortMetrics;
   longs      : ^PTableLongMetrics;

 begin
   Load_TrueType_Metrics := Failure;

   {$IFDEF DEBUG}
   if vertical then
     Write('vmtx ')
   else
     Write('hmtx ');
   {$ENDIF}

   if vertical then
     begin

       table := LookUp_TrueType_Table( face, 'vmtx' );
       if table < 0 then
         begin
           (* This is an optional table. Return silently if it *)
           (* wasn't found. Note : some fonts have a vertical  *)
           (* header, but no 'vmtx'. E.g. : mingliu.ttf        *)

           face^.verticalHeader.number_Of_VMetrics := 0;
           Load_TrueType_Metrics := Success;
           exit;
         end;

       header := @TT_Horizontal_Header(face^.verticalHeader);
     end
   else
     begin
       table := LookUp_Mandatory_Table( face, 'hmtx' );
       if table < 0 then
         exit;

       header := @face^.horizontalHeader;
     end;


   shorts     := @PTableShortMetrics(header^.short_metrics);
   longs      := @PTableLongMetrics (header^.long_metrics );

   num_longs  := header^.number_Of_HMetrics;
   num_shorts := face^.numGlyphs - num_longs;

   num_shorts_checked := (face^.dirTables^[table].Length - num_longs*4) div 2;

   if num_shorts < 0 then
   begin
     {$IFDEF DEBUG} Writeln('!! More metrics than glyphs !\n'); {$ENDIF}
     if vertical then  error := TT_Err_Invalid_Vert_Metrics
                 else  error := TT_Err_Invalid_Horiz_Metrics;
     exit;
   end;

   if Alloc( longs^,  sizeof(TLongMetrics) * num_longs )   or
      Alloc( shorts^, sizeof(TShortMetrics)* num_shorts )  or

      TT_Seek_File( face^.dirTables^[table].Offset )       or
      TT_Access_Frame( face^.dirTables^[table].Length )    then exit;

   for n := 0 to num_longs-1 do with longs^^[n] do
   begin
     advance := GET_UShort;
     bearing := GET_Short;
   end;

   (* do we have an inconsistent number of metric values ? *)
   if num_shorts > num_shorts_checked then
     begin
       for n := 0 to num_shorts_checked-1 do
         shorts^^[n] := GET_Short;

        (* we fill up the missing left side bearings with the    *)
        (* last valid value. Since this will occur for buggy CJK *)
        (* fonts usually, nothing serious will happen.           *)

        temp := shorts^^[num_shorts_checked-1];

        for n := num_shorts_checked to num_shorts-1 do
          shorts^^[n] := temp;
     end
   else
     for n := 0 to num_shorts-1 do
       shorts^^[n] := GET_Short;

   TT_Forget_Frame;

   {$IFDEF DEBUG} Writeln('loaded'); {$ENDIF}

   Load_TrueType_Metrics := Success;
 end;


(*******************************************************************
 *
 *  Function    : Load_TrueType_Metrics_Header
 *
 *  Description :
 *
 *  Input  :  face      current resident leading segment
 *            vertical  boolean. When set, try to load the vertical
 *                      header.
 *
 *  Output :  True on success. False on failure
 *
 ******************************************************************)

 function Load_TrueType_Metrics_Header( face     : PFace;
                                        vertical : Boolean ) : TError;
 var
   table  : int;
   header : ^TT_Horizontal_Header;
 begin
   Load_TrueType_Metrics_Header := Failure;

    {$IFDEF DEBUG}
    if vertical then
      Write('Vertical Header ')
    else
      Write('Horizontal Header ');
    {$ENDIF}

   if vertical then
     begin
       face^.verticalInfo := False;

       (* the vertical header is an optional table.. so return *)
       (* silently if we don't find it                         *)
       table := LookUp_TrueType_Table( face, 'vhea' );
       if (table < 0) then
         begin
           Load_TrueType_Metrics_Header := Success;
           exit;
         end;

       face^.verticalInfo := True;
       header := @TT_Horizontal_Header(face^.verticalHeader);
     end
   else
     begin
       table := LookUp_Mandatory_Table( face, 'hhea');
       if ( table < 0 ) then
         exit;
       header := @face^.horizontalHeader;
     end;

   with face^ do
   begin

     if TT_Seek_File( dirTables^[table].Offset ) or
        TT_Access_Frame( 36 ) then
        exit;

     with header^ do
     begin

       Long(Version) := GET_ULong;
       Ascender      := GET_Short;
       Descender     := GET_Short;
       Line_Gap      := GET_Short;

       advance_Width_Max := GET_UShort;

       min_Left_Side_Bearing  := GET_Short;
       min_Right_Side_Bearing := GET_Short;
       xMax_Extent            := GET_Short;
       caret_Slope_Rise       := GET_Short;
       caret_Slope_Run        := GET_Short;

       Reserved[0] := GET_Short;  (* this is cared offset for vertical *)

       Reserved[1] := GET_Short;
       Reserved[2] := GET_Short;
       Reserved[3] := GET_Short;
       Reserved[4] := GET_Short;

       metric_Data_Format := GET_Short;
       number_Of_HMetrics := GET_UShort;

       short_metrics := nil;
       long_metrics  := nil;

     end;

     TT_Forget_Frame;

   end;

   {$IFDEF DEBUG} Writeln('loaded'); {$ENDIF}

   Load_TrueType_Metrics_Header := Load_TrueType_Metrics( face, vertical );
 end;

(*******************************************************************
 *
 *  Function    :  Load_TrueType_Locations
 *
 *  Description :  Loads the location table in resident table
 *
 *  Input  :  face     Current Resident Leading Segment
 *
 *  Output :  True on success. False on failure
 *
 *  NOTES :
 *
 *    The Font Header *must* be loaded in the leading segment
 *    before calling this function.
 *
 *    This table is destroyed directly by the resident destructor.
 *
 ******************************************************************)

 function Load_TrueType_Locations( face : PFace ): TError;
 var
   t, n        : int;
   LongOffsets : int;
 begin

   Load_TrueType_Locations := Failure;

   {$IFDEF DEBUG} Write('Locations '); {$ENDIF}

   with face^ do
   begin

     LongOffsets :=  fontHeader.Index_To_Loc_Format;

     t := LookUp_Mandatory_Table( face, 'loca' );
     if t < 0 then exit;

     if TT_Seek_File( dirTables^[T].Offset ) then exit;

     if LongOffsets <> 0 then
       begin

         numLocations := dirTables^[T].Length shr 2;

         {$IFDEF DEBUG}
         Writeln('Glyph locations # ( 32 bits offsets ) : ', numLocations );
         {$ENDIF}

         if Alloc( glyphLocations, sizeof(Long)*numLocations ) or
            TT_Access_Frame( numLocations*4 ) then exit;

         for n := 0 to numLocations-1 do
           glyphLocations^[n] := GET_Long;

         TT_Forget_Frame;

       end
     else
       begin
         numLocations := dirTables^[T].Length shr 1;

         {$IFDEF DEBUG}
         Writeln('Glyph locations # ( 16 bits offsets ) : ', numLocations );
         {$ENDIF}

         if Alloc( glyphLocations, sizeof(Long)*numLocations ) or
            TT_Access_Frame( numLocations*2 ) then exit;

         for n := 0 to numLocations-1 do
           glyphLocations^[n] := Long(GET_UShort) * 2;

         TT_Forget_Frame;
       end;

   end;

   {$IFDEF DEBUG} Writeln('loaded'); {$ENDIF}

   Load_TrueType_Locations := Success;
 end;


(*******************************************************************
 *
 *  Function    :  Load_TrueType_Names
 *
 *  Description :  Loads the name table into the face table
 *
 *  Input  :  face
 *
 *  Output :  True on success. False on failure
 *
 *  Notes  :  This attribute table is destroyed by the resident
 *            destructor.
 *
 ******************************************************************)

  function Load_TrueType_Names( face : PFace ) : TError;
  var
    table, i : Int;
    bytes    : Long;
  begin
    Load_TrueType_Names := Failure;

    table := Lookup_Mandatory_Table( face, 'name' );
    if table < 0 then exit;

    with face^.nameTable do
    begin
      (* Seek to the beginning of the table and check the frame access. *)
      if TT_Seek_File( face^.dirTables^[table].Offset ) or
         TT_Access_Frame( 6 ) then exit;

      format         := GET_UShort;
      numNameRecords := GET_UShort;
      storageOffset  := GET_UShort;

      TT_Forget_Frame;

      if Alloc( names, numNameRecords*sizeof(TName_Record) ) or
         TT_Access_Frame( numNameRecords*12 ) then
      begin
        numNameRecords := 0;
        exit;
      end;

      (* Load the name records and determine how much storage is needed *)
      (* to hold the strings themselves                                 *)

      bytes := 0;
      for i := 0 to numNameRecords-1 do with names^[i] do
      begin
        platformID := GET_UShort;
        encodingID := GET_UShort;
        languageID := GET_UShort;
        nameID     := GET_UShort;
        length     := GET_UShort;
        offset     := GET_UShort;

        (* this test takes care of 'holes' in the names tabls, as *)
        (* reported by Erwin                                      *)
        if Offset + Length > bytes then
          bytes := Offset + Length;
      end;

      TT_Forget_Frame;

      storage := nil;
      if bytes > 0 then
      begin
        if Alloc( storage, bytes ) then exit;

        if TT_Read_At_File( face^.dirTables^[table].Offset + storageOffset,
                            storage^, bytes ) then
        begin
          Free(storage);
          exit;
        end;
      end;

    end;

    Load_TrueType_Names := Success;
    exit;
  end;

(*******************************************************************
 *
 *  Function    :  Load_TrueType_CVT
 *
 *  Description :
 *
 *  Input  :  face
 *
 *  Output :  True on success. False on failure
 *
 *  Notes  :  This attribute table is destroyed by the resident
 *            destructor.
 *
 ******************************************************************)

 function Load_TrueType_CVT( face : PFace ): TError;
 var
   t, n : Int;
 begin
   Load_TrueType_CVT := Failure;

   {$IFDEF DEBUG} Write('CVT '); {$ENDIF}

   (* the CVT table is optional *)

   t := LookUp_TrueType_Table( face, 'cvt ');
   if t < 0 then
   begin
     face^.cvt     := nil;
     face^.cvtSize := 0;
     Load_TrueType_CVT := Success;
     {$IFDEF DEBUG}  writeln('none'); {$ENDIF}
     exit;
   end;

   with face^ do
   begin

     cvtSize := dirTables^[t].Length div 2;

     if Alloc( cvt, sizeof(Short)*cvtSize )  or

        TT_Seek_File( dirTables^[t].Offset ) or

        TT_Access_Frame( 2*cvtSize )         then exit;

     for n := 0 to cvtSize-1 do
       cvt^[n] := GET_Short;

     TT_Forget_Frame;
   end;

   {$IFDEF DEBUG} Writeln('loaded'); {$ENDIF}
   Load_TrueType_CVT := Success;
 end;


(*******************************************************************
 *
 *  Function    :  Load_TrueType_CMap
 *
 *  Description :
 *
 *  Input  :  face
 *
 *  Output :  True on success. False on failure
 *
 *  Notes  :  The Cmap table directory is destroyed by the resident
 *            destructor. The Cmap subtables must be destroyed by
 *            Free_CMap_Table.
 *
 ******************************************************************)

 function Load_TrueType_CMap( face : PFace ) : TError;
 var
   off, table_start : Longint;
   n, limit, t      : Int;

   cmap_dir : TCMapDir;
   entry    : TCMapDirEntry;
   cmap     : PCMapTable;
 label
   Fail;
 begin

   Load_TrueType_CMap := Failure;

   {$IFDEF DEBUG} Write('CMaps '); {$ENDIF}

   t := LookUp_Mandatory_Table( face,'cmap' );
   if t < 0 then exit;

   with face^ do
   begin

     table_start := dirTables^[t].offset;

     if TT_Seek_File( dirTables^[t].Offset ) or
        TT_Access_Frame( 4 )  then exit;

     cmap_dir.tableVersionNumber := GET_UShort;
     cmap_dir.numCMaps           := GET_UShort;

     TT_Forget_Frame;

     off := TT_File_Pos;

     (* save space in face data for cmap tables *)
     numCMaps := cmap_dir.numCMaps;
     if Alloc( cMaps, numCMaps * sizeof(TCMapTable) ) then exit;

     for n := 0 to numCMaps-1 do
     begin

       if TT_Seek_File   ( off ) or
          TT_Access_Frame( 8 )   then exit;

       cmap := @cMaps^[n];

       entry.platformID         := GET_UShort;
       entry.platformEncodingID := GET_UShort;
       entry.offset             := GET_Long;

       cmap^.loaded             := False;
       cmap^.platformID         := entry.platformID;
       cmap^.platformEncodingID := entry.platformEncodingID;

       TT_Forget_Frame;

       off := TT_File_Pos;

       if TT_Seek_File   ( table_start + entry.offset ) or
          TT_Access_Frame( 6 ) then exit;

       cmap^.format  := Get_UShort;
       cmap^.length  := Get_UShort;
       cmap^.version := Get_UShort;

       TT_Forget_Frame;

       cmap^.offset := TT_File_Pos;

     end;  (* for n *)

   end;  (* with face^ *)

   {$IFDEF DEBUG} Writeln('loaded'); {$ENDIF}

   Load_TrueType_CMap := Success;
   exit;

 Fail:
   Free( face^.cMaps );
   Load_TrueType_CMap := Failure;
 end;


(*
 procedure Free_CMap_Table( var cmap : TCMapTable );
 begin
   if cmap.cmap0 <> nil then
     with cmap do
       case format of

         0 : begin
               Free( cmap0^.glyphIdArray );
               Free( cmap0 );
             end;

         2 : begin
               Free( cmap2^.glyphIdArray );
               Free( cmap2^.subHeaders );
               Free( cmap2 );
             end;

         4 : begin
               Free( cmap4^.glyphIdArray );
               Free( cmap4^.segments );
               Free( cmap4 );
             end;

         6 : begin
               Free( cmap6^.glyphIdArray );
               Free( cmap6 );
             end;
       end;

   cmap.format  := 0;
   cmap.length  := 0;
   cmap.version := 0;
 end;
*)

(*******************************************************************
 *
 *  Function    :  Load_TrueType_Programs
 *
 *  Description :  Load the Font and CVT programs in the resident
 *                 table
 *
 *  Input  :  face
 *
 *  Output :  True on success. False on failure
 *
 ******************************************************************)

 function Load_TrueType_Programs( face : PFace ) : TError;
 var
   t : Int;
 begin

   Load_TrueType_Programs := Failure;

   {$IFDEF DEBUG} Write('Font program '); {$ENDIF}

   (* The font program is optional *)

   t := Lookup_TrueType_Table( face, 'fpgm' );

   if t < 0 then

     with face^ do
     begin
       fontProgram := nil;
       fontPgmSize := 0;

       {$IFDEF DEBUG} Writeln('none in file'); {$ENDIF}
     end

   else

     with face^ do
     begin

       fontPgmSize := dirTables^[t].Length;

       if Alloc( fontProgram, fontPgmSize ) or
          TT_Read_At_File( dirTables^[t].offset,
                           fontProgram^,
                           fontPgmSize ) then exit;

       {$IFDEF DEBUG} Writeln('loaded, ',fontPgmSize,' bytes'); {$ENDIF}
     end;

   {$IFDEF DEBUG} Write('CVT program '); {$ENDIF}

   t := LookUp_trueType_Table( face, 'prep' );

   (* The CVT table is optional *)

   if t < 0 then

     with face^ do
     begin
       cvtProgram := nil;
       cvtPgmSize := 0;

       {$IFDEF DEBUG} Writeln('none in file'); {$ENDIF}
     end

   else

     with face^ do
     begin

       cvtPgmSize := dirTables^[t].Length;

       if Alloc( cvtProgram, cvtPgmSize ) or
          TT_Read_At_File( dirTables^[t].offset,
                           cvtProgram^,
                           cvtPgmSize ) then exit;

       {$IFDEF DEBUG} Writeln('loaded, ',cvtPgmSize,' bytes'); {$ENDIF}
     end;

   Load_TrueType_Programs := Success;
 end;

(*******************************************************************
 *
 *  Function    :  Load_TrueType_OS2
 *
 *  Description :  Load the OS2 Table
 *
 *  Input  :  face
 *
 *  Output :  True on success. False on failure
 *
 ******************************************************************)

 function Load_TrueType_OS2( face : PFace ) : TError;
 var
   table : Int;
   i     : Int;
 begin
   Load_TrueType_OS2 := Failure;

   {$IFDEF DEBUG} Write('OS/2 table '); {$ENDIF}

   (* We now support Apple fonts who do not have an OS/2 table *)
   table := LookUp_Mandatory_Table( face, 'OS/2' );
   if table < 0 then begin
     face^.os2.version := $FFFF;
     Load_TrueType_OS2 := Success;
     error             := TT_Err_Ok;  (* clear error *)
     exit;
   end;

   if TT_Seek_File( face^.dirTables^[table].offset ) or
      TT_Access_Frame( 78 ) then exit;

   with face^.os2 do
   begin
     version             := Get_UShort;
     xAvgCharWidth       := Get_Short;
     usWeightClass       := Get_UShort;
     usWidthClass        := Get_UShort;
     fsType              := Get_Short;
     ySubscriptXSize     := Get_Short;
     ySubscriptYSize     := Get_Short;
     ySubscriptXOffset   := Get_Short;
     ySubscriptYOffset   := Get_Short;
     ySuperscriptXSize   := Get_Short;
     ySuperscriptYSize   := Get_Short;
     ySuperscriptXOffset := Get_Short;
     ySuperscriptYOffset := Get_Short;
     yStrikeoutSize      := Get_Short;
     yStrikeoutPosition  := Get_Short;
     sFamilyClass        := Get_Short;

     for i := 0 to 9 do panose[i] := Get_Byte;

     ulUnicodeRange1 := Get_ULong;
     ulUnicodeRange2 := Get_ULong;
     ulUnicodeRange3 := Get_ULong;
     ulUnicodeRange4 := Get_ULong;

     for i := 0 to 3 do achVendID[i] := Get_Byte;

     fsSelection      := Get_UShort;
     usFirstCharIndex := Get_UShort;
     usLastCharIndex  := Get_UShort;
     sTypoAscender    := Get_UShort;
     sTypoDescender   := Get_UShort;
     sTypoLineGap     := Get_UShort;
     usWinAscent      := Get_UShort;
     usWinDescent     := Get_UShort;

     TT_Forget_Frame;

     if version >= $0001 then
       begin
         if TT_Access_Frame(8) then exit;

         ulCodePageRange1 := Get_ULong;
         ulCodePageRange2 := Get_ULong;

         TT_Forget_Frame;
       end
     else
       begin
         ulCodePageRange1 := 0;
         ulCodePageRange2 := 0;
       end;

   end;

   {$IFDEF DEBUG} Writeln('loaded'); {$ENDIF}

   Load_TrueType_OS2 := Success;
 end;

(*******************************************************************
 *
 *  Function    :  Load_TrueType_Postscript
 *
 *  Description :  Load the 'post' table
 *
 *  Input  :  face
 *
 *  Output :  True on success. False on failure
 *
 ******************************************************************)

 function Load_TrueType_Postscript( face : PFace ) : TError;
 var
   table : Int;
   i     : Int;
 begin
   Load_TrueType_Postscript := Failure;

   {$IFDEF DEBUG} Write('post table '); {$ENDIF}

   table := LookUp_TrueType_Table( face, 'post' );
   if table < 0 then exit;

   if TT_Seek_File( face^.dirTables^[table].offset ) or
      TT_Access_Frame(32) then exit;

   with face^.postscript do
   begin
     formatType         := Get_ULong;
     italicAngle        := Get_ULong;
     underlinePosition  := Get_Short;
     underlineThickness := Get_Short;
     isFixedPitch       := Get_ULong;
     minMemType42       := Get_ULong;
     maxMemType42       := Get_ULong;
     minMemType1        := Get_ULong;
     maxMemType1        := Get_ULong;
   end;

   TT_Forget_Frame;

   {$IFDEF DEBUG} Writeln('loaded'); {$ENDIF}

   Load_trueType_Postscript := Success;
 end;

(*******************************************************************
 *
 *  Function    :  Load_TrueType_HDMX
 *
 *  Description :  Load the 'hdmx' tables
 *
 *  Input  :  face
 *
 *  Output :  True on success. False on failure
 *
 ******************************************************************)

 function Load_TrueType_Hdmx( face : PFace ) : TError;
 var
   table, n   : Int;
   num_glyphs : Int;

   version  : UShort;
   num_rec  : Short;
   recs     : PHdmx_Records;
   rec_size : Long;
   rec      : PHdmx_Record;
 label
   Fail;
 begin
   Load_TrueType_Hdmx := Failure;

   with face^.hdmx do
   begin
     version     := 0;
     num_records := 0;
     records     := nil;
   end;

   (* This table is optional *)

   table := LookUp_TrueType_Table( face, 'hdmx' );
   if table < 0 then
   begin
     Load_TrueType_Hdmx := Success;
     exit;
   end;

   if TT_Seek_File( face^.dirTables^[table].offset ) or
      TT_Access_Frame( 8 ) then exit;

   version  := Get_UShort;
   num_rec  := Get_Short;
   rec_size := Get_Long;

   TT_Forget_Frame;

   (* right now, we only recognize format 0 *)

   if version <> 0 then
     exit;

   if Alloc( face^.hdmx.records, sizeof(THdmx_Record)*num_rec ) then
     exit;

   face^.hdmx.num_records := num_rec;
   num_glyphs := face^.NumGlyphs;

   rec_size := rec_size - num_glyphs - 2;

   for n := 0 to num_rec-1 do
   begin
     rec := @face^.hdmx.records^[n];

     (* read record *)

     if TT_Access_Frame(2) then
       goto Fail;

     rec^.ppem      := Get_Byte;
     rec^.max_width := Get_Byte;

     TT_Forget_Frame;

     if Alloc( rec^.widths, num_glyphs ) or
        TT_Read_File( rec^.widths^, num_glyphs ) then
       goto Fail;

     (* skip padding bytes *)

     if rec_size > 0 then
       if TT_Skip_File( rec_size ) then
         goto Fail;
   end;

   Load_TrueType_HDMX := Success;
   exit;

 Fail:
   for n := 0 to num_rec-1 do
    Free( face^.hdmx.records^[n].widths );

   Free( face^.hdmx.records );
   face^.hdmx.num_records := 0;
 end;


(*******************************************************************
 *
 *  Function    :  Load_TrueType_Any
 *
 *  Description :  Load any TrueType table in user memory
 *
 *  Input  :  face    the font file's face object
 *            tag     the table
 *
 *  Output :  True on success. False on failure
 *
 ******************************************************************)

 function Load_TrueType_Any( face        : PFace;
                             tag         : longint;
                             offset      : longint;
                             var buffer;
                             var length  : longint ) : TError;
 var
   stream   : TT_Stream;
   found, i : integer;
 begin
   if tag <> 0 then
     begin
       found := -1;
       i     := 0;
       while i < face^.numTables do
         if Longint(face^.dirTables^[i].tag) = tag then
           begin
             found := i;
             i := face^.numTables;
           end
         else
           inc(i);

       if found < 0 then
         begin
           error := TT_Err_Table_Missing;
           Load_TrueType_Any := Failure;
           exit;
         end;

       inc( offset, face^.dirTables^[found].offset );

       (* if length = 0, the user requested the table's size *)
       if length = 0 then
         begin
           length := face^.dirTables^[found].length;
           Load_TrueType_Any := Success;
           exit;
         end;
     end
   else
     (* if length = 0 and tag = 0, the user requested the font file's size *)
     if length = 0 then
       begin
         (* return length of font file *)
         length := TT_Stream_Size( face^.stream );
         Load_TrueType_Any := Success;
         exit;
       end;

   TT_Use_Stream( face^.stream, stream );
   Load_TrueType_Any := TT_Read_At_File( offset, buffer, length );
   TT_Done_Stream( face^.stream );
 end;

end.

