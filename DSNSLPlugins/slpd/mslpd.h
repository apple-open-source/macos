
/*
 * mslpd.h : Minimal SLP v2 Service Agent Definitions
 *
 * Version: 1.11
 * Date:    10/05/99
 *
 * Licensee will, at its expense,  defend and indemnify Sun Microsystems,
 * Inc.  ("Sun")  and  its  licensors  from  and  against any third party
 * claims, including costs and reasonable attorneys' fees,  and be wholly
 * responsible for  any liabilities  arising  out  of  or  related to the
 * Licensee's use of the Software or Modifications.   The Software is not
 * designed  or intended for use in  on-line  control  of  aircraft,  air
 * traffic,  aircraft navigation,  or aircraft communications;  or in the
 * design, construction, operation or maintenance of any nuclear facility
 * and Sun disclaims any express or implied warranty of fitness  for such
 * uses.  THE SOFTWARE IS PROVIDED TO LICENSEE "AS IS" AND ALL EXPRESS OR
 * IMPLIED CONDITION AND WARRANTIES, INCLUDING  ANY  IMPLIED  WARRANTY OF
 * MERCHANTABILITY,   FITNESS  FOR  WARRANTIES,   INCLUDING  ANY  IMPLIED
 * WARRANTY  OF  MERCHANTABILITY,  FITNESS FOR PARTICULAR PURPOSE OR NON-
 * INFRINGEMENT, ARE DISCLAIMED. IN NO EVENT WILL SUN BE LIABLE HEREUNDER
 * FOR ANY DIRECT DAMAGES OR ANY INDIRECT, PUNITIVE, SPECIAL, INCIDENTAL
 * OR CONSEQUENTIAL DAMAGES OF ANY KIND.
 *
 * (c) Sun Microsystems, 1999, All Rights Reserved.
 * Author: Erik Guttman
 */


typedef struct sastate {
    SOCKET  sdUDP;            /* Socket for listening for udp requests.			*/
    SOCKET  sdTCP;            /* Socket for accepting connection requests.		*/
    SOCKET  sdReg;            /* Socket for sending registrations using TCP.	*/
    SOCKET  sdMax;            /* Max socket descriptor, used for selecting.		*/
    fd_set  fds;              /* Used for selecting.							*/
    struct  sockaddr_in sin;  /* Address of the server.							*/
    char    *pcSAHost;        /* the host name of the SA for PRList checking	*/
    char    *pcSANumAddr;     /* the numerical SA addr for PRList checking		*/
    struct  in_addr *pina;    /* A NULL terminated array of all interfaces.		*/
    SAStore store;            /* Data for the SA is kept here.					*/
    DATable *pdat;            /* Keeps track of all known DAs.					*/
    int     iTraceMode;       /* Determines the trace mode for the SA.			*/
    struct timeval tvTimeout; /* The value for timeouts for reads and writes.	*/
  
#ifdef EXTRA_MSGS
    char    *pcSAURL;         /* URL of the SA									*/
    void    *pvMutex;
#endif /* EXTRA_MSGS */  

#ifdef MAC_OS_X
    char    *pcDAURL;         	/* URL of the DA								*/
    long	statelessBootTime;
    char	*pcSPIList;			/* this is our supported SPI List 				*/
#endif

} SAState;

typedef struct MslpdResources {

  void   *pvMutex;
  SOCKET  sdTCP;
  SOCKET  sdUDP;
  
} MslpdResources;

/*
 * All internal mslpd functions are prototyped here.
 */

#ifdef __cplusplus
//extern "C" {
#endif

bool IsProcessTerminated( void );


SLPBoolean AreWeADirectoryAgent( void );

extern EXPORT void InitializeSLPdSystemConfigurator( void );

extern EXPORT int StartSLPDAAdvertiser( SAState* psa );
extern EXPORT void StopSLPDAAdvertiser( void );

extern EXPORT void InitializeSLPDARegisterer( SLPHandle serverState );

extern EXPORT void RegisterAllServicesWithDA( SLPHandle serverState, struct sockaddr_in sinDA, const char *pcScopes );
extern EXPORT void RegisterAllServicesWithKnownDAs( SLPHandle serverState );

