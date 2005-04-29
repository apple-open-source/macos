#ifndef __UTIL_H__
#define __UTIL_H__

#include "mk4.h"
#include "mk4str.h"

/////////////////////////////////////////////////////////////////////////////

extern void SetInternationalSettings();
extern c4_String CommaNum(DWORD num, int groups =0, BOOL zero =TRUE);
extern c4_String SizeString(DWORD kb_);
extern c4_String ShortDate(WORD date, bool display);
extern c4_String ShortTime(long time);

extern BOOL FitString(CDC* dc, CString& text, int width);
extern CSize GetSize(const CWnd& wnd_);
extern CPoint GetOrigin(const CWnd& wnd_);

/////////////////////////////////////////////////////////////////////////////
// Disables redraw and clears listbox, will reset normal state in destructor

class ListBoxFreezer
{
    CListBox& list;

public:
    ListBoxFreezer (CListBox& lb)
        : list (lb)
    {
        list.SetRedraw(FALSE);
        list.ResetContent();
    }
        
    ~ListBoxFreezer ()
    {
        list.SetRedraw(TRUE);
        list.Invalidate();
    }
};
    
/////////////////////////////////////////////////////////////////////////////
// This edit control catches some of the keys to avoid illegal filenames

class CMyEdit : public CEdit
{
public:
    CMyEdit (bool f_ =false) : acceptDot (f_) { }
    
protected:
    bool acceptDot;
    
    // Generated message map functions
    //{{AFX_MSG(CMyEdit)
    afx_msg void OnChar(UINT nChar, UINT nRep, UINT nFlags) ;
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// Manage preference storage

class Preferences
{
	c4_Storage _storage;
	c4_View _prefs;
	
public:
	typedef c4_IntRef Int_t;
	typedef c4_StringRef Str_t;
	
    Preferences (const c4_String& fileName_);
    ~Preferences ();
    
    void Flush();
    
    c4_IntRef Int(int id_);
    c4_StringRef Str(int id_);
    
    bool GetBin(int id_, void* ptr_, int len_);
    void SetBin(int id_, const void* ptr_, int len_);
    
    c4_View GetView(const char* name_);
};
    
/////////////////////////////////////////////////////////////////////////////

#endif // __UTIL_H__
