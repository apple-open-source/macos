(*******************************************************************
 *
 *  TTFile.Pas                                                1.2
 *
 *    File I/O Component (specification)
 *
 *  Copyright 1996 David Turner, Robert Wilhelm and Werner Lemberg
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 *  NOTES :
 *
 *
 *  Changes from 1.1 to 1.2 :
 *
 *  - Changes the stream operations semantics. See changes.txt
 *
 *  - stream records are now allocated on demand in the heap
 *
 *  - introduced the 'frame cache' to avoid Allocating/Freeing
 *    each frame, even tiny ones..
 *
 *  - support for thread-safety and re-entrancy
 *
 *    ( re-entrancy is there for information only.. )
 *
 *  Changes from 1.0 to 1.1 :
 *
 *  - defined the type TT_Stream for file handles
 *  - renamed ( and cleaned ) the API.
 *
 *  - caching and memory-mapped files use the same API :
 *
 *      TT_Access_Frame to notify
 *
 *  - only the interface was really rewritten. This component still
 *    only supports one opened file at a time.
 *
 ******************************************************************)

Unit TTFile;

interface

{$I TTCONFIG.INC}

uses FreeType,
     TTTypes,
     TTError;

  function  TTFile_Init : TError;
  procedure TTFile_Done;

 (*********************************************************************)
 (*                                                                   *)
 (*  Stream Functions                                                 *)
 (*                                                                   *)
 (*********************************************************************)

 function  TT_Open_Stream( name       : String;
                           var stream : TT_Stream ) : TError;
 (* Open a file and return a stream handle for it               *)
 (* should only be used for a new typeface object's main stream *)

 procedure TT_Close_Stream( var stream : TT_Stream );
 (* closes, then discards a stream, when it becomes unuseful *)
 (* should only be used for a typeface object's main stream  *)

 function  TT_Use_Stream( org_stream : TT_Stream;
                          var stream : TT_Stream ) : TError;
 (* notices the component that we're going to use the file   *)
 (* opened in 'org_stream', and report errors to the 'error' *)
 (* variable. the 'stream' variable is untouched, except in  *)
 (* re-entrant buids.                                        *)

 (* in re-entrant builds, the original file handle is duplicated *)
 (* to a new stream which reference is passed to the 'stream'    *)
 (* variable.. thus, each thread can have its own file cursor to *)
 (* access the same file concurrently..                          *)

 procedure TT_Flush_Stream( stream : TT_Stream );
 (* closes a stream's font handle. This is useful to save      *)
 (* system resources.                                          *)

 procedure TT_Done_Stream( stream : TT_Stream );
 (* notice the file component that we don't need to perform    *)
 (* file ops on the stream 'stream' anymore..                  *)
 (*                                                            *)
 (* in re-entrant builds, should also discard the stream       *)

 (*********************************************************************)
 (*                                                                   *)
 (*  File Functions                                                   *)
 (*                                                                   *)
 (*    the following functions perform file operations on the         *)
 (*    currently 'used' stream. In thread-safe builds, only one       *)
 (*    stream can be used at a time. Synchronisation is performed     *)
 (*    through the Use_Stream/Done_Stream functions                   *)
 (*                                                                   *)
 (*  Note:                                                            *)
 (*    re-entrant versions of these functions are only available      *)
 (*    in the C source tree. There, a macro is used to add a 'stream' *)
 (*    parameter to each of these routines..                          *)
 (*                                                                   *)
 (*********************************************************************)

 function TT_Read_File( var ABuff; ACount : Int ) : TError;
 (* Read a chunk of bytes directly from the file *)

 function TT_Seek_File( APos : LongInt ) : TError;
 (* Seek a new file position *)

 function TT_Skip_File( ADist : LongInt ) : TError;
 (* Skip to a new file position *)

 function TT_Read_At_File( APos : Long; var ABuff; ACount : Int ) : TError;
 (* Seek and read a chunk of bytes *)

 function TT_File_Size : Longint;

 function TT_File_Pos  : Longint;

 function TT_Stream_Size( stream : TT_Stream ) : longint;

 (*********************************************************************)
 (*                                                                   *)
 (*  Frame Functions                                                  *)
 (*                                                                   *)
 (*********************************************************************)

 function TT_Access_Frame( aSize : Int ) : TError;
 (* Access the next aSize bytes *)

 function TT_Check_And_Access_Frame( aSize : Int ) : TError;
 (* Access the next min(aSize,file_size-file_pos) bytes *)

 function TT_Forget_Frame :  TError;
 (* Forget the previously cached frame *)

 (* The four following functions should only be used after a *)
 (* TT_Access_Frame and before a TT_Forget_Frame             *)

 (* They do not provide error handling, intentionnaly, and are much faster *)
 (* moreover, they could be converted to MACROS in the C version           *)

 function GET_Byte   : Byte;
 function GET_Char   : ShortInt;
 function GET_Short  : Short;
 function GET_UShort : UShort;
 function GET_Long   : Long;
 function GET_ULong  : ULong;
 function GET_Tag4   : ULong;

implementation

uses
  TTMemory;

  (* THREADS: TTMutex, *)

const
  frame_cache_size = 2048;
  (* we allocate a single block where we'll place all of our frames *)
  (* instead of allocating an new block on each access. Note that   *)
  (* frames that are bigger than this constant are effectively      *)
  (* allocated in the heap..                                        *)

type
  PString = ^string;
  PFile   = ^FILE;
  PError  = ^TT_Error;

  PStream_Rec = ^TStream_Rec;
  TStream_Rec = record
                  name  : PString;  (* file pathname                     *)
                  open  : Boolean;  (* is the stream currently opened    *)
                  font  : PFILE;    (* file handle for opened stream     *)
                  base  : Longint;  (* base offset for embedding         *)
                  size  : Longint;  (* size of font in resource          *)
                  posit : Longint;  (* current offset for closed streams *)
                end;

var
  (* THREADS: File_Mutex : TMutex *)

  font_file  : PFile;
  cur_stream : PStream_Rec;

  current_frame : PByte;
  frame_cursor  : Longint;
  frame_size    : LongInt;

  dummy_error : TT_Error;

  frame_cache : PByte;

  function  TT_File_Size : Longint;
  begin
    TT_File_Size := FileSize( font_file^ );
  end;

  function TT_File_Pos : Longint;
  begin
    TT_File_Pos := FilePos( font_file^ );
  end;

  function TT_Stream_Size( stream : TT_Stream ) : longint;
  var
    rec : PStream_Rec;
  begin
    rec := PStream_Rec(stream);
    if rec = nil then
      TT_Stream_Size := 0
    else
      TT_Stream_Size := rec^.size;
  end;

(*******************************************************************
 *
 *  Function    :  TTFile_Init
 *
 *  Description :  Init the file component
 *
 *                 - create a file mutex for thread-safe builds
 *
 ******************************************************************)

 function TTFile_Init : TError;
 begin
   (* empty current file *)
   font_file  := nil;
   cur_stream := nil;

   (* empty frame *)
   current_frame := nil;
   frame_cursor  := 0;
   frame_size    := 0;

   (* create frame cache *)
   GetMem( frame_cache, frame_cache_size );

   TTFile_Init := Success;
 end;

(*******************************************************************
 *
 *  Function    :  TTFile_Done
 *
 *  Description :  Finalize the file component
 *
 *                 - destroys the file mutex for thread-safe builds
 *
 ******************************************************************)

 procedure TTFile_Done;
 begin
   (* empty current file *)
   font_file  := nil;
   cur_stream := nil;

   (* empty frame *)
   current_frame := nil;
   frame_cursor  := 0;
   frame_size    := 0;
 end;

(*******************************************************************
 *
 *  Function    :  Stream_New
 *
 *  Description :  allocates a new stream record
 *
 *  Input  :  stream  : the target stream variable
 *
 *  Output :  True on sucess.
 *
 ******************************************************************)

 function Stream_New( pathname   : string;
                      var stream : PStream_Rec ) : TError;
 var
   font : PFile;
   name : PString;
   len  : Integer;
 label
   Fail_Memory;
 begin
   name   := nil;
   font   := nil;
   stream := nil;
   len    := length(pathname)+1;

   (* allocate a new stream_rec in the heap *)
   if Alloc( pointer(stream), sizeof(TStream_Rec) ) or
      Alloc( pointer(font),   sizeof(FILE)        ) or
      Alloc( pointer(name),   len                 ) then
     goto Fail_Memory;

   move( pathname, name^, len );

   stream^.font  := font;
   stream^.name  := name;
   stream^.open  := false;
   stream^.base  := 0;
   stream^.size  := 0;
   stream^.posit := 0;

   Stream_New := Success;
   exit;

 Fail_Memory:
   Free( pointer(name)   );
   Free( pointer(font)   );
   Free( pointer(stream) );
   Stream_New := Failure;
 end;

(*******************************************************************
 *
 *  Function    :  Stream_Activate
 *
 *  Description :  activates a stream, if it needs it
 *
 *  Input  :  stream  : the target stream variable
 *
 *  Output :  Error condition
 *
 ******************************************************************)

 function Stream_Activate( stream : PStream_Rec ) : TError;
 var
   old_filemode : Long;
 begin
   Stream_Activate := Failure;
   if stream = nil then exit;

   with stream^ do
   begin
     Stream_Activate := Success;
     if open then exit;

     old_filemode    := System.FileMode;
     System.FileMode := 0;
     (* read-only mode *)

     Assign( font^, name^ );
     {$I-}
     Reset( font^, 1 );
     {$I+}

     System.FileMode := old_filemode;

     if IOResult <> 0 then
     begin
       error := TT_Err_Could_Not_Open_File;
       Stream_Activate := Failure;
       exit;
     end;

     open := true;
     base := 0;
     if size = -1 then size := FileSize(font^);

     if posit <> 0 then
       Seek( font^, posit );
   end;
 end;

(*******************************************************************
 *
 *  Function    :  Stream_Deactivate
 *
 *  Description :  closes an active stream
 *
 *  Input  :  stream  : the target stream variable
 *
 *  Output :  Error condition
 *
 ******************************************************************)

 function Stream_Deactivate( stream : PStream_Rec ) : TError;
 begin
   Stream_Deactivate := Failure;
   if stream = nil then exit;

   Stream_Deactivate := Success;
   if not stream^.open then exit;

   stream^.posit := FilePos( stream^.font^ );
   close( stream^.font^ );
   stream^.open := false;
 end;

(*******************************************************************
 *
 *  Function    :  Stream_Done
 *
 *  Description :  frees an active stream_rec
 *
 *  Input  :  stream  : the target stream variable
 *
 *  Output :  True on sucess.
 *
 *  Notes  : 'stream' is set to nil on exit..
 *
 ******************************************************************)

 function Stream_Done( var stream : PStream_Rec ) : TError;
 begin
   Stream_Deactivate( stream );

   Free( pointer(stream^.name) );
   Free( pointer(stream^.font) );
   Free( pointer(stream) );

   Stream_Done := Success;
 end;

(*******************************************************************
 *
 *  Function    :  TT_Open_Stream
 *
 *  Description :  opens the font file in a new stream
 *
 *  Input  :  stream  : target stream variable
 *            name    : file pathname
 *            error   : the variable that will be used to
 *                      report stream errors
 *
 *  Output :  True on sucess.
 *
 ******************************************************************)

 function TT_Open_Stream( name       : String;
                          var stream : TT_Stream ) : TError;
 var
   rec  : PStream_Rec;
   font : PFile;

   old_filemode : Long;
 begin
   TT_Open_Stream := Failure;

   if Stream_New( name, rec ) or
      Stream_Activate( rec )  then
     begin
       stream.z := nil;
       exit;
     end;

   cur_stream := rec;
   font_file  := rec^.font;
   stream     := TT_Stream(rec);

   TT_Open_Stream := Success;
 end;

(*******************************************************************
 *
 *  Function    : TT_Close_Stream
 *
 *  Description : Closes the font file and releases memory buffer
 *
 *  Input  :  None
 *
 *  Output :  True ( always )
 *
 ******************************************************************)

 procedure TT_Close_Stream( var stream : TT_Stream );
 begin
   if stream.z = nil then exit;

   Stream_Done( PStream_Rec(stream) );
   font_file  := nil;
   cur_stream := nil;
   stream.z   := nil;
 end;

(*******************************************************************
 *
 *  Function    : TT_Use_Stream
 *
 *  Description : Acquire the file mutex (blocking call)
 *
 *  Input  :  org_stream : original stream to use
 *            stream     : duplicate stream (in re-entrant builds)
 *                         set to 'org_stream' otherwise
 *            error      : error report variable
 *
 *  Output :  True on success. False on failure
 *
 ******************************************************************)

 function  TT_Use_Stream( org_stream : TT_Stream;
                          var stream : TT_Stream ) : TError;
 var
   rec : PStream_Rec;
 begin
   TT_Use_Stream := Failure;

   stream := org_stream;
   if org_stream.z = nil then exit;

   rec := PStream_Rec(stream);
   Stream_Activate(rec);
   cur_stream := rec;
   font_file  := rec^.font;

   TT_Use_Stream := Success;
 end;

(*******************************************************************
 *
 *  Function    : TT_Flush_Stream
 *
 *  Description : closes a stream
 *
 *  Input  :  stream : the stream
 *
 *  Output :  True on success. False on failure
 *
 ******************************************************************)

 procedure TT_Flush_Stream( stream : TT_Stream );
 begin
   if stream.Z <> nil then
     Stream_Deactivate( PStream_Rec(stream.z) );
 end;

(*******************************************************************
 *
 *  Function    : TT_Done_Stream
 *
 *  Description : Release the file mutex on a stream
 *
 *  Input  :  stream : the stream
 *
 *  Output :  True on success. False on failure
 *
 ******************************************************************)

 procedure TT_Done_Stream( stream : TT_Stream );
 begin
   if stream.z <> cur_stream then exit;
   cur_stream := nil;
   font_file  := nil;
 end;

(*******************************************************************
 *
 *  Function    : TT_Seek_File
 *
 *  Description : Seek the file cursor to a different position
 *
 *  Input  :  APos     new position on file
 *
 *  Output :  True on success. False if out of range
 *
 *  Notes  :  Does not set the error variable
 *
 ******************************************************************)

function TT_Seek_File( APos : LongInt ) : TError;
begin
  {$I-}
  Seek( Font_File^, APos );
  {$I+}
  if IOResult <> 0 then
    begin
      error        := TT_Err_Invalid_File_Offset;
      TT_Seek_File := Failure;
      exit;
    end;

  TT_Seek_File := Success;
end;

(*******************************************************************
 *
 *  Function    : TT_Skip_File
 *
 *  Description : Skip forward the file cursor
 *
 *  Input  :  ADist    number of bytes to skip
 *
 *  Output :  see Seek_Font_File
 *
 ******************************************************************)

function TT_Skip_File( ADist : LongInt ) : TError;
begin
  TT_Skip_File := TT_Seek_File( FilePos(Font_File^)+ADist );
end;

(*******************************************************************
 *
 *  Function    : TT_Read_File
 *
 *  Description : Reads a chunk of the file and copy it to memory
 *
 *  Input  :  ABuff     target buffer
 *            ACount    length in bytes to read
 *
 *  Output :  True if success. False if out of range
 *
 *  Notes  :  Current version prints an error message even if the
 *            debug state isn't on.
 *
 ******************************************************************)

function TT_Read_File( var ABuff; ACount : Int ) : TError;
begin
  TT_Read_File := Failure;
  {$I-}
  BlockRead( Font_File^, ABuff, ACount );
  {$I+}

  if IOResult <> 0 then
    begin
      error := TT_Err_Invalid_File_Read;
      exit;
    end;

  TT_Read_File := Success;
end;

(*******************************************************************
 *
 *  Function    : TT_Read_At_File
 *
 *  Description : Read file at a specified position
 *
 *  Input  :  APos     position to seek to before read
 *            ABuff    target buffer
 *            ACount   number of bytes to read
 *
 *  Output :  True on success. False if error.
 *
 *  Notes  :  prints an error message if seek failed.
 *
 ******************************************************************)

function TT_Read_At_File( APos : Long; var ABuff; ACount : Int ) : TError;
begin
  TT_Read_At_File := Failure;

  if TT_Seek_File( APos ) or
     TT_Read_File( ABuff, ACount ) then exit;

  TT_Read_At_File := Success;
end;

(*******************************************************************
 *
 *  Function    :  TT_Access_Frame
 *
 *  Description :  Notifies the component that we're going to read
 *                 aSize bytes from the current file position.
 *                 This function should load/cache/map these bytes
 *                 so that they will be addressed by the GET_xxx
 *                 functions easily.
 *
 *  Input  :  aSize   number of bytes to access.
 *
 *  Output :  True on success. False on failure
 *
 *            The function fails is the byte range is not within the
 *            the file, or if there is not enough memory to cache
 *            the bytes properly ( which usually means that aSize is
 *            too big in both cases ).
 *
 *            It will also fail if you make two consecutive calls
 *            to TT_Access_Frame, without a TT_Forget_Frame between
 *            them.
 *
 ******************************************************************)

 function TT_Access_Frame( aSize : Int ) : TError;
 var
   readBytes : Longint;
 begin
   TT_Access_Frame := Failure;

   if current_frame <> nil then
   begin
     error := TT_Err_Nested_Frame_Access;
     exit;
   end;
   (* We already are accessing one frame *)

   if aSize > frame_cache_size then
     GetMem( current_frame, aSize )
   else
     current_frame := frame_cache;

   if TT_Read_File( current_frame^, aSize ) then
   begin
     if aSize > frame_cache_size then
       FreeMem( current_frame, aSize );

     current_frame := nil;
     exit;
   end;

   frame_size   := aSize;
   frame_cursor := 0;

   TT_Access_Frame := Success;
 end;

(*******************************************************************
 *
 *  Function    :  TT_Check_And_Access_Frame
 *
 *  Description :  Notifies the component that we're going to read
 *                 aSize bytes from the current file position.
 *                 This function should load/cache/map these bytes
 *                 so that they will be addressed by the GET_xxx
 *                 functions easily.
 *
 *  Input  :  aSize   number of bytes to access.
 *
 *  Output :  True on success. False on failure
 *
 *            The function fails is the byte range is not within the
 *            the file, or if there is not enough memory to cache
 *            the bytes properly ( which usually means that aSize is
 *            too big in both cases ).
 *
 *            It will also fail if you make two consecutive calls
 *            to TT_Access_Frame, without a TT_Forget_Frame between
 *            them.
 *
 *
 * NOTE :  The only difference with TT_Access_Frame is that we check
 *         that the frame is within the current file.  We otherwise
 *         truncate it..
 *
 ******************************************************************)

 function TT_Check_And_Access_Frame( aSize : Int ) : TError;
 var
   readBytes : Longint;
 begin
   TT_Check_And_Access_Frame := Failure;

   if current_frame <> nil then
   begin
     error := TT_Err_Nested_Frame_Access;
     exit;
   end;
   (* We already are accessing one frame *)

   readBytes := TT_File_Size - TT_File_Pos;
   if aSize > readBytes then aSize := readBytes;

   if aSize > frame_cache_size then
     GetMem( current_frame, aSize )
   else
     current_frame := frame_cache;

   if TT_Read_File( current_frame^, aSize ) then
   begin
     if aSize > frame_cache_size then
       FreeMem( current_frame, aSize );
     exit;
   end;

   frame_size   := aSize;
   frame_cursor := 0;

   TT_Check_And_Access_Frame := Success;
 end;

(*******************************************************************
 *
 *  Function    :  TT_Forget_Frame
 *
 *  Description :  Releases a cached frame after reading
 *
 *  Input  :  None
 *
 *  Output :  True on success. False on failure
 *
 ******************************************************************)

 function TT_Forget_Frame : TError;
 begin
   TT_Forget_Frame := Failure;

   if current_frame = nil then exit;

   if frame_size > frame_cache_size then
     FreeMem( current_frame, frame_size );

   frame_size    := 0;
   current_frame := nil;
   frame_cursor  := 0;
 end;

(*******************************************************************
 *
 *  Function    :  GET_Byte
 *
 *  Description :  Extracts a byte from the current file frame
 *
 *  Input  :  None
 *
 *  Output :  Extracted Byte.
 *
 *  NOTES : We consider that the programmer is intelligent enough
 *          not to try to get a byte that is out of the frame. Hence,
 *          we provide no bounds check here. (A misbehaving client
 *          could easily page fault using this call).
 *
 ******************************************************************)

 function GET_Byte : Byte;
 begin
   GET_Byte := current_frame^[frame_cursor];
   inc( frame_cursor );
 end;

(*******************************************************************
 *
 *  Function    :  GET_Char
 *
 *  Description :  Extracts a signed byte from the current file frame
 *
 *  Input  :  None
 *
 *  Output :  Extracted char.
 *
 *  NOTES : We consider that the programmer is intelligent enough
 *          not to try to get a byte that is out of the frame. Hence,
 *          we provide no bounds check here. (A misbehaving client
 *          could easily page fault using this call).
 *
 ******************************************************************)

 function GET_Char : ShortInt;
 begin
   GET_Char := ShortInt( current_frame^[frame_cursor] );
   inc( frame_cursor );
 end;

(*******************************************************************
 *
 *  Function    :  GET_Short
 *
 *  Description :  Extracts a short from the current file frame
 *
 *  Input  :  None
 *
 *  Output :  Extracted short.
 *
 *  NOTES : We consider that the programmer is intelligent enough
 *          not to try to get a byte that is out of the frame. Hence,
 *          we provide no bounds check here. (A misbehaving client
 *          could easily page fault using this call).
 *
 ******************************************************************)

 function GET_Short : Short;
 begin
   GET_Short := (Short(current_frame^[ frame_cursor ]) shl 8) or
                 Short(current_frame^[frame_cursor+1]);
   inc( frame_cursor, 2 );
 end;

(*******************************************************************
 *
 *  Function    :  GET_UShort
 *
 *  Description :  Extracts an unsigned  short from the frame
 *
 *  Input  :  None
 *
 *  Output :  Extracted ushort.
 *
 *  NOTES : We consider that the programmer is intelligent enough
 *          not to try to get a byte that is out of the frame. Hence,
 *          we provide no bounds check here. (A misbehaving client
 *          could easily page fault using this call).
 *
 ******************************************************************)

 function GET_UShort : UShort;
 begin
   GET_UShort := (UShort(current_frame^[ frame_cursor ]) shl 8) or
                  UShort(current_frame^[frame_cursor+1]);
   inc( frame_cursor, 2 );
 end;

(*******************************************************************
 *
 *  Function    :  GET_Long
 *
 *  Description :  Extracts a long from the frame
 *
 *  Input  :  None
 *
 *  Output :  Extracted long.
 *
 *  NOTES : We consider that the programmer is intelligent enough
 *          not to try to get a byte that is out of the frame. Hence,
 *          we provide no bounds check here. (A misbehaving client
 *          could easily page fault using this call).
 *
 ******************************************************************)

 function GET_Long : Long;
 begin
   GET_Long := (Long(current_frame^[ frame_cursor ]) shl 24) or
               (Long(current_frame^[frame_cursor+1]) shl 16) or
               (Long(current_frame^[frame_cursor+2]) shl 8 ) or
               (Long(current_frame^[frame_cursor+3])       );
   inc( frame_cursor, 4 );
 end;

(*******************************************************************
 *
 *  Function    :  GET_ULong
 *
 *  Description :  Extracts an unsigned long from the frame
 *
 *  Input  :  None
 *
 *  Output :  Extracted ulong.
 *
 *  NOTES : We consider that the programmer is intelligent enough
 *          not to try to get a byte that is out of the frame. Hence,
 *          we provide no bounds check here. (A misbehaving client
 *          could easily page fault using this call).
 *
 ******************************************************************)

 function GET_ULong : ULong;
 begin
   GET_ULong := (ULong(current_frame^[ frame_cursor ]) shl 24) or
                (ULong(current_frame^[frame_cursor+1]) shl 16) or
                (ULong(current_frame^[frame_cursor+2]) shl 8 ) or
                (ULong(current_frame^[frame_cursor+3])       );
   inc( frame_cursor, 4 );
 end;

(*******************************************************************
 *
 *  Function    :  GET_Tag4
 *
 *  Description :  Extracts a Tag from the frame
 *
 *  Input  :  None
 *
 *  Output :  Extracted 4 byte Tag.
 *
 *  NOTES : We consider that the programmer is intelligent enough
 *          not to try to get a byte that is out of the frame. Hence,
 *          we provide no bounds check here. (A misbehaving client
 *          could easily page fault using this call).
 *
 ******************************************************************)

 function GET_Tag4 : ULong;
 var
   C : array[0..3] of Byte;
 begin
   move ( current_frame^[frame_cursor], c, 4 );
   inc( frame_cursor, 4 );

   GET_Tag4 := ULong(C);
end;

end.
