/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <stdio.h>

#ifndef _FSCK_MESSAGES_H
#define _FSCK_MESSAGES_H

/*
 * Internal structure for each fsck message.  This is the same 
 * structure in which the message number, message string and 
 * their corresponding attributes are mapped in fsck_strings.c 
 * and fsck_hfs_strings.c
 */
struct fsck_message {
    unsigned int msgnum;    /* fsck message number as an unsigned value */
    char *msg;              /* fsck message as a C string */
    int type;               /* type of message (see fsck_msgtype below) */
    int level;              /* verbosity level at which this message should be output/presented to the user */
    int numargs;            /* number of arguments for this message string */
    const int *argtype;     /* pointer to an array of argument types (see fsck_argtype below) */
};
typedef struct fsck_message fsck_message_t;

typedef void *fsck_ctx_t;

/* Type of fsck message string.
 * These values are internal values used in the mapping array of 
 * message number and string to identify the type of message for 
 * each entry.
 */
enum fsck_msgtype {
    fsckMsgUnknown = 0,
    fsckMsgVerify,          /* fsck is performing a read-only operation on the volume */
    fsckMsgRepair,          /* fsck is writing to file system to repair a corruption */
    fsckMsgSuccess,         /* verify found that the volume is clean, or repair was successful */
    fsckMsgFail,            /* verify found that the volume is corrupt, or verify did not complete due to error, or repair failed */
    fsckMsgError,           /* information of corruption found or condition that causes verify/repair to fail */
    fsckMsgDamageInfo,      /* information about corrupt files/folders */
    fsckMsgInfo,            /* information about an error message or any fsck operation */
    fsckMsgProgress,        /* percentage progress of verify/repair operation */
};

/* Type of parameter for fsck message string.  
 * These values are internal values used in the mapping array of 
 * message number and string to identify the type of parameter 
 * for each entry.
 */
enum fsck_arg_type {
    fsckTypeUnknown = 0,
    fsckTypeInt,            /* positive integer */
    fsckTypeLong,           /* positive long value */
    fsckTypeString,         /* UTF-8 string */
    fsckTypePath,           /* path to a file or directory on the volume */
    fsckTypeFile,           /* name of file */
    fsckTypeDirectory,      /* name of directory */
    fsckTypeVolume,         /* name of a volume */
    fsckTypeProgress,       /* percentage progress number */
    fsckTypeFSType,         /* type of file system being checked */
};

/* Verbosity of fsck message string. 
 * These values are internal values used in the mapping array of 
 * message number and string to identify the verbosity of each entry.
 */
enum fsck_message_levels {
    fsckLevel0  = 0,        /* level 0 messages should always be displayed to the user */
    fsckLevel1  = 1         /* level 1 messages should be only displayed in advanced mode */
};

/* Type of fsck_hfs output */
enum fsck_output_type {
    fsckOutputUndefined = 0,
    fsckOutputTraditional,  /* standard string output */
    fsckOutputGUI,          /* output for -g option */
    fsckOutputXML           /* XML output for -x option */
};

/* Types of default answers for user input questions in fsck */
enum fsck_default_answer_type {
    fsckDefaultNone = 0,
    fsckDefaultNo,
    fsckDefaultYes
};

extern fsck_ctx_t fsckCreate(void);
extern int fsckSetOutput(fsck_ctx_t, FILE*);
extern int fsckSetFile(fsck_ctx_t, int);
extern int fsckSetWriter(fsck_ctx_t, void (*)(fsck_ctx_t, const char *));
extern int fsckSetVerbosity(fsck_ctx_t, int);
extern int fsckGetVerbosity(fsck_ctx_t);
extern int fsckSetOutputStyle(fsck_ctx_t, enum fsck_output_type);
extern enum fsck_output_type fsckGetOutputStyle(fsck_ctx_t);
extern int fsckSetDefaultResponse(fsck_ctx_t, enum fsck_default_answer_type);
extern int fsckAskPrompt(fsck_ctx_t, const char *, ...);
extern int fsckAddMessages(fsck_ctx_t, fsck_message_t *msgs);
extern int fsckPrint(fsck_ctx_t, int msgNum, ...);
extern enum fsck_msgtype fsckMsgClass(fsck_ctx_t, int msgNum);
extern void fsckDestroy(fsck_ctx_t);

#endif /* _FSCK_MESSAGES_H */
