// rnamedlg.cpp : implementation file
//

#include "stdafx.h"
#include "catfish.h"
#include "rnamedlg.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRenameDlg dialog


CRenameDlg::CRenameDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CRenameDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CRenameDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

void CRenameDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRenameDlg)
	DDX_Control(pDX, IDOK, m_okBtn);
	DDX_Control(pDX, IDC_NEW_NAME_EDIT, m_newNameEdit);
	DDX_Control(pDX, IDC_OLD_NAME, m_oldName);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CRenameDlg, CDialog)
	//{{AFX_MSG_MAP(CRenameDlg)
	ON_EN_CHANGE(IDC_NEW_NAME_EDIT, OnChangeNewNameEdit)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CRenameDlg message handlers

void CRenameDlg::OnChangeNewNameEdit()
{
	c4_String s;
	m_newNameEdit.GetWindowText(s);
	
	m_okBtn.EnableWindow(!s.IsEmpty() && s.CompareNoCase(_name) != 0);
}

BOOL CRenameDlg::OnInitDialog()
{
    CenterWindow();
	CDialog::OnInitDialog();

	m_oldName.SetWindowText("Rename catalog '" + _name + "' to:");
	
	m_newNameEdit.SetWindowText(_name);
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CRenameDlg::OnOK()
{
	m_newNameEdit.GetWindowText(_name);
	
	CDialog::OnOK();
}
