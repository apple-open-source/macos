/*
 * This file is automagically created from comm.c -- DO NOT EDIT
 */

struct comm
{
  char *name;
  int flags;
#ifdef MULTIUSER
  AclBits userbits[ACL_BITS_PER_CMD];
#endif
};

#define ARGS_MASK	(3)

#define ARGS_0	(0)
#define ARGS_1	(1)
#define ARGS_2	(2)
#define ARGS_3	(3)

#define ARGS_PLUS1	(1<<2)
#define ARGS_PLUS2	(1<<3)
#define ARGS_PLUS3	(1<<4)
#define ARGS_ORMORE	(1<<5)

#define NEED_FORE	(1<<6)	/* this command needs a fore window */
#define NEED_DISPLAY	(1<<7)	/* this command needs a display */
#define NEED_LAYER	(1<<8)	/* this command needs a layer */

#define ARGS_01		(ARGS_0 | ARGS_PLUS1)
#define ARGS_02		(ARGS_0 | ARGS_PLUS2)
#define ARGS_12		(ARGS_1 | ARGS_PLUS1)
#define ARGS_23		(ARGS_2 | ARGS_PLUS1)
#define ARGS_24		(ARGS_2 | ARGS_PLUS2)
#define ARGS_34		(ARGS_3 | ARGS_PLUS1)
#define ARGS_012	(ARGS_0 | ARGS_PLUS1 | ARGS_PLUS2)
#define ARGS_0123	(ARGS_0 | ARGS_PLUS1 | ARGS_PLUS2 | ARGS_PLUS3)
#define ARGS_123	(ARGS_1 | ARGS_PLUS1 | ARGS_PLUS2)
#define ARGS_124	(ARGS_1 | ARGS_PLUS1 | ARGS_PLUS3)
#define ARGS_1234	(ARGS_1 | ARGS_PLUS1 | ARGS_PLUS2 | ARGS_PLUS3)

struct action
{
  int nr;
  char **args;
  int *argl;
};

#define RC_ILLEGAL -1

