/* Copyright (C) 1999, 2000, 2001, 2002, 2003 Apple Computer, Inc.
   Copyright (C) 1997, 2001 Free Software Foundation, Inc.

This file is part of KeyMgr.

KeyMgr is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

In addition to the permissions in the GNU General Public License,
Apple Computer and the Free Software Foundation gives you unlimited
permission to link the compiled version of this file into combinations
with other programs, and to distribute those combinations without any
restriction coming from the use of this file.  (The General Public
License restrictions do apply in other respects; for example, they
cover modification of the file, and distribution when not linked into
a combine executable.)

KeyMgr is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with KeyMgr; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

/* The FSF-copyright part is the definition of 'struct
   old_object'.  It was taken from unwind-dw2-fde.h from GCC 3.1.  */

/*
 * keymgr - Create and maintain process-wide global data known to 
 *	    all threads across all dynamic libraries. 
 *
 */

#include <mach-o/dyld.h>
#include <pthread.h>
#include <stdlib.h>
#include "keymgr.h"

/*
 * __keymgr_global - contains pointers to keymgr data that needs to be 
 * global and known across all dlls/plugins. The array elements are defined
 * as follows:
 * Element #0:	A pthreads semaphore for accessing the keymgr global data.
 *		This would replace keymgr static semaphore.
 *
 * Element #1:	Pointer to keymgr global list. This pointer was previously
 *		contained in __eh_global_dataptr.
 *
 * Element #2:	Pointer to keymgr information node. This is a new pointer.
 *		initially the information node would contain two fields:
 *		The number of words in the information node and 
 *		an API version number.
 *
 */

void *__keymgr_global[3] = { (void *) 0 };

#define GET_KEYMGR_LIST_ROOT()		((_Tkey_Data *)__keymgr_global[1])
#define SET_KEYMGR_LIST_ROOT(ptr)	(__keymgr_global[1] = ((void *) (ptr)))

#define GET_KEYMGR_LIST_MUTEX_PTR()	((pthread_mutex_t *) __keymgr_global[0])
#define SET_KEYMGR_LIST_MUTEX_PTR(ptr)	(__keymgr_global[0] = (void *)(ptr))
#define INIT_KEYMGR_LIST_MUTEX()	(SET_KEYMGR_LIST_MUTEX_PTR(malloc(sizeof(pthread_mutex_t))), \
					 (*GET_KEYMGR_LIST_MUTEX_PTR() = _local_mutex_initializer))
#define LOCK_KEYMGR_LIST_MUTEX()	(pthread_mutex_lock(GET_KEYMGR_LIST_MUTEX_PTR()) ? \
					 (abort(), 0) : 0 )
#define UNLOCK_KEYMGR_LIST_MUTEX()	(pthread_mutex_unlock(GET_KEYMGR_LIST_MUTEX_PTR()) ? \
					 (abort(), 0) : 0 )


#define LOCK_KEYMGR_INIT()	(pthread_mutex_lock(&_keymgr_init_lock) ? (abort(), 0) : 0 )
#define UNLOCK_KEYMGR_INIT()	(pthread_mutex_unlock(&_keymgr_init_lock) ? (abort(), 0) : 0 )

#define KEYMGR_CREATE_THREAD_DATA_KEY(thread_key) \
		((pthread_key_create(&thread_key,NULL) != 0) ?  \
				(abort(),0) : 0) 

/* Version of the above which specifies free () as the destructor.  */
#define KEYMGR_CREATE_MALLOCED_THREAD_DATA_KEY(thread_key) \
		((pthread_key_create(&thread_key, free) != 0) ?  \
				(abort(),0) : 0) 

#define KEYMGR_SET_THREAD_DATA(key,data)	\
		 ((pthread_setspecific((key), (data)) != 0) ? \
				(abort(),0) : 0)
											
#define KEYMGR_GET_THREAD_DATA(key)	(pthread_getspecific(key))
					 
#define INIT_KEYMGR_NODE_LOCK(node)	(node->thread_lock = _local_mutex_initializer) 

