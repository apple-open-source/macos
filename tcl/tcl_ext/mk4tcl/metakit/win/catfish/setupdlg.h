//  setupdlg.h  -  setup dialog sample code
//
//  Copyright (C) 1996-2000 Jean-Claude Wippler.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "scandisk.h"

class CMainDlgWindow;

/////////////////////////////////////////////////////////////////////////////
// CSetupDialog dialog

class CSetupDialog : public CDialog, public cStatusHandler
{
// Construction
public:
    CSetupDialog(CMainDlgWindow* pParent); // standard constructor
    
    void Execute(BOOL now);
    
// Dialog Data
    //{{AFX_DATA(CSetupDialog)
	enum { IDD = IDD_SETUP_DIALOG };
	CComboBox	m_drives;
	CStatic	m_volSerial;
	CStatic	m_volName;
    CButton m_okBtn;
    CButton m_browseBtn;
    CButton m_addBtn;
    CStatic m_status;
    CStatic m_root;
	CString	m_name;
	//}}AFX_DATA

private:
    CString m_origRoot;
    BOOL m_exists;
    int m_timer;
    BOOL m_allowScan;
    CMyEdit m_nameEditCtrl;
    bool m_scanNow;
    CMainDlgWindow* m_parent;
    
// Implementation
protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual void OnCancel();

    virtual bool UpdateStatus(const char* text);
    
    void NameChange(BOOL delay);
    void ScanDirectories();
    void InspectCatalog();
	void AdjustRoot();
    void AdjustButtons(BOOL inScan_ =FALSE);
    void SetDrive(int index_);
    
    // Generated message map functions
    //{{AFX_MSG(CSetupDialog)
    afx_msg void OnAddBtn();
    virtual BOOL OnInitDialog();
    afx_msg void OnBrowseBtn();
    afx_msg void OnChangeName();
    afx_msg void OnTimer(UINT nIDEvent);
    afx_msg void OnClose();
	afx_msg void OnSelchangeDrives();
	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
