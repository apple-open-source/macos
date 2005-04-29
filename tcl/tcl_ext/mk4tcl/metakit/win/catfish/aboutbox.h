// aboutbox.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CStaticURL class

class CStaticURL : public CStatic
{
// Construction
public:
	CStaticURL();

// Attributes
public:
    CFont* m_stdFont;	// standard
    CFont* m_urlFont;	// underlined
    CString m_url;
    HCURSOR m_urlCursor;

// Operations
public:        
	void SetupFonts(CFont* std, CFont* url);
	void SetURL(const char* name, const char* url ="");

// Implementation
protected:
	virtual BOOL OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam,
					LRESULT* pLResult);

	// Generated message map functions
	//{{AFX_MSG(CStaticURL)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CAboutBox dialog

class CAboutBox : public CDialog
{
// Construction
public:
	CAboutBox(CMainDlgWindow* pParent);

// Dialog Data
	//{{AFX_DATA(CAboutBox)
	enum { IDD = IDD_ABOUTBOX };
	CStaticURL	m_email;
	CStaticURL	m_url;
	//}}AFX_DATA

    CMainDlgWindow* m_parent;

// Implementation
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	// Generated message map functions
	//{{AFX_MSG(CAboutBox)
	virtual BOOL OnInitDialog();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
