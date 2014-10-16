// ======================================================================
//	File:		Radar.h
//
//	Repository of test cases which are entered into Radar.  Use them to
//	reproduce and regress Radar bugs.
//
//	Copyright:	Copyright (c) 2000,2003,2008 Apple Inc. All Rights Reserved.
//
//	Change History (most recent first):
//
//		 <1>	4/12/00	em		Created.
// ======================================================================

#ifndef __RADAR__
#define	__RADAR__

void	Radar_2456779(CTestApp *inClient);
void	Radar_2458217(CTestApp *inClient);
void	Radar_2458257(CTestApp *inClient);
void	Radar_2458503(CTestApp *inClient);
void	Radar_2458613(CTestApp *inClient);
void	Radar_2459096(CTestApp *inClient);
void	Radar_2462081(CTestApp *inClient);
void	Radar_2462265(CTestApp *inClient);
void	Radar_2462300(CTestApp *inClient);

typedef	void	 (*tRadarTestCaseFunc)(CTestApp *inClient);
typedef struct tRadarBug{
    tRadarTestCaseFunc	testCaseFunc;
    char *				ID;
    char *				desc;
} tRadarBug;


static tRadarBug	gRadarBugs[] ={
    {Radar_2456779, "2456779", "Wrong error number when creating duplicate keychain"},
    {Radar_2458217, "2458217", "GetData() causes bus error"},
    {Radar_2458257, "2458257", "GetKeychainManagerVersion() returns a wrong version"},
    {Radar_2458503, "2458503", "KCAddItem() fails to detect duplicate items"},
    {Radar_2458613, "2458613", "FindFirstItem returns an item from an empty keychain"},
    {Radar_2459096, "2459096", "InvalidItemRef error when deleting an item not previously added to keychain"},
    {Radar_2462081, "2462081", "AddAppleSharePassword returns DL_INVALID_FIELD_NAME"},
    {Radar_2462265, "2462265", "GetDataUI does not set ActualLength"},
    {Radar_2462300, "2462300", "No dialog for KCChangeSettings"},

    {0}};

#endif	// __RADAR__
