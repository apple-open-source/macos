/* FirewallTool.m */
/* Written by Elizabeth C. Douglas */
/* Mary Chan 4/2004 */
/* Copyright: Apple Computer, Inc. */

#include <CoreFoundation/CoreFoundation.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_fw.h>
#include <netinet6/ip6_fw.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>

#define RULE_0    10
#define RULE_1	2000
#define RULE_2	2010
#define RULE_3 	2020
#define RULE_4	2030
#define RULE_5	2040
#define RULE_6	2050
#define RULE_7	2060
#define RULE_8	12180
#define RULE_9 	12190

#define ICMPECHOREQRULE		20000
#define RULE_D	65535

#define NUM_DEFAULT 9

/* UDP Rules */
#define UDPRULEDNS		20310
#define UDPRULEBOOTP		20320
#define UDPRULEBOOTSERVER	20321
#define UDPRULEMDNSRESPONSE	20322
#define UDPRULENTP		20330
#define UDPRULESMB		20340
#define UDPRULESRVLOC		20350
#define UDPRULEIPP		20360
#define UDPRULEMDNS		20370
#define UDPRPCRULESTART		20400
#define UDPUSERDEFINEDSTARTS	22000
#define UDPRULECHECKSTATE	30500
#define UDPRULEKEEPSTATE	30510
#define UDPRULEDEFAULTFRAG	30520
#define	UDPRULEDEFAULTDENY	35000


/* UDP port number */
#define DNSPORT			53
#define BOOTSERVERPORT		67
#define BOOTPCLIENTPPORT	68
#define RPCPORT			111
#define NTPPORT			123
#define SMBPORT			137
#define SRVLOCPORT		427
#define IPPPORT			631
#define MDNSPORT		5353
#define ARDPORT			3283
#define RSTPCLIENTPORT		6970 #6970-6980

#define kFirewallKey 	CFSTR("firewall")
#define kStateKey 	CFSTR("state")
#define kEnableKey 	CFSTR("enable")
#define kPortKey 	CFSTR("port")
#define kAllPortsKey 	CFSTR("allports")
#define kFirewallAppSignature CFSTR("com.apple.sharing.firewall")

#define kUDPEnableKey	CFSTR("udpenabled")
#define kLogEnableKey	CFSTR("loggingenabled")
#define kStealthEnableKey	CFSTR("stealthenabled")
#define kUDPPortKey	CFSTR("udpport")

#define RULE_START 2070

#define SUCCESS 	0
#define ERROR 		1
#define SIGNAL_FTP     -1

#define		kAppleRule		0
#define		kOneRule		-1
#define		kThirdPartyRule		-2
#define		kError			ERROR
#define		LOGDEFAULTVALUE		0			// default value for logging when the key is missing
#define		STEALTHDEFAULTVALUE	0			// default value for stealth mode when the key is missing

#define	MAXDEFAULTUDPRULES 6				// needs to be updated whenever DEFAULTUDPSET changes

#define LOGGERPROGPATH  "/usr/libexec/ipfwloggerd"
#define LOGGERPIDPATH   "/var/run/ipfwlogger.pid"

typedef struct DEFAULTUDPRULETYPE {
    int	theport;
    int therule;
} DEFAULTUDPRULETYPE;

typedef struct DEFAULTUDPRULESET{
    DEFAULTUDPRULETYPE	set[MAXDEFAULTUDPRULES];
}DEFAULTUDPRULESET;

struct DEFAULTUDPRULESET	DEFAULTUDPSET={
	{
            { DNSPORT		, UDPRULEDNS},
            { BOOTPCLIENTPPORT	, UDPRULEBOOTP},
            { SMBPORT		, UDPRULESMB},
            { SRVLOCPORT	, UDPRULESRVLOC},
            { IPPPORT		, UDPRULEIPP},
            { MDNSPORT		, UDPRULEMDNS}
	}
};

int ReadFile(CFPropertyListRef *dictionaryRef, CFPropertyListRef *stateRef, Boolean *UDPenabled, Boolean *LOGGINGenabled, Boolean *STEALTHenabled);

int DoV4Firewall(CFPropertyListRef dictionaryRef, CFPropertyListRef stateRef, Boolean udpenabled, Boolean loggingenabled, Boolean stealthenabled);
int DoV6Firewall(CFPropertyListRef dictionaryRef, CFPropertyListRef stateRef, Boolean udpenabled, Boolean loggingenabled, Boolean stealthenabled);

int FlushOldRules(int controlSocket);
int FlushOldRulesV6(int controlSocket);

int WriteRulesToSockets(int controlSocket, CFPropertyListRef dictionaryRef, Boolean isV6, Boolean udpenabled, Boolean loggingenabled, Boolean stealthenabled);

int AddDefaultRules(int controlSocket, Boolean doUDP, Boolean loggingenabled, Boolean stealthenabled);
int AddDefaultRulesV6(int controlSocket, Boolean doUDP, Boolean loggingenabled, Boolean stealthenabled);
int AddDefaultRule1LoopBack(int controlSocket);
int AddDefaultRule1LoopBackV6(int controlSocket);
int AddDefaultRule2DiscardLoopbackIn(int controlSocket);
int AddDefaultRule2DiscardLoopbackInV6(int controlSocket);
int AddDefaultRule3DiscardLoopbackOut(int controlSocket);
int AddDefaultRule3DiscardLoopbackOutV6(int controlSocket);
int AddDefaultRule4DiscardBroadcastMulticast(int controlSocket);
int AddDefaultRule4DiscardBroadcastMulticastV6(int controlSocket);
int AddDefaultRule5DiscardTCPBroadcastMulticast(int controlSocket);
int AddDefaultRule5DiscardTCPBroadcastMulticastV6(int controlSocket);
int AddDefaultRule6OutboundOK(int controlSocket);
int AddDefaultRule6OutboundOKV6(int controlSocket);
int AddDefaultRule7KeepEstablished(int controlSocket);
int AddDefaultRule7KeepEstablishedV6(int controlSocket);
int AddDefaultRule9DenyTCP(int controlSocket, Boolean loggingenabled);
int AddDefaultRule9DenyTCPV6(int controlSocket, Boolean loggingenabled);


int AddDefaultUDPRULE(int controlSocket, int udpport, int rulenumber );
int AddDefaultUDPBootServer(int controlSocket );
int AddDefaultUDPMDNS( int  controlSocket);
int AddDefaultUDPCheckState(int controlSocket);
int AddDefaultUDPKeepState(int controlSocket);
int AddDefaultUDPFRAG(int controlSocket );
int AddDefaultUDPDeny(int controlSocket, Boolean loggingenabled );
int AddDefaultUDPRPCRULE(int controlSocket, Boolean isV6 );

int AddDefaultUDPRULEV6(int controlSocket, int udpport, int rulenumber );
int AddDefaultUDPV6BootServer(int controlSocket );
int AddDefaultUDPCheckStateV6(int controlSocket);
int AddDefaultUDPKeepStateV6(int controlSocket);
int AddDefaultUDPFRAGV6(int controlSocket );
int AddDefaultUDPDenyV6(int controlSocket, Boolean loggingenabled );

int AddICMPRule( int controlSocket, Boolean loggingenabled );
int AddICMPRuleV6( int controlSocket, Boolean loggingenabled );

boolean_t	checklist( long unsigned *udpportlist, long unsigned theport, int max );

int AddUserDefinedRule(int controlSocket, int ruleNumber, SInt32 start, SInt32 end, boolean_t useRange, boolean_t doUDP);
int AddUserDefinedRuleV6(int controlSocket, int ruleNumber, SInt32 start, SInt32 end, boolean_t useRange, boolean_t doUDP);

int WriteOutInternetSharingRule(int controlSocket, struct ip_fw *rule);
boolean_t isInternetSharingRunning(int controlSocket, struct ip_fw *rule);
boolean_t IsExpectedCFType(CFPropertyListRef variableInQuestion, CFTypeID expectedType);
int GetPortRange(CFStringRef portString, SInt32 *start, SInt32 *end, boolean_t *useRange);

void EnableLogging(boolean_t enable);
void EnableBlackHole(boolean_t enable);
void KillLogDaemon();


int DoCheckApplerules();
int SetFirewallRules();
int checkV4Firewall( CFBooleanRef state);
int checkV6Firewall( CFBooleanRef state);
int compareRulesToFile( struct ip_fw *rules, int numRules, int numExpected);
int compareV6RulesToFile( struct ip6_fw *rules, int numRules, int numExpected);
int UnblockPort( int controlSocket, CFDictionaryRef portDict, int *ruleNumber, Boolean isV6, Boolean doUDP );

//********************************************************************************
// main
//********************************************************************************
int main(int argc, char *argv[])
{

    int error = SUCCESS;
    int c;
    boolean_t	checkapplerules = false;
    
    
    /* do we want to check for Apple rules */
    while ((c = getopt(argc, argv, "a")) != EOF) { 
    switch (c) {
        case 'a':
                checkapplerules = true;
            break;
        default:
            error = 99;
            break;
        }
    }
    
    if ( checkapplerules )
    {
        error = DoCheckApplerules();
        exit( error );
    }
    else if ( error == 99 )
    {
        exit( 1 );			// firewall tool was called with an unknown options, exit with status = 1
    }
    
    error = SetFirewallRules();
    
    return error;

}


//********************************************************************************
// DoCheckApplerules
//********************************************************************************
int DoCheckApplerules()
{

    int returncode;

    CFBooleanRef state = (CFBooleanRef)CFPreferencesCopyValue( 
                                                        (CFStringRef)kStateKey, 
                                                        (CFStringRef)kFirewallAppSignature,
                                                        (CFStringRef)kCFPreferencesAnyUser, 
                                                        (CFStringRef)kCFPreferencesCurrentHost);

    
    returncode = checkV4Firewall( state );
    if ( returncode == kAppleRule )
    {
	returncode = checkV6Firewall( state );
    }
    return returncode;
}

