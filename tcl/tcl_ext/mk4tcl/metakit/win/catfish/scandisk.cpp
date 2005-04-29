//  scandisk.cpp  -  recursive directory scanner sample code
//
//  Copyright (C) 1996-2000 Jean-Claude Wippler.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "scandisk.h"

#include "dtwinver.cpp"

#ifndef _WIN32 

    #include <dos.h>

    #define FILE_ATTRIBUTE_DIRECTORY        0x00000010  
    
    typedef struct _SYSTEMTIME {
        WORD wYear;
        WORD wMonth;
        WORD wDayOfWeek;
        WORD wDay;
        WORD wHour;
        WORD wMinute;
        WORD wSecond;
        WORD wMilliseconds;
    } SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;
    
    typedef struct _FILETIME {
       DWORD dwLowDateTime;
       DWORD dwHighDateTime;
    } FILETIME;
    
    #define LFN_MAX_PATH 260
    
    typedef struct _WIN32_FIND_DATA { // wfd
       DWORD dwFileAttributes;
       FILETIME ftCreationTime;
       FILETIME ftLastAccessTime;
       FILETIME ftLastWriteTime;
       DWORD    nFileSizeHigh;
       DWORD    nFileSizeLow;
       DWORD    dwReserved0;
       DWORD    dwReserved1;
       char     cFileName[ LFN_MAX_PATH ];
       char    cAlternateFileName[ 14 ];
    } WIN32_FIND_DATA;

    static BOOL IsCDROMDrive(int nDrive)
    // Calls MSCDEX and checks that MSCDEX is loaded, and that MSCDEX
    // reports the drive is a CD-ROM.
    {
       BOOL bResult = FALSE;   // Assume not a CD-ROM 
       _asm {
          mov  ax,150Bh      // MSCDEX CD-ROM drive check
          xor  bx,bx
          mov  cx,[nDrive]
          int  2fh
          cmp  bx,0adadh     // Check for MSCDEX signature
          jne  done
          or   ax, ax        // get the drive type
          jz   done          // 0 = no CD-ROM
          mov  [bResult],TRUE
    done:
       }
       return bResult;
    }
    
    #define DRIVE_CDROM   5
    
    static t4_u32 GetVolumeInfo( unsigned drive,
                  c4_String& volname, c4_String& fstype )
    {
       #pragma pack(1) 
        struct {
            unsigned  _infoLevel;
            unsigned long _serialNum;
            char  _volLabel[11];
            char  _fileSysType[8];
        } mid [1];
       #pragma pack()

       bool result = true;      // Assume success

       _asm {
          push ds
          mov  bx,[drive]
          mov  cx,0866h   // MS-DOS IOCTL Get Media ID
          lds  dx,[mid]
          mov  ax,440dh
          int  21h
          jnc  done        // carry set if error
          mov  [result], false
    done:
          pop  ds
       }
       
        if (!result)
            return 0;
            
        int i = sizeof mid->_volLabel;
        while (i > 0 && mid->_volLabel[i-1] == ' ')
            --i;
            
        volname = c4_String (mid->_volLabel, i);
               
        int j = sizeof mid->_fileSysType;
        while (j > 0 && mid->_fileSysType[j-1] == ' ')
            --j;
            
        fstype = c4_String (mid->_fileSysType, j);
               
        return mid->_serialNum;
    }
    
#endif
    
/////////////////////////////////////////////////////////////////////////////
// Property definitions

    c4_ViewProp     NEAR pFiles ("files");
    c4_IntProp      NEAR pParent ("parent");
    c4_IntProp      NEAR pSize ("size");
    c4_IntProp      NEAR pDate ("date");
    c4_StringProp   NEAR pName ("name");

/////////////////////////////////////////////////////////////////////////////

DirScanner::DirScanner ()     
{
}

DirScanner::~DirScanner ()
{
}

