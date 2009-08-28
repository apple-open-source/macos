/* Host support for NextStep/Rhapsody on PowerPC for GDB, the GNU debugger.
   Copyright (C) 1997
   Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _XM_ARM_MACOSX_H_
#define _XM_ARM_MACOSX_H_

#define HOST_BYTE_ORDER BFD_ENDIAN_LITTLE
#define HOST_FLOAT_FORMAT &floatformat_ieee_single_little 
#define HOST_DOUBLE_FORMAT &floatformat_ieee_double_little
#define HOST_LONG_DOUBLE_FORMAT &floatformat_ieee_double_little

#include "xm-macosx.h"

#endif /* _XM_ARM_MACOSX_H_ */
