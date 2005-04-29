// convdlg.cpp : implementation file
//

#include "stdafx.h"
#include "catfish.h"
#include "convdlg.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CConvDlg dialog


CConvDlg::CConvDlg (CWnd* pParent /*=NULL*/)
{
	Create(CConvDlg::IDD, pParent);
	
	//{{AFX_DATA_INIT(CConvDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CConvDlg::~CConvDlg ()
{
	VERIFY(DestroyWindow());
}

void CConvDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConvDlg)
	DDX_Control(pDX, IDC_CURR_CAT, m_currCat);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CConvDlg, CDialog)
	//{{AFX_MSG_MAP(CConvDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CConvDlg message handlers

BOOL CConvDlg::OnInitDialog()
{
    CenterWindow();
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}