#define LOCK_KEYMGR_NODE(node)		\
		((pthread_mutex_lock(&(node->thread_lock)) ? \
		(abort(), 0) : 0 ),((node)->flags |= NM_LOCKED),\
		((node)->owning_thread = pthread_self()),node->refcount++)
					 					
#define UNLOCK_KEYMGR_NODE(node)	\
		(((node)->flags &= ~NM_LOCKED),((node)->refcount--),\
		 (pthread_mutex_unlock(&(node->thread_lock)) ? \
				 (abort(), 0) : 0 ))
					 					 
#define LOCK_IS_MINE(node)	(((node)->flags & NM_LOCKED)&& \
				 ((node)->owning_thread == pthread_self()))
					 
#define GET_KEYMGR_INFO_NODE_PTR()	((_Tinfo_Node *)__keymgr_global[2])
#define SET_KEYMGR_INFO_NODE_PTR(ptr)	(__keymgr_global[2] = (void *)(ptr))


#define CHECK_KEYMGR_INIT()		if (!_keymgr_init_mutex) _init_keymgr() ;

#define KEYMGR_NODE_FLAGS(node,pflags)	((node)->flags & (pflags))

static int _keymgr_init_mutex = 0 ;

static pthread_mutex_t _local_mutex_initializer = PTHREAD_MUTEX_INITIALIZER ;
static pthread_mutex_t _keymgr_init_lock = PTHREAD_MUTEX_INITIALIZER ;

/*
 * Base node kind for keymgr node list.
 * Also used to represent per thread variables (NODE_THREAD_SPECIFIC_DATA)
 */
 
typedef struct _Skey_data {
	struct _Skey_data *next ;
	unsigned int handle ;		/*key id of variable.*/
	void *ptr ;			/*Pointer to variable data (or variable data).*/
	TnodeKind node_kind;		/*What kind of variable is this?*/
	pthread_mutex_t thread_lock;	/*Semaphore for this specific variable.*/
	pthread_t owning_thread ;	/*the thread that owns, if tracking enabled.*/
	unsigned int refcount ;		/*If recursion has been enabled, reference count.*/
	unsigned int flags ;		/*Flags controlling behavior of variable.*/
	} _Tkey_Data ;

	
typedef struct _Sinfo_Node {
	unsigned int size ; 		/*size of this node*/
	unsigned short major_version ; 	/*API major version.*/
	unsigned short minor_version ;	/*API minor version.*/
	} _Tinfo_Node ;
	

/*
 * Initialize keymgr and its related semaphore. 
 * It is very important that the client be running
 * single threaded until this initialization completes.
 * Thus, threads should not be spawned at module initialization time.
 */


void _init_keymgr(void) {

	_Tinfo_Node *ti ;
	unsigned long address ;
	void *module ;


	/* this semaphore is just an accelerator to prevent repeated calls to pthreads.*/
	
	if (_keymgr_init_mutex)
		return ;

	/*
	 * If keymgr is living in system framework, it must be fully bound 
	 * before being used. Because calling dyld could cause another
	 * user initialization routine to run, we don't set the initialization
	 * semaphore until we get back. Once we are back, we are gauranteed not to
	 * recurse. We then only have to worry about other threads.
	 */

	_dyld_lookup_and_bind_fully("__init_keymgr", &address, &module); 
	

	/*
	 * Check to see if a recursion occured and ran the initialization.
	 */
	 		
	if (_keymgr_init_mutex)
		return ;

	 /*Lock out other threads.*/
	LOCK_KEYMGR_INIT() ;
	
	/*
	 * Check and see if another thread has already done the job.
	 */
	 
	if (_keymgr_init_mutex) {
		UNLOCK_KEYMGR_INIT() ;
		return ;
		}

	_keymgr_init_mutex++ ;
	
	/* 
	 * now we check to see if another instance has already initialized keymgr.
	 * If keymgr is in the system framework, this will always be false.
	 */
	
	if (GET_KEYMGR_INFO_NODE_PTR()) {
		UNLOCK_KEYMGR_INIT() ;
		return ;
		}
		
	INIT_KEYMGR_LIST_MUTEX() ;
	LOCK_KEYMGR_LIST_MUTEX() ;
	ti = malloc(sizeof(_Tinfo_Node)) ;
	ti->size = sizeof(_Tinfo_Node) ;
	ti->major_version = KEYMGR_API_REV_MAJOR ;
	ti->minor_version = KEYMGR_API_REV_MINOR ;
	SET_KEYMGR_INFO_NODE_PTR(ti) ;
	UNLOCK_KEYMGR_LIST_MUTEX() ;

	UNLOCK_KEYMGR_INIT() ;
	}


