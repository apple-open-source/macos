/*
	File:		CLDAPNode.h

	Contains:	LDAP node management class

	Copyright:	© 2001 by Apple Computer, Inc., all rights reserved.

	NOT_FOR_OPEN_SOURCE <to be reevaluated at a later time>

*/


#ifndef __CLDAPNode_h__
#define __CLDAPNode_h__	1

#include <stdio.h>
#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include <DirectoryServiceCore/PrivateTypes.h>
#include <DirectoryServiceCore/DSCThread.h>
#include <DirectoryServiceCore/DSMutexSemaphore.h>


#include <LDAP/lber.h>
#include <LDAP/ldap.h>

#include <stdio.h>
#include <map>				//STL map class
#include <string>			//STL string class
#include <time.h>			//time_t usage

using namespace std;

// Context data structure
//KW need to get away from using UserDefName, Name, and Port in the context but get it
// from the config table
typedef struct sLDAPContextData {
	LDAP		   *fHost;				//LDAP session handle
	DSMutexSemaphore
				   *fLDAPSessionMutex;	//mutex for changing the session handle if authCallActive
	uInt32			fConfigTableIndex;	//gConfigTable Hash index
	char		   *fName;				//LDAP domain name ie. ldap.apple.com
	int				fPort;				//LDAP port number - default is 389 - SSL default port is 636
	int				fType;				//KW type of reference entry - not used yet
    bool			authCallActive;		//indicates if authentication was made through the API
    									//call and if set means don't use config file auth name/password
    uInt32			offset;				//offset into the data buffer
    uInt32			index;
    char		   *fOpenRecordType;	//record type used to open a record
    char		   *fOpenRecordName;	//record name used to open a record
    char		   *fOpenRecordDN;		//record name used to open a record
	char		   *fUserName;			//LDAP user name
	void		   *fAuthCredential;	//LDAP user authentication credential default is char* password
	char		   *fAuthType;			//LDAP authentication type ie. kDSStdAuthClearText means password used
    
    tDirReference	fPWSRef;
    tDirNodeReference	fPWSNodeRef;
} sLDAPContextData;

typedef struct sLDAPNodeStruct {
	LDAP		   *fHost;					//LDAP session handle
	DSMutexSemaphore
				   *fLDAPSessionMutex;		//mutex for changing the session handle
	uInt32			fRefCount;				//ref count on host handle
	uInt32			fLDAPConfigTableIndex;	//gConfigTable index
	char		   *fServerName;			//server name used if directed open ie. fLDAPConfigTableIndex = 0
	int				fDirectLDAPPort;		//port if directed open ie. fLDAPConfigTableIndex = 0
	char		   *fUserName;				//LDAP user name
	void		   *fAuthCredential;		//LDAP user authentication credential default is char* password
	char		   *fAuthType;				//LDAP authentication type ie. kDSStdAuthClearText means password used
	bool			bHasFailed;				//a previous bind has failed - checked to delay successive attempts
	time_t			fDelayedBindTime;		//time after which to retry to bind
} sLDAPNodeStruct;

typedef map<string, sLDAPNodeStruct*>	LDAPNodeMap;
typedef LDAPNodeMap::iterator			LDAPNodeMapI;

//KW current concept is to NOT save the AuthNodeMaps for reuse but allow rebinding if no Continue

class CLDAPNode
{
public:
                	CLDAPNode		(	void );
	virtual		   ~CLDAPNode		(	void );
	sInt32			SafeOpen		(	char	   *inNodeName,
										LDAP	  **outLDAPHost,
										uInt32	   *outLDAPConfigTableIndex );
									//if already open then just get host and config index if host okay
									//else try rebind first
									//if not open then bind to get host and search for config index
									//called from OpenDirNode
	sInt32			AuthOpen		(	char	   *inNodeName,
										LDAP	   *inHost,
										char	   *inUserName,
										void	   *inAuthCredential,
										char	   *inAuthType,
										LDAP	  **outLDAPHost,
										uInt32	   *inOutLDAPConfigTableIndex,
										bool	   shouldCloseOld );
									//there must be a fLDAPNodeMap entry for this ie. SafeOpen was already called since
									//this would come from a know Node Ref
									//this is not ref counted NOR reused
									//inOutLDAPConfigTableIndex could be passed in for a directed open ie. not zero
									//called from the hierarchy below DoAuthentication
	sInt32			RebindSession	(	char	   *inNodeName,
										LDAP	   *inHost,
										LDAP	  **outLDAPHost );
									//must already be open
									//check if already rebound during a continue for some other node ref
									//use rebind if continue not set
									//called from ??
	sInt32			SimpleAuth		(	char	   *inNodeName,
										char	   *inUserName,
										void	   *inAuthCredential );
									//use rebind to do the auth
									//called from DoClearTextAuth
	sInt32			RebindAuthSession(	char	   *inNodeName,
										LDAP	   *inHost,
										char	   *inUserName,
										void	   *inAuthCredential,
										char	   *inAuthType,
										uInt32	   inLDAPConfigTableIndex,
										LDAP	  **outLDAPHost );
									//use rebind if continue not set
									//called from ??
	sInt32			SafeClose		(	char	   *inNodeName,
										LDAP	   *inHost);
									//decrement ref count and delete if ref count zero
									//called from CloseDirNode

	void			GetSchema		( sLDAPContextData *inContext );
	LDAP* 			LockSession		( sLDAPContextData *inContext );
	void			UnLockSession	( sLDAPContextData *inContext );

protected:
	
	sInt32			CleanLDAPNodeStruct	(	sLDAPNodeStruct	   *inLDAPNodeStruct );
	sInt32			BindProc			(	sLDAPNodeStruct	   *inLDAPNodeStruct );
	sInt32			ParseLDAPNodeName	(	char	   *inNodeName,
											char	  **outLDAPName,
											int		   *outLDAPPort );
											//parse a string with a name and possibly a suffix of ':' followed by a port number
	sInt32			GetSchemaMessage	(	LDAP *inHost,
											int inSearchTO,
											LDAPMessage **outResultMsg );
	bool			IsTokenNotATag		(	char *inToken );

private:

	LDAPNodeMap			fLDAPNodeMap;
	DSMutexSemaphore	fLDAPNodeOpenMutex; //used for the ldap_bind as well as the LDAPNodeMap container

};

#endif	// __CLDAPNode_h__
