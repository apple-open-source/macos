/* Copyright (C) 1989, 92-97, 1998, 1999, Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


/*
 * This file added by Apple Computer Inc. for its OS X 
 * environment.
 */


/*#if defined(MACOSX) && !defined(APPLE_KERNEL_EXTENSION) */

#ifdef PART_OF_SYSTEM_FRAMEWORK
#include <mach-o/dyld.h>
#endif

/*
 * keymgr - Create and maintain process-wide global data known to 
 *	    all threads across all dynamic libraries. 
 *
 */

#include "keymgr.h"
#include <pthread.h>
#include <stdlib.h>


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

extern void *__eh_global_dataptr ; /*from system framework, being deprecated.*/ 
extern void *__keymgr_global[3] ; /*also from system framework*/

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

static pthread_mutex_t _keyArray_lock=PTHREAD_MUTEX_INITIALIZER ;
static pthread_mutex_t *_keyArray_lock_ptr=&_keyArray_lock ;
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
	int node_kind ;			/*What kind of variable is this?*/
	pthread_mutex_t thread_lock ;	/*Semaphore for this specific variable.*/
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

#ifdef PART_OF_SYSTEM_FRAMEWORK

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

#endif  /*PART_OF_SYSTEM_FRAMEWORK*/

	_keymgr_init_mutex++ ;
	
	/* 
	 * now we check to see if another instance has already initialized keymgr.
	 * If keymgr is in the system framework, this will always be false.
	 */
	
	if (GET_KEYMGR_INFO_NODE_PTR()) {
#ifdef PART_OF_SYSTEM_FRAMEWORK
		UNLOCK_KEYMGR_INIT() ;
#endif
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

#ifdef PART_OF_SYSTEM_FRAMEWORK
	UNLOCK_KEYMGR_INIT() ;
#endif
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

     void * pthread_data ;
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
    	KEYMGR_CREATE_THREAD_DATA_KEY(pthread_key) ;
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

/* The following two macros  are required for the inclusion
   of "gcc/frame.h".  FRAME_SECTION_DESCRIPTOR should be a
   bunch of field declarations with which we extend the OBJECT 
   structure.  FIRST_PSEUDO_REGISTER can be 1 since we don't
   use it here.  */

#define FRAME_SECTION_DESCRIPTOR	long section_size;
#define FIRST_PSEUDO_REGISTER		1

#include "gcc/frame.h"

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

  if (0) printf ("add_image_hook: mach_header %08x vm_slide %08x "
		 "<fde %08x>\n", mh, vm_slide, fde);

  if (fde != 0)
    {
      struct object *obp;

      obp = (struct object *) calloc (1, sizeof (struct object) + 8);

      obp->pc_begin = obp->pc_end = 0;
      obp->fde_array = 0;
      obp->count = 0;
      obp->section_size = sz;
      obp->fde_begin = (struct dwarf_fde *) (fde + vm_slide);

      obp->next = (struct object *)
                _keymgr_get_and_lock_processwide_ptr (KEYMGR_ZOE_IMAGE_LIST);

      _keymgr_set_and_unlock_processwide_ptr (KEYMGR_ZOE_IMAGE_LIST, obp);
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
      struct object *objlist, **obp;

      objlist = (struct object *)
	 _keymgr_get_and_lock_processwide_ptr (KEYMGR_ZOE_IMAGE_LIST);

      for (obp = &objlist; *obp; obp = &(*obp)->next)
        if ((char *)(*obp)->fde_begin == fde + vm_slide)
          {
            struct object *p = *obp;
            *obp = p->next;
            if (p->pc_begin)
              free (p->fde_array);
            free (p);
            break;
          }

      _keymgr_set_and_unlock_processwide_ptr (KEYMGR_ZOE_IMAGE_LIST, objlist);
    }
}

/* __keymgr_dwarf2_register_sections is called by crt1.  */

void __keymgr_dwarf2_register_sections (void)
{
  _dyld_register_func_for_add_image (dwarf2_unwind_dyld_add_image_hook);
  _dyld_register_func_for_remove_image (dwarf2_unwind_dyld_remove_image_hook);
}

/*#endif MACOSX*/
