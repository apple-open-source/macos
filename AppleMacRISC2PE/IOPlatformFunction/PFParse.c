#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#include <libkern/OSTypes.h>
#include <c.h>

#include "IOPlatformFunction.h"

	//clock speed: 00000000 10000000 00000000 00000001 0000000e 00000004 00024618
#define clockspeed "    | |   | |   \"platform-do-clockspeed\" = <000000001000000000000000000000010000000e0000000400024618>"
#define disable "ff9adfe0 08000000 00000003 00000020 00000000 ffffffef"
#define enable "0xff9adfe0 0x08000000 00000000 00000003 00000003 00000020 00000010 ffffffef \
				00000013 0000000c 00000004 00000015 0000000c 00000004 00000004 00000004 \
				ffff0000 00002008"

/************************  Test Cases  ***********************************************************/
static char *gPFTests[] = {
	// Single tests - one for each command; values must be fully enumerated because they represent a byte stream
	"platform-do-CommandWriteGPIO 				0x00000001 0x0c000000 0x00000001 0x00000001 0xfffffffe",
	"platform-do-CommandReadGPIO 				0x00000002 0x0c000000 0x00000002 0x00000002 0x00000001 0xfffffffd",
	"platform-do-CommandWriteReg32				0x00000003 0x0c000000 0x00000003 0x00000003 0x00000003 0xfffffffc",
	"platform-do-CommandReadReg32				0x00000004 0x0c000000 0x00000004 0x00000004",
	"platform-do-CommandWriteReg16				0x00000005 0x0c000000 0x00000005 0x00000005 0x00000005 0xfffffffa",
	"platform-do-CommandReadReg16				0x00000006 0x0c000000 0x00000006 0x00000006",
	"platform-do-CommandWriteReg8				0x00000007 0x0c000000 0x00000007 0x00000007 0x00000007 0xfffffff8",
	"platform-do-CommandReadReg8				0x00000008 0x0c000000 0x00000008 0x00000008",
	"platform-do-CommandDelay					0x00000009 0x0c000000 0x00000009 0x00000100",
	"platform-do-CommandWaitReg32				0x0000000a 0x0c000000 0x0000000a 0x000000a0 0x0000000a 0xfffffff5",
	"platform-do-CommandWaitReg16				0x0000000b 0x0c000000 0x0000000b 0x000000b0 0x0000000b 0xfffffff4",
	"platform-do-CommandWaitReg8				0x0000000c 0x0c000000 0x0000000c 0x000000c0 0x0000000c 0xfffffff3",
	"platform-do-CommandReadI2C					0x0000000d 0x0c000000 0x0000000d 0x0000000d",
	"platform-do-CommandWriteI2C				0x0000000e 0x0c000000 0x0000000e 0x00000006 0x12345678 0x9abc",
	"platform-do-CommandRMWI2C					0x0000000f 0x0c000000 0x0000000f 0x00000006 0x00000006 0x00000006 0x12345678 0x9abccba9 0x87654321",
	"platform-do-CommandShiftBytesRight			0x00000011 0x0c000000 0x00000011 0x00000006 0x00000002",
	"platform-do-CommandShiftBytesLeft			0x00000012 0x0c000000 0x00000012 0x00000006 0x00000004",
	"platform-do-CommandReadConfig				0x00000013 0x0c000000 0x00000013 0x00000013 0x00000002",
	"platform-do-CommandWriteConfig				0x00000014 0x0c000000 0x00000014 0x00000014 0x00000006 0x12345678 0x9abc",
	"platform-do-CommandRMWConfig				0x00000015 0x0c000000 0x00000015 0x00000015 0x00000006 0x00000006 0x00000006 0x12345678 0x9abccba9 0x87654321",
	"platform-do-CommandReadI2CSubAddr			0x00000016 0x0c000000 0x00000016 0x00000016 0x00000005",
	"platform-do-CommandWriteI2CSubAddr			0x00000017 0x0c000000 0x00000017 0x00000017 0x00000006 0x12345678 0x9abc",
	"platform-do-CommandI2CMode					0x00000018 0x0c000000 0x00000018 0x00000002",
	"platform-do-CommandRMWI2CSubAddr			0x00000019 0x0c000000 0x00000019 0x00000019 0x00000006 0x00000006 0x00000006 0x12345678 0x9abccba9 0x87654321",
	"platform-do-CommandReadReg32MaskShRtXOR	0x0000001a 0x0c000000 0x0000001a 0x0000001a 0x00000001 0x00000002 0x00000003",
	"platform-do-CommandReadReg16MaskShRtXOR	0x0000001b 0x0c000000 0x0000001b 0x0000001b 0x00000004 0x00000005 0x00000006",
	"platform-do-CommandReadReg8MaskShRtXOR		0x0000001c 0x0c000000 0x0000001c 0x0000001c 0x00000007 0x00000008 0x00000009",
	"platform-do-CommandWriteReg32ShLtMask		0x0000001d 0x0c000000 0x0000001d 0x0000001d 0x0000000a 0x0000000b",
	"platform-do-CommandWriteReg16ShLtMask		0x0000001e 0x0c000000 0x0000001e 0x0000001e 0x0000000c 0x0000000d",
	"platform-do-CommandWriteReg8ShLtMask		0x0000001f 0x0c000000 0x0000001f 0x0000001f 0x0000000e 0x0000000f",
	"platform-do-CommandMaskandCompare			0x00000020 0x0c000000 0x00000020 0x00000006 0x12345678 0x9abccba9 0x87654321",
	"platform-do-CommandShiftBitStreamRight		0x00000021 0x0c000000 0x00000021 0x00000008 0x00000001",
	"platform-do-CommandShiftBitStreamLeft		0x00000022 0x0c000000 0x00000022 0x00000008 0x00000001",
	"platform-do-CommandMaskByteStream			0x00000023 0x0c000000 0x00000023 0x00000008 0x12345678 0x9abcdef0",
	"platform-do-CommandXorByteStream			0x00000024 0x0c000000 0x00000024 0x00000008 0x12345678 0x9abcdef0",
	"platform-do-CommandWriteI2CValueToSubAddr	0x00000025 0x0c000000 0x00000025 0x0000007f 0x00000002",
	"platform-do-CommandORValueToByteStream		0x00000026 0x0c000000 0x00000026 0x00000002",
	"platform-do-CommandImplementationSpecific	0x00000027 0x0c000000 0x00000027",
	

	// Complex tests - command lists; values must be fully enumerated because they represent a byte stream
	"platform-do-CmdLWriteReadGPIO 				0x00000201 0x0c000000 0x00000000 0x00000002 0x00000001 0x00000001 0xfffffffe 0x00000002 0x00000002 0x00000001 0xfffffffd",
	"platform-do-CmdL2WriteI2C					0x00000e0e 0x0c000000 0x00000000 0x00000002 0x0000000e 0x00000006 0x12345678 0x9abc0000 0x000e0000 0x00062345 0x6789abcd",
	"platform-do-CmdL3WriteI2C					0x000e0e0e 0x0c000000 0x00000000 0x00000003 0x0000000e 0x00000006 0x12345678 0x9abc0000 0x000e0000 0x00062345 0x6789abcd 0x0000000e 0x00000006 0x3456789a 0xbcde",
	"platform-do-CmdLWI2CDlyMandC				0x0020090e 0x0c000000 0x00000000 0x00000003 0x0000000e 0x00000006 0x12345678 0x9abc0000 0x00090000 0x01000000 0x00200000 0x00061234 0x56789abc 0xcba98765 0x4321",

	// Complex tests - multiple single commands; values must be fully enumerated because they represent a byte stream
	"platform-do-MplWI2CSubRMWI2CSub			0x00001917 0x0c000000 0x00000017 0x00000017 0x00000006 0x12345678 0x9abc0000 0x00190c00 0x00000000 0x00190000 0x00190000 0x00060000 0x00060000 0x00061234 0x56789abc 0xcba98765 0x4321",
	"platform-do-MplRI2CWI2CRMWI2C				0x00151413 0x0c000000 0x0000000d 0x0000000d 0x0000000e 0x0c000000 0x0000000e 0x00000006 0x12345678 0x9abc0000 0x000f0c00 0x00000000 0x000f0000 0x00060000 0x00060000 0x00061234 0x56789abc 0xcba98765 0x4321",
	"platform-do-MplRCfgWCfgRMWCfg				0x000f0e0d 0x0c000000 0x00000013 0x00000013 0x00000002 0x00000014 0x0c000000 0x00000014 0x00000014 0x00000006 0x12345678 0x9abc0000 0x00150c00 0x00000000 0x00150000 0x00150000 0x00060000 0x00060000 0x00061234 0x56789abc 0xcba98765 0x4321",

	// Complex tests - mixed single commands and command lists; values must be fully enumerated because they represent a byte stream
	"platform-do-CmplxRCfgWCfgRMWCfgLWI2CDlyMC	0x000f0e0d 0x0c000000 0x00000013 0x00000013 0x00000002 0x00000014 0x0c000000 0x00000014 0x00000014 0x00000006 0x12345678 0x9abc0000 0x00150c00 0x00000000 0x00150000 0x00150000 0x00060000 0x00060000 0x00061234 0x56789abc 0xcba98765 0x43210020 0x090e0c00 0x00000000 0x00000000 0x00030000 0x000e0000 0x00061234 0x56789abc 0x00000009 0x00000100 0x00000020 0x00000006 0x12345678 0x9abccba9 0x87654321",
	"platform-do-CmplxLWI2CDlyMCRI2CWI2CRMWI2C	0x0020090e 0x0c000000 0x00000000 0x00000003 0x0000000e 0x00000006 0x12345678 0x9abc0000 0x00090000 0x01000000 0x00200000 0x00061234 0x56789abc 0xcba98765 0x43210015 0x14130c00 0x00000000 0x000d0000 0x000d0000 0x000e0c00 0x00000000 0x000e0000 0x00061234 0x56789abc 0x0000000f 0x0c000000 0x0000000f 0x00000006 0x00000006 0x00000006 0x12345678 0x9abccba9 0x87654321",
	NULL										// Terminate list
};
/**********************  End Test Cases  *********************************************************/

