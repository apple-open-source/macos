/******************************************************************************
** 
**  $Id: p11x_state.c,v 1.2 2003/02/13 20:06:42 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Global state functions
** 
******************************************************************************/

#include "cryptoki.h"

/* Global state variable */
P11_State st = { 0,                             /* initialized             */
                 { 0,                           /* prefs.multi_app         */
                   0,                           /* prefs.threaded          */
                   LOG_HIGH,                    /* prefs.log_level         */
                   P11_OBJ_SORT_NEWEST_FIRST,   /* prefs.obj_sort_order    */
                   P11_SLOT_WATCH_THREAD_FULL,  /* prefs.slot_watch_scheme */
                   0,                           /* prefs.cache_pin         */
                   PKCS11_MAJOR,                /* prefs.version_major     */
                   PKCS11_MINOR,                /* prefs.version_minor     */
                   PKCS11_MAX_PIN_TRIES,        /* prefs.max_pin_tries     */
                   PKCS11_SO_USER_PIN,          /* prefs.so_user_pin_num   */
                   PKCS11_USER_PIN,             /* prefs.user_pin_num      */
                   P11_DEFAULT_CERT_ATTRIB_OBJ_SIZE, /* prefs.cert_attrib_size  */
                   P11_DEFAULT_ATTRIB_OBJ_SIZE,      /* prefs.pubkey_attrib_size*/
                   P11_DEFAULT_PRK_ATTRIB_OBJ_SIZE,  /* prefs.prvkey_attrib_size*/
                   P11_DEFAULT_ATTRIB_OBJ_SIZE,      /* prefs.data_attrib_size  */
                   0,                                /* prefs.disable_security  */
                   P11_DEFAULT_LOG_FILENAME },       /* prefs.log_filename      */
                 0,                             /* slots                   */ 
                 0,                             /* slot_count              */
                 0,                             /* sessions                */
                 0,                             /* slot_status             */
                 0,                             /* log_lock                */
                 0,                             /* async_lock              */
                 0,                             /* native_locks            */
                 0                              /* create_threads          */
               };

/******************************************************************************
** Function: state_Init
**
** Initializes global state.  Read preferences via util_ReadPreferences
**
** Parameters:
**  none
**
** Returns:
**  CKR_HOST_MEMORY if memory alloc fails
**  CKR_OK
*******************************************************************************/
CK_RV state_Init()
{
    CK_RV rv = CKR_OK;

    if (!st.initialized)
        util_ReadPreferences();

    return rv;
}

/******************************************************************************
** Function: state_Free
**
** Frees the global state information
**
** Parameters:
**  none
**
** Returns:
**  Error from slot_FreeAllSlots (not checked and everything gets blasted)
**  CKR_OK
*******************************************************************************/
CK_RV state_Free()
{
    CK_RV rv = CKR_OK;

    if (st.initialized)
    {
        rv = slot_FreeAllSlots();
    
        if (st.log_lock)
        {
            thread_MutexDestroy(st.log_lock);
            free(st.log_lock);
            st.log_lock = 0;
        }
    
        if (st.async_lock)
        {
            thread_MutexDestroy(st.async_lock);
            free(st.async_lock);
            st.async_lock = 0;
        }
    
        if (st.prefs.threaded)
            thread_Finalize();
    
        st.initialized = 0;
    }

    return rv;
}

