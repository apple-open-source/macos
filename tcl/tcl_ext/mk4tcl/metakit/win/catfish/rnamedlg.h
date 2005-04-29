// rnamedlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CRenameDlg dialog

class CRenameDlg : public CDialog
{
// Construction
public:
	CRenameDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CRenameDlg)
	enum { IDD = IDD_RENAME_DLG };
	CButton	m_okBtn;
	CEdit	m_newNameEdit;
	CStatic	m_oldName;
	//}}AFX_DATA
	
	c4_String _name;
	
// Implementation
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	// Generated message map functions
	//{{AFX_MSG(CRenameDlg)
	afx_msg void OnChangeNewNameEdit();
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
