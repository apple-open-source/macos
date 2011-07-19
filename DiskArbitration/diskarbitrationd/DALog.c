/*
 * Copyright (c) 1998-2011 Apple Inc. All Rights Reserved.
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

#include "DALog.h"

#include "DABase.h"

#include <asl.h>
#include <syslog.h>

static Boolean __gDALogDebug            = FALSE;
static FILE *  __gDALogDebugFile        = NULL;
static char *  __gDALogDebugHeaderLast  = NULL;
static char *  __gDALogDebugHeaderNext  = NULL;
static Boolean __gDALogDebugHeaderReset = FALSE;
static Boolean __gDALogError            = FALSE;
static Boolean __gDALogVerbose          = FALSE;

static void __DALog( int level, const char * format, va_list arguments )
{
    char * message;

    if ( arguments )
    {
        message = ___CFStringCreateCStringWithFormatAndArguments( format, arguments );
    }
    else
    {
        message = strdup( format );
    }

    if ( message )
    {
        switch ( level )
        {
            case LOG_DEBUG:
            {
                if ( __gDALogDebug )
                {
                    if ( __gDALogDebugFile )
                    {
                        time_t clock = time( NULL );
                        char   stamp[10];

                        if ( strftime( stamp, sizeof( stamp ), "%T ", localtime( &clock ) ) )
                        {
                            fprintf( __gDALogDebugFile, "%s", stamp );
                        }

                        fprintf( __gDALogDebugFile, "%s", message );
                        fprintf( __gDALogDebugFile, "\n" );
                        fflush( __gDALogDebugFile );
                    }
                }

                break;
            }
            case LOG_ERR:
            {
                if ( __gDALogError )
                {
                    syslog( level, "%s", message );

                    break;
                }
            }
            case LOG_INFO:
            {
                if ( __gDALogVerbose )
                {
                    syslog( level, "%s", message );
                }

                break;
            }
            default:
            {
                syslog( level, "%s", message );

                break;
            }
        }

        free( message );
    }
}

void DALog( const char * format, ... )
{
    va_list arguments;

    va_start( arguments, format );

    __DALog( LOG_NOTICE, format, arguments );

    va_end( arguments );
}

void DALogClose( void )
{
    __gDALogDebug   = FALSE;
    __gDALogError   = FALSE;
    __gDALogVerbose = FALSE;

    if ( __gDALogDebugFile )
    {
        fclose( __gDALogDebugFile );

        __gDALogDebugFile = NULL;
    }

    closelog( );
}

void DALogDebug( const char * format, ... )
{
    va_list arguments;

    va_start( arguments, format );

    if ( __gDALogDebugHeaderReset )
    {
        assert( __gDALogDebugHeaderNext );

        if ( __gDALogDebugHeaderLast )
        {
            free( __gDALogDebugHeaderLast );
        }

        __gDALogDebugHeaderLast  = __gDALogDebugHeaderNext;
        __gDALogDebugHeaderNext  = NULL;
        __gDALogDebugHeaderReset = FALSE;

        __DALog( LOG_DEBUG, "", NULL );

        __DALog( LOG_DEBUG, __gDALogDebugHeaderLast, NULL );
    }

    __DALog( LOG_DEBUG, format, arguments );

    va_end( arguments );
}

void DALogDebugHeader( const char * format, ... )
{
    va_list arguments;

    va_start( arguments, format );

    if ( __gDALogDebugHeaderNext )
    {
        free( __gDALogDebugHeaderNext );

        __gDALogDebugHeaderNext  = NULL;
        __gDALogDebugHeaderReset = FALSE;
    }

    if ( format )
    {
        char * header;

        header = ___CFStringCreateCStringWithFormatAndArguments( format, arguments );

        if ( header )
        {
            if ( __gDALogDebugHeaderLast )
            {
                if ( strcmp( __gDALogDebugHeaderLast, header ) )
                {
                    __gDALogDebugHeaderNext  = header;
                    __gDALogDebugHeaderReset = TRUE;
                }
                else
                {
                    free( header );
                }
            }
            else
            {
                __gDALogDebugHeaderNext  = header;
                __gDALogDebugHeaderReset = TRUE;
            }
        }
    }

    va_end( arguments );
}

void DALogError( const char * format, ... )
{
    va_list arguments;

    va_start( arguments, format );

    __DALog( LOG_DEBUG, format, arguments );

    va_end( arguments );

    va_start( arguments, format );

    __DALog( LOG_ERR, format, arguments );

    va_end( arguments );
}

void DALogOpen( char * name, Boolean debug, Boolean error, Boolean verbose )
{
    asl_set_filter( NULL, ASL_FILTER_MASK_UPTO( ASL_LEVEL_DEBUG ) );

    openlog( name, LOG_PID, LOG_DAEMON );

    if ( debug )
    {
        char * path;

        asprintf( &path, "/var/log/%s.log", name );

        if ( path )
        {
            __gDALogDebugFile = fopen( path, "a" );

            free( path );
        }
    }

    __gDALogDebug   = debug;
    __gDALogError   = error;
    __gDALogVerbose = verbose;
}

void DALogVerbose( const char * format, ... )
{
    va_list arguments;

    va_start( arguments, format );

    __DALog( LOG_INFO, format, arguments );

    va_end( arguments );
}
