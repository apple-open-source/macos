/*
 *  CCIDprivate.h
 *  ifd-CCID
 *
 *  Created by JL on Mon Jul 21 2003.
 *  Copyright (c) 2003 Jean-Luc Giraud. All rights reserved.
 *  See COPYING file for license.
 *
 */

#ifndef __CCIDPRIVATE_H__
#define __CCIDPRIVATE_H__
extern CCIDReaderState CCIDReaderStates[PCSCLITE_MAX_CHANNELS];

// Performs a Bulk-IN/Bluk-Out exchange with reader
// Seq and Slot are automatically managed
// "TimeExtension" messages can be handled through bTimeExtRetry:
// If set to 1, CCID_Exchange_Command does not send the command and reads
// directly
// If set to 0, CCID_Exchange_Command sends the command (used for a normal call
// or time extension management in T=1)
CCIDRv CCID_Exchange_Command(DWORD Lun, BYTE bMessageTypeCmd,
                             BYTE *abMessageSpecificCmd,
                             BYTE *abDataCmd, DWORD dwDataCmdLength,
                             BYTE *pbMessageTypeResp,
                             BYTE *pbStatus, BYTE *pbError,
                             BYTE *pbMessageSpecificResp,
                             BYTE *abDataResp, DWORD *pdwDataRespLength,
                             BYTE bTimeExtRetry);
CCIDRv CCID_XfrBlockSAPDU(DWORD Lun, BYTE bBWI,
                         BYTE *abDataCmd, DWORD dwDataCmdLength,
                         BYTE *abDataResp, DWORD *pdwDataRespLength);

CCIDRv CCID_XfrBlockTPDU(DWORD Lun, BYTE bBWI,
                         BYTE *abDataCmd, DWORD dwDataCmdLength,
                         BYTE *abDataResp, DWORD *pdwDataRespLength);


#endif