/*
 * Try to get module initialization to initialize keymgr before 
 * anyone uses it. If not, the api calls check for lack of initialization
 * force initialization to occur anyway. Initialization invoked
 * by the API calls is more dangerous since there is a small
 * window where two threads could simultaneously enter the 
 * initialization region. In general, to be safe,
 * this initialization should be run before multi-threading
 * is started.
 */	
	
#pragma CALL_ON_LOAD	_init_keymgr

/*
 * These routines provide  customized locking and
 * unlocking services. If enhanced locking is chosen
 * then recursion can be allowed or disallowed.
 */
 
static void _keymgr_lock(_Tkey_Data *node) {

	if (KEYMGR_NODE_FLAGS(node,NM_ENHANCED_LOCKING)) {
		if (LOCK_IS_MINE(node)) {
			if (KEYMGR_NODE_FLAGS(node,NM_RECURSION_ILLEGAL)) {
				abort() ;
				}
			else if (KEYMGR_NODE_FLAGS(node,NM_ALLOW_RECURSION)) {
				if(node->refcount) {
					node->refcount++ ;
					return ;
					}
				}
				
			else ;
			}
		}
		
	else ;
		
		
	LOCK_KEYMGR_NODE(node) ;
	}
	
	
static void _keymgr_unlock(_Tkey_Data *node) {

	if (node->refcount > 1) {
		node->refcount-- ;
		return ;
		}

	UNLOCK_KEYMGR_NODE(node) ;
	}
	
	
/*
 * These routines are the layer 1 (lowest level) routines.
 * No layer 1 routine should perform any locking/unlocking.
 */


static _Tkey_Data *_keymgr_get_key_element(unsigned int key) {

	_Tkey_Data  *keyArray ;

	for (keyArray = GET_KEYMGR_LIST_ROOT()  ; keyArray != NULL ; keyArray = keyArray->next) {
	      if (keyArray->handle == key)
		 break ;
	      }

	return(keyArray) ;
	}
    


static _Tkey_Data *_keymgr_create_key_element(unsigned int key, void *ptr, TnodeKind kind) {

	_Tkey_Data  *keyArray ;

	keyArray = (_Tkey_Data *) malloc(sizeof(_Tkey_Data)) ;
	keyArray->handle = key ;
	keyArray->ptr = ptr ;
	keyArray->node_kind = kind ;
	keyArray->owning_thread = pthread_self() ;
	keyArray->refcount = 0;
	keyArray->flags = 0 ;
	INIT_KEYMGR_NODE_LOCK(keyArray) ;
	keyArray->next = GET_KEYMGR_LIST_ROOT() ;
	SET_KEYMGR_LIST_ROOT(keyArray) ;

	return(keyArray) ;
	}



static _Tkey_Data *_keymgr_set_key_element(unsigned int key, void *ptr, TnodeKind kind) {


	_Tkey_Data  *keyArray ;

	keyArray = _keymgr_get_key_element(key) ; 
	   
	if (keyArray == NULL) {
	   keyArray = _keymgr_create_key_element(key,ptr,kind) ;
	   }
	   
	else if (keyArray->node_kind != kind) {
		abort() ;
		}

	else {
	   keyArray->ptr = ptr ;
	   }

	return(keyArray) ;

	}

