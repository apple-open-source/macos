/*
    File:       ttppdu.h

    Contains:   TinyTP PDU routines

*/
#ifndef _TTPPDU_
#define _TTPPDU_

#include "ttp.h"

//
// Connect PDU
//
#define PiMaxSduSize 0x01       // Only defined Pi value as of TinyTP 1.0

// Generate a connect PDU, does *not* change or free input databuffer
TTPBuf *
ttp_pdu_connect(int P, int InitialCredit, int MaxSduSize, TTPBuf *UserData);

// Parse a connect PDU, copies parameters into *plist (sizeof >= 60 bytes)
// input data buffer has parameters removed
void
ttp_pdu_connect_parse(TTPBuf *data, int *p, int *n, unsigned char *plist);

// parse a connect PDU's parameter list to extract the MaxSduSize
// returns true if the parameter was found and *maxsize is set
Boolean
ttp_pdu_connect_get_max_sdu_size(unsigned char *buf, UInt32 *maxsize);

//
// Data PDU
//

// returns a new TTPBuf with the flag byte at the start, any data appended
//   this needs to be designed better!
TTPBuf *
ttp_pdu_data(Boolean m, int credit, TTPBuf *data);

void
ttp_pdu_data_setbyte(Boolean m, int credit, TTPBuf *data);      // put in ttp byte at start of data


// extracts More flag, credit, and incr's pointer in userdata to skip TTP overhead byte
void ttp_pdu_data_parse(TTPBuf *userData, Boolean *m, int *credit);

void BufHideRest(TTPBuf *data);         // misc buffer manipulation

//********* TEMP UNTIL BETTER QUEUE ROUTINES FOUND FOR OS-X ********************
// note - currently do *not* need atomic enqueue/dequeue
IrDAErr Enqueue(QElemPtr qElement, QHdrPtr qHeader);
IrDAErr Dequeue(QElemPtr qElement, QHdrPtr qHeader);
//******************************************************************************


#endif // _TTPPDU_