c4_String DirScanner::NameCase(const c4_String& name_)
{
    CString s = name_;
    s.MakeUpper();       
                    
        // if only upper case, then capitalize the first letter only
    if (s == name_ && !s.IsEmpty())
    {
        char c = s[0];
        s.MakeLower();
        s.SetAt(0, c);
        return s;
    }
    
    return name_;
}
    
void DirScanner::NewFile(const c4_String& nm_, long sz_, long dt_)
{
    if (nm_.IsEmpty())
        return;         // skip garbage (my Linux6 CD has this...)

    int n = _files.GetSize();
    _files.SetSize(n + 1);
              
    c4_RowRef r = _files[n];
    pName (r) = NameCase(nm_);
    pSize (r) = sz_;
    pDate (r) = dt_;
}

void DirScanner::NewSubDir(const c4_String& nm_)
{
    if (nm_.IsEmpty() || nm_ == "." || nm_ == "..")
        return;

    int n = _subDirs.GetSize();
    _subDirs.SetSize(n + 1);

    c4_RowRef r = _subDirs[n];
    pName (r) = NameCase(nm_);
}

c4_String DirScanner::Info(const c4_String& path_, VolInfo* vi_)
{
    VolInfo temp;
    if (vi_ == 0)
        vi_ = &temp;
    
    vi_->_rootPath = path_.SpanExcluding("\\") + "\\";
    
    if (vi_->_rootPath.Mid(1) != ":\\")
    {       // probably a network drive, we can't get disk stats
        static VolInfo NEAR zero;
        *vi_ = zero; // clear all VolInfo fields
    }
    else if (DoInfo(*vi_))
        return vi_->_volName;

    return "";
}

bool DirScanner::Scan(c4_View& dirs_, int dirNum_, bool withTime_)
{
    _files.SetSize(0, 100);     // growth granularity
    _subDirs.SetSize(0, 100);
    
    if (!DoScan(fFullPath(dirs_, dirNum_), withTime_))
        return false;
        
    pFiles (dirs_[dirNum_]) = _files.SortOn(pName);
        
    for (int i = 0; i < _subDirs.GetSize(); ++i)
        pParent (_subDirs[i]) = dirNum_;
        
    dirs_.InsertAt(dirs_.GetSize(), _subDirs);
    
    return true;
}

c4_String DirScanner::ShortName(const c4_String& fileName_) const
{
    return fileName_; // unchanged
}

/////////////////////////////////////////////////////////////////////////////

class Win16Scanner : public DirScanner
{
public:
    virtual bool DoInfo(VolInfo& info_);
    virtual bool DoScan(const c4_String& path_, bool withTime_);
    virtual bool DoMove(const c4_String& from_, const c4_String& to_) const;
};
      
/////////////////////////////////////////////////////////////////////////////

bool Win16Scanner::DoInfo(VolInfo& info_)
{
    unsigned drive = info_._rootPath[0] & 0x1F;
        
    struct _diskfree_t df;
    if (_dos_getdiskfree(drive, &df) != 0)
        return false;

    info_._secPerCluster = df.sectors_per_cluster;
    info_._bytesPerSec = df.bytes_per_sector;
    info_._availClusters = df.avail_clusters;
    info_._totalClusters = df.total_clusters;
                
    info_._driveType = GetDriveType(drive - 1);
    if (info_._driveType == DRIVE_REMOTE && IsCDROMDrive(drive - 1))
        info_._driveType = DRIVE_CDROM;
        
    info_._serialNum = GetVolumeInfo(drive, info_._volName, info_._fsType);

    info_._maxCompLen = 12; // 8.3 names
    info_._fsFlags = 0; // ? (this also flags the catalog as made in Win16)
            
        // don't trust volume label, use findfirst _A_VOLID if possible
        // on CD-ROMs and WFW, GetVolumeInfo returns garbage (why?)
    _find_t fd;
    if (_dos_findfirst(info_._rootPath + "*.*", _A_VOLID, &fd) == 0)
    {
        CString t = fd.name;
        int i = t.Find('.');
        if (i >= 0)
            t = t.Left(i) + t.Mid(i+1);
        
        info_._volName = t;
    }
    
    return true;
}

