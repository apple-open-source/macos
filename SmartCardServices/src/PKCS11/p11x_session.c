/******************************************************************************
** 
**  $Id: p11x_session.c,v 1.2 2003/02/13 20:06:41 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Session & object management functions
** 
******************************************************************************/

#include "cryptoki.h"

/******************************************************************************
** Function: session_AddSession
**
** Adds a new session 
**
** Parameters:
**  phSession - Returns a pointer to the new session
**
** Returns:
**  CKR_HOST_MEMORY if memory alloc failed
**  CKR_OK
*******************************************************************************/
CK_RV session_AddSession(CK_SESSION_HANDLE *phSession)
{
    CK_RV rv = CKR_OK;

    *phSession = 0;

    if (st.sessions)
    {
        st.sessions->prev = (P11_Session *)calloc(1, sizeof(P11_Session));
        if (!st.sessions->prev)
            rv = CKR_HOST_MEMORY;
        else
        {
            st.sessions->prev->next = st.sessions;
            st.sessions = st.sessions->prev;
            st.sessions->check = st.sessions;
            *phSession = (CK_SESSION_HANDLE)st.sessions;
        }
    }
    else
    {
        st.sessions = (P11_Session *)calloc(1, sizeof(P11_Session));
        if (!st.sessions)
            rv = CKR_HOST_MEMORY;
        else
        {
            st.sessions->check = st.sessions;
            *phSession = (CK_SESSION_HANDLE)st.sessions;
        }
    }

    return rv;
}

/******************************************************************************
** Function: session_FreeSession
**
** Deletes/removes a session
**
** Parameters:
**  hSession - Handle of session to remove
**
** Returns:
**  CKR_SESSION_HANDLE_INVALID if session handle is invalid
**  CKR_OK
*******************************************************************************/
CK_RV session_FreeSession(CK_SESSION_HANDLE hSession)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;

    if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else
    {
        log_Log(LOG_LOW, "Removing session: %lX", hSession);

        if (session->prev) /* Fixme: check for head of list? st.sessions */
        {
            session->prev->next = session->next;

            if (session == st.sessions) /* Fixme: Is this needed? */
                st.sessions = session->prev;
        }

        if (session->next)
        {
            session->next->prev = session->prev;

            if (session == st.sessions)
                st.sessions = session->next;
        }

        if (!session->prev && !session->next)
            st.sessions = 0x00;

        if (session->search_attrib)
            free(session->search_attrib);
        
        /* Clear memory, just to be safe */
        memset(session, 0x00, sizeof(P11_Session));
    
        free(session);
    }

    return rv;
}