static UInt32	gCmdArray[256];
static UInt32	gDebugLevel;
static bool		gDoStandardTests;

// globals for IOPlatformFunction code
UInt32				*commandPtr, *platformFunctionPtr;
UInt32				dataLengthRemaining;
bool				isCommandList;
bool				commandDone;
UInt32				totalCommandCount;
UInt32				currentCommandCount;

// extern functions from IOPlatformFunction.cpp
extern bool getNextCommand(UInt32 *cmd, UInt32 *cmdLen,
					UInt32 *param1, UInt32 *param2, UInt32 *param3, UInt32 *param4, 
					UInt32 *param5, UInt32 *param6, UInt32 *param7, UInt32 *param8, 
					UInt32 *param9, UInt32 *param10, UInt32 *result);
					
extern UInt32 *scanSubCommand (UInt32 *cmdPtr, UInt32 lenRemaining,
					bool quickScan, UInt32 *cmd, UInt32 *cmdLen,
					UInt32 *param1, UInt32 *param2, UInt32 *param3, UInt32 *param4, 
					UInt32 *param5, UInt32 *param6, UInt32 *param7, UInt32 *param8, 
					UInt32 *param9, UInt32 *param10, UInt32 *result);
					
extern UInt32 *scanCommand (UInt32 *cmdPtr, UInt32 dataLen, UInt32 *cmdTotalLen,
					UInt32 *flags, UInt32 *pHandle, UInt32 *result);

