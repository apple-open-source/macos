/*
    File:       ttppdu.cpp

    Contains:   TinyTP PDU & buffer subroutines

*/

#include "ttp.h"
#include "ttppdu.h"
#include "CBufferSegment.h"
#include "IrDALog.h"

#if(hasTracing > 0 && hasTTPPduTracing > 0)

enum IrTinyTPpduTraceCodes
{
    kPduConnect = 1,
    kPduConnectParse,
    kPduGetMax,
    kPduData,
    kPduDataParse,
    kBufferHideRest
};

static
EventTraceCauseDesc gTraceEvents[] = {
    {kPduConnect,       "TinyTPpdu: connect"},
    {kPduConnectParse,  "TinyTPpdu: connect parse"},
    {kPduGetMax,        "TinyTPpdu: Get Sdu Max"},
    {kPduData,          "TinyTPpdu: data"},
    {kPduDataParse,     "TinyTPpdu: data parse, ttp byte=, bufsize="},
    {kBufferHideRest,   "TinyTPpdu: buffer hide rest"}
};

#define XTRACE(x, y, z)  IrDALogAdd( x, y, z, gTraceEvents, true )  

#else
    #define XTRACE(x, y, z) ((void)0)
#endif


// allocates a new TTPBuf and fills it in with the parameter list (if any)
// followed by the userdata (if any)
// at a minimum, the result has the first byte with P and initial credit set
// ToDo: look into doing this in-place w/out an allocate/free issue
TTPBuf *
ttp_pdu_connect(int p, int initialCredit, int maxSduSize, TTPBuf *data)
{
    TTPBuf *m;          // return message
    int    datalen;     // length of user data
    UInt32  byte;
    
    XTRACE(kPduConnect, initialCredit, maxSduSize);
    
    if (maxSduSize == 0) {      // don't send zero value maxsdu
	if (p) { DebugLog("IrDA-TTPPDU: Request to send zero value maxsdu"); }
	p = 0;
    }
    if (data)   datalen = BufSize(data);    // number of bytes of userdata (if any)
    else        datalen = 0;
    
    m = BufAlloc(1000);             // get object please
    require(m, NoMem);                      // no memory
    
    byte = ((p & 0x01) << 7) | (initialCredit & 0x7f);
    BufPut(m, byte);

    if (p > 0) {                // any parameters (zero or one really) ...
	BufPut(m, 4);                   // plen, total length of all parameters

					// first (and only) parameter
	BufPut(m, PiMaxSduSize);                // PI (parameter id=1)
	BufPut(m, 2);                           // PL (length of following value)
	BufPut(m, (maxSduSize >> 8) & 0xff);    // high bits first
	BufPut(m, maxSduSize & 0xff);           // low bits (big-endian for a change)
    }

    if (datalen) {                      // copy the userdata portion
	BufSeekStart(data);                 // make sure we're copying from the start
	while (datalen--) {                 // could do this much faster, but it's short
	    BufPut(m, BufGet(data));
	}
    }
    BufHideRest(m);         // setup for the write by hiding the rest of the buffer
    XTRACE(kPduConnect, 0, BufSize(m));     // check on it ...
    // We do *not* free the input data buffer here
NoMem:
    return m;
}

// pull out parameter list from buffer returned from LMP connect indication
// copies parameters to *plist, which better be at least 60 bytes long
// "compacts" userdata in-place
void
ttp_pdu_connect_parse(
    TTPBuf *data,               // input & output.  parameters stripped out
    int *p,                     // output: p flag
    int *n,                     // output: initial credit
    unsigned char *plist)       // output parameter list (if any)
{
    unsigned char flag;
    
    XTRACE(kPduConnectParse, 0, 0);
    
    //BufSeekStart(data);       // should already be done for me
    flag = BufGet(data);        // start parsing data buffer
    BufHideStart(data, 1);      // remove it from client's view
    *p = (flag >> 7) & 0x01;    // extract P flag
    *n =  flag       & 0x7f;    // extract initial credit
    
    if (*p) {                   // if flag set, parameters follow
	int plen, i;
	plen = BufGet(data);    // first byte of params in param length
	BufHideStart(data, 1);          // remove it from client's view
	if (plen > 60) return;  // sanity
	*plist++ = plen;        // copy length to plist first
	for (i = 0 ; i < plen; i++) {
	    *plist++ = BufGet(data);    // copy to plist, assume big enough!
	    BufHideStart(data, 1);          // remove it from client's view
	}
    }
    else *plist = 0;            // just in case, tell caller it's zero len plist
    
    XTRACE(kPduConnectParse, *p, *n);
    // parameters should now be consumed out of the buffer
}

