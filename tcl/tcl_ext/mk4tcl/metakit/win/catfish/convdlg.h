// convdlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CConvDlg dialog

class CConvDlg : public CDialog
{
// Construction
public:
	CConvDlg (CWnd* pParent = NULL);	// standard constructor
    ~CConvDlg ();
    
// Dialog Data
	//{{AFX_DATA(CConvDlg)
	enum { IDD = IDD_CONV_DLG };
	CStatic	m_currCat;
	//}}AFX_DATA

// Implementation
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	// Generated message map functions
	//{{AFX_MSG(CConvDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
