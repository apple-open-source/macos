/*
	File:		slpipc.h

	Contains:	C calls that bridge into our C++ code

	Written by:	Kevin Arnold

	Copyright:	© 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):


*/
#ifndef _slpipc_
#define _slpipc_
#pragma once

    SLPInternalError	reset_slpd( int argc, char *pcArgv[], struct sockaddr_in *psin, SAState* psa );

    int			handle_udp(SAState *psa, char* pcInBuf, int inBufSz, struct sockaddr_in sinIn);
    int			handle_tcp(SAState *psa, SOCKET sdRqst, struct sockaddr_in sinIn);

    int			InitializeInternalProcessListener( SAState* psa );
    int			InitializeTCPListener( SAState* psa );
    int			InitializeUDPListener( SAState* psa );

    int			RunSLPInternalProcessListener( SAState* psa );
    int			StartSLPUDPListener( SAState* psa );
    void		CancelSLPUDPListener( void );
    int			StartSLPTCPListener( SAState* psa );
    void		CancelSLPTCPListener( void );

#endif


