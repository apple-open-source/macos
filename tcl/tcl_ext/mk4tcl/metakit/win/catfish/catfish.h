//  catfish.h  -  main application sample code
//
//  Copyright (C) 1996-2000 Jean-Claude Wippler.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __AFXWIN_H__
    #error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols
#include "finddlg.h"
#include "util.h"

#define FILE_TYPE   ".cf4"

/////////////////////////////////////////////////////////////////////////////

	class DirScanner;
	class CSetupDialog;

	extern c4_IntProp    NEAR pNumV;
	extern c4_StringProp NEAR pNamV;
	
/////////////////////////////////////////////////////////////////////////////
// The main application class

class CTheApp : public CWinApp
{
public:
    CTheApp ();
    
    virtual BOOL InitInstance();
    virtual BOOL ProcessMessageFilter(int code, LPMSG lpMsg);
};

/////////////////////////////////////////////////////////////////////////////
// The main dialog window class

class CMainDlgWindow : public CDialog
{
private:
        // these are set during listbox owner-draw
    DRAWITEMSTRUCT* m_item;
    CDC* m_dc;
    
        // set to the current catalog
    c4_Storage* m_storage;
    c4_View m_currCat;
    CString m_currCatName;
    CString m_currLabel;
    
    	// preference file
    Preferences m_prefs;
    Preferences::Int_t _prefDays;
    Preferences::Int_t _prefFirstDay;
    Preferences::Int_t _prefLastDay;
    Preferences::Int_t _prefCheck;
    Preferences::Int_t _prefRuns;
    c4_View m_cats;
        
		// size of initial window layout
    CSize m_baseSize;
    int m_nameWidth, m_sizeDiff, m_dateDiff;
    
    	// used to draw the tree icons
    CDC m_memDC;
    CBitmap m_icons;
    
        // the directory currently shown in the tree list
    int m_treeDir;
    
        // the directory currently shown in the file list
    int m_fileDir;
    c4_View m_fileView;
    c4_View m_fileSort;

        // cumulative totals, one entry for each directory in the catalog
	c4_View m_stats;
    
        // the find dialog is a member so it can remember the current settings
    CFindDialog m_findDlg;
    
        // sort order of the file list
    c4_Property* m_sortProp;
    bool m_sortReverse;
    
        // true if the application should start with a find window
    bool m_startFind;
    
    	// the scanner, either a Win16 version or a WOW version
    DirScanner* _scanner;
    
	void GetStats(const c4_RowRef& arg);
    void ListAllCatalogs(bool initing_);
    void ConstructTitle();

    void SetCatalog(const char* catName);
    void SetTreeDir(int dirNum);           
    void SetFileDir(int dirNum);           
    
    void DrawItemText(const CString& text, int off =0);
    void OnDrawCatItem(int n);
    void OnDrawDirItem(int n);
    void OnDrawFileItem(int n);

    void AdjustDisplay(CCmdUI&, int, c4_Property&, int, const char*);
    void SortFileList(c4_Property& prop_, bool toggle_ =false);
    
    void DoSetup(bool now);
	void AdjustWindowSizes();
	
public:
    CMainDlgWindow ();
    ~CMainDlgWindow ();
    
    int Execute(bool find);
	CString GetCatalogDate(CString& catName);

    //{{AFX_DATA(CMainDlgWindow)
	enum { IDD = IDD_MAIN_DIALOG };
	CBitmapButton	m_logoBtn;
    CListBox    m_treeList;
    CListBox    m_fileList;
    CListBox    m_catList;
    CStatic m_msgText;
    CStatic m_infoText;
	//}}AFX_DATA

        // a small font used for several dialog box items
    CFont m_font;
    CFont m_fontBold;
      
    //{{AFX_VIRTUAL(CMainDlgWindow)
    virtual void DoDataExchange(CDataExchange* pDX);
    //}}AFX_VIRTUAL

    virtual void OnCancel();

protected:
    //{{AFX_MSG(CMainDlgWindow)
    virtual BOOL OnInitDialog();
    afx_msg void OnClose();
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    afx_msg void OnSelchangeCatList();
    afx_msg void OnSelchangeTreeList();
    afx_msg void OnDblclkTreeList();
    afx_msg void OnSelchangeFileList();
    afx_msg void OnFindBtn();
    afx_msg void OnSetupBtn();
    afx_msg void OnDblclkFileList();
    afx_msg void OnFindNext();
    afx_msg void OnFindPrev();
    afx_msg void OnSortByName();
    afx_msg void OnSortBySize();
    afx_msg void OnSortByDate();
    afx_msg void OnSortReverse();
    afx_msg void OnDestroy();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnAppAbout();
    afx_msg void OnFileExport();
	afx_msg void OnFileRefresh();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI);
	afx_msg int OnCharToItem(UINT nChar, CListBox* pListBox, UINT nIndex);
	afx_msg int OnVKeyToItem(UINT nKey, CListBox* pListBox, UINT nIndex);
	afx_msg void OnLogoBtn();
	afx_msg void OnFileDelete();
	afx_msg void OnFileRename();
	//}}AFX_MSG
    afx_msg void OnHelp();
    DECLARE_MESSAGE_MAP()
    
    friend class CFindState;
    friend class CSetupDialog;
};

/////////////////////////////////////////////////////////////////////////////