void dprintf(const char *fmt, ...)
{
	va_list   listp;
	
	if (gDebugLevel > 0) {
		va_start(listp, fmt);
		vprintf(fmt, listp);
		va_end(listp);
	}
	
	return;
}

bool scanString (char *str, char *returnFunctionName, UInt8 *outArray, UInt32 *byteCount)
{
	bool result, gotChar;
	char c;
	UInt32 nybbleCount;
	UInt8 baseChar, byte, *outByteArray;
	
	*byteCount = 0;
	nybbleCount = 0;
	byte = 0;
	outByteArray = (UInt8 *)outArray;
	
	result = gotChar = false;
	
	// prescan
	while (c = *str) {
		// Skip white space and non-essential characters
		if (isblank (c) || (c == '|') || (c == '=')) {
			str++;
			continue;
		}
		if (c == '"' || c == 'p') {
			if (c == '"') str++;

			// Copy platform function name
			while ((c = *str) && (c != '"') && (!isblank (c))) *(returnFunctionName++) = *(str++);	

			if (*str) {
				str++;
				continue;
			} else
				break;
		}
		
		// If we get here, done with prescan
		break;
	}
	*returnFunctionName = '\0';

	while (c = *(str++)) {
		baseChar = 0xff;
		switch (c) {
			case '0':
				// If this is 0x..., skip it
				if ((*str == 'x') || (*str == 'X')) {
					str++;
					break;
				} // else fall thru
			case '1': case '2': case '3': case '4': case '5':
			case '6': case '7': case '8': case '9':
				baseChar = '0';
				break;
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
				baseChar = 'A' - 10;
				break;
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
				baseChar = 'a' - 10;
				break;
			// Look for delimiter
			case ' ':
			case '\t':
			case '>':
			case '<':
				// Skip
				break;
			case '\n':
			case '\r':
				return true; 	// Done
			default:
				printf ("Non-hex input character '%c' (0x%x\n", c, c);
				return false;				
		}
		if (baseChar != 0xff) {
			byte = ((byte << 4) & 0xFFFFFFF0) + (c - (char )baseChar);
			nybbleCount++;
			if (nybbleCount >= 2) {
				outArray[(*byteCount)++] = byte;
				nybbleCount = 0;
				byte = 0;
			}
		}
	}
	
	return true;
}


