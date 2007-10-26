#! /usr/sbin/dtrace -C -s

/* Copyright 2007 Apple Inc. All rights reserved. */

inline int KAUTH_EXTLOOKUP_REGISTER = 0;
inline int KAUTH_EXTLOOKUP_RESULT = (1<<0);
inline int KAUTH_EXTLOOKUP_WORKER = (1<<1);

inline int KAUTH_EXTLOOKUP_SUCCESS = 0; /* results here are good */
inline int KAUTH_EXTLOOKUP_BADRQ = 1; /* request badly formatted */
inline int KAUTH_EXTLOOKUP_FAILURE = 2; /* transient failure during lookup */
inline int KAUTH_EXTLOOKUP_FATAL = 3; /* permanent failure during lookup */
inline int KAUTH_EXTLOOKUP_INPROG = 100; /* request in progress */

inline int KAUTH_EXTLOOKUP_VALID_UID   =   (1<<0);
inline int KAUTH_EXTLOOKUP_VALID_UGUID =   (1<<1);
inline int KAUTH_EXTLOOKUP_VALID_USID  =   (1<<2);
inline int KAUTH_EXTLOOKUP_VALID_GID   =   (1<<3);
inline int KAUTH_EXTLOOKUP_VALID_GGUID =   (1<<4);
inline int KAUTH_EXTLOOKUP_VALID_GSID  =   (1<<5);
inline int KAUTH_EXTLOOKUP_WANT_UID    =   (1<<6);
inline int KAUTH_EXTLOOKUP_WANT_UGUID  =   (1<<7);
inline int KAUTH_EXTLOOKUP_WANT_USID   =   (1<<8);
inline int KAUTH_EXTLOOKUP_WANT_GID    =   (1<<9);
inline int KAUTH_EXTLOOKUP_WANT_GGUID  =   (1<<10);
inline int KAUTH_EXTLOOKUP_WANT_GSID   =   (1<<11);
inline int KAUTH_EXTLOOKUP_WANT_MEMBERSHIP  = (1<<12);
inline int KAUTH_EXTLOOKUP_VALID_MEMBERSHIP = (1<<13);
inline int KAUTH_EXTLOOKUP_ISMEMBER    =   (1<<14);

#define GET_WORK_ITEM(from) \
    (struct kauth_identity_extlookup *)copyin((from), \
	    sizeof(struct kauth_identity_extlookup));

#define KAUTH_OPCODE_STRING(opcode) \
    (int)opcode == KAUTH_EXTLOOKUP_REGISTER ? "KAUTH_EXTLOOKUP_REGISTER" : \
    (int)opcode == KAUTH_EXTLOOKUP_RESULT ? "KAUTH_EXTLOOKUP_RESULT" : \
    (int)opcode == KAUTH_EXTLOOKUP_WORKER ? "KAUTH_EXTLOOKUP_WORKER" : \
    stringof(opcode)

#define KAUTH_RESULT_STRING(res) \
    (int)res == KAUTH_EXTLOOKUP_SUCCESS ? "KAUTH_EXTLOOKUP_SUCCESS" : \
    (int)res == KAUTH_EXTLOOKUP_BADRQ ? "KAUTH_EXTLOOKUP_BADRQ" : \
    (int)res == KAUTH_EXTLOOKUP_FAILURE ? "KAUTH_EXTLOOKUP_FAILURE" : \
    (int)res == KAUTH_EXTLOOKUP_FATAL ? "KAUTH_EXTLOOKUP_FATAL" : \
    (int)res == KAUTH_EXTLOOKUP_INPROG ? "KAUTH_EXTLOOKUP_INPROG" : \
    stringof(res)

#define APPEND_FLAG(string, flagset, flag) \
    (string) = strjoin((string), (int)(flagset) & (flag) ?  \
		    strjoin((string) == "" ? "" : "|", #flag) : "")

#define KAUTH_FLAGS_STRING(string, flagset) \
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_VALID_UID);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_VALID_UGUID);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_VALID_USID);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_VALID_GID);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_VALID_GGUID);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_VALID_GSID);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_WANT_UID);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_WANT_UGUID);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_WANT_USID);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_WANT_GID);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_WANT_GGUID);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_WANT_GSID);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_WANT_MEMBERSHIP);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_VALID_MEMBERSHIP);\
    APPEND_FLAG(string, flagset, KAUTH_EXTLOOKUP_ISMEMBER)

#define PRINTIT(value) \
    printf("%s=%d\n", #value, (int)(value))

#define TRACE_EXTLOOKUP(ext) \
    this->temp = ""; \
    printf("el_result=%s\n", KAUTH_RESULT_STRING(ext->el_result)); \
    KAUTH_FLAGS_STRING(this->temp, ext->el_flags); \
    printf("el_flags=%s\n", this->temp); \
    PRINTIT(ext->el_uguid_valid); \
    PRINTIT(ext->el_usid_valid); \
    PRINTIT(ext->el_gguid_valid); \
    PRINTIT(ext->el_gsid_valid); \
    PRINTIT(ext->el_member_valid)

syscall::identitysvc:entry
{
    self->arg1 = arg1;
    printf("opcode %s", KAUTH_OPCODE_STRING(arg0));
}

/* Directory service is giving a result to the kernel. */
syscall::identitysvc:entry
/ (int)arg0 == KAUTH_EXTLOOKUP_RESULT && self->arg1 != 0 /
{
    this->work = GET_WORK_ITEM(self->arg1);
    TRACE_EXTLOOKUP(this->work);
}

/* Kernel is giving a work request to the directory service. */ 
syscall::identitysvc:return
/ (int)arg0 == KAUTH_EXTLOOKUP_WORKER && self->arg1 != 0 /
{
    this->work = GET_WORK_ITEM(self->arg1);
    TRACE_EXTLOOKUP(this->work);
}

