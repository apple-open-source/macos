(*******************************************************************
 *
 *  ttcache.pas                                                 1.0
 *
 *    Generic object cache
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
 *
 *  This component defines and implement object caches.
 *
 *  An object class is a structure layout that encapsulate one
 *  given type of data used by the FreeType engine. Each object
 *  class is completely described by :
 *
 *    - a 'root' or 'leading' structure containing the first
 *      important fields of the class. The root structure is
 *      always of fixed size.
 *
 *      It is implemented as a simple C structure, and may
 *      contain several pointers to sub-tables that can be
 *      sized and allocated dynamically.
 *
 *      examples : TFace, TInstance, TGlyph & TExecution_Context
 *                 ( defined in 'ttobjs.h' )
 *
 *    - we make a difference between 'child' pointers and 'peer'
 *      pointers. A 'child' pointer points to a sub-table that is
 *      owned by the object, while a 'peer' pointer points to any
 *      other kind of data the object isn't responsible for.
 *
 *      An object class is thus usually a 'tree' of 'child' tables.
 *
 *    - each object class needs a constructor and a destructor.
 *
 *      A constructor is a function which receives the address of
 *      freshly allocated and zeroed object root structure and
 *      'builds' all the valid child data that must be associated
 *      to the object before it becomes 'valid'.
 *
 *      A destructor does the inverse job : given the address of
 *      a valid object, it must discards all its child data and
 *      zero its main fields (essentially the pointers and array
 *      sizes found in the root fields).
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 ******************************************************************)

unit TTCache;

interface

uses TTError, TTTypes;