static _Tkey_Data *_keymgr_get_or_create_key_element(unsigned int key, TnodeKind kind) {

	_Tkey_Data  *keyArray ;

	LOCK_KEYMGR_LIST_MUTEX() ;
    keyArray = _keymgr_get_key_element(key) ;

    if (keyArray == NULL) {
       keyArray = _keymgr_create_key_element(key, NULL, kind) ;
       }
       
    else if (kind && (keyArray->node_kind != kind))
    	abort() ;
    	
    else ;

	UNLOCK_KEYMGR_LIST_MUTEX() ;
	return(keyArray) ;
	}
	

/*
 * External interfaces to keymgr. 
 * The purpose of these routines is to store
 * runtime information in a global place 
 * accessible from all threads and all dlls.
 * Thread safety is guaranteed.
 * _keymgr_get_key must be called first. It locks all keys
 * and returns the value of the specified key.
 * Next either _keymgr_unlock_keys can be called to unlock
 * the keys and allow them to change in value or
 * _keymgr_set_key can be called to set the value
 * of the key and then unlock all keys. The key number itself
 * is determined by the user. To prevent collisions, register
 * your key via a define in keymgr.h. The first use of
 * a key causes a memory location for the key value
 * to be reserved. As long as no key number collision
 * occurs, this interface allows different versions
 * of the runtime to exists together.
 */

static void *_keymgr_get_and_lock_key(unsigned int key, TnodeKind kind) {

    _Tkey_Data *keyArray ;
    void *retptr ;

	keyArray = _keymgr_get_or_create_key_element(key,kind) ;
	_keymgr_lock(keyArray) ;

    retptr = keyArray->ptr ;

    return(retptr) ;
    }

static void _keymgr_unlock_key(unsigned int key) {

    _Tkey_Data *keyArray ;
    LOCK_KEYMGR_LIST_MUTEX() ;
    keyArray = _keymgr_get_key_element(key) ;
    UNLOCK_KEYMGR_LIST_MUTEX() ;
    if (keyArray)
		_keymgr_unlock(keyArray) ;
    }
    



static void *_keymgr_set_and_unlock_key(unsigned int key, void *ptr, TnodeKind kind) {

    _Tkey_Data *keyArray ;
    void * retptr ;

	LOCK_KEYMGR_LIST_MUTEX() ;

    keyArray = _keymgr_set_key_element(key, ptr, kind) ;

    retptr = keyArray->ptr ;

    UNLOCK_KEYMGR_LIST_MUTEX() ;
    _keymgr_unlock(keyArray) ;
    
    return(retptr) ;
    }

/*
 * The following routines allow the user to store per thread
 * data using a key of the user's choice. 
 *
 * _keymgr_init_per_thread_data creates a pthread's key
 * if it doesn't exist already. The data associated with
 * the pthreads' key is initialized to NULL. The pthread key
 * is associated with the specified keymgr key. The key must have
 * been locked by a _keymgr_get_and_lock_key operation.
 * The key will be returned unlocked. 
 *
 * _keymgr_get_per_thread_data lock's the user's key
 * and maps it to a pthreads' key. The pthreads' data associated
 * with that key is returned. The user key is then unlocked.
 *
 * _keymgr_set_per_thread_data lock's the user's key
 * and maps it to a pthreads' key. The argument data is then
 * used to update the data associated with the pthreads' key.
 * Then the user's key is unlocked.
 *
 */