void FormatByteArray (char *str, UInt8 *array, UInt32 len)
{
	UInt32 i;
	char	tmpString[16];
	
	*str = '\0';
	for (i = 0; i < len; i++) {
		sprintf (tmpString, "%02X ", array[i]);
		strcat (str, tmpString);
	}
	return;
}


UInt32 *scanThisCommand (UInt32 *cmdArray, UInt32 cmdLength)
{
	bool   printBar;
	char   tmpString[128];
	UInt32 *moreData;
	UInt32 totalLen, flags;
	UInt32 cmd, cmdLen, result, pHandle, param1, param2, param3, param4, param5, 
				param6, param7, param8, param9, param10;
		
	platformFunctionPtr = cmdArray;
	
	dataLengthRemaining = cmdLength;
	commandDone = isCommandList = false;
	totalCommandCount = currentCommandCount = 0;
	
	moreData = scanCommand (cmdArray, cmdLength, &totalLen,
		&flags, &pHandle, &result);
	
	commandPtr = platformFunctionPtr;
	dataLengthRemaining = cmdLength;
	commandDone = false;
	totalCommandCount = currentCommandCount = 0;
	
	while (getNextCommand (&cmd, &cmdLen, &param1, &param2, &param3, &param4, 
		&param5, &param6, &param7, &param8, &param9, &param10, &result)) {
		if (isCommandList) {
			switch (currentCommandCount) {
				case 1:  printf ("   First "); break;
				case 2:  printf ("  Second "); break;
				case 3:  printf ("   Third "); break;
				case 4:  printf ("  Fourth "); break;
				case 5:  printf ("   Fifth "); break;
				case 6:  printf ("   Sixth "); break;
				case 7:  printf (" Seventh "); break;
				case 8:  printf ("  Eighth "); break;
				case 9:  printf ("   Ninth "); break;
				case 10: printf ("   Tenth "); break;
				case 11: printf ("Eleventh "); break;
				case 12: printf (" Twelfth "); break;
				default: printf ("    %ldth ", currentCommandCount+1);  break;
			}
		}
			else printf ("  Single ");
		printf ("command[0x%lx(%ldD)]: pHandle 0x%lx, flags 0x%lx ( ", cmd, cmd, pHandle, flags);
		printBar = false;
		if (flags & kIOPFFlagOnInit)		{ if (printBar) printf (" | "); printf ("OnInit");       printBar = true; }
		if (flags & kIOPFFlagOnTerm)		{ if (printBar) printf (" | "); printf ("OnTerm");       printBar = true; }
		if (flags & kIOPFFlagOnSleep)		{ if (printBar) printf (" | "); printf ("OnSleep");      printBar = true; }
		if (flags & kIOPFFlagOnWake)		{ if (printBar) printf (" | "); printf ("OnWake");       printBar = true; }
		if (flags & kIOPFFlagOnDemand)		{ if (printBar) printf (" | "); printf ("OnDemand");     printBar = true; }
		if (flags & kIOPFFlagIntGen)		{ if (printBar) printf (" | "); printf ("IntGen");       printBar = true; }
		if (flags & kIOPFFlagHighSpeed) 	{ if (printBar) printf (" | "); printf ("HighSpeed");    printBar = true; }
		if (flags & kIOPFFlagLowSpeed)		{ if (printBar) printf (" | "); printf ("LowSpeed");     printBar = true; }
		if (flags & kIOPFFlagSideEffects)	{ if (printBar) printf (" | "); printf ("SideEffects");  printBar = true; }
		printf (" )\n");
		switch (cmd) {
			case kCommandCommandList:
				break;
			case kCommandWriteGPIO:
				printf ("      WriteGPIO - Parameters: ");
				printf ("Byte value 0x%lx, byte mask 0x%lx\n", param1, param2);
				break;
			case kCommandReadGPIO:
				printf ("      ReadGPIO - Parameters: ");
				printf ("Byte mask 0x%lx, right shift 0x%lx, byte XOR 0x%lx \n", param1, param2, param3);
				break;
			case kCommandWriteReg32:
				printf ("      WriteReg32 - Parameters: ");
				printf ("Offset 0x%lx (%ldD), long value 0x%lx, long mask 0x%lx\n", param1, param1, param2, param3);
				break;
			case kCommandReadReg32:
				printf ("      ReadReg32 - Parameters: ");
				printf ("Offset 0x%lx (%ldD)\n", param1, param1);
				break;
			case kCommandWriteReg16:
				printf ("      WriteReg16 - Parameters: ");
				printf ("Offset 0x%lx (%ldD), short value 0x%lx, short mask 0x%lx\n", param1, param1, param2, param3);
				break;
			case kCommandReadReg16:
				printf ("      ReadReg16 - Parameters: ");
				printf ("Offset 0x%lx (%ldD)\n", param1, param1);
				break;
			case kCommandWriteReg8:
				printf ("      WriteReg8 - Parameters: ");
				printf ("Offset 0x%lx (%ldD), byte value 0x%lx, byte mask 0x%lx\n", param1, param1, param2, param3);
				break;
			case kCommandReadReg8:
				printf ("      ReadReg8 - Parameters: ");
				printf ("Offset 0x%lx (%ldD)\n", param1, param1);
				break;
			case kCommandDelay:
				printf ("      Delay - Parameters: ");
				printf ("Delay %ld microseconds\n", param1);
				break;
			case kCommandWaitReg32:
				printf ("      WaitReg32 - Parameters: ");
				printf ("Offset 0x%lx (%ldD), long completion value 0x%lx, long mask 0x%lx\n", param1, param1, param2, param3);
				break;
			case kCommandWaitReg16:
				printf ("      WaitReg16 - Parameters: ");
				printf ("Offset 0x%lx (%ldD), short completion value 0x%lx, short mask 0x%lx\n", param1, param1, param2, param3);
				break;
			case kCommandWaitReg8:
				printf ("      WaitReg8 - Parameters: ");
				printf ("Offset 0x%lx (%ldD), byte completion value 0x%lx, byte mask 0x%lx\n", param1, param1, param2, param3);
				break;
			case kCommandReadI2C:
				printf ("      ReadI2C - Parameters: ");
				printf ("Number of bytes to read %ld\n", param1);
				break;
			case kCommandWriteI2C:
				printf ("      WriteI2C - Parameters: ");
				FormatByteArray (tmpString, (UInt8 *)param2, param1);
				printf ("Bytes to write: %s\n", tmpString);
				break;
			case kCommandRMWI2C:
				printf ("      Modify and write I2C - Parameters: \n");
				FormatByteArray (tmpString, (UInt8 *)param4, param1);
				printf ("             Mask array: %s\n", tmpString);
				FormatByteArray (tmpString, (UInt8 *)param5, param2);
				printf ("            Value array: %s\n", tmpString);
				printf ("            Transfer %ld bytes\n", param3);
				break;
			case kCommandGeneralI2C:
				printf ("      GeneralI2C: ");
				printf ("This command makes no sense\n");
				break;
			case kCommandShiftBytesRight:
				printf ("      Shift bytes right - Parameters: ");
				printf ("Number of bytes to shift %ld, number of bytes to shift by %ld\n", param1, param2);
				break;
			case kCommandShiftBytesLeft:
				printf ("      Shift bytes left - Parameters: ");
				printf ("Number of bytes to shift %ld, number of bytes to shift by %ld\n", param1, param2);
				break;
			case kCommandReadConfig:
				printf ("      ReadConfig - Parameters: ");
				printf ("Offset 0x%lx (%ldD), number of bytes to read %ld\n", param1, param1, param2);
				break;
			case kCommandWriteConfig:
				printf ("      WriteConfig - Parameters: ");
				printf ("Offset 0x%lx (%ldD), number of bytes to read %ld\n", param1, param1, param2);
				FormatByteArray (tmpString, (UInt8 *)param3, param2);
				printf ("            Bytes to write array: %s\n", tmpString);
				break;
			case kCommandRMWConfig:
				printf ("      Modify and write config - Parameters: ");
				printf ("Offset 0x%lx (%ldD)\n", param1, param1);
				FormatByteArray (tmpString, (UInt8 *)param5, param2);
				printf ("             Mask array: %s\n", tmpString);
				FormatByteArray (tmpString, (UInt8 *)param6, param3);
				printf ("            Value array: %s\n", tmpString);
				printf ("            Transfer %ld bytes\n", param4);
				break;
			case kCommandReadI2CSubAddr:
				printf ("      Read I2C subaddress - Parameters: ");
				printf ("8-bit subaddress to read %ld, number of bytes read %ld\n", param1, param2);
				break;
			case kCommandWriteI2CSubAddr:
				printf ("      Write I2C subaddress - Parameters: ");
				printf ("8-bit subaddress to read %ld\n", param1);
				FormatByteArray (tmpString, (UInt8 *)param3, param2);
				printf ("            Bytes to write array: %s\n", tmpString);
				break;
			case kCommandI2CMode:
				printf ("      I2C mode - set ");
				switch (param1) {
					case 1:
						printf ("dumb mode\n");
						break;
					case 2:
						printf ("standard mode\n");
						break;
					case 3:
						printf ("standard sub mode\n");
						break;
					case 4:
						printf ("combined mode\n");
						break;
					default:
						printf ("OOOPS mode\n");	// unknown mode
						break;
				}
				break;
			case kCommandRMWI2CSubAddr:
				printf ("      Modify and write config - Parameters: ");
				printf (" 8-bit subaddress %ld\n", param1);
				FormatByteArray (tmpString, (UInt8 *)param5, param2);
				printf ("             Mask array: %s\n", tmpString);
				FormatByteArray (tmpString, (UInt8 *)param6, param3);
				printf ("            Value array: %s\n", tmpString);
				printf ("            Transfer %ld bytes\n", param4);
				break;
			case kCommandReadReg32MaskShRtXOR:
				printf ("      Read reg 32 with mask, shift right and XOR - Parameters: ");
				printf ("offset 0x%lx, mask 0x%lx, right shift 0x%lx, XOR 0x%lx\n", param1, param2, param3, param4);
				break;
			case kCommandReadReg16MaskShRtXOR:
				printf ("      Read reg 16 with mask, shift right and XOR - Parameters: ");
				printf ("offset 0x%lx, mask 0x%lx, right shift 0x%lx, XOR 0x%lx\n", param1, param2, param3, param4);
				break;
			case kCommandReadReg8MaskShRtXOR:
				printf ("      Read reg 8 with mask, shift right and XOR - Parameters: ");
				printf ("offset 0x%lx, mask 0x%lx, right shift 0x%lx, XOR 0x%lx\n", param1, param2, param3, param4);
				break;
			case kCommandWriteReg32ShLtMask:
				printf ("      Write reg 32 with shift left and mask - Parameters: ");
				printf ("offset 0x%lx, left shift 0x%lx, mask 0x%lx\n", param1, param2, param3);
				break;
			case kCommandWriteReg16ShLtMask:
				printf ("      Write reg 16 with shift left and mask - Parameters: ");
				printf ("offset 0x%lx, left shift 0x%lx, mask 0x%lx\n", param1, param2, param3);
				break;
			case kCommandWriteReg8ShLtMask:
				printf ("      Write reg 8 with shift left and mask - Parameters: ");
				printf ("offset 0x%lx, left shift 0x%lx, mask 0x%lx\n", param1, param2, param3);
				break;
			case kCommandMaskandCompare:
				printf ("      Mask and compare byte stream - Parameters: \n");
				FormatByteArray (tmpString, (UInt8 *)param2, param1);
				printf ("             Mask array: %s\n", tmpString);
				FormatByteArray (tmpString, (UInt8 *)param3, param1);
				printf ("          Compare array: %s\n", tmpString);
				break;

			case kCommandShiftBitStreamRight:
				printf ("      Shift bit stream right - Parameters: \n");
				printf ("             number of bytes in bytes stream: %ld\n", param1);
				printf ("             number of bits to shift by: %ld\n", param2);
				break;
			case kCommandShiftBitStreamLeft:
				printf ("      Shift bit stream left - Parameters: \n");
				printf ("             number of bytes in bytes stream: %ld\n", param1);
				printf ("             number of bits to shift by: %ld\n", param2);
				break;
			case kCommandMaskByteStream:
				printf ("      Mask byte stream - Parameters: \n");
				FormatByteArray (tmpString, (UInt8 *)param2, param1);
				printf ("             Mask array: %s\n", tmpString);
				break;
			case kCommandXorByteStream:
				printf ("      Xor byte stream - Parameters: \n");
				FormatByteArray (tmpString, (UInt8 *)param2, param1);
				printf ("             Xor array: %s\n", tmpString);
				break;
			case kCommandWriteI2CValueToSubAddr:
				printf ("      Write supplied value to I2C subaddress - Parameters: \n");
				printf ("             subaddress: 0x%lx\n", param1);
				printf ("             number of bytes: %ld\n", param2);
				break;
			case kCommandXORValueToByteStream:
				printf ("      XOR supplied value to byte stream - Parameters: \n");
				printf ("             number of bytes in supplied value: %ld\n", param1);
				break;
			case kCommandImplementationSpecific:
				printf ("      Implementation specific function - NOP\n");
				break;
			default: 
				printf (" Unknown command %ld\n", cmd);
				break;
		}
		if (!isCommandList || (currentCommandCount == totalCommandCount)) break;
	}
	
	if (result != kIOPFNoError) {
		printf ("getNextCommand returned error %ld\n", result);
		return NULL;
	}
	//printf ("Scanned, moreData = 0x%lx, result = %ld\n", moreData, result);
	
	return moreData;
}