#define RC_ACLADD 0
#define RC_ACLCHG 1
#define RC_ACLDEL 2
#define RC_ACLGRP 3
#define RC_ACLUMASK 4
#define RC_ACTIVITY 5
#define RC_ADDACL 6
#define RC_ALLPARTIAL 7
#define RC_ALTSCREEN 8
#define RC_AT 9
#define RC_ATTRCOLOR 10
#define RC_AUTODETACH 11
#define RC_AUTONUKE 12
#define RC_BACKTICK 13
#define RC_BCE 14
#define RC_BELL 15
#define RC_BELL_MSG 16
#define RC_BIND 17
#define RC_BINDKEY 18
#define RC_BLANKER 19
#define RC_BLANKERPRG 20
#define RC_BREAK 21
#define RC_BREAKTYPE 22
#define RC_BUFFERFILE 23
#define RC_C1 24
#define RC_CAPTION 25
#define RC_CHACL 26
#define RC_CHARSET 27
#define RC_CHDIR 28
#define RC_CLEAR 29
#define RC_COLON 30
#define RC_COMMAND 31
#define RC_COMPACTHIST 32
#define RC_CONSOLE 33
#define RC_COPY 34
#define RC_CRLF 35
#define RC_DEBUG 36
#define RC_DEFAUTONUKE 37
#define RC_DEFBCE 38
#define RC_DEFBREAKTYPE 39
#define RC_DEFC1 40
#define RC_DEFCHARSET 41
#define RC_DEFENCODING 42
#define RC_DEFESCAPE 43
#define RC_DEFFLOW 44
#define RC_DEFGR 45
#define RC_DEFHSTATUS 46
#define RC_DEFKANJI 47
#define RC_DEFLOG 48
#define RC_DEFMODE 49
#define RC_DEFMONITOR 50
#define RC_DEFNONBLOCK 51
#define RC_DEFOBUFLIMIT 52
#define RC_DEFSCROLLBACK 53
#define RC_DEFSHELL 54
#define RC_DEFSILENCE 55
#define RC_DEFSLOWPASTE 56
#define RC_DEFUTF8 57
#define RC_DEFWRAP 58
#define RC_DEFWRITELOCK 59
#define RC_DETACH 60
#define RC_DIGRAPH 61
#define RC_DINFO 62
#define RC_DISPLAYS 63
#define RC_DUMPTERMCAP 64
#define RC_ECHO 65
#define RC_ENCODING 66
#define RC_ESCAPE 67
#define RC_EVAL 68
#define RC_EXEC 69
#define RC_FIT 70
#define RC_FLOW 71
#define RC_FOCUS 72
#define RC_GR 73
#define RC_HARDCOPY 74
#define RC_HARDCOPY_APPEND 75
#define RC_HARDCOPYDIR 76
#define RC_HARDSTATUS 77
#define RC_HEIGHT 78
#define RC_HELP 79
#define RC_HISTORY 80
#define RC_HSTATUS 81
#define RC_IDLE 82
#define RC_IGNORECASE 83
#define RC_INFO 84
#define RC_KANJI 85
#define RC_KILL 86
#define RC_LASTMSG 87
#define RC_LICENSE 88
#define RC_LOCKSCREEN 89
#define RC_LOG 90
#define RC_LOGFILE 91
#define RC_LOGTSTAMP 92
#define RC_MAPDEFAULT 93
#define RC_MAPNOTNEXT 94
#define RC_MAPTIMEOUT 95
#define RC_MARKKEYS 96
#define RC_MAXWIN 97
#define RC_META 98
#define RC_MONITOR 99
#define RC_MSGMINWAIT 100
#define RC_MSGWAIT 101
#define RC_MULTIUSER 102
#define RC_NETHACK 103
#define RC_NEXT 104
#define RC_NONBLOCK 105
#define RC_NUMBER 106
#define RC_OBUFLIMIT 107
#define RC_ONLY 108
#define RC_OTHER 109
#define RC_PARTIAL 110
#define RC_PASSWORD 111
#define RC_PASTE 112
#define RC_PASTEFONT 113
#define RC_POW_BREAK 114
#define RC_POW_DETACH 115
#define RC_POW_DETACH_MSG 116
#define RC_PREV 117
#define RC_PRINTCMD 118
#define RC_PROCESS 119
#define RC_QUIT 120
#define RC_READBUF 121
#define RC_READREG 122
#define RC_REDISPLAY 123
#define RC_REGISTER 124
#define RC_REMOVE 125
#define RC_REMOVEBUF 126
#define RC_RESET 127
#define RC_RESIZE 128
#define RC_SCREEN 129
#define RC_SCROLLBACK 130
#define RC_SELECT 131
#define RC_SESSIONNAME 132
#define RC_SETENV 133
#define RC_SETSID 134
#define RC_SHELL 135
#define RC_SHELLTITLE 136
#define RC_SILENCE 137
#define RC_SILENCEWAIT 138
#define RC_SLEEP 139
#define RC_SLOWPASTE 140
#define RC_SORENDITION 141
#define RC_SOURCE 142
#define RC_SPLIT 143
#define RC_STARTUP_MESSAGE 144
#define RC_STUFF 145
#define RC_SU 146
#define RC_SUSPEND 147
#define RC_TERM 148
#define RC_TERMCAP 149
#define RC_TERMCAPINFO 150
#define RC_TERMINFO 151
#define RC_TIME 152
#define RC_TITLE 153
#define RC_UMASK 154
#define RC_UNSETENV 155
#define RC_UTF8 156
#define RC_VBELL 157
#define RC_VBELL_MSG 158
#define RC_VBELLWAIT 159
#define RC_VERBOSE 160
#define RC_VERSION 161
#define RC_WALL 162
#define RC_WIDTH 163
#define RC_WINDOWLIST 164
#define RC_WINDOWS 165
#define RC_WRAP 166
#define RC_WRITEBUF 167
#define RC_WRITELOCK 168
#define RC_XOFF 169
#define RC_XON 170
#define RC_ZMODEM 171
#define RC_ZOMBIE 172

#define RC_LAST 172
