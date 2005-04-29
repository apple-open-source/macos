//  finddlg.cpp  -  find dialog sample code
//
//  Copyright (C) 1996-2000 Jean-Claude Wippler.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "catfish.h"
#include "scandisk.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CFindDialog dialog

CFindDialog::CFindDialog(CWnd* pParent /*=NULL*/)
    : CDialog(CFindDialog::IDD, pParent), m_nameEditCtrl (true)
{
    //{{AFX_DATA_INIT(CFindDialog)
    m_maxDate = "";
    m_minDate = "";
    m_nameEdit = "";
    m_singleCat = FALSE;
    m_minSize = "";
    m_maxSize = "";
    //}}AFX_DATA_INIT

    FixCriteria(); 
}

void CFindDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CFindDialog)
    DDX_Text(pDX, IDC_MAX_DATE, m_maxDate);
    DDV_MaxChars(pDX, m_maxDate, 6);
    DDX_Text(pDX, IDC_MIN_DATE, m_minDate);
    DDV_MaxChars(pDX, m_minDate, 6);
    DDX_Text(pDX, IDC_NAME_EDIT, m_nameEdit);
    DDV_MaxChars(pDX, m_nameEdit, 15);
    DDX_Check(pDX, IDC_SINGLE_CAT, m_singleCat);
    DDX_Text(pDX, IDC_MIN_SIZE, m_minSize);
    DDV_MaxChars(pDX, m_minSize, 6);
    DDX_Text(pDX, IDC_MAX_SIZE, m_maxSize);
    DDV_MaxChars(pDX, m_maxSize, 6);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CFindDialog, CDialog)
    //{{AFX_MSG_MAP(CFindDialog)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CFindDialog message handlers

BOOL CFindDialog::OnInitDialog()
{
    CenterWindow();
    CDialog::OnInitDialog();
    
    m_nameEditCtrl.SubclassDlgItem(IDC_NAME_EDIT, this);
    
    CWnd* wnd = GetDlgItem(IDC_SINGLE_CAT);
    ASSERT(wnd);
    
    wnd->SetWindowText("&Only search in '" + m_currCatName + "'");
    
    FixCriteria(); 
    return TRUE;  // return TRUE  unless you set the focus to a control
}

bool CFindDialog::Execute(const char* name)
{
    m_currCatName = name;
    
    if (DoModal() != IDOK)
        return false;
        
    FixCriteria();
    return true;    
}

    static int ConvDate(const CString& s_, WORD default_ =0)
    {
        if (s_.GetLength() == 6)
        {
            int y = atoi(s_.Left(2)) - 80;
            int m = atoi(s_.Mid(2, 2));
            int d = atoi(s_.Right(2));
            
            if (y < -40)    // accepts 1940 .. 2039
                y += 100;
                
            if (1 <= m && m <= 12 && 1 <= d && d <= 31)
                return (y << 9) | (m << 5) | d;
        }
        
        return default_;
    }
    
void CFindDialog::FixCriteria()
{
    m_upName = m_nameEdit;
    m_upName.MakeUpper(); 
     
        // if there are no wildcards, then add them at both ends (unanchored)
    if (m_upName.SpanExcluding("?*") == m_upName)
        m_upName = "*" + m_upName + "*";
        
    m_loDate = ConvDate(m_minDate);
    m_hiDate = ConvDate(m_maxDate, 0x7FFF);
    
    m_loSize = atol(m_minSize) * 1000;
    m_hiSize = m_maxSize.IsEmpty() ? 0x7FFFFFFFL
                                   : atol(m_maxSize) * 1000 + 999;

        // by avoiding all unnecessary access we also avoid on-demand loading
    m_checkName = !m_nameEdit.IsEmpty();
    m_checkDate = !m_minDate.IsEmpty() || !m_maxDate.IsEmpty();
    m_checkSize = !m_minSize.IsEmpty() || !m_maxSize.IsEmpty();
}

bool CFindDialog::NeedsCompare() const
{
    return m_checkName || m_checkDate || m_checkSize;
}

        // a very simple wildcard matching routine, does just '*' and '?'
    static bool fWildMatch(const char* crit, const char* val)
    {
        for (;;)
        {                
            switch (*crit)
            {                          
                default:    if (*crit != *val)  // if different chars
                                return false;
                            if (!*crit)         // if equal and both null
                                return true;
                            // else fall through
                            
                case '?':   if (!*val)          // if at end
                                return false;

                            ++crit;
                            ++val;
                            break;              // ok, both advanced one

                case '*':   ++crit;
                            
                            for (;;)
                                if (fWildMatch(crit, val))  // if rest matches
                                    return true;
                                else if (!*val)             // if nothing left
                                    return false;   
                                else
                                    ++val;      // try again one char further
            }
        }
    }
    
bool CFindDialog::Match(const c4_RowRef& row_) const
{
    if (m_checkName)
    {
        CString s = pName (row_);
        s.MakeUpper(); // compare in uppercase to avoid case sensitivity
        
//      if (s.Find(m_upName) < 0)
        if (!fWildMatch(m_upName, s))
            return false;
    }
                        
    if (m_checkDate)
    {
        int v = (int) pDate (row_);
        if (v < m_loDate || v > m_hiDate)
            return false;
    }
                        
    if (m_checkSize)
    {
        long v = pSize (row_);
        if (v < m_loSize || v > m_hiSize)
            return false;
    } 
    
    return true;
}

bool CFindDialog::MatchDir(const c4_RowRef& row_) const
{
    if (m_checkName)
    {
        CString s = pName (row_);
        s.MakeUpper(); // compare in uppercase to avoid case sensitivity
        
        if (fWildMatch(m_upName, s))
            return true;
    }
    
    return false;
}

/////////////////////////////////////////////////////////////////////////////
