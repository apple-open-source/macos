/******************************************************************************
** 
**  $Id: p11x_util.c,v 1.2 2003/02/13 20:06:42 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Utility functions
** 
******************************************************************************/

#include <stdio.h>
#include "cryptoki.h"

/******************************************************************************
** Function: util_byterev
**
** Reverses a byte string in-place
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
void util_byterev(CK_BYTE *data, CK_ULONG len)
{
    CK_ULONG i;
    CK_BYTE temp;

    for (i = 0; i < len / 2; i++)
    {
        temp = data[i];
        data[i] = data[len - i - 1];
        data[len - i - 1] = temp;
    }
}

/******************************************************************************
** Function: util_strpadlen
**
** Find length of string that has been padded with spaces
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
CK_ULONG util_strpadlen(CK_CHAR *string, CK_ULONG max_len)
{
    CK_ULONG i;

    for (i = max_len; i > 0; i--)
    {
        if (string[i - 1] != 0x20)
            break;
    }

    return (i);
}

/******************************************************************************
** Function: util_PadStrSet
**
** Pads a string with spaces (of size length), then sets the value to a null
** terminated string.
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
CK_RV util_PadStrSet(CK_CHAR *string, CK_CHAR *value, CK_ULONG size)
{
    memset(string, 0x20, size);
    memcpy((char *)string, value, strnlen((char *)value, size));

    return CKR_OK;
}

/******************************************************************************
** Function: util_StripPKCS1
**
** Strips PKCS #1 padding
**
** Note that this function strips the data in-place so the original information
** is lost.
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
CK_RV util_StripPKCS1(CK_BYTE *data, CK_ULONG len, CK_BYTE *output, CK_ULONG *out_len)
{
    CK_RV rv = CKR_OK;
    CK_ULONG i, pos, tag;

    pos = tag = 0;

    for (i = 0; i < len; i++)
    {
        if (data[i] == 0x02)
            tag = 1;
        else if (tag && !data[i] && (i+1 < len))
        {
            pos = i + 1;
            break;
        }
    }

    if (!pos)
        rv = CKR_FUNCTION_FAILED;
    else
    {
        len -= pos;

        if (len > *out_len)
            rv = CKR_BUFFER_TOO_SMALL;
        else
        {
            memcpy(output, &data[pos], len);
            *out_len = len;
        }
    }

    return rv;
}

/******************************************************************************
** Function: strnlen
**
** Limited length strlen function (normally included with GNU compiler)
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
#ifndef __USE_GNU
#ifndef WIN32
size_t strnlen(__const char *__string, size_t __maxlen)
#else
size_t strnlen(const char *__string, size_t __maxlen)
#endif
{
    size_t i;

    for (i = 0; i < __maxlen; i++)
        if (__string[i] == 0x00) break;

    return i;
}
#endif

/******************************************************************************
** Function: util_IsLittleEndian
**
** Parameters:
**  none
**
** Returns:
**  True/False if machine is little endian or not
*******************************************************************************/
CK_BBOOL util_IsLittleEndian()
{
    CK_ULONG rv = 1;

    return ((char *)&rv)[0];
}
