// testw32.h : main header file for the TESTW32 application
//
// $XFree86: xc/extras/FreeType/contrib/win32/testw32.h,v 1.2 2003/01/12 03:55:44 tsi Exp $

#if !defined(AFX_TESTW32_H__70F52CA9_06A4_11D2_9AC4_0060978849F3__INCLUDED_)
#define AFX_TESTW32_H__70F52CA9_06A4_11D2_9AC4_0060978849F3__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
#	error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CTestw32App:
// See testw32.cpp for the implementation of this class
//

class CTestw32App : public CWinApp
{
public:
	CTestw32App();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTestw32App)
	public:
	virtual BOOL InitInstance();
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CTestw32App)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TESTW32_H__70F52CA9_06A4_11D2_9AC4_0060978849F3__INCLUDED_)