//********************************************************************************
// checkV4Firewall
//********************************************************************************
int checkV4Firewall( CFBooleanRef state)
{
    int returncode = kError; 
        
    struct ip_fw *rules = NULL;
    int dataSize = 0;
    int actualDataSize = 0;
    int numRules = 0;
    int controlSocket, i;
    int iterations = 1;
    // boolean_t internetSharingRunning = false;
    int internetSharingRunning = false;
    int numRulesExpected = 1;
    int num = 0;

    // open the control socket
    controlSocket = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if( controlSocket < 0 )
    {
        printf("Firewall Tool: Error opening socket!\n");
        return kError;
    }
            
    // do the silly socket memory thing to figure out how much to allocate
    do
    {       
        dataSize = (sizeof(struct ip_fw) * 10) * iterations; 
                    
        rules = (struct ip_fw *)realloc(rules, dataSize);

        if (rules == NULL )
        {
            printf("Firewall Tool: Error getting rules! Realloc Failed!\n");
            return kError;
        }
        
        rules->version = IP_FW_CURRENT_API_VERSION; // send down the version
                
        actualDataSize = dataSize;
        
        i = getsockopt(controlSocket, IPPROTO_IP, IP_FW_GET, rules, &actualDataSize);
        if( i < 0 )
        {
            printf("Firewall Tool: Error getting rules! Not written into structure...\n");
            free(rules);
            return kError;
        }
        
        iterations++;
                    
    }while( dataSize == actualDataSize );
                
    numRules = actualDataSize/sizeof(struct ip_fw);
    
    /* check for dynamic rules */
    while (rules[num].fw_number < RULE_D)
            num++;
    num++;
    
    if (num * sizeof (rules[0]) != actualDataSize) {
        /* we have dynamic rules, ignore them when comparing rules to apple defined rules  */
        numRules = num;
    }

    
    internetSharingRunning = isInternetSharingRunning( controlSocket, rules);
    
    if( internetSharingRunning )
        numRulesExpected = 2;
                                                                        
    if( state == NULL ) // one rule, corrupted file
    {
        returncode = (numRules == 1 || numRules == numRulesExpected) ? kOneRule : kThirdPartyRule; 
        if( returncode == kThirdPartyRule )
            printf("Firewall Tool: ThirdPartyRule NumRules != 1. State is not defined!\n");
    }
    else if( CFGetTypeID(state) != CFBooleanGetTypeID() )
    {
        returncode = (numRules == 1 || numRules == numRulesExpected) ? kOneRule : kError;
        if( returncode == kError )
            printf("Firewall Tool: Corrupted File! State is not of type Boolean!\n");
    }
    else if( state == kCFBooleanFalse ) // state is off but there are rules
    {
        returncode = (numRules == 1 || numRules == numRulesExpected) ? kAppleRule : kThirdPartyRule;
        if( returncode == kThirdPartyRule )
            printf("Firewall Tool: Apple Firewall is off, but there are %d rules in IP Firewall!\n", numRules);
    }
    else if( state == kCFBooleanTrue && (numRules == 1 || numRules == numRulesExpected) ) 
    {
        returncode = kOneRule; // state is true and there is only one rule
        printf("Firewall Tool: Apple Firewall is on, but the rules aren't there!\n");
    }
    else // state is true and there is more then one rule
    {
        // the firewall is on so we have to compare what's in ipfw to pref file
        returncode = compareRulesToFile( rules, numRules, numRulesExpected);
    }
    
    if( rules )
        free(rules);
    
    close(controlSocket);
   
    
    return returncode;
}


//********************************************************************************
// checkV6Firewall
//********************************************************************************
int checkV6Firewall( CFBooleanRef state)
{
    int returncode = kError; 
        
    struct ip6_fw *rules = NULL;
    int dataSize = 0;
    int actualDataSize = 0;
    int numRules = 0;
    int controlSocket, i;
    int iterations = 1;
    int numRulesExpected = 1;

    // open the control socket
    controlSocket = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW);
    if( controlSocket < 0 )
    {
        printf("Firewall: Error opening V6 socket!\n");
        return kError;
    }
            
    // do the silly socket memory thing to figure out how much to allocate
    do
    {       
        dataSize = (sizeof(struct ip6_fw) * 10) * iterations; 
                    
        rules = (struct ip6_fw *)realloc(rules, dataSize);

        if (rules == NULL )
        {
            printf("Firewall: Error getting V6 rules! Realloc Failed!\n");
            return kError;
        }
        
        rules->version = IPV6_FW_CURRENT_API_VERSION; // send down the version
                
        actualDataSize = dataSize;
        
        i = getsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_GET, rules, &actualDataSize);
        
        if( i < 0 )
        {
            printf("Firewall: Error getting V6 rules! Not written into structure...\n");
            free(rules);
            return kError;
        }
        
        iterations++;
                    
    }while( dataSize == actualDataSize );
        
    //printf("Actual Data Size: %d, struct size: %d", actualDataSize, sizeof(struct ip_fw));
    
    numRules = actualDataSize/sizeof(struct ip6_fw);
    
    if( state == NULL ) // one rule, corrupted file
    {
        returncode = (numRules == 1 || numRules == numRulesExpected) ? kOneRule : kThirdPartyRule; 
        if( returncode == kThirdPartyRule )
            printf("ThirdParty: NumRules V6 != 1. State is not defined!");
    }
    else if( CFGetTypeID(state) != CFBooleanGetTypeID() )
    {
        returncode = (numRules == 1 || numRules == numRulesExpected) ? kOneRule : kError;
        if( returncode == kError )
            printf("Corrupted File! State V6 is not of type Boolean!");
    }
    else if( state == kCFBooleanFalse ) // state is off but there are rules
    {
        returncode = (numRules == 1 || numRules == numRulesExpected) ? kAppleRule : kThirdPartyRule;
        if( returncode == kThirdPartyRule )
            printf("Apple Firewall is off, but there are %d rules in IP V6 Firewall!", numRules);
    }
    else if( state == kCFBooleanTrue && (numRules == 1 || numRules == numRulesExpected) ) 
    {
        returncode = kOneRule; // state is true and there is only one rule
        printf("Apple Firewall is on, but the V6 rules aren't there!");
    }
    else // state is true and there is more then one rule
    {
        // the firewall is on so we have to compare what's in ipfw to pref file
        returncode = compareV6RulesToFile( rules, numRules, numRulesExpected);
    }
    
    if( rules )
        free(rules);
    
    close(controlSocket);
   
    
    return returncode;
}


// compareRulesToFile
int compareRulesToFile( struct ip_fw *rules, int numRules, int numExpected)
{
    int returncode = kAppleRule;
    int i;
#ifdef oldcode
    int defaultRulesFound = 0;
#endif     
    int defaultExpected = NUM_DEFAULT;
    boolean_t nonapple = false;
    
    if( numExpected == 2 ) // we found a nat rule
        defaultExpected+=1;
    
		
    CFPropertyListRef ports =  CFPreferencesCopyValue( 
                                                        (CFStringRef)kAllPortsKey, 
                                                        (CFStringRef)kFirewallAppSignature,
                                                        kCFPreferencesAnyUser, 
                                                        kCFPreferencesCurrentHost);
	
    if( ports == NULL )
    {
        returncode = (numRules == 1 || numRules == numExpected ) ? kOneRule : kError; 
        if( returncode == kError )
            printf("Firewall Tool: Corrupted File! No Port List defined!\n");
    }
    else if( CFGetTypeID(ports) != CFArrayGetTypeID() ) // the pref file is corrupted....
    {
        returncode = (numRules == 1 || numRules == numExpected ) ? kOneRule : kError; 
        if( returncode == kError )
            printf("Firewall Tool: Corrupted File! Port List is not of type Array!\n");
    }
                                                                            
    for( i = 0; i < numRules; i++ )			
    {
        int ruleNumber = rules[i].fw_number;
        int signature = (int)rules[i].context;

        if ( (signature != 'AAPL') && (ruleNumber != RULE_D))
        {
			/* ignore divert ruler */
			if ((rules[i].fw_flg & IP_FW_F_COMMAND) == IP_FW_F_DIVERT)
			{
				continue;			
			}
            nonapple = true;
            break;
        }
#ifdef oldcode                
        // we are expecting these rules!
        // RULE_8 is commented out because of radar 3066015 - reset rule causing havoc
        if( ruleNumber == RULE_1 || ruleNumber == RULE_2 || ruleNumber == RULE_3 ||
                ruleNumber == RULE_4 || ruleNumber == RULE_5 || ruleNumber == RULE_6 ||
                ruleNumber == RULE_7 /*|| ruleNumber == RULE_8 */ || ruleNumber == RULE_9 ||
                ruleNumber == RULE_D || ruleNumber == RULE_0)
        {
            defaultRulesFound++;
        }else
        {
            // we only define in between RULE_1 and RULE_9 so any other number is invalid!
            if( (ruleNumber < RULE_1 || ruleNumber > RULE_9) && ruleNumber != RULE_D && ruleNumber != RULE_0 )
            {
                returnString = kThirdPartyString;
                printf("Firewall: Non recognized rule: %d", ruleNumber);
                break;
            }
            else // see if this port should be listed!
            {
                NSString *portString = NULL;
                int startVal = rules[i].fw_uar.fw_pts[0];
                int endVal = rules[i].fw_uar.fw_pts[1];
                                                
                if( !endVal )
                {
                    portString = [NSString stringWithFormat: @"%d", startVal];
                }
                else
                {
                    portString = [NSString stringWithFormat: @"%d-%d", startVal, endVal];
                }
                                            
                if( [(NSArray *)ports indexOfObject: portString] == NSNotFound )
                {
                    returnString = kThirdPartyRule;
                    printf("Firewall: Non recognized rule: %d Port: %@", ruleNumber, portString);
                    break;
                }
            }
        }// end of big else
#endif
    }// end of for loop

#ifdef oldcode
    // 
    if( defaultRulesFound != defaultExpected ) 
    {
        printf("Apple firewall is supposed to be on but we only found %d defaultRules!", defaultRulesFound);
        returnString = kThirdPartyString;
    }
#endif
    if ( nonapple )
    {
        //printf("Apple firewall is supposed to be on but we only found %d defaultRules!\n", defaultRulesFound);
        returncode = kThirdPartyRule;
    }
                    
    if( ports != NULL )
        CFRelease(ports);
	
    return returncode;
}