static void _keymgr_init_per_thread_data(unsigned int key) {

     pthread_key_t pthread_key ;
     _Tkey_Data *keyArray ;
     

	/*
	 * Caller insures that node has been created already.
	 */
	 
	LOCK_KEYMGR_LIST_MUTEX() ;
	keyArray = _keymgr_get_key_element(key) ;
    UNLOCK_KEYMGR_LIST_MUTEX() ;
    
    /*
     * The caller has already created a node, if it didn't
     * exist, and has locked it in all cases.
     */
    
    pthread_key = (pthread_key_t) keyArray->ptr ;
    if (pthread_key == NULL) {
	    switch (key) {
			case KEYMGR_EH_CONTEXT_KEY:
				KEYMGR_CREATE_MALLOCED_THREAD_DATA_KEY (pthread_key);
				break;
			default:
				KEYMGR_CREATE_THREAD_DATA_KEY(pthread_key) ;
				break;
			}
		_keymgr_set_and_unlock_key(key,(void *) pthread_key, NODE_THREAD_SPECIFIC_DATA) ;
		}

    else {
    	/*
    	 * since we are done messing around with the key itself, 
    	 * we can unlock it.
    	 */
    	_keymgr_unlock(keyArray) ;
    	
    	/* Now we set the data associated with the key to NULL.*/
    	
    	KEYMGR_SET_THREAD_DATA(pthread_key, NULL) ;
		}

    }

 
void * _keymgr_get_per_thread_data(unsigned int key) {

    pthread_key_t pthread_key ;
    void * pthread_data ;

     
     _init_keymgr() ; /*confirm keymgr has been initialized.*/

    pthread_key = (pthread_key_t) _keymgr_get_and_lock_key(key,NODE_THREAD_SPECIFIC_DATA) ;

	/*
	 * If the user key has not had a pthread's key associated
	 * with it, do so now.
	 */
    if (pthread_key == NULL) {
		_keymgr_init_per_thread_data(key) ; /*unlocks keys.*/
		pthread_data = NULL ;
		}

	/*
	 * The key can be unlocked because now we are just
	 * dealing with the current thread's data.
	 */
    else {
		_keymgr_unlock_key(key) ;
		pthread_data = KEYMGR_GET_THREAD_DATA(pthread_key) ;
		}

    return(pthread_data) ;
    }


void _keymgr_set_per_thread_data(unsigned int key, void *keydata) {

	pthread_key_t pthread_key ;

     
     _init_keymgr() ; /*confirm keymgr has been initialized.*/

	pthread_key = (pthread_key_t) _keymgr_get_and_lock_key(key,NODE_THREAD_SPECIFIC_DATA) ;

	if (pthread_key == NULL) {
		_keymgr_init_per_thread_data(key) ;
		}

	/*
	 * The key can be unlocked because now we are just
	 * dealing with the current thread's data.
	 */
	_keymgr_unlock_key(key) ;

	KEYMGR_SET_THREAD_DATA(pthread_key, keydata) ;

	}


/*
 * These routines provide management of a process wide, thread safe,
 * persistent pointer. If a pointer is created by a bundle/plug-in
 * and placed in here, it will persist for the life of the process, 
 * even after the bundle has been unloaded. This is especially useful
 * for data shared across plugins and across repeated plugin loads and 
 * unloads.
 */



void *_keymgr_get_and_lock_processwide_ptr(unsigned int key) {

	void * retptr ;
	
     
     _init_keymgr() ; /*confirm keymgr has been initialized.*/

	retptr = _keymgr_get_and_lock_key(key,NODE_PROCESSWIDE_PTR) ;
	
		
	return(retptr) ;

	}
	
	
	

void _keymgr_set_and_unlock_processwide_ptr(unsigned int key, void *ptr) {


     
     _init_keymgr() ; /*confirm keymgr has been initialized.*/

	_keymgr_set_and_unlock_key(key,ptr,NODE_PROCESSWIDE_PTR) ;

	}
	
	

void _keymgr_unlock_processwide_ptr(unsigned int key) {

     
     _init_keymgr() ; /*confirm keymgr has been initialized.*/

	_keymgr_unlock_key(key) ;
	
	}
	

void _keymgr_set_lockmode_processwide_ptr(unsigned int key, unsigned int mode) {

    _Tkey_Data *keyArray ;

     
     _init_keymgr() ; /*confirm keymgr has been initialized.*/

	keyArray = _keymgr_get_or_create_key_element(key,NODE_PROCESSWIDE_PTR) ;
	
	keyArray->flags = (keyArray->flags & ~NM_ENHANCED_LOCKING) | mode ;
	}
	
	