type

  (* Simple list node record. A List element is said to be 'unlinked' *)
  (* when it doesn't belong to any list                               *)
  (*                                                                  *)
  PList_Element = ^TList_Element;
  TList_Element = record

     next : PList_Element; (* Pointer to next element of list *)
     data : Pointer;       (* Pointer to the listed object    *)
  end;


  (* Simple singly-linked list record *)
  (* LIFO - style, no tail field      *)
  TSingle_List = PList_Element;


  TConstructor = function(  _object : Pointer;
                            _parent : Pointer  ) : TError;

  TDestructor = function( _object : Pointer ) : TError;

  PCache_Class = ^TCache_Class;
  TCache_Class = record
                   Object_Size : Int;
                   Idle_Limit  : Int;
                   Init        : TConstructor;
                   Done        : TDestructor;
                 end;
  (* A Cache class record holds the data necessary to define *)
  (* a cache kind.                                           *)

  PCache = ^TCache;
  TCache = record
             clazz      : PCache_Class;  (* 'class' reserved in VP & Delphi *)
             active     : TSingle_List;
             idle       : TSingle_List;
             idle_count : Int;
           end;

  (* An object cache holds two lists tracking the active and *)
  (* idle objects that are currently created and used by the *)
  (* engine. It can also be 'protected' by a mutex           *)

  function Cache_Create( var clazz : TCache_Class;
                         var cache : TCache        ) : TError;
  (* Initialize a new cache named 'cache', of class 'clazz', and   *)
  (* protected by the 'lock' mutex. Note that the mutex is ignored *)
  (* as the pascal version isn't thread-safe                       *)

  function Cache_Destroy( var cache : TCache ) : TError;
  (* Destroys a cache and all its listed objects *)

  function Cache_New( var cache      : TCache;
                      var new_object : Pointer;
                      parent_data    : Pointer ) : TError;
  (* Extracts a new object from the cache. *)

  function Cache_Done( var cache : TCache; obj : Pointer ) : TError;
  (* returns an object to the cache, or discards it depending *)
  (* on the cache class' "idle_limit" field                   *)

  (********************************************************)
  (*                                                      *)
  (* Two functions used to manage list elements           *)
  (*                                                      *)
  (* Note that they're thread-safe in multi-threaded      *)
  (* builds.                                              *)
  (*                                                      *)

  function  Element_New : PList_Element;
  (* Returns a new list element, either fresh or recycled *)
  (* Note : the returned element is unlinked              *)

  procedure Element_Done( element : PList_Element );
  (* Recycles or discards an element.                     *)
  (* Note : The element must be unlinked !!               *)




  function  TTCache_Init : TError;

  function  TTCache_Done : TError;


implementation

uses TTMemory;

const
  Null_Single_List = nil;

var
  Free_Elements : PList_Element;

(*******************************************************************
 *
 *  Function    :  Element_New
 *
 *  Description :  Gets a new ( either fresh or recycled ) list
 *                 element. The element is unlisted.
 *
 *  Notes  :  returns nil if out of memory
 *
 *****************************************************************)

  function Element_New : PList_Element;
  var
    element : PList_Element;
  begin
    (* LOCK *)

    if Free_Elements <> nil then
      begin
        element       := Free_Elements;
        Free_Elements := element^.next;
      end
    else
      begin
        Alloc( element, sizeof(TList_Element) );
        (* by convention, an allocated block is always zeroed *)
        (* the fields of element need not be set to NULL then *)
      end;

    (* UNLOCK *)

    Element_New := element;
  end;

(*******************************************************************
 *
 *  Function    :  Element_Done
 *
 *  Description :  recycles an unlisted list element
 *
 *  Notes  :  Doesn't check that the element is unlisted
 *
 *****************************************************************)

  procedure Element_Done( element : PList_Element );
  begin
    (* LOCK *)

    element^.next := Free_Elements;
    Free_Elements := element;

    (* UNLOCK *)
  end;


(*******************************************************************
 *
 *  Function    :  Cache_Create
 *
 *  Description :  Create a new cache object
 *
 *****************************************************************)
  function Cache_Create( var clazz : TCache_Class;
                         var cache : TCache       ) : TError;
  begin
    cache.clazz      := @clazz;
    cache.idle_count := 0;
    cache.active     := Null_Single_List;
    cache.idle       := Null_Single_List;

    Cache_Create := Success;
  end;


(*******************************************************************
 *
 *  Function    :  Cache_Destroy
 *
 *  Description :  Destroy a given cache object
 *
 *****************************************************************)
  function Cache_Destroy( var cache : TCache ) : TError;
  var
    destroy : TDestructor;
    current : PList_Element;
    next    : PList_Element;
  begin
    (* now destroy all active and idle listed objects *)

    destroy := cache.clazz^.done;

    (* active list *)
    current := cache.active;
    while current <> nil do
    begin
      next := current^.next;
      destroy( current^.data );
      Free( current^.data );
      Element_Done( current );
      current := next;
    end;
    cache.active := Null_SIngle_List;

    (* idle list *)
    current := cache.idle;
    while current <> nil do
    begin
      next := current^.next;
      destroy( current^.data );
      Free( current^.data );
      Element_Done( current );
      current := next;
    end;
    cache.idle := Null_Single_List;

    cache.clazz      := nil;
    cache.idle_count := 0;

    Cache_Destroy := Success;
  end;


(*******************************************************************
 *
 *  Function    :  Cache_New
 *
 *  Description :  Extracts one 'new' object from a cache
 *
 *  Notes  :  The 'parent_data' pointer is passed to the object's
 *            initialiser when the new object is created from
 *            scratch. Recycled objects do not use this pointer
 *
 *****************************************************************)
  function Cache_New( var cache      : TCache;
                      var new_object : Pointer;
                      parent_data    : Pointer ) : TError;
  var
    error   : TError;
    current : PList_Element;
    obj     : Pointer;
  label
    Fail;
  begin
    (* LOCK *)
    current := cache.idle;
    if current <> nil then
    begin
      cache.idle := current^.next;
      dec( cache.idle_count )
    end;
    (* UNLOCK *)

    if current = nil then
      begin
        (* if no object was found in the cache, create a new one *)
        if Alloc( obj, cache.clazz^.object_size ) then exit;

        current := Element_New;
        if current = nil then goto Fail;

        current^.data := obj;

        error := cache.clazz^.init( obj, parent_data );
        if error then goto Fail;
      end;

    (* LOCK *)
    current^.next := cache.active;
    cache.active  := current;
    (* UNLOCK *)

    new_object := current^.data;

    Cache_New := Success;
    exit;

  Fail:
    Free( obj );
    Cache_New := Failure;
  end;

(*******************************************************************
 *
 *  Function    :  Cache_Done
 *
 *  Description :  Discards an object intro a cache
 *
 *****************************************************************)

  function Cache_Done( var cache : TCache; obj : Pointer ) : TError;
  var
    error   : TError;
    element : PList_Element;
    parent  : ^PList_Element;
  label
    Suite;
  begin
    Cache_Done := failure;

    (* find element in list *)
    (* LOCK *)
    parent  := @cache.active;
    element := parent^;
    while element <> nil do
    begin
      if element^.data = obj then
      begin
        parent^ := element^.next;
        (* UNLOCK *)
        goto Suite;
      end;
      parent  := @element^.next;
      element := parent^;
    end;
    (* UNLOCK *)

    (* Element wasn't found !! *)
    {$IFDEF DEBUG}
    {$ENDIF}
    exit;

  Suite:
    if ( cache.idle_count >= cache.clazz^.idle_limit ) then
      begin
        (* destroy the object when the cache is full *)
        cache.clazz^.done( element^.data );
        Free( element^.data );
        Element_Done( element );
      end
    else
      begin
        (* simply add the object to the idle list *)
        (* LOCK *)
        element^.next := cache.idle;
        cache.idle    := element;
        inc( cache.idle_count );
        (* UNLOCK *)
      end;

    Cache_Done := Success;
  end;


  function  TTCache_Init : TError;
  begin
    Free_Elements := nil;
    TTCache_Init  := Success;
  end;


  function  TTCache_Done : TError;
  var
    current, next : PList_ELement;
  begin
    current := free_elements;
    while current <> nil do
    begin
      next := current^.next;
      Free( current );
      current := next;
    end;
    TTCache_Done := success;
  end;

end.