// compareV6RulesToFile
int compareV6RulesToFile( struct ip6_fw *rules, int numRules, int numExpected)
{
    int returncode = kAppleRule;
    int i;
#ifdef oldcode
    int defaultRulesFound = 0;
#endif
    int defaultExpected = NUM_DEFAULT;
    boolean_t nonapple = false;
 
    
    if( numExpected == 2 ) // we found a nat rule
        defaultExpected+=1;
    
		
    CFPropertyListRef ports =  CFPreferencesCopyValue( 
                                                        (CFStringRef)kAllPortsKey, 
                                                        (CFStringRef)kFirewallAppSignature,
                                                        kCFPreferencesAnyUser, 
                                                        kCFPreferencesCurrentHost);
    
    if( ports == NULL )
    {
        returncode = (numRules == 1 || numRules == numExpected ) ? kOneRule : kError; 
        if( returncode == kError )
            printf("Firewall Tool: Corrupted File! No V6 Port List defined!");
    }
    else if( CFGetTypeID(ports) != CFArrayGetTypeID() ) // the pref file is corrupted....
    {
        returncode = (numRules == 1 || numRules == numExpected ) ? kOneRule : kError; 
        if( returncode == kError )
            printf("Firewall Tool: Corrupted File! V6 Port List is not of type Array!");
    }
                                                                            
    for( i = 0; i < numRules; i++ )
    {
        int ruleNumber = rules[i].fw_number;
        int signature = (int)rules[i].context;

        if ( (signature != 'AAPL') && (ruleNumber != RULE_D))
        {
			/* ignore divert ruler */
			if ((rules[i].fw_flg & IP_FW_F_COMMAND) == IP_FW_F_DIVERT)
			{
				continue;			
			}
            nonapple = true;
            break;
        }

#ifdef oldcode                
        // we are expecting these rules!
        // RULE_8 is commented out because of radar 3066015 - reset rule causing havoc
        if( ruleNumber == RULE_1 || ruleNumber == RULE_2 || ruleNumber == RULE_3 ||
                ruleNumber == RULE_4 || ruleNumber == RULE_5 || ruleNumber == RULE_6 ||
                ruleNumber == RULE_7 /*|| ruleNumber == RULE_8 */ || ruleNumber == RULE_9 ||
                ruleNumber == RULE_D || ruleNumber == RULE_0)
        {
            defaultRulesFound++;
        }else
        {
            // we only define in between RULE_1 and RULE_9 so any other number is invalid!
            if( (ruleNumber < RULE_1 || ruleNumber > RULE_9) && ruleNumber != RULE_D && ruleNumber != RULE_0 )
            {
                returncode = kThirdPartyRule;
                printf("Firewall: Non recognized rule: %d", ruleNumber);
                break;
            }
            else // see if this port should be listed!
            {
                NSString *portString = NULL;
                int startVal = rules[i].fw_pts[0];
                int endVal = rules[i].fw_pts[1];
                                                
                if( !endVal )
                {
                    portString = [NSString stringWithFormat: @"%d", startVal];
                }
                else
                {
                    portString = [NSString stringWithFormat: @"%d-%d", startVal, endVal];
                }
                                            
                if( [(NSArray *)ports indexOfObject: portString] == NSNotFound )
                {
                    returncode = kThirdPartyRule;
                    printf("Firewall: Non recognized rule: %d Port: %@", ruleNumber, portString);
                    break;
                }
            }
        }// end of big else
#endif
    }// end of for loop
    
        
#ifdef oldcode
    if( defaultRulesFound != defaultExpected ) 
    {
        printf("Apple firewall is supposed to be on but we only found %d defaultRules!", defaultRulesFound);
        returncode = kThirdPartyRule;
    }
#endif			
    if ( nonapple )
    {
        //printf("Apple firewall is supposed to be on but we only found %d defaultRules!\n", defaultRulesFound);
        returncode = kThirdPartyRule;
    }

    if( ports != NULL )
        CFRelease(ports);
	
    return returncode;
}


//********************************************************************************
// KillLogDaemon
//********************************************************************************
void KillLogDaemon()
{
	pid_t   pid;
    FILE *fp;
	
	/* kill logging daemon? */
	/* read PID to file */
	fp = fopen(LOGGERPIDPATH, "r");
	if (fp != NULL) {
		fscanf(fp, "%d", &pid);
		fclose(fp);
		if ( kill( pid, SIGKILL ))
			printf("cannot kill logger daemon pid %d errno = %d\n", (int)pid, errno);
		unlink( LOGGERPIDPATH );
	}
}

//********************************************************************************
// SetFirewallRules
//********************************************************************************
int SetFirewallRules()
{

    CFPropertyListRef dictionaryRef = NULL;
    CFPropertyListRef stateRef = NULL;
    int error = SUCCESS;
    Boolean udpenabled;
	Boolean loggingenabled;
	Boolean stealthenabled;
    char	*argv[2];
	struct stat sb;

    /* setting Apple Rules */
    do
    {
        // read in the firewall configuration file
        error = ReadFile(&dictionaryRef, &stateRef, &udpenabled, &loggingenabled, &stealthenabled );
        
        if( error != SUCCESS )
        {
            printf("Firewall Tool: Error, Error reading in file!\n");
            break;
        }
        
		if ( stateRef == kCFBooleanTrue )
		{
			/* Firewall is on */
			
			/* check stealth mode */
			EnableBlackHole( stealthenabled );
			
			EnableLogging( loggingenabled );
			if( loggingenabled )			/* logging enabled, do logging */
			{
				pid_t   pid;
				/* start up logging deamon if it's not already running */
				/* check for running deamon */
				
				if ( stat(LOGGERPIDPATH, &sb) )
				{
					if (errno == ENOENT)
					{
						pid = fork();

						switch (pid) {
							case -1 : {     /* if error */
									printf("fork() failed, cannot start up logger daemon %d\n", errno);
									break;
							}

							case 0: {      /* if child */
										argv[0]=LOGGERPROGPATH;
										argv[1]=NULL;
										(void)execv( LOGGERPROGPATH, argv);
										printf("child::execv failed, cannot start up logger daemon errno = %d\n", errno);
										exit(1);
									}
							default:
								waitpid( pid, NULL, 0);
								break;
						}
					}
				}
			}else
				KillLogDaemon();
			
		}else
		{
			/* Firewall is off, turn off logging */
			EnableLogging( false );
			/* turn off stealth mode */
			EnableBlackHole( false );
 			stealthenabled = false;         /* set stealthenabled to false if firewall is not enabled, reset ICMP rule */
			KillLogDaemon();
		}
        
       error =  DoV4Firewall(dictionaryRef, stateRef, udpenabled, loggingenabled, stealthenabled);
        
        if( error != SUCCESS )
        {
            printf("Firewall Tool: Error %d, Error setting up V4 Firewall!\n", error);
        }

        error = DoV6Firewall(dictionaryRef, stateRef, udpenabled, loggingenabled, stealthenabled);
        
        if( error != SUCCESS )
        {
            printf("Firewall Tool: Error %d, Error setting up V6 Firewall!\n", error);
        } 
        
    }while(false);
    
    if( dictionaryRef )
        CFRelease(dictionaryRef);
    
    if( stateRef )
        CFRelease(stateRef);
        
    return error;
}


//********************************************************************************
// DoV4Firewall
//********************************************************************************
int DoV4Firewall(CFPropertyListRef dictionaryRef, CFPropertyListRef stateRef, Boolean udpenabled, Boolean loggingenabled, Boolean stealthenabled)
{
    int controlSocket;
    int error = SUCCESS;
    boolean_t internetSharingOn = false;
    struct ip_fw rule;
        
    do
    {
        // open the socket
        controlSocket = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    
        if( controlSocket < 0 )
        {
            printf("Firewall Tool: Error opening socket\n");
            error = ERROR;
            break;
        }
        
        memset(&rule, 0, sizeof(rule));
        internetSharingOn = isInternetSharingRunning( controlSocket, &rule);
        
        // flush the old rules
        error = FlushOldRules(controlSocket);
        
        if( error != SUCCESS )
        {
            printf("Firewall Tool: Error Flusing Old Rules!\n");
            break;
        }
    
        if( internetSharingOn )
            error = WriteOutInternetSharingRule(controlSocket, &rule); // do something with error or keep going?
    
        // write to the sockets based on the rules
       if( stateRef == kCFBooleanTrue )
            error = WriteRulesToSockets(controlSocket, dictionaryRef, false, udpenabled, loggingenabled, stealthenabled);
        
        if( error != SUCCESS )
        {
            printf("Firewall Tool: Error writing out rules! Attempting to Flush...\n");
            FlushOldRules(controlSocket); // if that doesn't work try to flush all...
            break;
        }
   
     }while(false);
        
    if( controlSocket >= 0 )
        close(controlSocket);

    return error;
}

//********************************************************************************
// DoV6Firewall
//********************************************************************************
int DoV6Firewall(CFPropertyListRef dictionaryRef, CFPropertyListRef stateRef, Boolean udpenabled, Boolean loggingenabled, Boolean stealthenabled)
{
    int controlSocket;
    int error = SUCCESS;
        
    do
    {
        // open the socket
        controlSocket = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW);
    
        if( controlSocket < 0 )
        {
            printf("Firewall Tool: Error opening socket for V6!\n");
            error = ERROR;
            break;
        }
                
        // flush the old rules
        error = FlushOldRulesV6(controlSocket);
        
        if( error != SUCCESS )
        {
            printf("Firewall Tool: Error Flusing Old V6 Rules!\n");
            break;
        }
    
        // write to the sockets based on the rules
		if( stateRef == kCFBooleanTrue )
            error = WriteRulesToSockets(controlSocket, dictionaryRef, true, udpenabled, loggingenabled, stealthenabled);
        
        if( error != SUCCESS )
        {
            printf("Firewall Tool: Error writing out V6 rules! Attempting to Flush...\n");
            FlushOldRulesV6(controlSocket); // if that doesn't work try to flush all...
            break;
        }
   
     }while(false);
        
    if( controlSocket >= 0 )
        close(controlSocket);

    return error;
}

//********************************************************************************
// FlushOldRules
//********************************************************************************
int FlushOldRules(int controlSocket)
{
    int i;
    struct ip_fw rule;
    
    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
        
    // flush the old rules (all of them!)
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_FLUSH, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error flushing rules\n");
        return ERROR;
    }
    
    return SUCCESS;
}

//********************************************************************************
// FlushOldRulesV6
//********************************************************************************
int FlushOldRulesV6(int controlSocket)
{
    int i;
    struct ip6_fw rule;
    
    rule.version = IPV6_FW_CURRENT_API_VERSION; // send down the version
        
    // flush the old rules (all of them!)
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_FLUSH, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error flushing rules (V6) \n");
        return ERROR;
    }
    
    return SUCCESS;
}

