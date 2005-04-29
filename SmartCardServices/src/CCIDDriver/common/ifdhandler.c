/*****************************************************************
/
/ File   :   ifdhandler.c
/ Authors :   David Corcoran <corcoran@linuxnet.com>
/             Jean-Luc Giraud <jlgiraud@mac.com>
/ Date   :   April 9, 2003
/ Purpose:   This provides reader specific low-level calls
/            for the GemPC family of Gemplus. The function
/            stubs were written by D. Corcoran, the CCID
/            +specific code was added by JL Giraud.
/            See http://www.linuxnet.com for more information.
/ License:   See file COPYING
/
/
******************************************************************/

#include <stdio.h>
#include <string.h>

#include "wintypes.h"
#include "pcscdefines.h"
#include "ifdhandler.h"
#include "global.h"

#include "tools.h"
#include "CCID.h"

#define POWERFLAGS_RAZ 0x00
//Flag set when a power up has been requested
#define MASK_POWERFLAGS_PUP 0x01
//Flag set when a power down is requested
#define MASK_POWERFLAGS_PDWN 0x02

//+++ NEEDS TO BE READ FROM CCID DESC
#define MAX_SLOT_NB 2

enum
{
    T_0                    = 0,
    T_1                    = 1
};




typedef struct 
{
    DWORD nATRLength;
    UCHAR pcATRBuffer[MAX_ATR_SIZE];
    UCHAR bPowerFlags;
} IFDDesc;

int iLunCheck(DWORD Lun)
{
    if ((LunToReaderLun(Lun) >= PCSCLITE_MAX_CHANNELS))
        return TRUE;

    return FALSE;
} /* iLunCheck */


// Array of structures to hold the ATR and other state value of each slot
static IFDDesc pstIFDDescs[PCSCLITE_MAX_CHANNELS][MAX_SLOT_NB];
static int iLogValue = LogLevelCritical;

BYTE bLogPeriodic = 0;

RESPONSECODE IFDHCreateChannel(DWORD Lun, DWORD Channel)
{
	/*
	 * Lun - Logical Unit Number, use this for multiple card slots or
	 * multiple readers. 0xXXXXYYYY - XXXX multiple readers, YYYY multiple
	 * slots. The resource manager will set these automatically.  By
	 * default the resource manager loads a new instance of the driver so
	 * if your reader does not have more than one smartcard slot then
	 * ignore the Lun in all the functions. Future versions of PC/SC might
	 * support loading multiple readers through one instance of the driver
	 * in which XXXX would be important to implement if you want this.
	 */

	/*
	 * Channel - Channel ID.  This is denoted by the following: 0x000001 -
	 * /dev/pcsc/1 0x000002 - /dev/pcsc/2 0x000003 - /dev/pcsc/3
	 *
	 * USB readers may choose to ignore this parameter and query the bus
	 * for the particular reader.
	 */

	/*
	 * This function is required to open a communications channel to the
	 * port listed by Channel.  For example, the first serial reader on
	 * COM1 would link to /dev/pcsc/1 which would be a sym link to
	 * /dev/ttyS0 on some machines This is used to help with intermachine
	 * independance.
	 *
	 * Once the channel is opened the reader must be in a state in which
	 * it is possible to query IFDHICCPresence() for card status.
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_COMMUNICATION_ERROR
	 */
    const char *pcStringValue;

    StartLogging();
    SetLogType(LogTypeSysLog);
    pcStringValue =  ParseInfoPlist(BUNDLE_IDENTIFIER, "ifdLogType");
    if ( pcStringValue == NULL )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Could not get ifdLogType info, use syslog");
    }
    else
    {
        if (!strncmp(pcStringValue, "stderr", strlen("stderr")))
        {
            SetLogType(LogTypeStderr);            
        }
        else
        {
            SetLogType(LogTypeSysLog);
        }
    }
    
    
    pcStringValue =  ParseInfoPlist(BUNDLE_IDENTIFIER, "ifdLogLevel");
    if ( pcStringValue == NULL )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Could not get ifdLogLevel info, use minimal level");
        SetLogLevel(LogLevelCritical);
    }
    else
    {
        iLogValue = atoi(pcStringValue);
        SetLogLevel((BYTE)iLogValue);
    }

    bLogPeriodic = 0;
    pcStringValue =  ParseInfoPlist(BUNDLE_IDENTIFIER, "ifdLogPeriodic");
    if ( pcStringValue == NULL )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Could not get ifdLogPeriodic info, will not log periodic info");
    }
    else
    {
        if ( toupper(*pcStringValue) == 'Y' )
            bLogPeriodic = 1;
    }
    

    LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "Entering IFDHCreateChannel");

	if (iLunCheck(Lun))
		return IFD_COMMUNICATION_ERROR;

	// Reset ATR buffer
	pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].nATRLength = 0;
	*pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].pcATRBuffer = '\0';

	// Reset PowerFlags
	pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].bPowerFlags =
		POWERFLAGS_RAZ;

	if (CCID_OpenChannel(Lun, Channel) != CCIDRv_OK)
	{
		LogMessage( __FILE__,  __LINE__, LogLevelCritical, "OpenReader failed");
		return IFD_COMMUNICATION_ERROR;
	}

	return IFD_SUCCESS;
} /* IFDHCreateChannel */