extern EXPORT void propogate_all_advertisements(SAState *psa);
extern EXPORT void propogate_registration( SAState *psa, const char* lang, const char* srvtype, const char* url, const char* scopeList, const char* attrlist, int life );
extern EXPORT void propogate_deregistration( SAState *psa, const char* lang, const char* srvtype, const char* url, const char* scopeList, const char* attrlist, int life );

void delete_regfile(const char *pcFile);
SLPInternalError   process_regfile(SAStore *, const char *);

long GetCurrentTime( void );
long GetStatelessBootTime( void );

void TurnOnDA( void );
void TurnOffDA( void );

void InitSLPRegistrar( void );
void TearDownSLPRegistrar( void );
void ResetStatelessBootTime( void );
extern SLPReturnError HandleRegistration( const char* pcInBuf, int iInSz, struct sockaddr_in* sinIn );
extern SLPReturnError HandleDeregistration( const char* pcInBuf, int iInSz, struct sockaddr_in* sinIn );
extern SLPReturnError DAHandleRequest( SAState *psa, struct sockaddr_in* sinIn, SLPBoolean viaTCP, Slphdr *pslphdr, const char *pcInBuf, int iInSz, char **ppcOutBuf, int *piOutSz, int *piGot );

#ifdef MAC_OS_X
extern char* MakeDAAdvertisementMessage(	Slphdr*	pslph,		// if this is null then we assume an unsolicited DAAdvert
                                            char*	url,
                                            const char*	scopeList,
                                            const char*	attributeList,
                                            long	timeStamp,
                                            int*	outSize );
#endif // MAC_OS_X                                
                                
/* ------------------------------------------------------------ SLPRegistrar.cpp */
extern SLPInternalError   mslpd_init_network(SAState *);
extern char *     serialize_values(SAStore *pstore, int item, int attr);

/* ------------------------------------------------------------- mslpd_net.c */

//extern int        handle_udp(SAState *psa);
//extern int        handle_tcp(SAState *psa);
extern SLPInternalError propogate_registrations(	SAState *pstate, 
                                            struct sockaddr_in sinDA, 
                                            const char *pcScopes);

extern SLPInternalError propogate_registration_with_DA(SAState *pstate, struct sockaddr_in sinDA, const char *lang, const char *url, const char *srvtype, const char *scope, const char *attrlist, int life ); 
extern SLPInternalError propogate_deregistration_with_DA(SAState *pstate, struct sockaddr_in sinDA, const char *lang, const char *url, const char *srvtype, const char *scope, const char *attrlist, int life ); 

extern SLPInternalError srvdereg_out(const char *pcLang, const char *pcURL, 
                            const char *pcSrvType, const char *pcScope,
                            const char *pcAttrList, int iLifetime,
                            char **ppcOutBuf, int *piOutSz);

extern void mslpd_daadvert_callback(	SLPHandle hSLP, 
                                        int iErrCode,
                                        struct sockaddr_in sin, 
                                        const char *pcScopeList, 
                                        const char *pcAttrs,
                                        long lBootTime, 
                                        void *pvUser);

/* ----------------------------------------------------------- mslpd_query.c */
extern int store_request(	SAState *psa,
                            SLPBoolean viaTCP, 
                            Slphdr *pslphdr,
                            const char *pcInBuf, 
                            int iInSz,
                            char **ppcOutBuf, 
                            int *piOutSz,
                            int *piNumResults);

/* this function is used by mslpd_opt.c to handle tag lists int AttrRqst */
extern int isWildMatch(const char *pcQuery, const char *pcString);
extern int on_PRList(SAState *psa, char *pcPRList);
extern int match_langtag(const char *pc1, const char *pc2);
extern int match_srvtype(const char *pcstRqst, const char *pcstStore);

/* ------------------------------------------------------------- mslpd_opt.c */
#ifdef EXTRA_MSGS

extern int opt_tag_list_includes(const char *pcTagList, 
				 const char *pcTag);
extern int opt_attr_request(SAState *psa, Slphdr *pslphdr, const char *pcInBuf,
  int iInSz, char **ppcOutBuf, int *piOutSz, int *piGot);
extern int opt_type_request(SAState *psa, Slphdr *pslphdr, const char *pcInBuf,
  int iInSz, char **ppcOutBuf, int *piOutSz, int *piGot);


#ifdef __cplusplus
//}
#endif


#endif /* EXTRA_MSGS */

