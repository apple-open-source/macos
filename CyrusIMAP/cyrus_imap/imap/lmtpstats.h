
/* lmtpstats.h -- statistics push interface

 * generated automatically from lmtpstats.snmp by snmpgen

 *

 * Copyright 2000 Carnegie Mellon University

 *

 * No warranty, yadda yadda

 */                                       

                                          

#ifndef lmtpstats_H    

#define lmtpstats_H



#define SNMPDEFINE_cmutree "1.3.6.1.4.1.3.2.2.3"
#define SNMPDEFINE_mib2 "1.3.6.1.2.1"
#define SNMPDEFINE_cmulmtp "1.3.6.1.4.1.3.2.2.3.2"
#define SNMPDEFINE_mtamib "1.3.6.1.2.1.28"
#define SNMPDEFINE_mgmt "1.3.6.1.2"
#define SNMPDEFINE_cmusieve "1.3.6.1.4.1.3.2.2.3.3"



#ifndef USING_SNMPGEN



#define snmp_connect()

#define snmp_close()

#define snmp_increment(a, b)

#ifdef __GNUC__

#define snmp_increment_args(args...)

#else

#define snmp_increment_args(args)

#endif

#define snmp_set(a, b)

#define snmp_set_str(a, b)

#define snmp_set_oid(a, b)

#define snmp_set_time(a, b)

#define snmp_getdescription(a)

#define snmp_getoid(a, b, c, d)

#define snmp_setvariable(a, b)



typedef void lmtpstats_t;



#else



typedef enum {

    SIEVE_REDIRECT,
    SIEVE_DISCARD,
    SERVER_UPTIME,
    mtaSuccessfulConvertedMessages,
    mtaReceivedMessages,
    mtaTransmittedMessages,
    SIEVE_VACATION_REPLIED,
    SIEVE_NOTIFY,
    SIEVE_FILEINTO,
    mtaReceivedRecipients,
    mtaTransmittedVolume,
    SIEVE_VACATION_TOTAL,
    SIEVE_REJECT,
    AUTHENTICATION_YES,
    AUTHENTICATION_NO,
    TOTAL_CONNECTIONS,
    SIEVE_KEEP,
    mtaReceivedVolume,
    SERVER_NAME_VERSION,
    SIEVE_MESSAGES_PROCESSED,
    ACTIVE_CONNECTIONS
} lmtpstats_t;



typedef enum {

    VARIABLE_LISTEND,
    VARIABLE_AUTH,
    VARIABLE_MTA


} lmtpstats_variable_t;



int snmp_connect(void);



int snmp_close(void);          

                                    

/* only valid on counters */

int snmp_increment(lmtpstats_t cmd, int);

int snmp_increment_args(lmtpstats_t cmd, int incr, ...);



/* only valid on values */

int snmp_set(lmtpstats_t cmd, int);



int snmp_set_str(lmtpstats_t cmd, char *value);



int snmp_set_oid(lmtpstats_t cmd, char *str);



int snmp_set_time(lmtpstats_t cmd, time_t t);

                                    

const char *snmp_getdescription(lmtpstats_t cmd); 

 

const char *snmp_getoid(const char *name, lmtpstats_t cmd, char* buf, int buflen); 



void snmp_setvariable(lmtpstats_variable_t, int);



#endif /* USING_SNMPGEN */

 

#endif /* lmtpstats_H */ 



