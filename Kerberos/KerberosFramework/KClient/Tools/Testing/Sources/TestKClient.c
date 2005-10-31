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
	char user[40];
	KClientSessionInfo session;
	short len,ofs,majorVersion,minorVersion;
	char text[64];

	InitToolbox();

	err = KClientInitSession(&session,100,10,200,20);	
	printf("KClientInitSession, rc: %d\n",err);

	err = KClientVersion( &majorVersion, &minorVersion, text );
	printf("KClientVersion, rc: %d, version: %d.%d...%s\n",err,majorVersion,minorVersion,text);

	status = KClientStatus( );
	printf("KClientStatus: %s\n",(status==KClientLoggedIn) ? "logged in" : "logged out");

	/* Test setting user & password */
	
	err =  KClientSetUserName("ns10-demo");
	printf("KClientSetUserName, rc: %d\n",err);

	err = KClientPasswordToKey( "$tester", &privateKey);
	printf("KClientPasswordToKey, rc: %d key:  %.8s\n",err,&privateKey);

	bufLen = 1250;	
	err = KClientGetTicketForService(&session,"kfront.cusockets",buf,&bufLen);
	if (err) KClientErrorText(err,text);
	printf("KClientGetTicketForService, rc: %d (%s)\n",err,err ? text : "");

	status = KClientStatus( );
	printf("KClientStatus: %s\n",(status==KClientLoggedIn) ? "logged in" : "logged out");

	err = KClientGetUserName(user);
	printf("KClientGetUserName user is: %s, rc: %d\n",user,err);

	err = KClientLogout( );
	printf("KClientLogout, rc: %d\n",err);
		
	status = KClientStatus( );
	printf("KClientStatus: %s\n",(status==KClientLoggedIn) ? "logged in" : "logged out");

	/* Test getting special initial tickets */

	err =  KClientCacheInitialTicket(&session,"changepw.kerberos");
	if (err) KClientErrorText(err,text);
	printf("KClientCacheInitialTicket, rc: %d (%s)\n",err,err ? text : "");

	err = KClientLogout( );
	printf("KClientLogout, rc: %d\n",err);

	bufLen = 1250;	
	err = KClientGetTicketForService(&session,"kfront.cusockets",buf,&bufLen);
	printf("KClientGetTicketForService, rc: %d\n",err);		
	
	/* Test data encryption */
	
	bufLen = 1250;	
	err =  KClientEncrypt(&session,"hi there freddy boy",19,buf,&bufLen);
	printf("KClientEncrypt, rc: %d encrypted length: %d\n",err,bufLen);
		
	err = KClientInitSession(&session, 200,20,100,10);	/* pretend I'm the other guy */
	printf("KClientMakeSessionInfo, rc: %d\n",err);

	err =  KClientDecrypt(&session, buf,bufLen,&decryptOffset,&decryptLength);
	printf("KClientDecrypt, rc: %d\n",err);
	if (!err) {
		decryptBuf = buf + decryptOffset;
		decryptBuf[decryptLength] = '\0';
		printf("----> message is %s, msgLen: %ld msgofs: %ld\n",decryptBuf,decryptLength,decryptOffset);
	}

	err = KClientLogout( );
	printf("KClientLogout, rc: %d\n",err);

	err = KClientLogin( &session, &privateKey );
	printf("KClientLogin, rc: %d key:  %.8s\n",err,&privateKey);

	err = KClientLogout( );
	printf("KClientLogout, rc: %d\n",err);

	err = KClientPasswordLogin( &session, "$tester", &privateKey );
	printf("KClientPasswordLogin, rc: %d key: %.8s\n",err,&privateKey);

	err = KClientLogout( );
	printf("KClientLogout, rc: %d\n",err);

	err = KClientKeyLogin( &session, &privateKey );
	printf("KClientKeyLogin, rc: %d\n",err);

	bufLen = 1250;	
	err =  KClientMakeSendAuth(&session,"kfront.cusockets",buf,&bufLen,0, "version1");
	printf("KClientMakeSendAuth, rc: %d\n",err);
				
/*	err =  KClientVerifySendAuth(KClientSessionInfo *session, void *buf,unsigned long *bufLen );
	printf("KClientMakeSendAuth, rc: %d\n",err); */

	return 0;
}
