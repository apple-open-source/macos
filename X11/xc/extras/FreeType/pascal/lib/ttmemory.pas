(*******************************************************************
 *
 *  TTMemory.Pas                                             2.1
 *
 *    Memory management component (specification)
 *
 *  Copyright 1996 David Turner, Robert Wilhelm and Werner Lemberg
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 *  Differences between 2.1 and 2.0 :
 *
 *  - Added a memory mutex to make the component thread-safe
 *
 *  Differences between 2.0 and 1.1 :
 *
 *  - The growing heap was completely removed in version 2.0
 *
 *  - The support for small mini-heaps may be re-introduced later
 *    to allow the storage of several consecutive arrays in one
 *    single block.
 *
 *  IMPORTANT NOTICE :
 *
 *  The Alloc and Free functions mimic their C equivalent,
 *  however, some points must be noticed :
 *
 *  - both functions return a boolean. As usual, True indicates
 *    success, while False indicates failure.
 *
 *  - the Alloc function puts a small header on front of each
 *    allocated block. The header contains a magic cookie and
 *    the size of the allocated block. This allows calls to
 *    Free without passing a block size as an argument, and thus
 *    reduces the risks of memory leaks.
 *
 *  - it is possible to call Free with a nil pointer, in which
 *    case nothing happens, and the result is set to True (success)
 *
 *    The pointer is set to nil after a call to Free in all cases.
 *
 *    This is done to clear the destructors code, allowing
 *
 *      if (pointer) then
 *      begin
 *        Free(pointer);
 *        pointer := nil;
 *      end;
 *
 *    to be replaced by a single line :
 *
 *      Free(pointer);
 *
 *
 ******************************************************************)

unit TTMemory;

interface

uses TTTypes;

{$I TTCONFIG.INC}

type
  TMarkRecord = record
                  Magic : longint;
                  Top   : integer;
                end;

const
  Font_Pool_Allocated : boolean = False;

  function Alloc( var P; size : Longint ) : TError;
  (* Allocates a new memory block in the current heap of 'size' bytes *)
  (* - returns failure if no memory is left in the heap               *)

  procedure  Free ( var P );
  (* Releases a block previously allocated through 'Alloc' *)
  (* - returns True (success) of P is nil before the call  *)
  (* - sets P to nil before exit                           *)

  function  TTMemory_Init : TError;
  procedure TTMemory_Done;

implementation

uses TTError;

type
  TByte = array[0..0] of Byte;
  PByte = ^TByte;

  PBlock_Header = ^TBlock_Header;
  TBlock_Header = record
                    magic : Longint;  (* magic cookie                     *)
                    size  : Longint;  (* allocated size, including header *)
                  end;

  TBlock_Headers = array[0..1] of TBlock_Header;
  PBlock_Headers = ^TBlock_Headers;

  (* Note that the Turbo-Pascal GetMem/FreeMem functions use no block *)
  (* headers. That's why a byte size is needed for FreeMem. Thus, we  *)
  (* do not waste space here compared to a C malloc implementation    *)

const
  Mark_Magic = $BABE0007;
  (* This is the magic cookie used to recognize valide allocated blocks *)

  Header_Size = sizeof(TBlock_Header);

 (************************************************************************)
 (*                                                                      *)
 (* MyHeapErr :                                                          *)
 (*                                                                      *)
 (*   By default, a call to GetMem with insufficient memory left will    *)
 (*   generate a runtime error. We define here a function that is used   *)
 (*   to allow GetMem to return nil in such cases.                       *)
 (*                                                                      *)
 (************************************************************************)

 function MyHeapErr( Size: Integer ): Integer; far;
 begin
   MyHeapErr := 1;
 end;

(*******************************************************************
 *
 *  Function    :  Alloc
 *
 *  Description :  allocate a new block in the current heap
 *
 *  Notes       :  If you want to replace this function with
 *                 your own, please be sure to respect these
 *                 simple rules :
 *
 *                 - P must be set to nil in case of failure
 *
 *                 - The allocated block must be zeroed !
 *
 *****************************************************************)

 function Alloc( var P; size : Longint ) : TError;
 var
   OldHeapError : Pointer;

   L  : Longint;
   P2 : Pointer;
 begin
 {$IFNDEF DELPHI32}
   OldHeapError := HeapError;
   HeapError    := @MyHeapErr;
 {$ENDIF}

   L := ( size + Header_Size + 3 ) and -4;

   {$IFDEF MSDOS}
   if L shr 16 <> 0 then
   begin
     Writeln('Sorry, but this font is too large to be handled by a 16-bit program' );
     Alloc := Failure;
   end;
   {$ENDIF}

   GetMem( Pointer(P), L );

 {$IFNDEF DELPHI32}
   HeapError := OldHeapError;
 {$ENDIF}

   if Pointer(P) <> nil then
     begin
       PBlock_Headers(P)^[0].magic := Mark_Magic;
       PBlock_Headers(P)^[0].size  := L;

       P2 := Pointer( @(PBlock_Headers(P)^[1]) );

       {$IFDEF MSDOS}
       if (ofs(P2^) <> ofs(Pointer(P)^)+Header_Size) or
          (seg(P2^) <> seg(Pointer(P)^)) then
         begin
           Writeln('AAARGH !!: Sorry, but I have problems with 64 Kb segments');
           halt(1);
         end;
       {$ENDIF}

       Pointer(P) := P2;
       fillchar( P2^, size, 0 );
       (* zero block *)

       Alloc := Success;
     end
   else
     Alloc := Failure;

 end;


(*******************************************************************
 *
 *  Function    :  Free
 *
 *  Description :  frees a block that was previsouly allocated
 *                 by the Alloc function
 *
 *  Notes  :  Doesn't need any size parameter.
 *
 *  If you want to replace this function with your own, please
 *  be sure to respect these two rules :
 *
 *  - the argument pointer can be nil, in which case the function
 *    should return immediately, with a success report.
 *
 *  - the pointer P should be set to nil when exiting the
 *    function, except in case of failure.
 *
 *****************************************************************)

 procedure Free( var P );
 var
   head : PBlock_Header;
   i    : Integer;
   size : Longint;
 begin
   if Pointer(P) = nil then exit;

   i    := -1;
   head := @(PBlock_Headers(P)^[i]);
   (* A hack to get the header in PB, as the line             *)
   (*  @(PBlock_Headers(P)^[-1] would give a 'constant error' *)
   (* at compile time. I'm unsure this works correctly in BP  *)

   if head^.magic <> Mark_Magic then
   begin
     (* PANIC : An invalid Free call *)
     Writeln('Invalid Free call');
     halt(1);
   end;

   size := head^.size;

   head^.magic := 0;  (* cleans the header *)
   head^.size  := 0;

   FreeMem( head, size );

   Pointer(P) := nil;
 end;

(*******************************************************************
 *
 *  Function    : TTMemory_Init
 *
 *  Description : Initializes the Memory component
 *
 *****************************************************************)

 function TTMemory_Init : TError;
 begin
   (* nothing to be done *)
   TTMemory_Init := Success;
 end;

(*******************************************************************
 *
 *  Function    : TTMemory_Done
 *
 *  Description : Finalize the memory component
 *
 *****************************************************************)

 procedure TTMemory_Done;
 begin
   (* nothing to be done *)
 end;

end.
