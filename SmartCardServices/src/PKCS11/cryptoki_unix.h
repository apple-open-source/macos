/******************************************************************************
** 
**  $Id: cryptoki_unix.h,v 1.2 2003/02/13 20:06:37 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: UNIX specific cryptoki definitions
** 
******************************************************************************/

#ifndef __CRYPTOKI_UNIX_H__
#define __CRYPTOKI_UNIX_H__
#ifndef WIN32

#define CK_PTR *

#define CK_DEFINE_FUNCTION(returnType, name) \
  returnType name

#define CK_DECLARE_FUNCTION(returnType, name) \
  returnType name

#define CK_DECLARE_FUNCTION_POINTER(returnType, name) \
  returnType (* name)

#define CK_CALLBACK_FUNCTION(returnType, name) \
  returnType (* name)

#ifndef NULL_PTR
#define NULL_PTR 0
#endif

#include "pkcs11.h"

#endif /* ifndef WIN32 */
#endif /* __CRYPTOKI_UNIX_H__ */