bool Win16Scanner::DoScan(const c4_String& path_, bool withTime_)
{
    _find_t fd;
        
    const unsigned mask = _A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_SUBDIR;
    if (_dos_findfirst(path_ + "*.*", mask, &fd) == 0)
    {
        do
        {
            if ((fd.attrib & _A_SUBDIR) == 0)
            {
                long date = fd.wr_date;
                if (withTime_)
                {
                    int h = fd.wr_time >> 11;
                    int m = (fd.wr_time >> 5) & 0x3F;
                    int s = (fd.wr_time & 0x1F) << 1;
                    
                    date = ((date * 24 + h) * 60 + m) * 60 + s;
                }
                
                NewFile(fd.name, fd.size, date);
            }
            else
                NewSubDir(fd.name);
                
        } while (_dos_findnext(&fd) == 0);
    }

    return true;
}

bool Win16Scanner::DoMove(const c4_String& from_, const c4_String& to_) const
{
    return rename(from_, to_) == 0;
}

/////////////////////////////////////////////////////////////////////////////

class WOWScanner : public DirScanner
{
    DWORD _ghLib;
    FARPROC _FindFirstFile;
    FARPROC _FindNextFile;
    FARPROC _FindClose;
    FARPROC _FileTimeToLocalFileTime;
    FARPROC _FileTimeToSystemTime;
    
public:
    WOWScanner ();
    virtual ~WOWScanner ();

    bool Setup();
    
    virtual bool DoInfo(VolInfo& info_);
    virtual bool DoScan(const c4_String& path_, bool withTime_);
    virtual bool DoMove(const c4_String& from_, const c4_String& to_) const;
    virtual c4_String ShortName(const c4_String& fileName_) const;
};
      
/////////////////////////////////////////////////////////////////////////////

WOWScanner::WOWScanner ()
    : _ghLib (0)
{
}

bool WOWScanner::Setup()
{
    OS_VERSION_INFO ovi;
    ovi.dwOSVersionInfoSize = sizeof (OS_VERSION_INFO);
    if (GetOSVersion(&ovi))
        switch (ovi.dwUnderlyingPlatformId)
        {
            case PLATFORM_WIN32S:
            case PLATFORM_DOS:                  ASSERT(0); // can't happen;
            
            default:                            return false;
            
            case PLATFORM_WINDOWS:          
            case PLATFORM_NT_WORKSTATION:   
            case PLATFORM_NT_SERVER:        
            case PLATFORM_NT_ADVANCEDSERVER:    ;
        }
    
    bool useW32calls = false;
       
    _ghLib = WOWLoadLibraryEx32( "kernel32.dll", 0, 0);
    if (_ghLib == 0)
        return false;
        
    _FindFirstFile = WOWGetProcAddress32(_ghLib, "FindFirstFileA");
    _FindNextFile  = WOWGetProcAddress32(_ghLib, "FindNextFileA");
    _FindClose     = WOWGetProcAddress32(_ghLib, "FindClose");
    _FileTimeToLocalFileTime = WOWGetProcAddress32(_ghLib,
                                                   "FileTimeToLocalFileTime");
    _FileTimeToSystemTime = WOWGetProcAddress32(_ghLib,
                                                   "FileTimeToSystemTime");
    
    return _FindFirstFile != 0
        && _FindNextFile != 0
        && _FindClose != 0
        && _FileTimeToLocalFileTime != 0
        && _FileTimeToSystemTime != 0;
}

WOWScanner::~WOWScanner ()
{
    if (_ghLib != 0)
        WOWFreeLibrary32(_ghLib);
}

