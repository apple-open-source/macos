//
//  MsgTrace.c
//  AppleCDC
//
//  Created by test on 8/19/12.
//
//

#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/fslog.h>
#include <IOKit/IOLib.h>


/* Message Tracing Defines */
#define CDC_ASL_MAX_FMT_LEN         1024
#define CDC_ASL_MSG_LEN             "         0"
#define CDC_ASL_LEVEL_NOTICE		5
#define CDC_ASL_KEY_DOMAIN          "com.apple.message.domain"
#define CDC_ASL_KEY_SIG             "com.apple.message.signature"
#define CDC_ASL_KEY_SIG2			"com.apple.message.signature2"
#define CDC_ASL_KEY_SIG3			"com.apple.message.signature3"
#define CDC_ASL_KEY_SUCCESS         "com.apple.message.success"
#define CDC_ASL_SUCCESS_VALUE		1
#define CDC_ASL_KEY_VALUE			"com.apple.message.value"
#define CDC_ASL_KEY_MSG             "Message"

#define CDC_ASL_DOMAIN              "com.apple.commssw.cdc.device"

/*
 * Log a message for MessageTracer.
 */
__private_extern__ void cdc_LogToMessageTracer(const char *domain, const char *signature, const char *signature2, const char *signature3, u_int64_t optValue, int optSucceeded)
{
	char *fmt = NULL;		/* Format string to use with vaddlog */
	char *temp = NULL;
    
	if ( (domain == NULL) || (signature == NULL) ) {
		/* domain and signature are required */
		return;
	}
	
    fmt = (char *)IOMalloc(CDC_ASL_MAX_FMT_LEN);
	if (fmt == NULL)
		return;
    
    temp = (char *)IOMalloc(CDC_ASL_MAX_FMT_LEN);
	if (temp == NULL) {
        IOFree(fmt, CDC_ASL_MAX_FMT_LEN);
		return;
	}
    
    if ((signature3 != NULL) && (signature2 != NULL))
    {
        snprintf(fmt, CDC_ASL_MAX_FMT_LEN, "%s [%s %d] [%s %s] [%s %s] [%s %s] [%s %s]",
                 CDC_ASL_MSG_LEN,
                 FSLOG_KEY_LEVEL, CDC_ASL_LEVEL_NOTICE,
                 CDC_ASL_KEY_DOMAIN, domain,
                 CDC_ASL_KEY_SIG, signature,
                 CDC_ASL_KEY_SIG2, signature2,
                 CDC_ASL_KEY_SIG3, signature3);
    }
    else
        if ((signature3 == NULL) && (signature2 != NULL))
        {
            snprintf(fmt, CDC_ASL_MAX_FMT_LEN, "%s [%s %d] [%s %s] [%s %s] [%s %s]",
                     CDC_ASL_MSG_LEN,
                     FSLOG_KEY_LEVEL, CDC_ASL_LEVEL_NOTICE,
                     CDC_ASL_KEY_DOMAIN, domain,
                     CDC_ASL_KEY_SIG, signature,
                     CDC_ASL_KEY_SIG2, signature2);
        }
        else
            snprintf(fmt, CDC_ASL_MAX_FMT_LEN, "%s [%s %d] [%s %s] [%s %s]",
                     CDC_ASL_MSG_LEN,
                     FSLOG_KEY_LEVEL, CDC_ASL_LEVEL_NOTICE,
                     CDC_ASL_KEY_DOMAIN, domain,
                     CDC_ASL_KEY_SIG, signature);
    
	if (optSucceeded == 1) {
		/* Optional mark successes as such.  (Don't mark failures.) */
		snprintf(temp, CDC_ASL_MAX_FMT_LEN, " [%s %d]", CDC_ASL_KEY_SUCCESS, CDC_ASL_SUCCESS_VALUE);
		strlcat(fmt, temp, CDC_ASL_MAX_FMT_LEN);
	}
    
	if (optValue != 0) {
		/* Optional value */
		snprintf(temp, CDC_ASL_MAX_FMT_LEN, " [%s %lld]", CDC_ASL_KEY_VALUE, optValue);
		strlcat(fmt, temp, CDC_ASL_MAX_FMT_LEN);
	}
    
	/* Print the key-value pairs in ASL format */
	IOLog("%s\n", fmt);
	
	if (fmt != NULL)
        IOFree(fmt, CDC_ASL_MAX_FMT_LEN);
	if (temp != NULL)
        IOFree(temp, CDC_ASL_MAX_FMT_LEN);
}