void ShowLine (char *line)
{
	char	funcName[128];
	UInt32	*array, *moreData, arrayByteCount;
	int 	i;

	if (scanString (line, funcName, (UInt8 *)gCmdArray, &arrayByteCount)) {
		if (*funcName)
			printf ("%s: ", funcName);
		else
			printf ("nofuncName: ");
		printf ("\n");
		array = gCmdArray;
		do {
			moreData = scanThisCommand (array, arrayByteCount);
			if (moreData) {
				// Adjust count
				arrayByteCount -= ((UInt32) moreData - (UInt32)array);
				array = moreData;
				DLOG ("MoreData : ");
				for (i = 0; i < arrayByteCount/4; i++)
					DLOG ("0x%lx ", array[i]);
			}
			printf ("\n");
		} while (moreData);
		printf ("\n");
	} else printf (" line scan failed\n\n");
	
	return;
}

void PerformStandardTests () 
{
	char *cp;
	int i;
	
	i = 0;
	
	while (cp = gPFTests[i++]) {
		ShowLine (cp);
	}
	
	return;
}

int main(int argc, const char *argv[])
{
	int		i, lineCount;
	char	line[1024];
	
	gDebugLevel = 0;
	gDoStandardTests = false;
	for (i = 1; i < argc; i++) {
		if (!strcmp ("-d", argv[i])) {
			gDebugLevel = 1;
		} else if (!strcmp ("-s", argv[i])) {
			gDoStandardTests = true;
		} else {
			if (strcmp ("-h", argv[i]))
				printf ("%s: invalid argument %s\n", argv[0], argv[i]);
				
			printf ("%s usage: %s -s [-d] - perform standard tests\n", argv[0], argv[0]);
			printf ("%s usage: ioreg -l -w 0 | grep platform-do | %s [-d]\n", argv[0], argv[0]);
			return 1;
		}
	}
	
	if (gDoStandardTests) {
		printf ("Performing standard tests\n");
		PerformStandardTests();
		return 0;
	}

	lineCount = 0;
	while (fgets (line, 1023, stdin)) {
		DLOG ("Line(%d): %s\n", lineCount, line);
		ShowLine (line);
		lineCount++;
	}
	if (!lineCount)
		printf ("%s usage: ioreg -l -w 0 | grep platform-do | %s [-d]\n", argv[0], argv[0]);
	
	return 0;
}