unsigned int  _keymgr_get_lockmode_processwide_ptr(unsigned int key) {

    _Tkey_Data *keyArray ;

     
     _init_keymgr() ; /*confirm keymgr has been initialized.*/

	keyArray = _keymgr_get_or_create_key_element(key,NODE_PROCESSWIDE_PTR) ;
	
	return(keyArray->flags) ;
	}
	
	

int _keymgr_get_lock_count_processwide_ptr(unsigned int key) {

    _Tkey_Data *keyArray ;

     
     _init_keymgr() ; /*confirm keymgr has been initialized.*/

	keyArray = _keymgr_get_or_create_key_element(key,NODE_PROCESSWIDE_PTR) ;
	
	return(keyArray->refcount) ;
	}
	
	

/*********************************************/


/* We *could* include <mach.h> here but since all we care about is the
   name of the mach_header struct, this will do instead.  */

struct mach_header;
extern char *getsectdatafromheader();

/* Beware, this is an API.  */

struct __live_images {
  unsigned long this_size;			/* sizeof (__live_images)  */
  struct mach_header *mh;			/* the image info  */
  unsigned long vm_slide;
  void (*destructor)(struct __live_images *);	/* destructor for this  */
  struct __live_images *next;
  unsigned int examined_p;
  void *fde;
  void *object_info;
  unsigned long info[2];			/* GCC3 use  */
};

/* Bits in the examined_p field of struct live_images.  */
enum {
  EXAMINED_IMAGE_MASK = 1,	/* We've seen this one.  */
  ALLOCED_IMAGE_MASK = 2,	/* The FDE entries were allocated by
				   malloc, and must be freed.  This isn't
				   used by newer libgcc versions.  */
  IMAGE_IS_TEXT_MASK = 4,	/* This image is in the TEXT segment.  */
  DESTRUCTOR_MAY_BE_CALLED_LIVE = 8  /* The destructor may be called on an
					object that's part of the live
					image list.  */
};

struct old_object
{
  void *pc_begin;
  void *pc_end;
  struct dwarf_fde *fde_begin;
  struct dwarf_fde **fde_array;
  size_t count;
  struct old_object *next;
  long section_size;
};

static const char __DWARF2_UNWIND_SECTION_TYPE[] = "__TEXT";
static const char __DWARF2_UNWIND_SECTION_NAME[] = "__dwarf2_unwind";

/* Called by dyld when an image is added to the executable.
   If it has a dwarf2_unwind section, register it so the C++ runtime
   can get at it.  All of this is protected by dyld thread locks.  */

static void dwarf2_unwind_dyld_add_image_hook (struct mach_header *mh,
                                               unsigned long vm_slide)
{
  unsigned long sz;
  char *fde;

  /* See if the image has a __TEXT __dwarf2_unwind section.  */

  fde = getsectdatafromheader (mh, __DWARF2_UNWIND_SECTION_TYPE,
				   __DWARF2_UNWIND_SECTION_NAME, &sz);

  if (fde != 0)
    {
      struct old_object *obp;

      obp = (struct old_object *) calloc (1, sizeof (struct old_object) + 8);

      obp->pc_begin = obp->pc_end = 0;
      obp->fde_array = 0;
      obp->count = 0;
      obp->section_size = sz;
      obp->fde_begin = (struct dwarf_fde *) (fde + vm_slide);

      obp->next = (struct old_object *)
                _keymgr_get_and_lock_processwide_ptr (KEYMGR_ZOE_IMAGE_LIST);

      _keymgr_set_and_unlock_processwide_ptr (KEYMGR_ZOE_IMAGE_LIST, obp);
    }

  {
    struct __live_images *l = (struct __live_images *)calloc (1, sizeof (*l));
    l->mh = mh;
    l->vm_slide = vm_slide;
    l->this_size = sizeof (*l);
    l->next = (struct __live_images *)
      _keymgr_get_and_lock_processwide_ptr (KEYMGR_GCC3_LIVE_IMAGE_LIST);
    _keymgr_set_and_unlock_processwide_ptr (KEYMGR_GCC3_LIVE_IMAGE_LIST, l);
  }
}

