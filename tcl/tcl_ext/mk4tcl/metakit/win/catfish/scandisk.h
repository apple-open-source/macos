//  scandisk.h  -  recursive directory scanner sample code
//
//  Copyright (C) 1996-2000 Jean-Claude Wippler.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "util.h"

	// The following properties are used in this code
extern c4_ViewProp		NEAR pFiles;
extern c4_IntProp		NEAR pParent;
extern c4_IntProp		NEAR pSize;
extern c4_IntProp		NEAR pDate;
extern c4_StringProp	NEAR pName;

	// Derive from this mixin class to handle status updates and idle time
class cStatusHandler
{
public:
	virtual bool UpdateStatus(const char* text);
};
	
	// Scan a directory tree and fill in a corresponding structure for it.
	// The data structure of the object set up by this routine is:
	//		[parent:i, name:s, files [name:s, size:i, date:i]]
extern bool fScanDirectories(const char* path_, c4_View, cStatusHandler* =0);

extern c4_View fDiskInfo(const char* path_);

	// Reconstruct the full path name from a subdirectory index in the tree
extern CString fFullPath(c4_View& dirs_, int dirNum_, bool relative_ =false);

/////////////////////////////////////////////////////////////////////////////

class DirScanner
{
public:
	enum { kInfoNumCount = 8, kInfoStrCount = 3 };
	
	struct VolInfo
	{     
	    t4_u32 _secPerCluster;
	    t4_u32 _bytesPerSec;
	    t4_u32 _availClusters;
	    t4_u32 _totalClusters;
		t4_u32 _driveType;
		t4_u32 _serialNum;
		t4_u32 _maxCompLen;
		t4_u32 _fsFlags;
		
		c4_String _rootPath;
		c4_String _fsType;
		c4_String _volName;
	};

	static bool CanUseLongNames();
	static bool Move(const c4_String& from_, const c4_String& to_);

	static DirScanner* CreateFILE();
//	static DirScanner* CreateFTP();
	
	virtual ~DirScanner ();
	
	c4_String Info(const c4_String& path_, VolInfo* info_ =0);
	
	bool Scan(c4_View& dirs_, int dirNum_, bool withTime_ =false);
	
	virtual c4_String ShortName(const c4_String& fileName_) const;

protected:
	c4_View _files, _subDirs;
	
	DirScanner ();

	static c4_String NameCase(const c4_String& name_);
	
	void NewFile(const c4_String& nm_, long sz_, long dt_);
	void NewSubDir(const c4_String& nm_);
			  
	virtual bool DoInfo(VolInfo& info_) =0;
	virtual bool DoScan(const c4_String& path_, bool withTime_) =0;
	virtual bool DoMove(const c4_String& from_, const c4_String& to_) const =0;
};
	  
/////////////////////////////////////////////////////////////////////////////
