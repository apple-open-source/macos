/*********************************************************/
/* Test program driver for freetype  on Win32 Platform   */
/* CopyRight(left) G. Ramat 1998 (gcramat@radiostudio.it)*/
/*                                                       */
/*********************************************************/

// testw32.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "testw32.h"
#include "testw32dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTestw32App

BEGIN_MESSAGE_MAP(CTestw32App, CWinApp)
	//{{AFX_MSG_MAP(CTestw32App)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTestw32App construction

CTestw32App::CTestw32App()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CTestw32App object

CTestw32App theApp;

/////////////////////////////////////////////////////////////////////////////
// CTestw32App initialization

BOOL CTestw32App::InitInstance()
{
	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

	CTestw32Dlg dlg;
	m_pMainWnd = &dlg;
	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

BOOL CTestw32App::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CWinApp::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}
