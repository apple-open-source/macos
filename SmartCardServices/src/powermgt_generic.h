/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	Title  : powermgt_generic.h
	Package: pcsc lite
	Author : David Corcoran
	Date   : 04/22/02
	License: Copyright (C) 2002 David Corcoran
		<corcoran@linuxnet.com>
	Purpose: This handles power management routines. 

$Id: powermgt_generic.h,v 1.2 2002/04/25 22:44:10 stuartha Exp $

********************************************************************/

#ifndef __powermgt_generic_h__
#define __powermgt_generic_h__

#ifdef __cplusplus
extern "C"
{
#endif


/* 
 * Registers for Power Management callbacks 
 */

ULONG PMRegisterForPowerEvents();


#ifdef __cplusplus
}
#endif

#endif
