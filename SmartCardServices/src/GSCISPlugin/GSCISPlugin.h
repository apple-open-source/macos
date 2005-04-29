/******************************************************************
 GSCIS
        MUSCLE SmartCard Development ( http://www.musclecard.com )
            Title  : GSCISPlugin.h
            Package: GSCISPlugin
            Author : David Corcoran
            Date   : 02/19/02
            License: Copyright (C) 2002 David Corcoran
                     <corcoran@linuxnet.com>
            Purpose: A MuscleCard plugin for GSCIS compliant cards.
 
 
********************************************************************/

#ifndef __GSCISPlugin_h__
#define __GSCISPlugin_h__

#ifdef __cplusplus
  extern "C" {
#endif

/* Some useful offsets in the buffer */
#define OFFSET_CLA	0x00
#define OFFSET_INS	0x01
#define OFFSET_P1	0x02
#define OFFSET_P2	0x03
#define OFFSET_P3	0x04
#define OFFSET_DATA	0x05


#ifdef __cplusplus
  }
#endif

#endif /* __GSCISPlugin_h__ */