bool WOWScanner::DoInfo(VolInfo& info_)
{
    if (_ghLib == 0)
        return false;
        
    const char* path = info_._rootPath;
    
    FARPROC fp;
    
    fp = WOWGetProcAddress32(_ghLib, "GetDiskFreeSpaceA");
    if (fp == 0 || !WOWCallProc32(fp, 0x1F, 5,
                                    (DWORD) path,
                                    (DWORD) &info_._secPerCluster, 
                                    (DWORD) &info_._bytesPerSec,
                                    (DWORD) &info_._availClusters, 
                                    (DWORD) &info_._totalClusters))
        return false;
                
    DWORD args[6];

    fp = WOWGetProcAddress32(_ghLib, "GetDiskFreeSpaceExA");
    if (fp == 0 || !WOWCallProc32(fp, 0x0F, 4,
                                    (DWORD) path,
                                    (DWORD) args, 
                                    (DWORD) (args + 2),
                                    (DWORD) (args + 4)))
        return false;
                
    int sh = 0;
    for (DWORD dw = info_._secPerCluster * info_._bytesPerSec; dw > 1; dw >>= 1)
        ++sh;
                    
    info_._availClusters = (args[4] >> sh) | (args[5] << (32-sh));
    info_._totalClusters = (args[2] >> sh) | (args[3] << (32-sh));
                
    fp = WOWGetProcAddress32(_ghLib, "GetDriveTypeA");
    if (fp == 0)
        return false;
    
    info_._driveType = WOWCallProc32(fp, 0x01, 1, (DWORD) path);
                                            
    char buf1 [100];
    char buf2 [100];
    buf1[0] = buf2[0] = 0;

    fp = WOWGetProcAddress32(_ghLib, "GetVolumeInformationA");
    if (fp == 0 || !WOWCallProc32(fp, 0xDE, 8, // reversed, was 0x7B (!)
                                    (DWORD) path,
                                    (DWORD) buf1,
                                    (DWORD) sizeof buf1,
                                    (DWORD) &info_._serialNum,
                                    (DWORD) &info_._maxCompLen,
                                    (DWORD) &info_._fsFlags,
                                    (DWORD) buf2,
                                    (DWORD) sizeof buf2))
        return false;

    info_._volName = buf1;
    info_._fsType = buf2;
    
    return true;
}

bool WOWScanner::DoScan(const c4_String& path_, bool withTime_)
{
    WIN32_FIND_DATA fd;
        
    CString s = path_ + "*";
        
    DWORD h = WOWCallProc32(_FindFirstFile, 0x03, 2,
                            (DWORD) (const char*) s, (DWORD) &fd);
    if (h != (DWORD) -1L)
    {
        FILETIME temp;
        SYSTEMTIME st;
                
        do
        {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                if (!WOWCallProc32(_FileTimeToLocalFileTime, 0x03, 2,
                                    (DWORD) &fd.ftLastWriteTime, (DWORD) &temp))
                    return false;
                    
                if (!WOWCallProc32(_FileTimeToSystemTime, 0x03, 2,
                                        (DWORD) &temp, (DWORD) &st))
                    return false;
                    
                long date = (short) (((st.wYear-1980) << 9) |
                                    (st.wMonth << 5) | st.wDay);
                if (withTime_)
                    date = ((date * 24 + st.wHour) * 60
                                        + st.wMinute) * 60 + st.wSecond;

                NewFile(fd.cFileName, fd.nFileSizeLow, date);
            }
            else
                NewSubDir(fd.cFileName);
                
        } while (WOWCallProc32(_FindNextFile, 0x01, 2,
                    (DWORD) h, (DWORD) &fd));
            
        WOWCallProc32(_FindClose, 0x00, 1, (DWORD) h);
    }

    return true;
}

bool WOWScanner::DoMove(const c4_String& from_, const c4_String& to_) const
{
    FARPROC fp = WOWGetProcAddress32(_ghLib, "MoveFileA");
    if (fp == 0 || !WOWCallProc32(fp, 0x03, 2,
                                    (DWORD) (const char*) from_,
                                    (DWORD) (const char*) to_))
        return false;
        
    return true;
}

