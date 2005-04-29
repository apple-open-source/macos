//  finddlg.h  -  find dialog sample code
//
//  Copyright (C) 1996-2000 Jean-Claude Wippler.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "util.h"

/////////////////////////////////////////////////////////////////////////////
// CFindDialog dialog

class CFindDialog : public CDialog
{
// Construction
public:
    CFindDialog(CWnd* pParent = NULL);  // standard constructor

    bool Execute(const char* name);
    
// Dialog Data
    //{{AFX_DATA(CFindDialog)
    enum { IDD = IDD_FIND_DIALOG };
    CString m_maxDate;
    CString m_minDate;
    CString m_nameEdit;
    BOOL    m_singleCat;
    CString m_minSize;
    CString m_maxSize;
    //}}AFX_DATA
    
    bool NeedsCompare() const;
    bool Match(const c4_RowRef& row_) const;
    bool MatchDir(const c4_RowRef& row_) const;
    
// Implementation
private:
    CMyEdit m_nameEditCtrl;
    CString m_currCatName;

    bool m_checkName, m_checkDate, m_checkSize;
    
    CString m_upName;
    int m_loDate;
    int m_hiDate;
    long m_loSize;
    long m_hiSize;
    
    void FixCriteria();
    
protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

    // Generated message map functions
    //{{AFX_MSG(CFindDialog)
    virtual BOOL OnInitDialog();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
