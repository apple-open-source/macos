// testw32Dlg.h : header file
//

#if !defined(AFX_TESTW32DLG_H__70F52CAB_06A4_11D2_9AC4_0060978849F3__INCLUDED_)
#define AFX_TESTW32DLG_H__70F52CAB_06A4_11D2_9AC4_0060978849F3__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

/////////////////////////////////////////////////////////////////////////////
// CTestw32Dlg dialog

class CTestw32Dlg : public CDialog
{
// Construction
public:
	CTestw32Dlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CTestw32Dlg)
	enum { IDD = IDD_TESTW32_DIALOG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTestw32Dlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CTestw32Dlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	virtual void OnOK();
	afx_msg void OnSelectAction();
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TESTW32DLG_H__70F52CAB_06A4_11D2_9AC4_0060978849F3__INCLUDED_)