c4_String WOWScanner::ShortName(const c4_String& fileName_) const
{
    char path [_MAX_PATH];
    
    FARPROC fp = WOWGetProcAddress32(_ghLib, "GetShortPathNameA");
    if (fp == 0 || !WOWCallProc32(fp, 0x06, 3,
                                    (DWORD) (const char*) fileName_,
                                    (DWORD) path,
                                    (DWORD) sizeof path))
        return fileName_;
        
    return path;
}

/////////////////////////////////////////////////////////////////////////////

bool DirScanner::CanUseLongNames()
{
    WOWScanner wow;
    return wow.Setup();
}

bool DirScanner::Move(const c4_String& from_, const c4_String& to_)
{
    WOWScanner wow;
    if (wow.Setup())
        return wow.DoMove(from_, to_);
        
    Win16Scanner win16;
    return win16.DoMove(from_, to_);
}
    
DirScanner* DirScanner::CreateFILE()
{
    WOWScanner* wow = new WOWScanner;

    if (wow->Setup())
        return wow;
    
    delete wow;

    return new Win16Scanner;
}

/////////////////////////////////////////////////////////////////////////////

bool cStatusHandler::UpdateStatus(const char* /* text */)
{
    MSG msg;
    while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            ::PostQuitMessage(msg.wParam);
            return FALSE;
        }
        if (!AfxGetApp()->PreTranslateMessage(&msg))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }
    AfxGetApp()->OnIdle(0);   // updates user interface
    AfxGetApp()->OnIdle(1);   // frees temporary objects
    
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// Reconstruct the full path name from a subdirectory index in the tree

CString fFullPath(c4_View& dirs_, int dirNum_, bool relative_)
{
        // Prefix all parent dir names until the root level is reached
    CString path;
    
    while (dirNum_ >= 0)
    {
        path = pName (dirs_[dirNum_]) + "\\" + path;

        if (dirNum_ == 0)
            break;
            
        dirNum_ = (int) pParent (dirs_[dirNum_]);

        if (dirNum_ == 0 && relative_)
            break; // don't prefix with the root entry
    }

    return path; // this result always has a trailing backslash
}

/////////////////////////////////////////////////////////////////////////////
// Scan a directory tree and return a corresponding structure for it

    static BOOL tryWin32 = FALSE;

bool fScanDirectories(const char* path_, c4_View dirs, cStatusHandler* handler)
{
        // Start with a view containing the root directory entry
    dirs.SetSize(0);
    dirs.Add(pName [path_]);
    
    OS_VERSION_INFO ovi;
    ovi.dwOSVersionInfoSize = sizeof (OS_VERSION_INFO);
    if (GetOSVersion(&ovi))
        switch (ovi.dwUnderlyingPlatformId)
        {
            case PLATFORM_WIN32S:
            case PLATFORM_DOS:                  ASSERT(0); // can't happen;
            
            default:                            break;
            
            case PLATFORM_WINDOWS:          
            case PLATFORM_NT_WORKSTATION:   
            case PLATFORM_NT_SERVER:        
            case PLATFORM_NT_ADVANCEDSERVER:    tryWin32 = TRUE;
        }
    
    DirScanner* ds = DirScanner::CreateFILE();
    ASSERT(ds);
    bool ok = ds != 0;
    
        // This loop "automagically" handles the recursive traversal of all
        // subdirectories. The trick is that each scan may add new entries
        // at the end, causing this loop to continue (GetSize() changes!).

    for (int i = 0; ok && i < dirs.GetSize(); ++i)
    {
        char buffer [30];
        wsprintf(buffer, "Scanning #%d... ", i);
        
        CString path = buffer + pName (dirs[i]);
        
        if (handler && !handler->UpdateStatus(path) || !ds->Scan(dirs, i))
            ok = false;
    }
    
    delete ds;
    
    return ok;
}