RESPONSECODE IFDHCloseChannel(DWORD Lun)
{
	/*
	 * This function should close the reader communication channel for the
	 * particular reader.  Prior to closing the communication channel the
	 * reader should make sure the card is powered down and the terminal
	 * is also powered down.
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_COMMUNICATION_ERROR
	 */
    BYTE bClockStatus;
	LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "entering IFDHCloseChannel");

	if (iLunCheck(Lun))
		return IFD_COMMUNICATION_ERROR;

	CCID_IccPowerOff(Lun, &bClockStatus);
	// No error status check, if it failed, what can you do ? :)

	CCID_CloseChannel(Lun);
	return IFD_SUCCESS;
} /* IFDHCloseChannel */


RESPONSECODE IFDHGetCapabilities(DWORD Lun, DWORD Tag,
	PDWORD Length, PUCHAR Value)
{
	/*
	 * This function should get the slot/card capabilities for a
	 * particular slot/card specified by Lun.  Again, if you have only 1
	 * card slot and don't mind loading a new driver for each reader then
	 * ignore Lun.
	 *
	 * Tag - the tag for the information requested example: TAG_IFD_ATR -
	 * return the Atr and it's size (required). these tags are defined in
	 * ifdhandler.h
	 *
	 * Length - the length of the returned data Value - the value of the
	 * data
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_ERROR_TAG
	 */

	LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "entering IFDHGetCapabilities\n");

	if (iLunCheck(Lun))
		return IFD_COMMUNICATION_ERROR;

	switch (Tag)
	{
		case TAG_IFD_ATR:
			// If Length is not zero, powerICC has been performed.
			// Otherwise, return NULL pointer
			// Buffer size is stored in *Length
			*Length = (*Length < pstIFDDescs[LunToReaderLun(Lun)]
				[LunToSlotNb(Lun)].nATRLength) ?
				*Length : pstIFDDescs[LunToReaderLun(Lun)]
				[LunToSlotNb(Lun)].nATRLength;

			if (*Length)
				memcpy(Value, pstIFDDescs[LunToReaderLun(Lun)]
					[LunToSlotNb(Lun)].pcATRBuffer, *Length);
			break;

		case TAG_IFD_SIMULTANEOUS_ACCESS:
			if (*Length >= 1)
			{
				*Length =1;
				*Value = PCSCLITE_MAX_CHANNELS;
				break;
			}

		default:
			return IFD_ERROR_TAG;
	}
	return IFD_SUCCESS;
} /* IFDHGetCapabilities */


RESPONSECODE IFDHSetCapabilities(DWORD Lun, DWORD Tag,
	DWORD Length, PUCHAR Value)
{
	/*
	 * This function should set the slot/card capabilities for a
	 * particular slot/card specified by Lun.  Again, if you have only 1
	 * card slot and don't mind loading a new driver for each reader then
	 * ignore Lun.
	 *
	 * Tag - the tag for the information needing set
	 *
	 * Length - the length of the returned data Value - the value of the
	 * data
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_ERROR_TAG IFD_ERROR_SET_FAILURE
	 * IFD_ERROR_VALUE_READ_ONLY
	 */

	// By default, say it worked

	LogMessage( __FILE__,  __LINE__, LogLevelVeryVerbose, "entering IFDHSetCapabilities");

	if (iLunCheck(Lun))
		return IFD_COMMUNICATION_ERROR;

	return IFD_SUCCESS;
} /* IFDHSetCapabilities */