//********************************************************************************
// ReadFile
//********************************************************************************
int ReadFile(CFPropertyListRef *dictionaryRef, CFPropertyListRef *stateRef, Boolean *UDPenabled, Boolean *LOGGINGenabled, Boolean *STEALTHenabled )
{
    CFPropertyListRef udpenabledRef, logenabledRef, stealthenabledRef;
    int		      value;
    
    *dictionaryRef = NULL;
    *stateRef = NULL;
    logenabledRef = NULL;
    udpenabledRef = NULL;
    stealthenabledRef = NULL;
    *UDPenabled = false;		// set UDP firewall default value to disabled
	*LOGGINGenabled = LOGDEFAULTVALUE;			// default value if key is missing
	*STEALTHenabled = STEALTHDEFAULTVALUE;		// default value if key is missing
    
    *dictionaryRef = CFPreferencesCopyValue( kFirewallKey, 
                                    kFirewallAppSignature,
                                    kCFPreferencesAnyUser, 
                                    kCFPreferencesCurrentHost);
                                
    if( !IsExpectedCFType(*dictionaryRef, CFDictionaryGetTypeID()) )
    {
        printf("Firewall Tool: Error, kFirewallKey does not return a CFDictionary\n");
        return ERROR;
    }
                                        
    *stateRef = CFPreferencesCopyValue( kStateKey, 
                                    kFirewallAppSignature,
                                    kCFPreferencesAnyUser, 
                                    kCFPreferencesCurrentHost);
                                    
    if( !IsExpectedCFType(*stateRef, CFBooleanGetTypeID()) )
    {
        printf("Firewall Tool: Error, kStateKey does not return a CFBoolean\n");
        return ERROR;
    }
    
                                        
    logenabledRef = CFPreferencesCopyValue( kLogEnableKey, 
                                    kFirewallAppSignature,
                                    kCFPreferencesAnyUser, 
                                    kCFPreferencesCurrentHost);
    if ( logenabledRef != NULL ) 
    {
        if( !IsExpectedCFType( logenabledRef, CFNumberGetTypeID()) )
        {
            printf("Firewall Tool: Error, logenabledRef does not return the correct type.\n");
            return ERROR;
        }
        
        if( !CFNumberGetValue((CFNumberRef)logenabledRef, kCFNumberIntType, &value) )
        {
                printf("Firewall Tool: Error, could not convert CFNumber to LOGGINGenabled value\n");
        }
        else if ( value == 0 )
            *LOGGINGenabled = false;
		else if ( value == 1 )
			*LOGGINGenabled = true;
        CFRelease(logenabledRef);
    }
                                        
    stealthenabledRef = CFPreferencesCopyValue( kStealthEnableKey, 
                                    kFirewallAppSignature,
                                    kCFPreferencesAnyUser, 
                                    kCFPreferencesCurrentHost);
    if ( stealthenabledRef != NULL ) 
    {
        if( !IsExpectedCFType( stealthenabledRef, CFNumberGetTypeID()) )
        {
            printf("Firewall Tool: Error, stealthenabledRef does not return the correct type.\n");
            return ERROR;
        }
        
        if( !CFNumberGetValue((CFNumberRef)stealthenabledRef, kCFNumberIntType, &value) )
        {
                printf("Firewall Tool: Error, could not convert CFNumber to STEALTHenabled value\n");
        }
        else if ( value == 0 )
            *STEALTHenabled = false;
		else if ( value == 1 )
			*STEALTHenabled = true;
        CFRelease(stealthenabledRef);
    }
    
                                        
    udpenabledRef = CFPreferencesCopyValue( kUDPEnableKey, 
                                    kFirewallAppSignature,
                                    kCFPreferencesAnyUser, 
                                    kCFPreferencesCurrentHost);
    
    if ( udpenabledRef != NULL ) 
    {
        if( !IsExpectedCFType( udpenabledRef, CFNumberGetTypeID()) )
        {
            printf("Firewall Tool: Error, udpenabledRef does not return the correct type.\n");
            return ERROR;
        }
        
        if( !CFNumberGetValue((CFNumberRef)udpenabledRef, kCFNumberIntType, &value) )
        {
                printf("Firewall Tool: Error, could not convert CFNumber to UDPenabled value\n");
        }
        else if ( value == 1 )
            *UDPenabled = true;
        //printf("Firewall Tool: eabledUDP value = %d\n", value);
        //printf("Firewall Tool: udp is enabled\n");
        CFRelease(udpenabledRef);
    }
    
    return SUCCESS;
}

//********************************************************************************
// UnblockPort
//********************************************************************************
int UnblockPort( int controlSocket, CFDictionaryRef portDict, int *ruleNumber, Boolean isV6, Boolean doUDP )
{

	// if the user wants this port to be allowed
        CFPropertyListRef arrayOfPorts = NULL; 
        CFPropertyListRef portString = NULL;   
        SInt32 start, end;
        boolean_t useRange;
        int j, portCount = 0;
        
        if ( doUDP )
            arrayOfPorts = CFDictionaryGetValue(portDict, kUDPPortKey);
        else 
            arrayOfPorts = CFDictionaryGetValue(portDict, kPortKey);
            
        if ( arrayOfPorts == NULL )				// no port or udpport list, try next one
            return SUCCESS;
            
        if( !IsExpectedCFType(arrayOfPorts, CFArrayGetTypeID()) )
        {
            printf("Firewall Tool: Error, kPortKey did not return a CFArray\n");
            return ERROR;
        }
        
        if( arrayOfPorts )
            portCount = CFArrayGetCount((CFArrayRef)arrayOfPorts);
            
        for( j = 0; j < portCount; j++)
        {
            portString = CFArrayGetValueAtIndex((CFArrayRef)arrayOfPorts, j);
            
            if( !IsExpectedCFType(portString, CFStringGetTypeID()) )
            {
                printf("Firewall Tool: Error, port array did not return a CFString value\n");
                return ERROR;
            }
        
            // figure out if its one port or if its a port range
            if( GetPortRange((CFStringRef)portString, &start, &end, &useRange) == SUCCESS )
            {
                if( !isV6 ) // set up the v4 user defined rules
                {
                    // add the rule
                    if( AddUserDefinedRule(controlSocket, *ruleNumber, start, end, useRange, doUDP) != SUCCESS)
                    {
                        printf("Firewall Tool: Error, could not add user defined rule!\n");
                        return ERROR;
                    }
                }
                else // set up the v6 user defined rules
                {
                    // add the rule, for the v6 firewall
                    if( AddUserDefinedRuleV6(controlSocket, *ruleNumber, start, end, useRange, doUDP) != SUCCESS)
                    {
                        printf("Firewall Tool: Error, could not add user defined rule!\n");
                        return ERROR;
                    }
                }
                
                *ruleNumber+=10;
            }
            
            portString = NULL;
        }
    
    return SUCCESS;

}

