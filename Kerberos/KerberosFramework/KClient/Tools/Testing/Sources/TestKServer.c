#include "kclient.h"
#include <stdio.h>

void InitToolbox(void);

void InitToolbox()
{
	InitGraf((Ptr) &qd.thePort);
	InitFonts();
	InitWindows();
	InitMenus();
	FlushEvents(everyEvent,0);
	TEInit();
	InitDialogs(0L);
	InitCursor();
}

main()
{
	OSErr err;
	char buf[1250],*decryptBuf;
	unsigned long bufLen,decryptLength,decryptOffset;
	KClientKey sessionKey,privateKey;
	short status;
	char user[64];
	KClientSessionInfo session,ksession;
	short len,ofs,majorVersion,minorVersion;
	char text[64];
	long local_addr;
	unsigned char *addrPtr = (unsigned char *) &local_addr;
	long seconds;
	
	// Set local address
	addrPtr[0] = 128;
	addrPtr[1] = 84;
	addrPtr[2] = 143;
	addrPtr[3] = 25;
	
	InitToolbox();

	err = KServerNewSession( &session, "ns10-demo",local_addr,20,local_addr,10);
	printf("KServerNewSession err: %d\n",err);

	if (!err) {
		err =  KClientSetPrompt( &session,"Enter the server's id and password");
		printf("KClientSetPrompt err: %d\n",err);
	}

	if (!err) {
		err =  KServerAddKey( &session,&privateKey, NULL, 0, "srvtab" );
		printf("KServerAddKey err: %d\n",err);
	}
	if (!err) {
		err =  KServerGetKey( &session, &privateKey, "ns10-demo", 0, "srvtab" );
		printf("Key: 0x%lX %lX\n",*((long*)&privateKey),*((long*)&(privateKey.keyBytes[4])) );
	}
	
	if (!err) {
		err = KClientNewSession(&ksession, local_addr,10,local_addr,20);
		printf("KClientNewSession err: %d\n",err);
	}
	if (!err) {
		err = KClientGetTicketForService(&ksession, "ns10-demo",buf,&bufLen);
		printf("KClientGetTicketForService err: %d\n",err);
	}
	if (!err) {
		err = KClientGetSessionUserName(&ksession, user,KClientCommonName);
		printf("KClientGetSessionUserName err: %d, user: %s\n",err,user);
	}
	if (!err) {
		err = KServerVerifyTicket( &session, buf, "srvtab" );
		printf("KServerVerifyTicket err: %d\n",err);
	}
	if (!err) {
		err = KServerGetSessionTimeRemaining( &session, &seconds );
		printf("KServerGetSessionTimeRemaining err: %d, seconds: %ld\n",err,seconds);
	}
	if (!err) {
		err = KClientGetSessionUserName(&session, user, KClientLocalName);
		printf("KServerGetUserName err: %d, user: %s\n",err,user);
	}
	if (!err) {
		err = KServerGetReplyTicket( &session, buf, &bufLen );
		printf("KServerGetReplyTicket err: %d\n",err);
	}
	if (!err) {
		err = KClientVerifySendAuth(&ksession, buf,&bufLen );
		printf("KClientVerifySendAuth err: %d\n",err);
	}


	if (!err) {
		err = KClientDisposeSession( &session );
		printf("KClientDisposeSession err: %d\n",err);
	}	
	if (!err) {
		err = KClientDisposeSession( &ksession );
		printf("KClientDisposeSession err: %d\n",err);
	}	
	
	
	printf("\n\nDone err: %d\n",err);
	printf("Done err: %d\n",err);
	printf("Done err: %d\n",err);
	printf("Done err: %d\n",err);
	printf("Done err: %d\n",err);
	fflush(stdout);
	printf("Done err: %d\n",err);
	printf("Done err: %d\n",err);
	printf("Done err: %d\n",err);
	fflush(stdout);

	return 0;
}