RESPONSECODE IFDHSetProtocolParameters(DWORD Lun, DWORD Protocol,
	UCHAR Flags, UCHAR PTS1, UCHAR PTS2, UCHAR PTS3)
{
	/*
	 * This function should set the PTS of a particular card/slot using
	 * the three PTS parameters sent
	 *
	 * Protocol - 0 .... 14 T=0 .... T=14 Flags - Logical OR of possible
	 * values: IFD_NEGOTIATE_PTS1 IFD_NEGOTIATE_PTS2 IFD_NEGOTIATE_PTS3 to
	 * determine which PTS values to negotiate. PTS1,PTS2,PTS3 - PTS
	 * Values.
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_ERROR_PTS_FAILURE IFD_COMMUNICATION_ERROR
	 * IFD_PROTOCOL_NOT_SUPPORTED
	 */

	LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "entering IFDHSetProtocolParameters");

	if (iLunCheck(Lun))
		return IFD_COMMUNICATION_ERROR;

	return IFD_SUCCESS;
} /* IFDHSetProtocolParameters */


RESPONSECODE IFDHPowerICC(DWORD Lun, DWORD Action,
	PUCHAR Atr, PDWORD AtrLength)
{
	/*
	 * This function controls the power and reset signals of the smartcard
	 * reader at the particular reader/slot specified by Lun.
	 *
	 * Action - Action to be taken on the card.
	 *
	 * IFD_POWER_UP - Power and reset the card if not done so (store the
	 * ATR and return it and it's length).
	 *
	 * IFD_POWER_DOWN - Power down the card if not done already
	 * (Atr/AtrLength should be zero'd)
	 *
	 * IFD_RESET - Perform a quick reset on the card.  If the card is not
	 * powered power up the card.  (Store and return the Atr/Length)
	 *
	 * Atr - Answer to Reset of the card.  The driver is responsible for
	 * caching this value in case IFDHGetCapabilities is called requesting
	 * the ATR and it's length.  This should not exceed MAX_ATR_SIZE.
	 *
	 * AtrLength - Length of the Atr.  This should not exceed
	 * MAX_ATR_SIZE.
	 *
	 * Notes:
	 *
	 * Memory cards without an ATR should return IFD_SUCCESS on reset but
	 * the Atr should be zero'd and the length should be zero
	 *
	 * Reset errors should return zero for the AtrLength and return
	 * IFD_ERROR_POWER_ACTION.
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_ERROR_POWER_ACTION IFD_COMMUNICATION_ERROR
	 * IFD_NOT_SUPPORTED
	 */

    DWORD nlength;
    CCIDRv rv;
    BYTE bClockStatus;
    BYTE pcbuffer[200];
	LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "entering IFDHPowerICC");

	// By default, assume it won't work :)
	*AtrLength = 0;

	if (iLunCheck(Lun))
		return IFD_COMMUNICATION_ERROR;

	switch (Action)
	{
		case IFD_POWER_UP:
		case IFD_RESET:
			nlength = sizeof(pcbuffer);
			if ((rv = CCID_IccPowerOn(Lun, pcbuffer, &nlength)) != CCIDRv_OK)
			{
				LogMessage( __FILE__,  __LINE__, LogLevelCritical, "PowerUp failed");
				return IFD_ERROR_POWER_ACTION;
			}
                
			// Power up successful, set state variable to memorise it
			pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
				bPowerFlags |= MASK_POWERFLAGS_PUP;
			pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
				bPowerFlags &= ~MASK_POWERFLAGS_PDWN;

			// Reset is returned, even if TCK is wrong
			pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
				nATRLength = *AtrLength =
				(nlength < MAX_ATR_SIZE) ? nlength : MAX_ATR_SIZE;
			memcpy(Atr, pcbuffer, *AtrLength);
			memcpy(pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
				pcATRBuffer, pcbuffer, *AtrLength);

			break;

		case IFD_POWER_DOWN:
			// Clear ATR buffer
			pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].nATRLength =
				0;
			*pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
				pcATRBuffer = '\0';
			// Memorise the request
			pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].bPowerFlags
				|= MASK_POWERFLAGS_PDWN;
			// send the command
			rv = CCID_IccPowerOff(Lun, &bClockStatus);
            if (rv != CCIDRv_OK)
            {
                return IFD_ERROR_POWER_ACTION;
            }
            break;

		default:
			LogMessage( __FILE__,  __LINE__, LogLevelCritical, "IFDHPowerICC Action not supported");
			return IFD_NOT_SUPPORTED;
	}
	return IFD_SUCCESS;
} /* IFDHPowerICC */


