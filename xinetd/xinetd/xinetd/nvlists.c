/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>

#include "nvlists.h"
#include "sconf.h"


/*
 * A NULL value for the name field marks the end of the table
 */

const struct name_value service_types[] =
   {
#ifndef NO_RPC
      { "RPC",                        ST_RPC                  },
#endif
      { "INTERNAL",                   ST_INTERNAL             },
      { "UNLISTED",                   ST_UNLISTED             },
      { "SPECIAL",                    ST_SPECIAL              },
      { "TCPMUX",                     ST_TCPMUX               },
      { "TCPMUXPLUS",                 ST_TCPMUXPLUS           },
      { CHAR_NULL,                    0                       }
   } ;


/* REUSE is only used for backward compatibility.  All services are now reuse */
const struct name_value service_flags[] =
   {
      { "REUSE",                      SF_REUSE               },
      { "INTERCEPT",                  SF_INTERCEPT           },
      { "NORETRY",                    SF_NORETRY             },
      { "IDONLY",                     SF_IDONLY              },
      { "NAMEINARGS",                 SF_NAMEINARGS          },
      { "NODELAY",                    SF_NODELAY             },
      { "KEEPALIVE",                  SF_KEEPALIVE           },
      { "NOLIBWRAP",                  SF_NOLIBWRAP           },
      { "SENSOR",                     SF_SENSOR              },
      { "IPv4",                       SF_IPV4                },
      { "IPv6",                       SF_IPV6                },
      { CHAR_NULL,                    0                      }
   } ;


const struct name_value socket_types[] =
   {
      { "stream",                     SOCK_STREAM           },
      { "dgram",                      SOCK_DGRAM            },
      { "raw",                        SOCK_RAW              },
      { "seqpacket",                  SOCK_SEQPACKET        },
      { CHAR_NULL,                    1                     },
      { "BAD SOCKET TYPE",            0                     }
   } ;


const struct name_value success_log_options[] =
   {
      { "HOST",        LO_HOST     },
      { "DURATION",    LO_DURATION },
      { "EXIT",        LO_EXIT     },
      { "PID",         LO_PID      },
      { "USERID",      LO_USERID   },

   } ;


const struct name_value failure_log_options[] = 
   {
      { "HOST",        LO_HOST     },
      { "ATTEMPT",     LO_ATTEMPT  },
      { "RECORD",      LO_RECORD   },
      { "USERID",      LO_USERID   },
      { CHAR_NULL,     0           }
   } ;



const struct name_value syslog_facilities[] =
   {
      { "daemon",         LOG_DAEMON      },
      { "auth",           LOG_AUTH        },
#ifdef linux
      { "authpriv",       LOG_AUTHPRIV    },
#endif
      { "user",           LOG_USER        },
      { "local0",         LOG_LOCAL0      },
      { "local1",         LOG_LOCAL1      },
      { "local2",         LOG_LOCAL2      },
      { "local3",         LOG_LOCAL3      },
      { "local4",         LOG_LOCAL4      },
      { "local5",         LOG_LOCAL5      },
      { "local6",         LOG_LOCAL6      },
      { "local7",         LOG_LOCAL7      },
      { CHAR_NULL,        1               },
      { "BAD FACILITY",   0               }
   } ;


const struct name_value syslog_levels[] = 
   {
      { "emerg",         LOG_EMERG      },
      { "alert",         LOG_ALERT      },
      { "crit",          LOG_CRIT       },
      { "err",           LOG_ERR        },
      { "warning",       LOG_WARNING    },
      { "notice",        LOG_NOTICE     },
      { "info",          LOG_INFO       },
      { "debug",         LOG_DEBUG      },
      { CHAR_NULL,       1              },
      { "BAD LEVEL",     0              }
   } ;

