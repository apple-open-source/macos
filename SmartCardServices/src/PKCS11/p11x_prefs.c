/******************************************************************************
** 
**  $Id: p11x_prefs.c,v 1.2 2003/02/13 20:06:41 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: PKCS #11 preference handling functions
** 
******************************************************************************/

#include <stdio.h>
#include "cryptoki.h"

#ifndef HAVE_STRTOKR
#undef strtok_r
#define strtok_r(x,y,z) strtok(x,y)
#endif

#ifdef WIN32
#define strcasecmp stricmp
#endif

/******************************************************************************
** Function: util_ParsePreference
**
** Parse a preference item from a line of text.  The text should be null
** terminated and buf_size keeps overruns from happening.
**
** If a preference item is successfully parsed then it is stored in the 
** st.prefs settings.
**
** This whole function is fairly verbose and could be broken into smaller
** pieces to handle things like "get true/false pref" but at least this is
** very straightforward.
**
** Fixme: put this in p11x_prefs.c
**
** Parameters:
**  buf      - Null terminated text buffer
**  buf_size - Size of buffer 
**
** Returns:
**  none
*******************************************************************************/
void util_ParsePreference(char *buf, CK_ULONG buf_size)
{
    char sep[] = "=\t\r\n ";
    char *token;
#ifdef HAVE_STRTOKR
    char *strtok_ptr;
#endif

    /* We will be using many unsafe string functions so force a NULL at the */
    /*     end of the buffer to protect ourselves */
    buf[buf_size - 1] = 0;
    token = strchr(buf, '#');

    if (token) 
        *token = 0;

    token = strtok_r(buf, sep, &strtok_ptr);

    if (token)
    {
        if (!strcasecmp("DebugLevel", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"DebugLevel\" failed");
            else
            {
                if (!strcasecmp("LOW", token))
                    st.prefs.log_level = LOG_LOW;
                else if (!strcasecmp("MED", token))
                    st.prefs.log_level = LOG_MED;
                else
                    st.prefs.log_level = LOG_HIGH;
            }
        }
        else if (!strcasecmp("LogFilename", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"LogFilename\" failed");
            else
                strncpy((char *)st.prefs.log_filename, token, sizeof(st.prefs.log_filename));
        }
        else if (!strcasecmp("MultiApp", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"MultiApp\" failed");
            else
            {
                if (!strcasecmp("True", token) || !strcasecmp("Yes", token))
                    st.prefs.multi_app = 1;
                else if (!strcasecmp("False", token) || !strcasecmp("No", token))
                    st.prefs.multi_app = 0;
                else
                    log_Log(LOG_HIGH, "Invalid MultiApp preference specified: %s", token);
            }
        }
        else if (!strcasecmp("Threaded", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"Threaded\" failed");
            else
            {
                if (!strcasecmp("True", token) || !strcasecmp("Yes", token))
                    st.prefs.threaded = 1;
                else if (!strcasecmp("False", token) || !strcasecmp("No", token))
                    st.prefs.threaded = 0;
                else
                    log_Log(LOG_HIGH, "Invalid Threaded preference specified: %s", token);
            }
        }
        else if (!strcasecmp("SlotStatusThreadScheme", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"SlotStatusThreadScheme\" failed");
            else
            {
                if (!strcasecmp("Full", token))
                    st.prefs.slot_watch_scheme = P11_SLOT_WATCH_THREAD_FULL;
                else if (!strcasecmp("Partial", token))
                    st.prefs.slot_watch_scheme = P11_SLOT_WATCH_THREAD_PARTIAL;
                else
                    log_Log(LOG_HIGH, "Invalid SlotStatusThreadScheme specified: %s", token);
            }
        }
        else if (!strcasecmp("ObjectSortOrder", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"ObjectSortOrder\" failed");
            else
            {
                if (!strcasecmp("NewestFirst", token))
                    st.prefs.obj_sort_order = P11_OBJ_SORT_NEWEST_FIRST;
                else if (!strcasecmp("NewestLast", token))
                    st.prefs.obj_sort_order = P11_OBJ_SORT_NEWEST_LAST;
                else
                    log_Log(LOG_HIGH, "Invalid ObjectSortOrder specified: %s", token);
            }
        }
        else if (!strcasecmp("CachePIN", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"CachePIN\" failed");
            else
            {
                if (!strcasecmp("True", token) || !strcasecmp("Yes", token))
                    st.prefs.cache_pin = 1;
                else if (!strcasecmp("False", token) || !strcasecmp("No", token))
                    st.prefs.cache_pin = 0;
                else
                    log_Log(LOG_HIGH, "Invalid cache_pin preference specified: %s", token);
            }
        }
        else if (!strcasecmp("Version", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"Version\" failed");
            else
            {
                char *pos;

                pos = strchr(token, '.');
                if (!pos)
                    P11_ERR("Config option \"Version\" failed");
                else
                {
                    *pos = 0;
                    st.prefs.version_major = atol(token);
                    st.prefs.version_minor = atol(pos + 1);
                }
            }
        }
        else if (!strcasecmp("MaxPinTries", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"MaxPinTries\" failed");
            else
                st.prefs.max_pin_tries = atol(token);
        }
        else if (!strcasecmp("SOUserPinNum", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"SOUserPinNum\" failed");
            else
                st.prefs.so_user_pin_num = atol(token);
        }
        else if (!strcasecmp("UserPinNum", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"UserPinNum\" failed");
            else
                st.prefs.user_pin_num = atol(token);
        }
        else if (!strcasecmp("CertAttribSize", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"CertAttribSize\" failed");
            else
                st.prefs.cert_attrib_size = atol(token);
        }
        else if (!strcasecmp("PubKeyAttribSize", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"PubKeyAttribSize\" failed");
            else
                st.prefs.pubkey_attrib_size = atol(token);
        }
        else if (!strcasecmp("PrvKeyAttribSize", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"PrvKeyAttribSize\" failed");
            else
                st.prefs.prvkey_attrib_size = atol(token);
        }
        else if (!strcasecmp("DataAttribSize", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"DataAttribSize\" failed");
            else
                st.prefs.data_attrib_size = atol(token);
        }
        else if (!strcasecmp("DisableSecurity", token))
        {
            token = strtok_r(0, sep, &strtok_ptr);
            if (!token)
                P11_ERR("Config option \"DisableSecurity\" failed");
            else
            {
                if (!strcasecmp("True", token) || !strcasecmp("Yes", token))
                    st.prefs.disable_security = 1;
                else if (!strcasecmp("False", token) || !strcasecmp("No", token))
                    st.prefs.disable_security = 0;
                else
                    log_Log(LOG_HIGH, "Invalid DisableSecurity preference specified: %s", token);
            }
        }
    }
}