RESPONSECODE IFDHTransmitToICC(DWORD Lun, SCARD_IO_HEADER SendPci,
	PUCHAR TxBuffer, DWORD TxLength,
	PUCHAR RxBuffer, PDWORD RxLength, PSCARD_IO_HEADER RecvPci)
{
	/*
	 * This function performs an APDU exchange with the card/slot
	 * specified by Lun.  The driver is responsible for performing any
	 * protocol specific exchanges such as T=0/1 ... differences.  Calling
	 * this function will abstract all protocol differences.
	 *
	 * SendPci Protocol - 0, 1, .... 14 Length - Not used.
	 *
	 * TxBuffer - Transmit APDU example (0x00 0xA4 0x00 0x00 0x02 0x3F
	 * 0x00) TxLength - Length of this buffer. RxBuffer - Receive APDU
	 * example (0x61 0x14) RxLength - Length of the received APDU.  This
	 * function will be passed the size of the buffer of RxBuffer and this
	 * function is responsible for setting this to the length of the
	 * received APDU.  This should be ZERO on all errors.  The resource
	 * manager will take responsibility of zeroing out any temporary APDU
	 * buffers for security reasons.
	 *
	 * RecvPci Protocol - 0, 1, .... 14 Length - Not used.
	 *
	 * Notes: The driver is responsible for knowing what type of card it
	 * has.  If the current slot/card contains a memory card then this
	 * command should ignore the Protocol and use the MCT style commands
	 * for support for these style cards and transmit them appropriately.
	 * If your reader does not support memory cards or you don't want to
	 * then ignore this.
	 *
	 * RxLength should be set to zero on error.
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_COMMUNICATION_ERROR IFD_RESPONSE_TIMEOUT
	 * IFD_ICC_NOT_PRESENT IFD_PROTOCOL_NOT_SUPPORTED
	 */

	RESPONSECODE return_value = IFD_SUCCESS;	// Assume it will work
    CCIDRv rv;

	LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "entering IFDHTransmitToICC");
	if (iLunCheck(Lun))
		return IFD_COMMUNICATION_ERROR;

	LogHexBuffer(__FILE__,  __LINE__, LogLevelVeryVerbose, TxBuffer, TxLength, "APDU sent: ");

	switch (SendPci.Protocol)
	{
		case T_0:
		case T_1:
            rv = CCID_XfrBlock(Lun, 0, SendPci.Protocol, TxBuffer, TxLength,
                               RxBuffer, RxLength);
            if ( rv != CCIDRv_OK)
            {
                switch (rv)
                {
                    case CCIDRv_ERR_ICC_MUTE:
                        return_value = IFD_RESPONSE_TIMEOUT;
                        break;
                    case CCIDRv_ERR_CARD_ABSENT:
                        return_value = IFD_ICC_NOT_PRESENT;
                        break;
                    default:
                        return_value = IFD_COMMUNICATION_ERROR;
                }
            }
            
			break;

		default:
			return_value = IFD_PROTOCOL_NOT_SUPPORTED;
	}

	if (return_value != CCIDRv_OK)
		*RxLength = 0;

	if (*RxLength)
		LogHexBuffer(__FILE__,  __LINE__, LogLevelVeryVerbose, RxBuffer, *RxLength, "APDU response: ");

	return return_value;

} /* IFDHTransmitToICC */


RESPONSECODE IFDHControl(DWORD Lun, PUCHAR TxBuffer,
	DWORD TxLength, PUCHAR RxBuffer, PDWORD RxLength)
{
	/*
	 * This function performs a data exchange with the reader (not the
	 * card) specified by Lun.  Here XXXX will only be used. It is
	 * responsible for abstracting functionality such as PIN pads,
	 * biometrics, LCD panels, etc.  You should follow the MCT, CTBCS
	 * specifications for a list of accepted commands to implement.
	 *
	 * TxBuffer - Transmit data TxLength - Length of this buffer. RxBuffer
	 * - Receive data RxLength - Length of the received data.  This
	 * function will be passed the length of the buffer RxBuffer and it
	 * must set this to the length of the received data.
	 *
	 * Notes: RxLength should be zero on error.
	 */

	LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "entering IFDHControl");

	if (iLunCheck(Lun))
		return IFD_COMMUNICATION_ERROR;

	return IFD_SUCCESS;
} /* IFDHControl */