//********************************************************************************
// WriteRulesToSockets
//********************************************************************************
int WriteRulesToSockets(int controlSocket, CFPropertyListRef dictionaryRef, Boolean isV6, Boolean udpenabled, Boolean loggingenabled, Boolean stealthenabled)
{
    int i;
    CFIndex count = 0;
    CFDictionaryRef *values = NULL;
    int error;
    int ruleNumber = RULE_START;
    int udpruleNumber = UDPUSERDEFINEDSTARTS;

    // we already flushed ipfw so its currently off!
                            
    // separate out the v6 and v4 calls to this function
    if( !isV6 )
        error = AddDefaultRules(controlSocket, udpenabled, loggingenabled, stealthenabled);
    else
        error = AddDefaultRulesV6(controlSocket, udpenabled, loggingenabled, stealthenabled);
    
    if( error != SUCCESS )
    {
        printf("Firewall Tool: Error adding default rules!\n");
        return ERROR;
    }
    
    if( dictionaryRef ) // don't call this if dictionary could be NULL
        count = CFDictionaryGetCount(dictionaryRef);
        
    if( count > 0 )
    {
        values = CFAllocatorAllocate(NULL, count * sizeof(CFDictionaryRef), 0);
        
        if( values == NULL )
        {
            printf("Firewall Tool: CFAllocatorAllocate failed!\n");
            return ERROR;
        }
        else
        {
            CFDictionaryGetKeysAndValues(dictionaryRef, NULL, (const void **)values);
        }
    }
    
    for( i = 0; i < count; i++)
    {
        CFDictionaryRef portDict = values[i];
        CFPropertyListRef enabled;
        int value;
                        
        enabled = CFDictionaryGetValue(portDict, kEnableKey);
        
        if( !IsExpectedCFType(enabled, CFNumberGetTypeID()) )
        {
            printf("Firewall Tool: Error, kEnableKey did not return a CFNumber\n");
            CFAllocatorDeallocate(NULL, values);
            return ERROR;
        }
            
        // conversion to a numeric useable value
        if( !CFNumberGetValue((CFNumberRef)enabled, kCFNumberIntType, &value) )
        {
            printf("Firewall Tool: Error, could not convert CFNumber to enabled value\n"); 
            CFAllocatorDeallocate(NULL, values);
            return ERROR;
        }
        
        if ( value )
        {
            if ( UnblockPort( controlSocket, portDict, &ruleNumber, isV6, false ))  // check TCP port
            {
                CFAllocatorDeallocate(NULL, values);
                return ERROR;
            }
            if ( udpenabled )
            {
                if ( UnblockPort( controlSocket, portDict, &udpruleNumber, isV6, true ))   // check UDP port
                {	
                    CFAllocatorDeallocate(NULL, values);
                    return ERROR;
                }
            }
            
        }
        
    }
    CFAllocatorDeallocate(NULL, values);
       
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRules
//********************************************************************************
int AddDefaultRules(int controlSocket, Boolean doUDP, Boolean loggingenabled, Boolean stealthenabled)
{
    int error = SUCCESS;
    int	i;
        
    do
    {
        error = AddDefaultRule1LoopBack(controlSocket);
        if( error != SUCCESS ) break;
        
        error = AddDefaultRule2DiscardLoopbackIn(controlSocket);
        if( error != SUCCESS ) break;

        error = AddDefaultRule3DiscardLoopbackOut(controlSocket);
        if( error != SUCCESS ) break;

        error = AddDefaultRule4DiscardBroadcastMulticast(controlSocket);
        if( error != SUCCESS ) break;

        error = AddDefaultRule5DiscardTCPBroadcastMulticast(controlSocket);
        if( error != SUCCESS ) break;
        
        error = AddDefaultRule6OutboundOK(controlSocket);
        if( error != SUCCESS ) break;
        
        error = AddDefaultRule7KeepEstablished(controlSocket);
        if( error != SUCCESS ) break;
        
        // RULE_8 is commented out because of radar 3066015 - reset rule causing havoc
        // error = AddDefaultRule8ResetTCPInSetup(controlSocket);
        // if( error != SUCCESS ) break;
        
        error = AddDefaultRule9DenyTCP(controlSocket, loggingenabled);
        if( error != SUCCESS ) break;
        
		if ( stealthenabled )
		{
			error = AddICMPRule( controlSocket, loggingenabled );
			if( error != SUCCESS ) break;
		}
		
        /* add default UDP rules */
        if ( doUDP )
        {
        
            for ( i= 0; i < MAXDEFAULTUDPRULES; i++ ){
                error = AddDefaultUDPRULE( controlSocket, DEFAULTUDPSET.set[i].theport, DEFAULTUDPSET.set[i].therule);
                if ( error != SUCCESS ) break;
            }
                       
            error = AddDefaultUDPBootServer( controlSocket);
            if ( error != SUCCESS ) break;
                                                                                    
            error = AddDefaultUDPMDNS( controlSocket);
            if ( error != SUCCESS ) break;

            error = AddDefaultUDPKeepState( controlSocket);
            if ( error != SUCCESS ) break;
    
            error = AddDefaultUDPFRAG( controlSocket );
            if ( error != SUCCESS ) break;
#ifdef doRPC            
            error = AddDefaultUDPRPCRULE( controlSocket, false);
            if ( error != SUCCESS ) break;
#endif            
            error = AddDefaultUDPDeny( controlSocket, loggingenabled );
            if ( error != SUCCESS ) break;
        }
        
    }while(false);
    
    return error;
}

//********************************************************************************
// AddDefaultRulesV6
//********************************************************************************
int AddDefaultRulesV6(int controlSocket, Boolean doUDP, Boolean loggingenabled, Boolean stealthenabled)
{
    int error = SUCCESS;
    int	i;
        
    do
    {
        error = AddDefaultRule1LoopBackV6(controlSocket);
        if( error != SUCCESS ) break;
        
        error = AddDefaultRule2DiscardLoopbackInV6(controlSocket);
        if( error != SUCCESS ) break;

        error = AddDefaultRule3DiscardLoopbackOutV6(controlSocket);
        if( error != SUCCESS ) break;

        error = AddDefaultRule4DiscardBroadcastMulticastV6(controlSocket);
        if( error != SUCCESS ) break;

        error = AddDefaultRule5DiscardTCPBroadcastMulticastV6(controlSocket);
        if( error != SUCCESS ) break; 
        
        error = AddDefaultRule6OutboundOKV6(controlSocket);
        if( error != SUCCESS ) break;
        
        error = AddDefaultRule7KeepEstablishedV6(controlSocket);
        if( error != SUCCESS ) break;
        
        // RULE_8 is commented out because of radar 3066015 - reset rule causing havoc
        // error = AddDefaultRule8ResetTCPInSetupV6(controlSocket);
        // if( error != SUCCESS ) break;
        
        error = AddDefaultRule9DenyTCPV6(controlSocket, loggingenabled);
        if( error != SUCCESS ) break;
        
		if ( stealthenabled )
		{
			error = AddICMPRuleV6( controlSocket, loggingenabled );
			if( error != SUCCESS ) break;
		}

        /* add default UDP rules */
        if ( doUDP )
        {
        
            for ( i= 0; i < MAXDEFAULTUDPRULES; i++ ){
                error = AddDefaultUDPRULEV6( controlSocket, DEFAULTUDPSET.set[i].theport, DEFAULTUDPSET.set[i].therule);
                if ( error != SUCCESS ) break;
            }
			
#ifdef later        
            error = AddDefaultUDPV6BootServer( controlSocket);
            if ( error != SUCCESS ) break;
                                                                                                                                                                    
            error = AddDefaultUDPCheckStateV6( controlSocket);
            if ( error != SUCCESS ) break;
        
            error = AddDefaultUDPKeepStateV6( controlSocket);
            if ( error != SUCCESS ) break;
#endif        


            error = AddDefaultUDPFRAGV6( controlSocket );
            if ( error != SUCCESS ) break;
                
#ifdef doRPC            
            error = AddDefaultUDPRPCRULE( controlSocket, true );
            if ( error != SUCCESS ) break;
#endif         
   
            error = AddDefaultUDPDenyV6( controlSocket, loggingenabled );
            if ( error != SUCCESS ) break;
        }
        
    }while(false);
    
    return error;
}

//********************************************************************************
// AddDefaultRule1LoopBack
//********************************************************************************
int AddDefaultRule1LoopBack(int controlSocket)
{
    struct ip_fw rule;
    int i;
    
    // ipfw add 2000 allow all from any to any via lo0 
    
    memset(&rule, 0, sizeof(rule));
    
    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_1; 			// rule #
    rule.fw_flg |= IP_FW_F_ACCEPT; 		// allow
    rule.fw_prot = IPPROTO_IP; 			// ip
    
    strcpy(rule.fw_in_if.fu_via_if.name, "lo"); // via lo0
    rule.fw_in_if.fu_via_if.unit = -1; 		 
    strcpy(rule.fw_out_if.fu_via_if.name, "lo");
    rule.fw_out_if.fu_via_if.unit = -1;
    rule.fw_flg |= (IP_FW_F_IIFACE | IP_FW_F_OIFACE); // check both interfaces (in & out)
    rule.fw_flg |= (IP_FW_F_IIFNAME | IP_FW_F_OIFNAME); // interface to check specified by name
    rule.fw_flg |= (IP_FW_F_OUT | IP_FW_F_IN); // packets both directions
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", RULE_1);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule1LoopBackV6
//********************************************************************************
int AddDefaultRule1LoopBackV6(int controlSocket)
{
    struct ip6_fw rule;
    int i;
    
    // ipfw add 2000 allow all from any to any via lo0 
    
    memset(&rule, 0, sizeof(rule));
    
    rule.version = IPV6_FW_CURRENT_API_VERSION; // send down the version

    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_1; 			// rule #
    rule.fw_flg |= IPV6_FW_F_ACCEPT; 		// allow
    rule.fw_prot = IPPROTO_IPV6; 			// ip
    
    strcpy(rule.fw_in_if.fu_via_if.name, "lo"); // via lo0
    rule.fw_in_if.fu_via_if.unit = -1; 		 
    strcpy(rule.fw_out_if.fu_via_if.name, "lo");
    rule.fw_out_if.fu_via_if.unit = -1;
    rule.fw_flg |= (IPV6_FW_F_IIFACE | IPV6_FW_F_OIFACE); // check both interfaces (in & out)
    rule.fw_flg |= (IPV6_FW_F_IIFNAME | IPV6_FW_F_OIFNAME); // interface to check specified by name
    rule.fw_flg |= (IPV6_FW_F_OUT | IPV6_FW_F_IN); // packets both directions
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding V6 rule: %d\n", RULE_1);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule2DiscardLoopbackIn
//********************************************************************************
int AddDefaultRule2DiscardLoopbackIn(int controlSocket)
{
    struct ip_fw rule;
    int i;
    
    // ipfw add 2010 deny all from 127.0.0.0/8 to any in
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_2; 			// rule #
    rule.fw_flg |= IP_FW_F_DENY; 		// deny
    //rule.fw_flg |= IP_FW_F_PRN;			// for logging
    rule.fw_prot = IPPROTO_IP; 			// ip
    
    rule.fw_src.s_addr = htonl(0x7F000000);	// 127.0.0.0
    rule.fw_smsk.s_addr = htonl(0xFF000000);  	// /8
    
    rule.fw_flg |= IP_FW_F_IN; // packets both directions
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", RULE_2);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule2DiscardLoopbackInV6
//********************************************************************************
int AddDefaultRule2DiscardLoopbackInV6(int controlSocket)
{
    struct ip6_fw rule;
    int i;
    
    // ip6fw add 2010 deny all from ::1 to any in
    
    memset(&rule, 0, sizeof(rule));
    
    rule.version = IPV6_FW_CURRENT_API_VERSION;
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_2; 			// rule #
    rule.fw_flg |= IPV6_FW_F_DENY; 		// deny
    //rule.fw_flg |= IPV6_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_IPV6; 		// ip
    
    // hmm, isn't this nice
    rule.fw_src.__u6_addr.__u6_addr8[0] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[1] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[2] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[3] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[4] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[5] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[6] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[7] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[8] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[9] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[10] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[11] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[12] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[13] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[14] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[15] = 0x01;

    rule.fw_smsk.__u6_addr.__u6_addr8[0] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[1] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[2] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[3] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[4] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[5] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[6] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[7] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[8] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[9] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[10] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[11] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[12] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[13] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[14] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[15] = 0xFF;
    
    rule.fw_flg |= IPV6_FW_F_IN; // in
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", RULE_2);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule3DiscardLoopbackOut
//********************************************************************************
int AddDefaultRule3DiscardLoopbackOut(int controlSocket)
{
    struct ip_fw rule;
    int i;
    
    // ipfw add 2020 deny all from any to 127.0.0.0/8 in
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_3; 			// rule #
    rule.fw_flg |= IP_FW_F_DENY; 		// deny
    //rule.fw_flg |= IP_FW_F_PRN;			// for logging
    rule.fw_prot = IPPROTO_IP; 			// ip
    
    rule.fw_dst.s_addr = htonl(0x7F000000);	// 127.0.0.0
    rule.fw_dmsk.s_addr = htonl(0xFF000000);  	// /8
    
    rule.fw_flg |= IP_FW_F_IN; // packets both directions
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", RULE_3);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule3DiscardLoopbackOutV6
//********************************************************************************
int AddDefaultRule3DiscardLoopbackOutV6(int controlSocket)
{
    struct ip6_fw rule;
    int i;
    
    // ip6fw add 2020 deny all from any to ::1 in
    
    memset(&rule, 0, sizeof(rule));
    
    rule.version = IPV6_FW_CURRENT_API_VERSION;
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_3; 			// rule #
    rule.fw_flg |= IPV6_FW_F_DENY; 		// deny
    //rule.fw_flg |= IPV6_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_IPV6; 		// ip
    
     // hmm, isn't this nice
    rule.fw_dst.__u6_addr.__u6_addr8[0] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[1] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[2] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[3] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[4] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[5] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[6] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[7] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[8] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[9] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[10] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[11] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[12] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[13] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[14] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[15] = 0x01;

    rule.fw_dmsk.__u6_addr.__u6_addr8[0] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[1] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[2] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[3] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[4] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[5] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[6] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[7] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[8] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[9] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[10] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[11] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[12] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[13] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[14] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[15] = 0xFF;

    rule.fw_flg |= IPV6_FW_F_IN; // in
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", RULE_3);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule4DiscardBroadcastMulticast
//********************************************************************************
int AddDefaultRule4DiscardBroadcastMulticast(int controlSocket)
{
    struct ip_fw rule;
    int i;
    
    // ipfw add 2030 deny all from 224.0.0.0/3 to any in
    
    memset(&rule, 0, sizeof(rule));
		
    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_4; 			// rule #
    rule.fw_flg |= IP_FW_F_DENY; 		// deny
    //rule.fw_flg |= IP_FW_F_PRN;			// for logging
    rule.fw_prot = IPPROTO_IP; 			// ip
    
    rule.fw_src.s_addr = htonl(0xE0000000);	// 224.0.0.0
    rule.fw_smsk.s_addr = htonl(0xE0000000);  	// /3
    
    rule.fw_flg |= IP_FW_F_IN; // packets both directions
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", RULE_4);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule4DiscardBroadcastMulticastV6
//********************************************************************************
int AddDefaultRule4DiscardBroadcastMulticastV6(int controlSocket)
{
    struct ip6_fw rule;
    int i;
    
    // ip6fw add 2030 deny all from FF::/8 to any in
    
    memset(&rule, 0, sizeof(rule));
    
    rule.version = IPV6_FW_CURRENT_API_VERSION;
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_4; 			// rule #
    rule.fw_flg |= IPV6_FW_F_DENY; 		// allow
    //rule.fw_flg |= IPV6_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_IPV6; 		// ip
    
    // hmm, isn't this nice
    rule.fw_src.__u6_addr.__u6_addr8[0] = 0xFF;
    rule.fw_src.__u6_addr.__u6_addr8[1] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[2] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[3] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[4] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[5] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[6] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[7] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[8] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[9] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[10] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[11] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[12] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[13] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[14] = 0x00;
    rule.fw_src.__u6_addr.__u6_addr8[15] = 0x00;

    rule.fw_smsk.__u6_addr.__u6_addr8[0] = 0xFF;
    rule.fw_smsk.__u6_addr.__u6_addr8[1] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[2] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[3] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[4] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[5] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[6] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[7] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[8] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[9] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[10] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[11] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[12] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[13] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[14] = 0x00;
    rule.fw_smsk.__u6_addr.__u6_addr8[15] = 0x00;
    
    rule.fw_flg |= IPV6_FW_F_IN; // packets both directions
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", RULE_4);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule5DiscardTCPBroadcastMulticast
//********************************************************************************
int AddDefaultRule5DiscardTCPBroadcastMulticast(int controlSocket)
{
    struct ip_fw rule;
    int i;
    
    // ipfw add 2040 deny tcp from any to 224.0.0.0/3 in
    
    memset(&rule, 0, sizeof(rule));
    
    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_5; 			// rule #
    rule.fw_flg |= IP_FW_F_DENY; 		// deny
    //rule.fw_flg |= IP_FW_F_PRN;			// for logging
    rule.fw_prot = IPPROTO_TCP; 		// ip
    
    rule.fw_dst.s_addr = htonl(0xE0000000);	// 224.0.0.0
    rule.fw_dmsk.s_addr = htonl(0xE0000000);  	// /3
    
    rule.fw_flg |= IP_FW_F_IN; // packets both directions
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", RULE_5);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule5DiscardTCPBroadcastMulticastV6
//********************************************************************************
int AddDefaultRule5DiscardTCPBroadcastMulticastV6(int controlSocket)
{
    struct ip6_fw rule;
    int i;
    
    // ip6fw add 2040 deny tcp from any to FF::/8 in
    
    memset(&rule, 0, sizeof(rule));
    
    rule.version = IPV6_FW_CURRENT_API_VERSION;
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_5; 			// rule #
    rule.fw_flg |= IPV6_FW_F_DENY; 		// deny
    //rule.fw_flg |= IPV6_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_TCP; 		// tcp
    
       // hmm, isn't this nice
    rule.fw_dst.__u6_addr.__u6_addr8[0] = 0xFF;
    rule.fw_dst.__u6_addr.__u6_addr8[1] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[2] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[3] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[4] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[5] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[6] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[7] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[8] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[9] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[10] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[11] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[12] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[13] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[14] = 0x00;
    rule.fw_dst.__u6_addr.__u6_addr8[15] = 0x00;

    rule.fw_dmsk.__u6_addr.__u6_addr8[0] = 0xFF;
    rule.fw_dmsk.__u6_addr.__u6_addr8[1] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[2] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[3] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[4] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[5] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[6] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[7] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[8] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[9] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[10] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[11] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[12] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[13] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[14] = 0x00;
    rule.fw_dmsk.__u6_addr.__u6_addr8[15] = 0x00;

    rule.fw_flg |= IPV6_FW_F_IN; // in
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", RULE_5);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule6OutboundOK
//********************************************************************************
int AddDefaultRule6OutboundOK(int controlSocket)
{
    struct ip_fw rule;
    int i;
    
    // ipfw add 2010 allow tcp from any to any out 
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_6; 			// rule #
    rule.fw_flg |= IP_FW_F_ACCEPT; 		// allow
    rule.fw_prot = IPPROTO_TCP; 		// tcp
       
    rule.fw_flg |= IP_FW_F_OUT;			// outbound   

    // fw_src, fw_dst, fw_smsk, fw_dmsk is 0 for any to any
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", RULE_6);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule6OutboundOKV6
//********************************************************************************
int AddDefaultRule6OutboundOKV6(int controlSocket)
{
    struct ip6_fw rule;
    int i;
    
    // ipfw add 2010 allow tcp from any to any out 
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IPV6_FW_CURRENT_API_VERSION; // send down the version
    
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_6; 			// rule #
    rule.fw_flg |= IPV6_FW_F_ACCEPT; 		// allow
    rule.fw_prot = IPPROTO_TCP; 		// tcp
       
    rule.fw_flg |= IPV6_FW_F_OUT;		// outbound   

    // fw_src, fw_dst, fw_smsk, fw_dmsk is 0 for any to any
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding V6 rule: %d\n", RULE_6);
        return i;
    }
    
    return SUCCESS;
}


//********************************************************************************
// AddDefaultRule7KeepEstablished
//********************************************************************************
int AddDefaultRule7KeepEstablished(int controlSocket)
{
    struct ip_fw rule;
    int i;
    
    // ipfw add 2020 allow tcp from any to any established 
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_7; 			// rule #
    rule.fw_flg |= IP_FW_F_ACCEPT; 		// allow
    rule.fw_prot = IPPROTO_TCP; 		// tcp
    
	#ifdef IP_FW_IF_TCPEST
		rule.fw_ipflg = IP_FW_IF_TCPEST; // new 
	#endif
	
    //rule.fw_tcpf |= IP_FW_TCPF_ESTAB; // old (erase later)
    rule.fw_flg |= (IP_FW_F_OUT | IP_FW_F_IN);	// any to any 

    // fw_src, fw_dst, fw_smsk, fw_dmsk is 0 for any to any
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", RULE_7);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule7KeepEstablishedV6
//********************************************************************************
int AddDefaultRule7KeepEstablishedV6(int controlSocket)
{
    struct ip6_fw rule;
    int i;
    
    // ipfw add 2020 allow tcp from any to any established 
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IPV6_FW_CURRENT_API_VERSION; // send down the version
    
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_7; 			// rule #
    rule.fw_flg |= IPV6_FW_F_ACCEPT; 		// allow
    rule.fw_prot = IPPROTO_TCP; 		// tcp
    
    rule.fw_ipflg = IPV6_FW_IF_TCPEST; 		// new 
	
    //rule.fw_tcpf |= IP_FW_TCPF_ESTAB; // old (erase later)
    rule.fw_flg |= (IPV6_FW_F_OUT | IPV6_FW_F_IN);	// any to any 

    // fw_src, fw_dst, fw_smsk, fw_dmsk is 0 for any to any
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding V6 rule: %d\n", RULE_7);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule9DenyTCP
//********************************************************************************
int AddDefaultRule9DenyTCP(int controlSocket, Boolean loggingenabled)
{
    struct ip_fw rule;
    int i;
    
    // ipfw add 12150 deny tcp from any to any
        
    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_9; 			// rule #
    rule.fw_flg |= IP_FW_F_DENY; 		// deny
	if ( loggingenabled )
		rule.fw_flg |= IP_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_TCP; 		// tcp
    
    rule.fw_flg |= (IP_FW_F_OUT | IP_FW_F_IN);	// any to any 

    // fw_src, fw_dst, fw_smsk, fw_dmsk is 0 for any to any
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", RULE_9);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultRule9DenyTCPV6
//********************************************************************************
int AddDefaultRule9DenyTCPV6(int controlSocket, Boolean loggingenabled)
{
    struct ip6_fw rule;
    int i;
    
    // ipfw add 12150 deny tcp from any to any
        
    memset(&rule, 0, sizeof(rule));

    rule.version = IPV6_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = RULE_9; 			// rule #
    rule.fw_flg |= IPV6_FW_F_DENY; 		// deny
	if ( loggingenabled )
		rule.fw_flg |= IPV6_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_TCP; 		// tcp
    
    rule.fw_flg |= (IPV6_FW_F_OUT | IPV6_FW_F_IN);	// any to any 

    // fw_src, fw_dst, fw_smsk, fw_dmsk is 0 for any to any
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding V6 rule: %d\n", RULE_9);
        return i;
    }
    
    return SUCCESS;
}

/**************************************** UDP RULES ******************************/


//********************************************************************************
// AddDefaultUDPRULE
//********************************************************************************
int AddDefaultUDPRULE(int controlSocket, int udpport, int rulenumber )
{
    struct ip_fw rule;
    int i;


    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = rulenumber;
    rule.fw_flg |= IP_FW_F_ACCEPT;
    rule.fw_prot = IPPROTO_UDP;
    rule.fw_uar.fw_pts[0] = udpport;
    IP_FW_SETNDSTP(&rule, 1);
    rule.fw_flg |= IP_FW_F_IN;

    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d, errno = %d\n", rulenumber, errno);
        return i;
    }
    
    return SUCCESS;
}

			
//********************************************************************************
// AddDefaultUDPBootServer
//********************************************************************************
int AddDefaultUDPBootServer( int  controlSocket)
{
    struct ip_fw rule;
    int i;

    // add allow udp from me to any out keep-state
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = UDPRULEBOOTSERVER; 		// rule #
    rule.fw_flg |= IP_FW_F_ACCEPT;	// allow keep state
    rule.fw_prot = IPPROTO_UDP; 		// udp
    rule.fw_uar.fw_pts[0] = BOOTSERVERPORT;
    IP_FW_SETNSRCP(&rule, 1);
    
    rule.fw_flg |= (IP_FW_F_DME | IP_FW_F_IN);	// dst me
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d, errno = %d\n", UDPRULEBOOTSERVER, errno);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultUDPMDNS
//********************************************************************************
int AddDefaultUDPMDNS( int  controlSocket)
{
    struct ip_fw rule;
    int i;

    // add allow udp from me to any out keep-state
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = UDPRULEMDNSRESPONSE; 		// rule #
    rule.fw_flg |= IP_FW_F_ACCEPT;	// allow keep state
    rule.fw_prot = IPPROTO_UDP; 		// udp
    rule.fw_uar.fw_pts[0] = MDNSPORT;
    IP_FW_SETNSRCP(&rule, 1);
    
    rule.fw_flg |= (IP_FW_F_DME | IP_FW_F_IN);	// dst me
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d, errno = %d\n", UDPRULEMDNSRESPONSE, errno);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddDefaultUDPCheckState
//********************************************************************************
int AddDefaultUDPCheckState(int controlSocket )
{
    struct ip_fw rule;
    int i;

    // add check-state
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = UDPRULECHECKSTATE; 		// rule #
    rule.fw_flg |= (IP_FW_F_CHECK_S | IP_FW_F_ACCEPT); 		// accept check state
    //rule.fw_flg |= IP_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_UDP; 		// udp
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d, errno = %d\n", UDPRULECHECKSTATE, errno);
        return i;
    }
    
    return SUCCESS;
   
}

//********************************************************************************
// AddDefaultUDPKeepState
//********************************************************************************
int AddDefaultUDPKeepState(int controlSocket )
{
    struct ip_fw rule;
    int i;

    // add allow udp from me to any out keep-state
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = UDPRULEKEEPSTATE; 		// rule #
    rule.fw_flg |= (IP_FW_F_KEEP_S | IP_FW_F_ACCEPT);	// allow keep state
    //rule.fw_flg |= IP_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_UDP; 		// udp
    
    rule.fw_flg |= (IP_FW_F_SME | IP_FW_F_OUT);	// source me to any
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d, errno = %d\n", UDPRULEKEEPSTATE, errno);
        return i;
    }
    
    return SUCCESS;
   
}

//********************************************************************************
// AddDefaultUDPFRAG
//********************************************************************************
int AddDefaultUDPFRAG(int controlSocket )
{
    struct ip_fw rule;
    int i;

    // add allow udp from any to any in frag
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = UDPRULEDEFAULTFRAG; 		// rule #
    rule.fw_flg |= IP_FW_F_ACCEPT; 		// deny
    //rule.fw_flg |= IP_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_UDP; 		// udp
    
    rule.fw_flg |= (IP_FW_F_IN | IP_FW_F_FRAG);	// any to in frag 
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d, errno = %d\n", UDPRULEDEFAULTFRAG, errno);
        return i;
    }
    
    return SUCCESS;
   
}

//********************************************************************************
// AddDefaultUDPDeny
//********************************************************************************
int AddDefaultUDPDeny(int controlSocket, Boolean loggingenabled )
{
    struct ip_fw rule;
    int i;

    // ipfw add deny udp from any to any in
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = UDPRULEDEFAULTDENY; 		// rule #
    rule.fw_flg |= IP_FW_F_DENY; 		// deny
	if (loggingenabled)
		rule.fw_flg |= IP_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_UDP; 		// udp
    
    rule.fw_flg |= IP_FW_F_IN;	// any to any 
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d, errno = %d\n", UDPRULEDEFAULTDENY, errno);
        return i;
    }
    
    return SUCCESS;
   
}


//********************************************************************************
// checklist
//********************************************************************************
boolean_t	checklist( long unsigned *udpportlist, long unsigned theport, int max )
{
    int	i;
    
    for ( i=0; i<max; i++)
    {
        if ( *udpportlist++ == theport)
            return false;
    }
    return true;
}

//********************************************************************************
// AddDefaultUDPRPCRULE
//********************************************************************************
int AddDefaultUDPRPCRULE(int controlSocket, Boolean isV6 )
{
#define	MAXRPCUDPPORT	80				

    int i;
    struct sockaddr_in server_addr;
    struct pmaplist *head = NULL;
    unsigned long	udpportlist[MAXRPCUDPPORT];		
    int	index=0;
    int	error = SUCCESS;

    // ipfw add allow udp from any to any RPC port in
    bzero((char *)&server_addr, sizeof server_addr);
    server_addr.sin_family = AF_INET;

    *( (u_int32_t*)&server_addr.sin_addr) = INADDR_LOOPBACK;
    server_addr.sin_len = sizeof( u_int32_t );
    server_addr.sin_port = htons(PMAPPORT);

    head = pmap_getmaps( &server_addr);
    
    for (; head != NULL; head = head->pml_next) {
        if (head->pml_map.pm_prot == IPPROTO_UDP)
        {
                if ( checklist( udpportlist, head->pml_map.pm_port, index ))
                {
                    // add UDP port to list if it is not already in the list
                    udpportlist[index++] = head->pml_map.pm_port;
                }
        }
    }
    // add all RPC ports to FireWall rule
    
    if ( isV6 )
    {
        for ( i = 0; i < index; i++){
            error = AddDefaultUDPRULEV6( controlSocket, udpportlist[i], UDPRPCRULESTART+i);
                if ( error != SUCCESS ) break;
        }
    }
    else
    {
        for ( i = 0; i < index; i++){
            error = AddDefaultUDPRULE( controlSocket, udpportlist[i], UDPRPCRULESTART+i);
                if ( error != SUCCESS ) break;
        }
    }
    
    return error;
}

/*********************** ICMP ******************************************/

//********************************************************************************
// AddICMPRule
//********************************************************************************
int AddICMPRule( int controlSocket, Boolean loggingenabled)
{

    struct ip_fw rule;
    int i;

    // ipfw add 12150 deny tcp from any to any

    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = ICMPECHOREQRULE;                   // rule #
    rule.fw_flg |= IP_FW_F_DENY;                // deny
        if ( loggingenabled )
                rule.fw_flg |= IP_FW_F_PRN;             // for logging
    rule.fw_prot = IPPROTO_ICMP;                // icmp

        rule.fw_uar.fw_icmptypes[ICMP_ECHO / (sizeof(unsigned) * 8)] |=
                        1 << (ICMP_ECHO % (sizeof(unsigned) * 8));
        rule.fw_flg |= IP_FW_F_ICMPBIT;                         // ICMP type
    rule.fw_flg |= (IP_FW_F_IN | IP_FW_F_DME);  // any to me
                
    // fw_src, fw_dst, fw_smsk, fw_dmsk is 0 for any to any
                    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
                
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", RULE_9);
        return i;
    }
    
    return SUCCESS;
            
                
}       
    
    
/**************************************** V6 UDP RULES ******************************/

//********************************************************************************
// AddDefaultUDPRULEV6
//********************************************************************************
int AddDefaultUDPRULEV6(int controlSocket, int udpport, int rulenumber )
{
    struct ip6_fw rule;
    int i;


    memset(&rule, 0, sizeof(rule));

    rule.version = IPV6_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = rulenumber;
    rule.fw_flg |= IPV6_FW_F_ACCEPT;
    rule.fw_prot = IPPROTO_UDP;
    rule.fw_pts[0] = udpport;
    IPV6_FW_SETNDSTP(&rule, 1);
    rule.fw_flg |= IPV6_FW_F_IN;

    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d, errno = %d\n", rulenumber, errno);
        return i;
    }
    
    return SUCCESS;
}
			
//********************************************************************************
// AddDefaultUDPV6BootServer
//********************************************************************************
int AddDefaultUDPV6BootServer( int  controlSocket)
{
    struct ip6_fw rule;
    int i;

    // add allow udp from me to any out keep-state
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IPV6_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = UDPRULEBOOTSERVER; 		// rule #
    rule.fw_flg |= IPV6_FW_F_ACCEPT;	// allow keep state
    rule.fw_prot = IPPROTO_UDP; 		// udp
    rule.fw_pts[0] = BOOTSERVERPORT;
    IPV6_FW_SETNSRCP(&rule, 1);
    
    //rule.fw_flg |= (IPV6_FW_F_DME | IPV6_FW_F_IN);	// dst me
    rule.fw_flg |= ( IPV6_FW_F_IN);	// dst me
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d, errno = %d\n", UDPRULEBOOTSERVER, errno);
        return i;
    }
    
    return SUCCESS;
}

#ifdef later
//********************************************************************************
// AddDefaultUDPCheckStateV6
//********************************************************************************
int AddDefaultUDPCheckStateV6(int controlSocket )
{
    struct ip6_fw rule;
    int i;

    // add check-state
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IPV6_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = UDPRULECHECKSTATE; 		// rule #
    /* no check state rule for ipv6 */
    rule.fw_flg |= (IPV6_FW_F_CHECK_S | IPV6_FW_F_ACCEPT); 		// accept check state
    //rule.fw_flg |= IPV6_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_UDP; 		// udp
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d, errno = %d\n", UDPRULECHECKSTATE, errno);
        return i;
    }
    
    return SUCCESS;
   
}

//********************************************************************************
// AddDefaultUDPKeepStateV6
//********************************************************************************
int AddDefaultUDPKeepStateV6(int controlSocket )
{
    struct ip6_fw rule;
    int i;

    // add allow udp from me to any out keep-state
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IPV6_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = UDPRULEKEEPSTATE; 		// rule #
    /* no keep state rule for ipv6 */
    rule.fw_flg |= (IPV6_FW_F_KEEP_S | IPV6_FW_F_ACCEPT);	// allow keep state
    //rule.fw_flg |= IPV6_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_UDP; 		// udp
    
   rule.fw_flg |= (IPV6_FW_F_SME | IPV6_FW_F_OUT);	// source me to any
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d, errno = %d\n", UDPRULEKEEPSTATE, errno);
        return i;
    }
    
    return SUCCESS;
   
}
#endif

//********************************************************************************
// AddDefaultUDPFRAGV6
//********************************************************************************
int AddDefaultUDPFRAGV6(int controlSocket )
{
    struct ip6_fw rule;
    int i;

    // add allow udp from any to any in frag
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IPV6_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = UDPRULEDEFAULTFRAG; 		// rule #
    rule.fw_flg |= IPV6_FW_F_ACCEPT; 		// deny
    //rule.fw_flg |= IP_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_UDP; 		// udp
    
    rule.fw_flg |= (IPV6_FW_F_IN | IPV6_FW_F_FRAG);	// any to in frag 
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d, errno = %d\n", UDPRULEDEFAULTFRAG, errno);
        return i;
    }
    
    return SUCCESS;
   
}

//********************************************************************************
// AddDefaultUDPDenyV6
//********************************************************************************
int AddDefaultUDPDenyV6(int controlSocket, Boolean loggingenabled )
{
    struct ip6_fw rule;
    int i;

    // ipfw add deny udp from any to any in
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IPV6_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = UDPRULEDEFAULTDENY; 		// rule #
    rule.fw_flg |= IPV6_FW_F_DENY; 		// deny
	if ( loggingenabled )
		rule.fw_flg |= IPV6_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_UDP; 		// udp
    
    rule.fw_flg |= IPV6_FW_F_IN;	// any to any 
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d, errno = %d\n", UDPRULEDEFAULTDENY, errno);
        return i;
    }
    
    return SUCCESS;
   
}


//********************************************************************************
// AddUserDefinedRule
//********************************************************************************
int AddUserDefinedRule(int controlSocket, int ruleNumber, SInt32 start, SInt32 end, boolean_t useRange, boolean_t doUDP)
{
    struct ip_fw rule;
    int i;
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IP_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = ruleNumber;
    rule.fw_flg |= IP_FW_F_ACCEPT;
    if (doUDP)
        rule.fw_prot = IPPROTO_UDP;
    else
        rule.fw_prot = IPPROTO_TCP;
    
    if( !doUDP && (start == SIGNAL_FTP && end == SIGNAL_FTP && useRange) ) // special FTP Case!
    {
        rule.fw_uar.fw_pts[0] = 21;
        IP_FW_SETNDSTP(&rule, 1);
        rule.fw_flg |= IP_FW_F_IN;
    }
    else // regular case: add allow tcp from any to any <port number> in (user configurable!)
    {
        rule.fw_uar.fw_pts[0] = start;
        IP_FW_SETNDSTP(&rule, 1);
        rule.fw_flg |= IP_FW_F_IN;
        
        if( useRange ) // a range of ports like 20-30
        {
            rule.fw_uar.fw_pts[1] = end;
            IP_FW_SETNDSTP(&rule, 2);
            rule.fw_flg |= IP_FW_F_DRNG;
        }
        
        // fw_src, fw_dst, fw_smsk, fw_dmsk is 0 for any to any
    }
    
    
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", ruleNumber);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// AddUserDefinedRuleV6
//********************************************************************************
int AddUserDefinedRuleV6(int controlSocket, int ruleNumber, SInt32 start, SInt32 end, boolean_t useRange, boolean_t doUDP)
{
    struct ip6_fw rule;
    int i;
    
    memset(&rule, 0, sizeof(rule));

    rule.version = IPV6_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = ruleNumber;
    rule.fw_flg |= IPV6_FW_F_ACCEPT;
    if (doUDP)
        rule.fw_prot = IPPROTO_UDP;
    else
        rule.fw_prot = IPPROTO_TCP;
    
    if( !doUDP && (start == SIGNAL_FTP && end == SIGNAL_FTP && useRange) ) // special FTP Case!
    {
        rule.fw_pts[0] = 21;
        IPV6_FW_SETNDSTP(&rule, 1);
        rule.fw_flg |= IPV6_FW_F_IN;
    }
    else // regular case: add allow tcp from any to any <port number> in (user configurable!)
    {
        rule.fw_pts[0] = start;
        IPV6_FW_SETNDSTP(&rule, 1);
        rule.fw_flg |= IPV6_FW_F_IN;
        
        if( useRange ) // a range of ports like 20-30
        {
            rule.fw_pts[1] = end;
            IPV6_FW_SETNDSTP(&rule, 2);
            rule.fw_flg |= IPV6_FW_F_DRNG;
        }
        
        // fw_src, fw_dst, fw_smsk, fw_dmsk is 0 for any to any
    }
    
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding V6 rule: %d\n", ruleNumber);
        return i;
    }
    
    return SUCCESS;
}


//********************************************************************************
// AddICMPRuleV6
//********************************************************************************
int AddICMPRuleV6( int controlSocket, Boolean loggingenabled )
{
    struct ip6_fw rule;
    int i;
    
    // ipfw add 12150 deny tcp from any to any
        
    memset(&rule, 0, sizeof(rule));

    rule.version = IPV6_FW_CURRENT_API_VERSION; // send down the version
    rule.context = (void*)'AAPL';
    rule.fw_number = ICMPECHOREQRULE; 			// rule #
    rule.fw_flg |= IPV6_FW_F_DENY; 		// deny
	if ( loggingenabled )
		rule.fw_flg |= IPV6_FW_F_PRN;		// for logging
    rule.fw_prot = IPPROTO_ICMPV6;			// icmpv6
    
	rule.fw_icmp6types[ICMP6_ECHO_REQUEST / (sizeof(unsigned) * 8)] |=
			1 << (ICMP6_ECHO_REQUEST % (sizeof(unsigned) * 8));
	rule.fw_flg |= IPV6_FW_F_ICMPBIT ;				// ICMP type
    rule.fw_flg |= IPV6_FW_F_IN;					// any in 

    // fw_src, fw_dst, fw_smsk, fw_dmsk is 0 for any to any
    
    i = setsockopt(controlSocket, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof(rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d\n", ICMPECHOREQRULE);
        return i;
    }
    
    return SUCCESS;
}

//********************************************************************************
// WriteOutInternetSharingRule
//********************************************************************************
int WriteOutInternetSharingRule(int controlSocket, struct ip_fw *rule)
{
    int i;
        
    i = setsockopt(controlSocket, IPPROTO_IP, IP_FW_ADD, rule, sizeof(*rule));
    
    if( i )
    {
        printf("Firewall Tool: Error adding rule: %d (Internet Sharing)\n", RULE_0);
        return i;
    }

    return SUCCESS;
}

//********************************************************************************
// GetPortRange
//********************************************************************************
int GetPortRange(CFStringRef portString, SInt32 *start, SInt32 *end, boolean_t *useRange)
{
    char buffer[256];
    
    if( portString == NULL )
        return ERROR;
    
    if( CFStringGetLength(portString) == 0 )
        return ERROR;
        
    if( CFStringGetCString(portString, buffer, 256, kCFStringEncodingMacRoman) )
    {
        char *ptr = NULL;
        char bufferCopy[256];
        
        strcpy(bufferCopy, buffer);
        
        ptr = strtok(bufferCopy, "-");
                    
        if( ptr ) // ptr does not equal NULL
        {
            if( strcmp(ptr, "*") == 0 ) // special FTP CASE
            {
                *start = SIGNAL_FTP;
                *end = SIGNAL_FTP;
                *useRange = true;
                return SUCCESS;
                
            }else
            {
                int startVal = strtol(ptr, (char**)NULL, 10);
                
                if( startVal > 0 && startVal <= 65535 && startVal != LONG_MAX && startVal != LONG_MIN )
                {
                    *start = startVal;    
                }
                else
                {
                    printf("Firewall Tool: Error, Start Port (%d) invalid!\n", startVal);
                    return ERROR;
                }
            }
            
        }else
        {
            printf("Firewall Tool: Error, No Start Port defined!\n");
            return ERROR;
        }
        
        ptr = strtok(NULL, "\0");
        
        if( ptr )
        {
            int endVal = strtol(ptr, (char**)NULL, 10);
            
            if( endVal > 0 && endVal <= 65535 && endVal != LONG_MAX && endVal != LONG_MIN )
            {
                *end = endVal;    
                *useRange = true;
            }
            else
            {
                printf("Firewall Tool: Error, End Port (%d) invalid!\n", endVal);
                return ERROR;
            }
        
        }else
        {
            *end = 0;
            *useRange = false;
        }
        
        return SUCCESS;    
    }else
    {
        return ERROR;
    }
}

// isInternetSharingRunning
boolean_t isInternetSharingRunning(int controlSocket, struct ip_fw *rule)
{	
    struct ip_fw *rules = NULL;
    int dataSize = 0;
    int actualDataSize = 0;
    int numRules = 0;
    int iterations = 1;
    int i, j;

    // do the stupid socket memory thing to figure out how much to allocate
    do
    {       
        dataSize = (sizeof(struct ip_fw) * 10) * iterations; 
        
        rules = (struct ip_fw *)realloc(rules, dataSize);

        if (rules == NULL )
        {
            printf("Firewall Tool: Error getting rules! Realloc Failed!\n");
            return false;
        }
        
        rules->version = IP_FW_CURRENT_API_VERSION; // send down the version
                
        actualDataSize = dataSize;
        
        i = getsockopt(controlSocket, IPPROTO_IP, IP_FW_GET, rules, &actualDataSize);
        if( i < 0 )
        {
            printf("Firewall Tool: Error getting rules! Not written into structure...\n");
            free(rules);
            return false;
        }
        
        iterations++;
                    
    }while( dataSize == actualDataSize );
        
    numRules = actualDataSize/sizeof(struct ip_fw);
    
    for( j = 0; j < numRules; j++ )
    {
        int ruleNumber = rules[i].fw_number;
        
        if( ruleNumber == RULE_0 )
        {
            rule = memcpy(rule, &rules[i], sizeof(struct ip_fw));
            return true;
        }
    }
    
    if( rules )
        free( rules );
    
    return false;
}


//********************************************************************************
// EnableLogging
//********************************************************************************
void EnableLogging(boolean_t enable)
{
    int newValue = enable ? 2 : 0;
    int errorCode;
    
    errorCode = sysctlbyname("net.inet.ip.fw.verbose", NULL, NULL, &newValue, sizeof(newValue));
    
    if( errorCode < 0 )
    {
        printf("Firewall Tool: Error, sysctlbyname returned: %d\n", errorCode);
    } 
}


//********************************************************************************
// IsExpectedCFType
//********************************************************************************
boolean_t IsExpectedCFType(CFPropertyListRef variableInQuestion, CFTypeID expectedType)
{
    if( variableInQuestion == NULL ) // variable in question does not exist!
    {
        printf("Firewall Tool: Error, IsExpectedCFType(variableInQuestion) variable == NULL!\n");
        return false;
    }    
    else if( CFGetTypeID(variableInQuestion) == expectedType ) 
    {
        return true;
    }
    else
    {
        return false;
    }
}

//********************************************************************************
// EnableBlackHole
//********************************************************************************
void EnableBlackHole(boolean_t enable)
{
    int lognewValue = enable ? 3 : 0;
    int newValue = enable ? 1 : 0;
    int errorCode;
	
    errorCode = sysctlbyname("net.inet.tcp.blackhole", NULL, NULL, &newValue, sizeof(newValue));
    if( errorCode < 0 )
    {
        printf("Firewall Tool: Error, sysctlbyname(net.inet.tcp.blackhole): %d\n", errno);
    }
    errorCode = sysctlbyname("net.inet.tcp.log_in_vain", NULL, NULL, &lognewValue, sizeof(newValue));
    if( errorCode < 0 )
    {
        printf("Firewall Tool: Error, sysctlbyname(net.inet.tcp.log_in_vain): %d\n", errno);
    }
	
    errorCode = sysctlbyname("net.inet.udp.blackhole", NULL, NULL, &newValue, sizeof(newValue));
    if( errorCode < 0 )
    {
        printf("Firewall Tool: Error, sysctlbyname(net.inet.udp.blackhole): %d\n", errno);
    }
    errorCode = sysctlbyname("net.inet.udp.log_in_vain", NULL, NULL, &lognewValue, sizeof(newValue));
    if( errorCode < 0 )
    {
        printf("Firewall Tool: Error, sysctlbyname(net.inet.udp.log_in_vain): %d\n", errno);
    }
}


    
    