/******************************************************************************
** Function: util_ReadPreferences
**
** Gets preferences, if available.  On UNIX, looks for .pkcs11rc
** in the $HOME directory, or root directory if $HOME is not 
** defined.  Having a preferences file is optional and it is assumed
** that most of the time users will not have one unless debug/logging
** or other special settings are required.
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
#ifndef WIN32
CK_RV util_ReadPreferences()
{
    CK_RV rv = CKR_OK;
    FILE *fp;
    char rcfilepath[256];
    char rcfilename[] = "/.pkcs11rc";
    char buf[1024];

    strncpy(rcfilepath, getenv("HOME"), sizeof(rcfilepath) - sizeof(rcfilename) - 1);
    strcat(rcfilepath, rcfilename);

    fp = fopen(rcfilepath, "rb");
    if (fp)
    {
        while (fgets(buf, sizeof(buf), fp))
            util_ParsePreference(buf, sizeof(buf));

        fclose(fp);
    }

    return rv;
}
#else
CK_RV util_ReadPreferences()
{
    CK_RV rv = CKR_OK;
    FILE *fp;
    char rcfilepath[] = "C:\\Program Files\\Muscle\\pkcs11rc";
    char buf[1024];

    fp = fopen(rcfilepath, "rb");
    if (fp)
    {
        while (fgets(buf, sizeof(buf), fp))
            util_ParsePreference(buf, sizeof(buf));

        fclose(fp);
    }

    return rv;
    
}
#endif