/////////////////////////////////////////////////////////////////////////////
// http://www.r2m.com/win-developer-FAQ/files/3.html
// Q: How do I detect a drive type in Windows?

/*
// additional drive types
#define DRIVE_CDROM   5
#define DRIVE_RAMDISK 6

    #pragma pack(1) 
    typedef struct {
       BYTE  bSpecFunc;        // Special functions
       BYTE  bDevType;         // Device type
       WORD  wDevAttr;         // Device attributes
       WORD  wCylinders;       // Number of cylinders
       BYTE  bMediaType;       // Media type
                               // Beginning of BIOS parameter block (BPB)
       WORD  wBytesPerSec;     // Bytes per sector
       BYTE  bSecPerClust;     // Sectors per cluster
       WORD  wResSectors;      // Number of reserved sectors
       BYTE  bFATs;            // Number of FATs
       WORD  wRootDirEnts;     // Number of root-directory entries
       WORD  wSectors;         // Total number of sectors
       BYTE  bMedia;           // Media descriptor
       WORD  wFATsecs;         // Number of sectors per FAT
       WORD  wSecPerTrack;     // Number of sectors per track
       WORD  wHeads;           // Number of heads
       DWORD dwHiddenSecs;     // Number of hidden sectors
       DWORD dwHugeSectors;    // Number of sectors if wSectors == 0
                            // End of BIOS parameter block (BPB)
    } DEVICEPARAMS, FAR * LPDEVICEPARAMS;
    #pragma pack() 

BOOL GetDeviceParameters( int nDrive, LPDEVICEPARAMS lpDP )
// Fills a DEVICEPARAMS struct with info about the given drive.
// Calls DOS IOCTL Get Device Parameters 440Dh, 60h function.
{
   BOOL bResult = TRUE;      // Assume success
   _asm {
      push ds
      mov  bx,[nDrive]
      inc  bx         // Convert 0-based #'s to 1-based #s
      mov  cx,0860h   // MS-DOS IOCTL Get Device Parameters
      lds  dx,[lpDP]
      mov  ax,440dh
      int  21h
      jnc  done        // carry set if error
      mov  [bResult], FALSE
done:
      pop  ds
   }
   return bResult;
}
*/

UINT GetDriveTypeEx(int nDrive)
// Returns the drive type as reported by GetDriveType(),
// with the addition of DRIVE_CDROM and DRIVE_RAMDISK
{
   UINT uType = GetDriveType(nDrive);  
   switch(uType) {
      case DRIVE_REMOTE:
         // GetDriveType() reports CD-ROMs as Remote drives. 
         if (IsCDROMDrive(nDrive))
            return DRIVE_CDROM;
         break;

      case DRIVE_FIXED:
         // we check the information returned by GetDeviceParameters
         // for the number of FATs on the drive.  RAM disks almost
         // always use 1 while hard disk do not.
//         if (GetDeviceParameters(nDrive,&dp) && dp.bFATs == 1 )
//            return DRIVE_RAMDISK;
         break;
   }
   return uType;  // return result of GetDriveType()
}

    #pragma pack(1) 
    typedef struct _MID {
        unsigned  midInfoLevel;
        unsigned long midSerialNum;
        char  midVolLabel[11];
        char  midFileSysType[8];
    } MID, *PMID;
    #pragma pack()