// searches the parameter list for one of type PiMaxSduSize
// if not found, returns false
// if found, sets *max with the value
Boolean
ttp_pdu_connect_get_max_sdu_size(unsigned char *plist, UInt32 *max)
{
    int plen;
    UInt32  temp;
    UInt32  pi;     // parameter type
    UInt32  pl;     // parameter length
    UInt32  i;

    XTRACE(kPduGetMax, 0, *plist);
    
    plen = *plist++;        // get length of entire param list
    while (plen > 0) {      // loop over the thing
	pi = *plist++;              // get parameter type
	pl = *plist++;              // get parameter length
	plen -= (2 + pl);           // subtract from total length
	if (pi == PiMaxSduSize) {   // found what we're looking for??
	    if (pl > sizeof(UInt32))            // sanity check
		return false;                   // what to do here??
	    temp = 0;
	    for (i = 0; i < pl ; i++) {     // pull in the parameter
		temp = temp << 8;
		temp |= *plist++;
	    }
	    XTRACE(kPduGetMax, 1, temp);
	    *max = temp;
	    return true;
	}
    }       // loop over all parameters
    return false;
}

//
//  *** Data PDU
//

// Make a Data PDU for sending on to LMP
// data can be nil if sending a dataless pdu for flow control
//   ***** This needs to be REWRITTEN w/out a buffer copy!
TTPBuf *
ttp_pdu_data(Boolean m, int credit, TTPBuf *data)
{   int len;
    TTPBuf *outbuf;             // output TTPBuf
    unsigned char byte;

    XTRACE(kPduData, m, credit);
    
    if (data) len = BufSize(data);      // current length of userdata
    else      len = 0;
    outbuf = BufAlloc(1 + len);     // make a new one with just the right size
    require(outbuf, NoMem);

    byte = credit & 0x7f;           // sanity check credit
    if (m) byte |= 0x80;            // turn on more bit if needed
    BufPut(outbuf, byte);           // set TTP overhead byte (overridden later)
    
    if (len) {                      // if have data to copy
	int count;
	UInt8 *base;
	base = BufBase(data);       // base of src buffer
	check(base);
	
	BufSeekStart(data);             // make sure mark is at front
	count = outbuf->Putn(base, len);    // copy packet to new buffer
	check(count == len);
    }
    BufHideRest(outbuf);            // this was missing!  
NoMem:
    return outbuf;
}

// set the ttp overhead byte's value in the first byte of the data buffer
void
ttp_pdu_data_setbyte(Boolean m, int credit, TTPBuf *data)
{
    unsigned char byte;
    unsigned char *wptr;
    
    check(data);
    byte = credit & 0x7f;           // sanity check credit
    if (m) byte |= 0x80;            // turn on more bit if needed
    wptr = BufBase(data);           // get the start of the buffer
    check(wptr);
    *wptr =byte;                    // zap the byte
}



// strips out ttp overhead byte
void
ttp_pdu_data_parse(TTPBuf *userData, Boolean *m, int *credit)
{
    unsigned char byte;
    check(userData);
    
    //XTRACE(kPduDataParse, 0, 0);
				    // assumes rewound buffer
    byte = BufGet(userData);        // grab the first byte please
    BufHideStart(userData, 1);      // hide the first byte from the consumer
    if (byte >> 7)  *m = true;      // more bit is on
    else            *m = false;
    *credit = (byte & 0x7f);        // delta credit
    XTRACE(kPduDataParse, byte, BufSize(userData));
}


// this takes a (CBufferSegment) with mark at the end
// of the valid data, hides from there to physical end
// and rewinds the mark to the start of the buffer
// this probably belongs elsewhere....
// CBufferSegment assumed.
void
BufHideRest(TTPBuf *data)
{   int size, length;

    length = data->Position();
    size = BufSize(data);       // get total amount of room in the buffer
    check(size);
    
    XTRACE (kBufferHideRest, 1, length);
    XTRACE (kBufferHideRest, 2, size);
    
    BufHideEnd(data, size-length);  // hide all but real data
    BufSeekStart(data);             // move mark to start

    XTRACE( kBufferHideRest, 3, BufSize(data));
}














