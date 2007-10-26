//
// Original Authors:  Jonathan M. Gilligan, Tony M. Hoyle
//
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the 
// Free Software Foundation; either version 2 of the License, or (at your
// option) any later version.
// 
// This program is distributed in the hope that it will be useful, but 
// WITHOUT ANY WARRANTY; without even the implied warranty of 
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
// General Public License for more details.
// 
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 
// 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// 
// Modification History:
// 18 May 2001, JMG -- First version

#include <windows.h>
#include <tchar.h>
#include <time.h>
#include <stdbool.h>

#include "JmgStat.h"


/* Tony Hoyle's function for testing whether a given volume uses UTC or 
 * local time to record file modification times
 * 
 * Reproduced here with permission of Tony Hoyle.
 * 
 * This code is copyright by Tony Hoyle and is licensed under the Gnu 
 * Public License. (See above)
 *
 * NTFS, HPFS, and OWFS store file times as UTC times.
 * FAT stores file times as local time.
 *
 * INPUTS:
 *      LPCSTR name: fully qualified path
 *
 * OUTPUTS:
 *      Return true if the file system on the volume in question 
 *      stores file times as UTC
 */
bool IsUTCVolume ( LPCTSTR name )
{
    _TCHAR szDrive[_MAX_DRIVE + 1] = _T("");
    _TCHAR szFs[32]=_T("");
    _tsplitpath(name, szDrive, NULL, NULL, NULL);

    _tcscat(szDrive, _T("\\"));
    GetVolumeInformation( szDrive, NULL, 0, NULL, NULL, NULL, szFs, 32 );
    return ! ( _tcsicmp( szFs, _T("NTFS") ) 
               && _tcsicmp( szFs, _T("HPFS") ) 
               && _tcsicmp( szFs, _T("OWFS") ) );
}

/* Convert a file time to a Unix time_t structure. This function is as 
 * complicated as it is because it needs to ask what time system the 
 * filetime describes.
 * 
 * INPUTS:
 *      const FILETIME * ft: A file time. It may be in UTC or in local 
 *                           time (see local_time, below, for details).
 *
 *      time_t * ut:         The destination for the converted time.
 *
 *      bool local_time:     TRUE if the time in *ft is in local time 
 *                           and I need to convert to a real UTC time.
 *
 * OUTPUTS:
 *      time_t * ut:         Store the result in *ut.
 */
static bool FileTimeToUnixTime ( const FILETIME* ft, time_t* ut, bool local_time )
{
    bool success = FALSE;
    if ( local_time ) 
    {
        struct tm atm;
        SYSTEMTIME st;

        success = FileTimeToSystemTime ( ft, &st );

        /* Important: mktime looks at the tm_isdst field to determine
         * whether to apply the DST correction. If this field is zero,
         * then no DST is applied. If the field is one, then DST is
         * applied. If the field is minus one, then DST is applied
         * if the United States rule calls for it (DST starts at 
         * 02:00 on the first Sunday in April and ends at 02:00 on
         * the last Sunday in October.
         *
         * If you are concerned about time zones that follow different 
         * rules, then you must either use GetTimeZoneInformation() to 
         * get your system's TIME_ZONE_INFO and use the information
         * therein to figure out whether the time in question was in 
         * DST or not, or else use SystemTimeToTzSpecifiedLocalTime()
         * to do the same.
         *
         * I haven't tried playing with SystemTimeToTzSpecifiedLocalTime()
         * so I am nor sure how well it handles funky stuff.
         */
        atm.tm_sec = st.wSecond;
        atm.tm_min = st.wMinute;
        atm.tm_hour = st.wHour;
        atm.tm_mday = st.wDay;
        /* tm_mon is 0 based */
        atm.tm_mon = st.wMonth - 1;
        /* tm_year is 1900 based */
        atm.tm_year = st.wYear>1900?st.wYear - 1900:st.wYear;     
        atm.tm_isdst = -1;      /* see notes above */
        *ut = mktime ( &atm );
    }
    else 
    {

       /* FILETIME = number of 100-nanosecond ticks since midnight 
        * 1 Jan 1601 UTC. time_t = number of 1-second ticks since 
        * midnight 1 Jan 1970 UTC. To translate, we subtract a
        * FILETIME representation of midnight, 1 Jan 1970 from the
        * time in question and divide by the number of 100-ns ticks
        * in one second.
        */

        /* One second = 10,000,000 * 100 nsec */
        const ULONGLONG second = 10000000L;

        SYSTEMTIME base_st = 
        {
            1970,   /* wYear            */
            1,      /* wMonth           */
            0,      /* wDayOfWeek       */
            1,      /* wDay             */
            0,      /* wHour            */
            0,      /* wMinute          */
            0,      /* wSecond          */
            0       /* wMilliseconds    */
        };
        
        ULARGE_INTEGER itime;
        FILETIME base_ft;

        success = SystemTimeToFileTime ( &base_st, &base_ft );
        if (success) 
        {
            itime.QuadPart = ((ULARGE_INTEGER *)ft)->QuadPart;

            itime.QuadPart -= ((ULARGE_INTEGER *)&base_ft)->QuadPart;
            itime.QuadPart /= second;

            *ut = itime.LowPart;
        }
    }
    if (!success)
    {
        *ut = -1;   /* error value used by mktime() */
    }
    return success;
}

/* Get file modification time using FileTimeToUnixTime()
 *
 * INPUTS:
 *      LPCTSTR name:   the file name
 */
bool GetUTCFileModTime ( LPCTSTR name, time_t * utc_mod_time )
{
    WIN32_FIND_DATA find_buf;
    FILETIME mod_time;
    HANDLE find_handle;
    bool success = FALSE;

    * utc_mod_time = 0L;

    find_handle = FindFirstFile ( name, &find_buf );
    success = ( find_handle != INVALID_HANDLE_VALUE );
    if (success)
    {
        /* Originally I thought that I needed to apply a correction 
         * LocalTimeToFileTime() to files from FAT volumes, but the 
         * FindFirstFile() system call thoughtfully applies this 
         * correction itself.
         *
         * Thus, the file time returned is allegedly in UTC.
         *
         * However, the correction from local to UTC is applied 
         * incorrectly (Thanks a lot, Microsoft!). As documented in the 
         * Win32 API (see MSDN or the PSDK), DST is applied if and only 
         * if the computer's system time is in DST at the time we call 
         * FindFirstFile(), irrespective or whether DST applied at the 
         * time the file was modified!
         *
         * Thus, we have to call FileTimeToLocalFileTime() to undo
         * Windows's good intentions. We correctly translate the time
         * In FileTimeToUnixTime().
         *
         */
        if ( IsUTCVolume ( name ) ) 
        {
            mod_time = find_buf.ftLastWriteTime;
            success = FileTimeToUnixTime ( &mod_time, utc_mod_time, FALSE );
        }
        else 
        { 
            // See notes above...
            success = FileTimeToLocalFileTime ( &find_buf.ftLastWriteTime, &mod_time );
            success = success && FileTimeToUnixTime ( &mod_time, utc_mod_time, TRUE );
        }        
    }
    FindClose ( find_handle );
    return success;
}