static unsigned long GetVolumeInfo( unsigned drive,
              char *volname, char *fstype )
{
   MID mid [1];
   int i;
    
   BOOL bResult = TRUE;      // Assume success
   _asm {
      push ds
      mov  bx,[drive]
      mov  cx,0866h   // MS-DOS IOCTL Get Media ID
      lds  dx,[mid]
      mov  ax,440dh
      int  21h
      jnc  done        // carry set if error
      mov  [bResult], FALSE
done:
      pop  ds
   }
   
   if (!bResult)
        return 0;

   if (volname)
   {
       strncpy(volname, mid->midVolLabel, i = 11);
       while (--i >= 0 && volname[i] == ' ')
             ;
       volname[++i] = 0;
   }
   
   if (fstype)
   {
       strncpy(fstype, mid->midFileSysType, i = 8);
       while (--i >= 0 && fstype[i] == ' ')
             ;
       fstype[++i] = 0;
   }
   
   return mid->midSerialNum;
}

c4_View fDiskInfo(const char* path_)
{
/*
    unsigned _dos_getdiskfree( unsigned drive, struct _diskfree_t * diskspace );
        unsigned total_clusters         // Total clusters on disk
        unsigned avail_clusters         // Available clusters on disk
        unsigned sectors_per_cluster    // Sectors per cluster
        unsigned bytes_per_sector       // Bytes per sector

    BOOL GetVersionEx( LPOSVERSIONINFO lpVersionInformation ); 
        DWORD dwOSVersionInfoSize; 
        DWORD dwMajorVersion; 
        DWORD dwMinorVersion; 
        DWORD dwBuildNumber; 
        DWORD dwPlatformId; 
        TCHAR szCSDVersion[ 128 ]; 
    
    BOOL GetDiskFreeSpace( 
        LPCTSTR lpRootPathName,         // address of root path 
        LPDWORD lpSectorsPerCluster,    // address of sectors per cluster 
        LPDWORD lpBytesPerSector,       // address of bytes per sector 
        LPDWORD lpNumberOfFreeClusters, // address of number of free clusters 
        LPDWORD lpTotalNumberOfClusters // address of total number of clusters 
    );

    BOOL GetDiskFreeSpaceEx( 
        LPCTSTR lpDirectoryName, 
        PULARGE_INTEGER lpFreeBytesAvailableToCaller, 
        PULARGE_INTEGER lpTotalNumberOfBytes, 
        PULARGE_INTEGER lpTotalNumberOfFreeBytes 
    ); 
    
    UINT GetDriveType( 
        LPCTSTR lpRootPathName          // address of root path
    );
    
    BOOL GetVolumeInformation( 
        LPCTSTR lpRootPathName,         // address of root dir of the file system 
        LPTSTR lpVolumeNameBuffer,      // address of name of the volume 
        DWORD nVolumeNameSize,          // length of lpVolumeNameBuffer 
        LPDWORD lpVolumeSerialNumber,   // address of volume serial number 
        LPDWORD lpMaximumComponentLength, // addr of system's max filename length 
        LPDWORD lpFileSystemFlags,      // address of file system flags 
        LPTSTR lpFileSystemNameBuffer,  // address of name of file system 
        DWORD nFileSystemNameSize       // length of lpFileSystemNameBuffer 
    );
    
    HANDLE FindFirstFile( 
        LPCTSTR lpFileName,             // pointer to name of file to search for 
        LPWIN32_FIND_DATA lpFindFileData // pointer to returned information
    );

    BOOL FindNextFile(
        HANDLE hFindFile,               // handle to search
        LPWIN32_FIND_DATA lpFindFileData // ptr to structure for data on found file
    );
    
    BOOL FindClose( 
        HANDLE hFindFile                // file search handle 
    ); 
*/    

    c4_IntProp pNumInfo ("i");
    c4_StringProp pStrInfo ("s");
     
    CString s = path_;
    s = s.SpanExcluding("\\") + "\\";
    path_ = s;
    
    c4_View info;

    DWORD args[20];
    char buf1 [100];
    char buf2 [100];
    buf1[0] = buf2[0] = 0;
    
    if (s.Mid(1) != ":\\")
    {       // probably a network drive, we can't get disk stats
        memset(args, 0, sizeof args);
        memset(buf1, 0, sizeof buf1);
        memset(buf2, 0, sizeof buf2);
    }
    else
    {
        DWORD ghLib = tryWin32 ? WOWLoadLibraryEx32( "kernel32.dll", 0, 0) : 0;
        if (ghLib != 0)
        {
            FARPROC fp;
            
            fp = WOWGetProcAddress32(ghLib, "GetDiskFreeSpaceA");          
            DWORD res1 = fp ? WOWCallProc32(fp, 0x1F, 5,
                            (DWORD) path_,
                            (DWORD) (args + 0), (DWORD) (args + 1),
                            (DWORD) (args + 2), (DWORD) (args + 3)) : 0;
            ASSERT(res1);
                
            fp = WOWGetProcAddress32(ghLib, "GetDiskFreeSpaceExA");        
            if (fp && WOWCallProc32(fp, 0x0F, 4,
                            (DWORD) path_, (DWORD) (args + 10),
                            (DWORD) (args + 12), (DWORD) (args + 14)))
            {    
                int sh = 0;
                for (DWORD dw = args[0] * args[1]; dw > 1; dw >>= 1)
                    ++sh;
                    
                args[2] = (args[14] >> sh) | (args[15] << (32-sh));
                args[3] = (args[12] >> sh) | (args[13] << (32-sh));
            }
                
            fp = WOWGetProcAddress32(ghLib, "GetDriveTypeA");          
            args[4] = fp ? WOWCallProc32(fp, 0x01, 1,
                                (DWORD) path_) : 0;
                
            fp = WOWGetProcAddress32(ghLib, "GetVolumeInformationA");          
            DWORD res2 = fp ? WOWCallProc32(fp, 0xDE, 8, // reversed, was 0x7B (!)
                            (DWORD) path_,
                            (DWORD) buf1, (DWORD) sizeof buf1,
                            (DWORD) (args + 5), (DWORD) (args + 6), (DWORD) (args + 7),
                            (DWORD) buf2, (DWORD) sizeof buf2) : 0;
            ASSERT(res2);
            
            WOWFreeLibrary32(ghLib);
        }
        else
        {
            unsigned drive = s[0] & 0x1F;
        
            struct _diskfree_t df;
            if (_dos_getdiskfree(drive, &df) == 0)
            {
                args[0] = df.sectors_per_cluster;
                args[1] = df.bytes_per_sector;
                args[2] = df.avail_clusters;
                args[3] = df.total_clusters;
            }
                
            args[4] = GetDriveTypeEx(drive - 1);
            args[5] = GetVolumeInfo(drive, buf1, buf2);
            args[6] = 12; // 8.3 names
            args[7] = 0; // ? (this also flags the catalog as made in Win16)
            
                // don't trust volume label, use findfirst _A_VOLID if possible
                // on CD-ROMs and WFW, GetVolumeInfo returns garbage (why?)
            _find_t fd;
            if (_dos_findfirst(s + "*.*", _A_VOLID, &fd) == 0)
            {
                CString t = fd.name;
                int i = t.Find('.');
                if (i >= 0)
                    t = t.Left(i) + t.Mid(i+1);
                strcpy(buf1, t);
            }
            
                // same for fat type on Win16 (just to be safe...)
//          if (strncmp(buf2, "FAT", 3) != 0)
//              strcpy(buf2, df.total_clusters >> 12 ? "FAT16" : "FAT12");
        }
    }
    
    info.Add(pNumInfo [args[0]] + pStrInfo [s]);
    info.Add(pNumInfo [args[1]] + pStrInfo [buf1]);
    info.Add(pNumInfo [args[2]] + pStrInfo [buf2]);
    info.Add(pNumInfo [args[3]]);
    info.Add(pNumInfo [args[4]]);
    info.Add(pNumInfo [args[5]]);
    info.Add(pNumInfo [args[6]]);
    info.Add(pNumInfo [args[7]]);
    
    return info;    
}

/////////////////////////////////////////////////////////////////////////////