static void
dwarf2_unwind_dyld_remove_image_hook (struct mach_header *mh,
				      unsigned long vm_slide)
{
  unsigned long sz;
  char *fde;

  /* See if the image has a __TEXT __dwarf2_unwind section.  */

  fde = getsectdatafromheader (mh, __DWARF2_UNWIND_SECTION_TYPE,
				   __DWARF2_UNWIND_SECTION_NAME, &sz);
  if (fde != 0)
    {
      struct old_object *objlist, **obp;

      objlist = (struct old_object *)
	 _keymgr_get_and_lock_processwide_ptr (KEYMGR_ZOE_IMAGE_LIST);

      for (obp = &objlist; *obp; obp = &(*obp)->next)
        if ((char *)(*obp)->fde_begin == fde + vm_slide)
          {
            struct old_object *p = *obp;
            *obp = p->next;
            if (p->pc_begin)
              free (p->fde_array);
            free (p);
            break;
          }

      _keymgr_set_and_unlock_processwide_ptr (KEYMGR_ZOE_IMAGE_LIST, objlist);
    }

  {
    struct __live_images *top, **lip, *destroy = NULL;

    /* This is a bit of cache.  _dyld_get_image_header_containing_address
       can be expensive, but most of the time the destructors come from
       one or two objects and are consecutive in the list.  */
    void (*prev_destructor)(struct __live_images *) = NULL;
    int was_in_object = 0;
    
    /* Look for it in the list of live images and delete it.  
       Also, call any destructors in case they are in this image and about
       to be unloaded.  The test with DESTRUCTOR_MAY_BE_CALLED_LIVE is
       because in GCC 3.1, the destructor would (uselessly) try to acquire
       the LIVE_IMAGE_LIST lock, and it's not practical to call it in
       that case (deadlock ensues, unless we release the lock, in which
       case the concurrency issues become impossible).  */
    
    top = (struct __live_images *)
      _keymgr_get_and_lock_processwide_ptr (KEYMGR_GCC3_LIVE_IMAGE_LIST);
    lip = &top;
    
    while (*lip != NULL)
      {
	if ((*lip)->destructor 
	    && ((*lip)->examined_p & DESTRUCTOR_MAY_BE_CALLED_LIVE))
	  {
	    if (! was_in_object && (*lip)->destructor != prev_destructor)
	      {
		prev_destructor = (*lip)->destructor;
		was_in_object = ((_dyld_get_image_header_containing_address
				  ((long) prev_destructor)) 
				 == mh);
	      }
	    if ((*lip)->destructor == prev_destructor && was_in_object)
	      (*lip)->destructor (*lip);
	  }

	if ((*lip)->mh == mh && (*lip)->vm_slide == vm_slide)
	  {
	    destroy = *lip;
	    *lip = destroy->next;                 /* unlink DESTROY  */
	    
	    if (destroy->this_size != sizeof (*destroy))  /* sanity check  */
	      abort ();
	    
	    continue;
	  }
	lip = &(*lip)->next;
      }
    _keymgr_set_and_unlock_processwide_ptr (KEYMGR_GCC3_LIVE_IMAGE_LIST, top);
    
    /* Now that we have unlinked this from the image list, toss it.  
       The destructor gets called here only to handle the GCC 3.1 case.  */
    if (destroy != NULL)
      {
	if (destroy->destructor != NULL)
	  (*destroy->destructor) (destroy);
	free (destroy);
      }
  }
}

/* __keymgr_dwarf2_register_sections is called by crt1.  */

void __keymgr_dwarf2_register_sections (void)
{
  _dyld_register_func_for_add_image (dwarf2_unwind_dyld_add_image_hook);
  _dyld_register_func_for_remove_image (dwarf2_unwind_dyld_remove_image_hook);
}