RESPONSECODE IFDHICCPresence(DWORD Lun)
{
	/*
	 * This function returns the status of the card inserted in the
	 * reader/slot specified by Lun.  It will return either:
	 *
	 * returns: IFD_ICC_PRESENT IFD_ICC_NOT_PRESENT
	 * IFD_COMMUNICATION_ERROR
	 */
    BYTE bStatus, bClockStatus;
    BYTE bICCStatus;
    // Set log level to only critical
    if ( !bLogPeriodic )
    {
        SetLogLevel((BYTE)LogLevelCritical);
    }
	LogMessage( __FILE__,  __LINE__, LogLevelVeryVerbose, "entering IFDHICCPresence");

	if (iLunCheck(Lun))
    {
        SetLogLevel((BYTE)iLogValue);
		return IFD_COMMUNICATION_ERROR;
    }
	if (CCID_GetSlotStatus(Lun, &bStatus, &bClockStatus) != CCIDRv_OK)
	{
		LogMessage( __FILE__,  __LINE__, LogLevelCritical, "GCCmdCardStatus failed");
        {
            SetLogLevel((BYTE)iLogValue);
            return IFD_COMMUNICATION_ERROR;
        }
	}
    bICCStatus = CCIDGetICCStatus(bStatus);
	if ( bICCStatus == CCID_ICC_STATUS_ABSENT )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelVeryVerbose, "Card absent");
        // Clear ATR buffer
        pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].nATRLength = 0;
        *pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
            pcATRBuffer = '\0';
        // Card removed, clear the flags
        pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].bPowerFlags =
            POWERFLAGS_RAZ;
        SetLogLevel((BYTE)iLogValue);
        return IFD_ICC_NOT_PRESENT;
    }
    else
	{
		// Card is present, but is it powered-up?
		if (bICCStatus == CCID_ICC_STATUS_ACTIVE)
		{
			LogMessage( __FILE__,  __LINE__, LogLevelVeryVerbose, "Card present and powered");
			// Powered, so the ressource manager did not miss a quick
			// removal/re-insertion
            SetLogLevel((BYTE)iLogValue);
			return IFD_ICC_PRESENT;
		}
		else
		{
			// Card present but not powered up
			// Check if a power down has been requested
			if (pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
				bPowerFlags & MASK_POWERFLAGS_PDWN)
			{
				LogMessage( __FILE__,  __LINE__, LogLevelVeryVerbose, "Card present not powered, power down requested");
				// Powerdown requested, so situation is normal
                SetLogLevel((BYTE)iLogValue);
				return IFD_ICC_PRESENT;
			}

			// Card inserted, not powered on but power down has not been
			// requested
			// Has the card been powered up already?
			if (pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
				bPowerFlags & MASK_POWERFLAGS_PUP)
			{
				LogMessage( __FILE__,  __LINE__, LogLevelVeryVerbose, "Card pull-out+re-insert detected CARD OUT SIMULATION");
				// Power-up has been requested, but not power down and power is
				// down.  This should happen only if the card has been pulled
				// out and reinserted too quickly for the resource manager to
				// realise. A card out event is therefore simulated. Clear ATR
				// buffer
				pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
					nATRLength = 0;
				*pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
					pcATRBuffer = '\0';
				// reset power flags
				pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
					bPowerFlags = POWERFLAGS_RAZ;
                SetLogLevel((BYTE)iLogValue);
				return IFD_ICC_NOT_PRESENT;
			}

			LogMessage( __FILE__,  __LINE__, LogLevelVeryVerbose, "Card present, just inserted");
			// If control gets here, the card is in, not powered on, with
			// no power down request and no previous power up request
			// it is therefore a card insertion event
			pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
				nATRLength = 0;
			*pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
				pcATRBuffer = '\0';
			// reset power flags
			pstIFDDescs[LunToReaderLun(Lun)][LunToSlotNb(Lun)].
				bPowerFlags = POWERFLAGS_RAZ;
            SetLogLevel((BYTE)iLogValue);
			return IFD_ICC_PRESENT;
		}
	}
} /* IFDHICCPresence */

