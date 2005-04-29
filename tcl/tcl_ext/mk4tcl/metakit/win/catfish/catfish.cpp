//  catfish.cpp  -  main application sample code
//
//  Copyright (C) 1996-2000 Jean-Claude Wippler.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "catfish.h"
#include "setupdlg.h"
#include "aboutbox.h"
#include "convdlg.h"
#include "rnamedlg.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#pragma warning(disable: 4702) // MSVC 1.52 gets confused: unreachable code
    
#define CF_URL	"http://purl.net/meta4/catfish"

/////////////////////////////////////////////////////////////////////////////
// MSDN Q100770: Using Accelerator Keys When Modal Dialog Box Main Window

    HWND    ghDlg;          // Handle to main dialog box
    HACCEL  ghAccelTable;   // Handle to accelerator table

    static CTheApp NEAR ThisApp;

/////////////////////////////////////////////////////////////////////////////

    static c4_IntProp    NEAR pFree ("free");
    static c4_IntProp    NEAR pNumD ("numd");
    static c4_IntProp    NEAR pNumF ("numf");
    static c4_IntProp    NEAR pKilo ("kilo");
    static c4_IntProp    NEAR pLast ("last");
    static c4_ViewProp   NEAR pStats ("stats");

	c4_IntProp    NEAR pNumV ("numv");
	c4_StringProp NEAR pNamV ("namv");

/////////////////////////////////////////////////////////////////////////////
// The one and only application object

CTheApp::CTheApp ()
    : CWinApp ("CatFish")
{    
}

BOOL CTheApp::InitInstance()
{
    SetDialogBkColor();
    SetInternationalSettings();

    ghAccelTable = LoadAccelerators(AfxGetInstanceHandle(),
                                    MAKEINTRESOURCE(IDD_MAIN_DIALOG));

        // the following is required to let a dialog box have an icon   
    static WNDCLASS NEAR wndclass;
    if (!wndclass.lpfnWndProc)
    {
        wndclass.lpfnWndProc    = DefDlgProc;
        wndclass.cbWndExtra     = DLGWINDOWEXTRA ;
        wndclass.hInstance      = m_hInstance;
        wndclass.hIcon          = LoadIcon(AFX_IDI_STD_FRAME);
        wndclass.lpszClassName  = "CATFISHCLASS";
        
        RegisterClass(&wndclass);
    }
    
        // enter a modal loop right now 
    CMainDlgWindow mainDlg;
    m_pMainWnd = &mainDlg;
    
    mainDlg.Execute(stricmp(m_lpCmdLine, "/f") == 0);
    
        // and then return false to skip the main application run loop
    return FALSE;
}

BOOL CTheApp::ProcessMessageFilter(int code, LPMSG lpMsg)
{
    if (code < 0)
        CWinApp::ProcessMessageFilter(code, lpMsg);
         
    if (ghDlg && ghAccelTable)
    {
        if (::TranslateAccelerator(ghDlg, ghAccelTable, lpMsg))
            return(TRUE);
    }
         
    return CWinApp::ProcessMessageFilter(code, lpMsg);
}

/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CMainDlgWindow, CDialog)
    //{{AFX_MSG_MAP(CMainDlgWindow)
    ON_WM_CLOSE()
    ON_WM_DRAWITEM()
    ON_LBN_SELCHANGE(IDC_CAT_LIST, OnSelchangeCatList)
    ON_LBN_SELCHANGE(IDC_TREE_LIST, OnSelchangeTreeList)
    ON_LBN_DBLCLK(IDC_TREE_LIST, OnDblclkTreeList)
    ON_LBN_SELCHANGE(IDC_FILE_LIST, OnSelchangeFileList)
    ON_COMMAND(ID_FIND_CMD, OnFindBtn)
    ON_COMMAND(ID_FILE_SETUP, OnSetupBtn)
    ON_LBN_DBLCLK(IDC_FILE_LIST, OnDblclkFileList)
    ON_COMMAND(ID_FIND_NEXT, OnFindNext)
    ON_COMMAND(ID_FIND_PREV, OnFindPrev)
    ON_COMMAND(ID_SORT_BY_NAME, OnSortByName)
    ON_COMMAND(ID_SORT_BY_SIZE, OnSortBySize)
    ON_COMMAND(ID_SORT_BY_DATE, OnSortByDate)
    ON_COMMAND(ID_SORT_REVERSE, OnSortReverse)
    ON_WM_DESTROY()
    ON_WM_LBUTTONDOWN()
    ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
    ON_COMMAND(ID_FILE_EXPORT, OnFileExport)
    ON_COMMAND(ID_FILE_REFRESH, OnFileRefresh)
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_WM_CHARTOITEM()
	ON_WM_VKEYTOITEM()
	ON_BN_CLICKED(IDC_LOGO_BTN, OnLogoBtn)
	ON_COMMAND(ID_FILE_DELETE, OnFileDelete)
    ON_LBN_DBLCLK(IDC_CAT_LIST, OnSetupBtn)
    ON_COMMAND(ID_EDIT_FIND, OnFindBtn)
	ON_BN_CLICKED(IDC_FIND_BTN, OnFindBtn)
	ON_BN_CLICKED(IDC_SETUP_BTN, OnSetupBtn)
	ON_COMMAND(ID_FILE_RENAME, OnFileRename)
	//}}AFX_MSG_MAP
    ON_COMMAND(ID_FILE_EXIT, OnClose)
    ON_COMMAND(ID_HELP, OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////

CMainDlgWindow::CMainDlgWindow ()
    : CDialog (IDD_MAIN_DIALOG),
      m_storage (0), m_fileDir (-1), m_treeDir (-1), m_dc (0),
      m_sortProp (&pName), m_sortReverse (false), m_startFind (false),
      m_prefs ("catfish.dat"),
	  _prefDays (m_prefs.Int(0)), 
	  _prefFirstDay (m_prefs.Int(1)), 
	  _prefLastDay (m_prefs.Int(2)), 
	  _prefCheck (m_prefs.Int(3)), 
	  _prefRuns (m_prefs.Int(4)) 
{
    //{{AFX_DATA_INIT(CMainDlgWindow)
	//}}AFX_DATA_INIT
    
	_scanner = DirScanner::CreateFILE();
	ASSERT(_scanner);
	
	m_cats = m_prefs.GetView("cats[name:S,size:I,free:I,date:I,namv:S,numv:I,"
									"numd:I,numf:I,kilo:I,last:I,"
									"stats[numd:I,numf:I,kilo:I,last:I]]");
	
	int day = (int) (time(0) / 86400L);
	
	if (_prefFirstDay == 0)				// this is the first time
	{
		_prefDays = 418265621;			// funny initial day count                            
		_prefFirstDay = day;            // first day started            
		_prefRuns = 736734923;   		// funny initial start count
	}
	
	if (_prefLastDay != day)			// # different days started
		_prefDays = _prefDays + 1;
	
	if (_prefLastDay < day)	
		_prefLastDay = day;       		// last day started
	
	_prefRuns = _prefRuns + 1;			// number of times started

	VERIFY(m_logoBtn.LoadBitmaps("LOGO"));
}

CMainDlgWindow::~CMainDlgWindow ()
{
	_prefCheck = (((_prefRuns * 211 + _prefDays) * 211
					+ _prefLastDay) * 211 + _prefFirstDay) * 211;

    delete m_storage;
    delete _scanner;
}

int CMainDlgWindow::Execute(bool find)
{                       
    m_startFind = find;
    
    return DoModal();
}

CString CMainDlgWindow::GetCatalogDate(CString& catName)
{
	int n = m_cats.Find(pName [catName]);
	if (n < 0)
		return "";
	
		// adjust case
	catName = pName (m_cats[n]);
	
	long date = pDate (m_cats[n]);
    return ShortDate((WORD) (date / 86400L), false) + "  " + ShortTime(date);
}

void CMainDlgWindow::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CMainDlgWindow)
	DDX_Control(pDX, IDC_LOGO_BTN, m_logoBtn);
    DDX_Control(pDX, IDC_TREE_LIST, m_treeList);
    DDX_Control(pDX, IDC_FILE_LIST, m_fileList);
    DDX_Control(pDX, IDC_CAT_LIST, m_catList);
    DDX_Control(pDX, IDC_MSG_TEXT, m_msgText);
    DDX_Control(pDX, IDC_INFO_TEXT, m_infoText);
	//}}AFX_DATA_MAP
}

void CMainDlgWindow::OnCancel()
{
    ::MessageBeep(0);                  // don't go away on ESC key
}

void CMainDlgWindow::OnClose()
{
    EndDialog(IDOK);
}

void CMainDlgWindow::OnDestroy()
{
    WINDOWPLACEMENT wp;
    wp.length = sizeof wp;
    
		// save window position and size	
    if (GetWindowPlacement(&wp))
    {
    	wp.showCmd = SW_SHOWNORMAL;
		c4_BytesProp pBin ("b");
        m_prefs.SetBin(0, &wp, sizeof wp);
    }

    CDialog::OnDestroy();
    
    SetCatalog("");
}

void CMainDlgWindow::GetStats(const c4_RowRef& arg)
{
	CString catFile = pName (arg);
	catFile += FILE_TYPE;
	
	ASSERT(_scanner);
	c4_Storage storage (_scanner->ShortName(catFile), false);

	c4_View info = storage.View("info");
	if (info.GetSize() >= 5)
	{
		c4_String root;
		long free = 0, size = 0;
    		
    	c4_IntProp pInt ("i");
    	long n = pInt (info[0]);
    	n = (n * pInt (info[1])) / 512;			// n = clustersize / 512b
    	free = (n * pInt (info[2]) + 1) / 2; // [2] = free Kb
    	size = (n * pInt (info[3]) + 1) / 2; // [3] = capacity Kb
	    	
    		// if the type is CD, 1 sec per cluster, size is 65535 clusters:
    		//	 clear size, this is a Win16 CD which always shows 128 Mb
    	if (pInt (info[4]) == 5 &&
    			pInt (info[0]) == 1 && pInt (info[3]) == 65535)
    		size = 0;
	    		
    	c4_View dirs = storage.View("dirs");
    	if (dirs.GetSize() > 0)
    	{
    		root = pName (dirs[0]);
    		if (root.GetLength() <= 0 || root.Mid(1) != ":")
    			size = 0; // don't show capacity on partial catalogs
    	}
	    
	    pNamV (arg) = root;
	    pNumV (arg) = pInt (info[5]);	
    	pFree (arg) = free;
    	pSize (arg) = size;
	}

	c4_View dirs = storage.View("dirs");
    int n = dirs.GetSize();

	c4_View stats;
    stats.SetSize(n);
	
        // this loop calculates all cumulative totals and dates,
        // mathematicians call this the "transitive closure" ...
    while (--n >= 0)
    {
        c4_RowRef dir = dirs[n];
        c4_RowRef row = stats[n];
	        
        int date = 0;
        DWORD total = 0;
	
        c4_View files = pFiles (dir);
	        
        for (int i = 0; i < files.GetSize(); ++i)
        {
            c4_RowRef file = files[i];
	            
            total += pSize (file);
            if (date < pDate (file))
                date = (int) pDate (file);
        }
	        
        ASSERT(i == files.GetSize());
        pNumF (row) = pNumF (row) + i;
        pKilo (row) = pKilo (row) + (total + 1023) / 1024;
	        
        if (pLast (row) < date)
            pLast (row) = date;
	        
        c4_RowRef par = stats[pParent (dir)];
        if (&par != &row)
        {
            pNumD (par) = pNumD (par) + pNumD (row) + 1;
            pNumF (par) = pNumF (par) + pNumF (row);
            pKilo (par) = pKilo (par) + pKilo (row);    
	    
            if (pLast (par) < pLast (row))
                pLast (par) = pLast (row);
        }
    }
	    
    pStats (arg) = stats;
    
    if (stats.GetSize() > 0)
    {
    	pNumD (arg) = pNumD (stats[0]) + 1; // count root as well
    	pNumF (arg) = pNumF (stats[0]);
    	pKilo (arg) = pKilo (stats[0]);
    	pLast (arg) = pLast (stats[0]);
    }	
}
	
void CMainDlgWindow::ListAllCatalogs(bool initing_)
{                       
	c4_View catDirs;
	catDirs.Add(pName ["."]);
	
	VERIFY(_scanner->Scan(catDirs, 0, true));
	
	c4_View catFiles = pFiles (catDirs[0]);                     
		
	int i = 0;
	m_catList.ResetContent();
		
	CConvDlg convDlg;
			
	for (int j = 0; j < catFiles.GetSize(); ++j)
	{   
		c4_String s = pName (catFiles[j]);
		if (s.Right(4).CompareNoCase(FILE_TYPE) != 0)
			continue;
		s = s.Left(s.GetLength() - 4);

		while (i < m_cats.GetSize() && pName (m_cats[i]) < s)
			m_cats.RemoveAt(i);
				
		long date = pDate (catFiles[j]);
		c4_RowRef r = m_cats[i];
				
		if (i >= m_cats.GetSize() || pName (r) != s)
			m_cats.InsertAt(i, pName [s]);
		
		c4_View stats = pStats (r);
		if (pDate (r) != date || stats.GetSize() == 0 || pNumD (r) == 0)
		{
			pDate (r) = date;
			
			UpdateWindow(); // adjusts catalog list contents
			
			convDlg.m_currCat.SetWindowText("Examining: " + s);
			
			if (initing_)
			{
				initing_ = false;
				convDlg.ShowWindow(SW_SHOW);
    			convDlg.UpdateWindow(); 
			}
			
			GetStats(r);
	
				// force changes, including new stats subview, to file
			m_prefs.Flush();
		}
				
		++i;

		m_catList.AddString("");
	}
		
	if (i < m_cats.GetSize())
		m_cats.RemoveAt(i, m_cats.GetSize() - i);
}

BOOL CMainDlgWindow::OnInitDialog()
{
		// bitmap button subclassing
	VERIFY(m_logoBtn.SubclassDlgItem(IDC_LOGO_BTN, this));

    CDialog::OnInitDialog();

    ghDlg = m_hWnd;
    
        // create a small font for several of the dialog box items
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfHeight = -8;
    strcpy(lf.lfFaceName, "MS Sans Serif");
    m_font.CreateFontIndirect(&lf);
    
    	// the bold font is used for entries with notes attached to them
    lf.lfWeight = 700;
    m_fontBold.CreateFontIndirect(&lf);
    
    m_msgText.SetFont(&m_font, FALSE);
    m_infoText.SetFont(&m_font, FALSE);
    m_catList.SetFont(&m_font, FALSE);
    m_treeList.SetFont(&m_font, FALSE);
    m_fileList.SetFont(&m_font, FALSE);
    
        // determine the character height and set owner-draw lists accordingly
    {
        CClientDC dc (this);
        CFont* oldFont = dc.SelectObject(&m_font);
            
        TEXTMETRIC tm;
        VERIFY(dc.GetTextMetrics(&tm));
               
        dc.SelectObject(oldFont);
            
        m_catList.SetItemHeight(0, tm.tmHeight);
        m_treeList.SetItemHeight(0, tm.tmHeight);
        m_fileList.SetItemHeight(0, tm.tmHeight);
    }
	
        // default file sort order is by filename
    SortFileList(pName);
    
    	// resizing madness
    m_baseSize = GetSize(*this);
    m_nameWidth = 85;
    m_dateDiff = GetOrigin(*GetDlgItem(IDC_DATE_LABEL)).x
    			- GetOrigin(*GetDlgItem(IDC_NAME_LABEL)).x;
    m_sizeDiff = GetOrigin(*GetDlgItem(IDC_SIZE_LABEL)).x
    			- GetOrigin(*GetDlgItem(IDC_DATE_LABEL)).x;
    
		// restore saved window position and size
	WINDOWPLACEMENT wp;
	if (m_prefs.GetBin(0, &wp, sizeof wp))
	{
		CRect r2 = wp.rcNormalPosition;
		
			// only restore window position if it is fully on screen
		CRect r;
		GetDesktopWindow()->GetWindowRect(&r);
		if (r.PtInRect(r2.TopLeft()) && r.PtInRect(r2.BottomRight()))
		{
			VERIFY(SetWindowPlacement(&wp));
			AdjustWindowSizes();
		}
	}
    
    	// tree icon setup
    VERIFY(m_icons.LoadBitmap(IDB_TREE_ICONS));
    VERIFY(m_memDC.CreateCompatibleDC(0));
    VERIFY(m_memDC.SelectObject(&m_icons));
    
        // show contents now, before potentially slow calls to GetStats
    ShowWindow(ThisApp.m_nCmdShow);
    UpdateWindow(); 
    
    	// use the new scanner so we can see long catalog file names
    ListAllCatalogs(true);

	SetCursor(LoadCursor(0, IDC_ARROW));    
	
//    m_catList.SetCurSel(0);
//    OnSelchangeCatList();
    
    long kilo = 0;
    long files = 0;
    
    for (int k = 0; k < m_cats.GetSize(); ++k)
    {
    	kilo += pKilo (m_cats[k]);
    	files += pNumF (m_cats[k]);
    }

	c4_String s = SizeString(kilo);
	while (s.Left(1) == " ")
		s = s.Mid(1);
	while (s.Right(1) == " ")
		s = s.Left(s.GetLength() - 1); 
	    
	c4_String t = CommaNum(files);
	    
	char buf [60];
	wsprintf(buf, "%d catalogs - %s in %s files",
					m_cats.GetSize(), (const char*) s, (const char*) t);
	
	m_infoText.SetWindowText(buf);
    
    if (m_cats.GetSize() == 0)
        OnHelp();
    
    if (m_startFind)
        OnFindBtn();
        
    return TRUE;    // return TRUE  unless you set the focus to a control
}

void CMainDlgWindow::OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI)
{
	lpMMI->ptMinTrackSize.x = 420;
	lpMMI->ptMinTrackSize.y = 340;
	
	CDialog::OnGetMinMaxInfo(lpMMI);
}

void CMainDlgWindow::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
	
	if (nType != SIZE_MINIMIZED)
		AdjustWindowSizes();
}

void CMainDlgWindow::AdjustWindowSizes()
{
    CSize newSz = GetSize(*this);
	CSize diff = newSz - m_baseSize;
	
	m_baseSize = newSz;
	m_nameWidth += diff.cx;
	
	CSize sz = GetSize(m_fileList) + diff;
	m_fileList.SetWindowPos(0, 0, 0, sz.cx, sz.cy,
		SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);
		
	sz = GetSize(m_treeList);
	sz.cy += diff.cy;
	m_treeList.SetWindowPos(0, 0, 0, sz.cx, sz.cy,
		SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);
		
	CPoint pt = GetOrigin(m_msgText);
	ScreenToClient(&pt);
	pt.y += diff.cy;
	m_msgText.SetWindowPos(0, pt.x, pt.y, 0, 0,
		SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);
		
	pt = GetOrigin(m_infoText);
	ScreenToClient(&pt);
	pt.y += diff.cy;
	m_infoText.SetWindowPos(0, pt.x, pt.y, 0, 0,
		SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);

	if (diff.cx != 0)
	{
		m_fileList.Invalidate();

	    int w = m_nameWidth;
	    if (w < 75)
	    	w += 65;
	    if (w < 70)
	    	w = 70;
			
		CWnd* w1 = GetDlgItem(IDC_NAME_LABEL);
		ASSERT(w1);		
		pt = GetOrigin(*w1);
		ScreenToClient(&pt);
			
		CWnd* wnd = GetDlgItem(IDC_DATE_LABEL);
		ASSERT(wnd);		

		pt.x += m_dateDiff + w - 85 - (w == m_nameWidth ? 0 : 65);
		wnd->SetWindowPos(0, pt.x, pt.y, 0, 0,
			SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);                                   
	
		wnd = GetDlgItem(IDC_SIZE_LABEL);
		ASSERT(wnd);		
			// make size invisible if the file list is too narrow
		wnd->ShowWindow(w != m_nameWidth ? SW_HIDE : SW_SHOW);

		pt.x += m_sizeDiff;
		wnd->SetWindowPos(0, pt.x, pt.y, 0, 0,
			SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);
	}
}

    // notification handler for owner-draw listboxes
void CMainDlgWindow::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (nIDCtl == IDC_LOGO_BTN)
	{
		CDialog::OnDrawItem(nIDCtl, lpDrawItemStruct);
		return;
	}
	
    m_item = lpDrawItemStruct;
    m_dc = CDC::FromHandle(m_item->hDC);
/*    
    char buf [100];
    wsprintf(buf, "%2d #%-2d [%03d,%03d,%03d,%03d]", (int) m_item->CtlID,
    				(int) m_item->itemID, m_item->rcItem);
    
    CString s = buf;
    if (m_item->itemAction & ODA_DRAWENTIRE) 	s += " aD";
    if (m_item->itemAction & ODA_FOCUS)			s += " aF";
    if (m_item->itemAction & ODA_SELECT)		s += " aS";
    if (m_item->itemState & ODS_FOCUS)			s += " sF";
    if (m_item->itemState & ODS_SELECTED)		s += " sS"; 
    
    TRACE("%s\n", (const char*) s);
*/
    int n = (int) m_item->itemID;
    if (n >= 0)
    {
	    bool highlight = (m_item->itemState & ODS_SELECTED) != 0;
	    bool focus = (m_item->itemState & ODS_FOCUS) != 0;
	    
	    if (!focus)
	    {
	    	focus = highlight;
	    	highlight = false; // turn off selection highlight if control lost focus
	    }
	    
	    if (highlight)
	    {
	        m_dc->SetBkColor(GetSysColor(COLOR_HIGHLIGHT));
	        m_dc->SetTextColor(GetSysColor(COLOR_HIGHLIGHTTEXT));
	    }
	    else
	    {
	        m_dc->SetBkColor(GetSysColor(COLOR_WINDOW));
	        m_dc->SetTextColor(GetSysColor(COLOR_WINDOWTEXT));
	    }
	
	    switch (nIDCtl)
	    {
	        case IDC_CAT_LIST:      OnDrawCatItem(n); break;
	        case IDC_TREE_LIST:     OnDrawDirItem(n); break;
	        case IDC_FILE_LIST:     OnDrawFileItem(n); break;
	    }

	    if (focus)
	        m_dc->DrawFocusRect(&m_item->rcItem);
    }
}

    // common code to draw a text string in a listbox item
void CMainDlgWindow::DrawItemText(const CString& text, int off)
{
    RECT& rect = m_item->rcItem;
    
    m_dc->ExtTextOut(rect.left + off + 2, rect.top,
                        off ? 0 : ETO_OPAQUE, &rect,
                        text, text.GetLength(), 0);
}

    // draw one item in the catalog listbox
void CMainDlgWindow::OnDrawCatItem(int n)
{
	c4_RowRef r = m_cats[n];
	
    CString s = pName (r);
    long size = pSize (r);
    long free = pFree (r);
    long date = pDate (r);

    FitString(m_dc, s, 85);
    DrawItemText(s);    
    
    s = (size > 0 ? SizeString(size) : CString (' ', 19))
      + (free > 0 ? SizeString(free) : CString (' ', 19))
      + "  " + ShortDate((WORD) (date / 86400L), true)
      + "  " + ShortTime(date);

    DrawItemText(s, 85);
}

    // draw one item in the tree listbox
void CMainDlgWindow::OnDrawDirItem(int n)
{
    int dir = (int) m_treeList.GetItemData(n);
    BOOL up = n == 0 || pParent (m_currCat[dir]) != m_treeDir;
    
    if (up && !(m_item->itemState & ODS_SELECTED))
	{
        m_dc->SetBkColor(GetSysColor(COLOR_GRAYTEXT));
        m_dc->SetTextColor(RGB(255,255,255));
	}
	    
    CString s = pName (m_currCat[dir]);
    if (dir == 0)
        s = m_currLabel;
    
		// logic to select the appropriate image to display
	int row = 0, col = 0;
	if (dir == m_treeDir)
		++col;
	else if (!up)
	{
		++row;
		if (pNumD (m_stats[dir]) > 0)
			++row;
		if (n == m_treeList.GetCount() - 1)
			++col;
	}
	
	VERIFY(m_dc->BitBlt(m_item->rcItem.left, m_item->rcItem.top, 18, 13,
						&m_memDC, 20 * col, 2 + 16 * row, SRCCOPY));
	
	m_item->rcItem.left += 20;
	
    FitString(m_dc, s, 70);
    DrawItemText(s);    

    s = SizeString(pKilo (m_stats[dir]))
    	+ CommaNum(pNumD (m_stats[dir]), 2, FALSE).Mid(2) + "  "
    	+ CommaNum(pNumF (m_stats[dir]), 2, FALSE) + "    "
    	+ ShortDate(pLast (m_stats[dir]), true);
       
    DrawItemText(s, 70);    
}

    // draw one item in the file listbox
void CMainDlgWindow::OnDrawFileItem(int n)
{
    c4_RowRef file = m_fileSort[n];
    
    int w = m_nameWidth;
    if (w < 75)
    	w += 65;
    if (w < 70)
    	w = 70;
    	
    CString s = pName (file);
    
    FitString(m_dc, s, w);
    DrawItemText(s);
    
    if (w == m_nameWidth)
    	s = CommaNum(pSize (file), 3) + "  ";
    else
    	s.Empty();
    s += "  " + ShortDate((WORD) pDate (file), true);
    
    DrawItemText(s, w);    
}

    // pressing F1 leads to an brief help screen
void CMainDlgWindow::OnHelp()
{
    CDialog dlg (IDD_WELCOME_DLG);
    dlg.DoModal();
}

    // there is of course also an about box
void CMainDlgWindow::OnAppAbout()
{
    CAboutBox dlg (this);
    dlg.DoModal();
}

	// same as menu command
void CMainDlgWindow::OnLogoBtn()
{
	OnAppAbout();
}

    // find file entries
void CMainDlgWindow::OnFindBtn()
{
    int n = m_catList.GetCurSel();
    if (n == LB_ERR)
    {
    	if (m_cats.GetSize() == 0)
    		return;
    		
    	n = 0;                  // select the first catalog if non was set
    	m_catList.SetCurSel(0); // make sure we have a selection
    	OnSelchangeCatList();	// adjust to the new selection
	    UpdateWindow();			// force a screen refresh right now 
    }
    
    CString s = pName (m_cats[n]);
    if (m_findDlg.Execute(s))
        OnFindNext();
}

    // setup catalogs
void CMainDlgWindow::OnSetupBtn()
{
    DoSetup(false);
}

    // update catalog
void CMainDlgWindow::OnFileRefresh()
{
    if (m_catList.GetCurSel() != LB_ERR)
			// the easy way out, better would be to disable the menu item
	    DoSetup(true);
}

    // setup catalogs
void CMainDlgWindow::DoSetup(bool now)
{
    CSetupDialog dlg (this);
    
    int n = m_catList.GetCurSel();
    if (n != LB_ERR)
        dlg.m_name = pName (m_cats[n]);    
        
//    SetCatalog(""); // make sure no catalog is in use during setup
    
    dlg.Execute(now);

	ListAllCatalogs(false); // update the list, don't warn about GetStats
    
        // attempt to maintain the current selection
	m_catList.SetCurSel(m_cats.Find(pName [dlg.m_name]));
    
    OnSelchangeCatList();
}

    // adjust the title to show which catalog is selected
void CMainDlgWindow::ConstructTitle()
{
    CString s = m_currCatName;
    ASSERT(s.Right(4).CompareNoCase(FILE_TYPE) == 0);
    s = s.Left(s.GetLength() - 4);
    
    CString root = pName (m_currCat[0]);
    if (!root.IsEmpty())
        s += " - " + root;
    
    s = "CatFish - " + s;
    if (!m_currLabel.IsEmpty())
    	s = s + " " + m_currLabel;
    	
    CString title;
    GetWindowText(title);

    if (title != s)
        SetWindowText(s);   
}

    // select a catalog and update the dialog contents
void CMainDlgWindow::SetCatalog(const char* catName)
{
    if (m_currCatName == catName)
        return; // don't bother, the catalog is currently loaded
    
    m_stats = c4_View ();

    SetTreeDir(-1);
    SetFileDir(-1);

    m_currCat = c4_View ();
    delete m_storage;
    m_storage = 0;
    
    m_currCatName = catName;
    if (m_currCatName.IsEmpty())
        return;                      
    
        // loading and calculations may take some time
    HCURSOR oldCursor = SetCursor(LoadCursor(0, IDC_WAIT));
    
    ASSERT(_scanner);
    m_storage = DEBUG_NEW c4_Storage (_scanner->ShortName(m_currCatName), false);
    m_currCat = m_storage->View("dirs");    
    
    m_stats = pStats (m_cats[m_catList.GetCurSel()]);
    
    	// get the disk label, if known
    m_currLabel.Empty();
    
    c4_View info = m_storage->View("info");
    if (info.GetSize() > 1)
    {
    	c4_StringProp pStrInfo ("s");

    	m_currLabel = pStrInfo (info[1]);
    	if (!m_currLabel.IsEmpty())
    		m_currLabel = "[" + m_currLabel + "]";
    }
    
    ConstructTitle(); // include label in title, even for partial catalogs    
    
    int n = m_currCat.GetSize();
	if (n > 0)
	{
    	CString s = pName (m_currCat[0]);
    	if (s.GetLength() == 0 || s.Mid(1) != ":")
    	{
    			// use last parent als label for partial catalogs
    		while (s.Find('\\') >= 0)
    			s = s.Mid(s.Find('\\') + 1);
    		
    		m_currLabel = s;
    	}	
    }

    if (m_currLabel.IsEmpty()) 
    {
    	ASSERT(0); // should no longer happen
    	m_currLabel = "(root)";              
    }
    
    SetCursor(oldCursor);
    
    if (m_currCat.GetSize() > 0)
    {
        SetTreeDir(0);
        SetFileDir(0);
    }
}

    // select a directory in the tree and update the dialog contents
void CMainDlgWindow::SetTreeDir(int dirNum)
{
    if (dirNum != m_treeDir)
    {
        m_treeDir = dirNum;
        
        ListBoxFreezer frozen (m_treeList);
    
        if (dirNum >= 0)
        {                  
                // select the appropriate subdirectories and sort them by name
            c4_View selsort = m_currCat.Select(pParent [dirNum]).SortOn(pName);

            for (int j = 0; j < selsort.GetSize(); ++j)
            {
                    // map each entry back to the m_currCat view
                int ix = m_currCat.GetIndexOf(selsort[j]);
                ASSERT(ix >= 0);
                
                    // don't add the root entry, it doesn't sort correctly
                if (ix > 0)
                {
                    int k = m_treeList.AddString(0);
                    m_treeList.SetItemData(k, ix);
                }
            }
            
                // insert the parent directories in reverse order in front
            for (;;)
            {
                m_treeList.InsertString(0, 0);
                m_treeList.SetItemData(0, dirNum);
                
                if (dirNum == m_treeDir)
                    m_treeList.SetCurSel(0);
                    
                if (dirNum <= 0)
                    break;
                    
                dirNum = (int) pParent (m_currCat[dirNum]);
            }
                                             
                // make sure the focus item is the same as the selection
                // InsertString moves the selection but not the focus...
            m_treeList.SetCurSel(m_treeList.GetCurSel());
        }
    }
    
//  SetFileDir(m_treeDir);
}

    // select a list of files and update the dialog contents
void CMainDlgWindow::SetFileDir(int dirNum)
{
    if (dirNum != m_fileDir)
    {
        m_fileDir = dirNum;
    
        ListBoxFreezer frozen (m_fileList);
    
        if (dirNum >= 0)
        {               
            m_fileView = pFiles (m_currCat[dirNum]);
        
            CString root = fFullPath(m_currCat, 0);           
            CString path = fFullPath(m_currCat, dirNum);           
            
                // remove common root prefix
            path = path.Mid(root.GetLength());
            if (path.IsEmpty())
                path = m_currLabel;

            m_msgText.SetWindowText(path);
            
            for (int i = 0; i < m_fileView.GetSize(); ++i)
                m_fileList.AddString(0);
        }
        else
        {
            m_fileSort = c4_View ();
            m_fileView = c4_View ();
            
            m_msgText.SetWindowText("");
        }
        
            // this sets up the appropriate m_fileSort view
        SortFileList(*m_sortProp);
	    
	        // reset the file list selection
	    m_fileList.SetCurSel(-1);
    }
    
    OnSelchangeFileList();
}

    // the catalog selection changed
void CMainDlgWindow::OnSelchangeCatList()
{
    int n = m_catList.GetCurSel();
    SetCatalog(n == LB_ERR ? "" : (pName (m_cats[n]) + FILE_TYPE));
}

    // the directory selection changed
void CMainDlgWindow::OnSelchangeTreeList()
{
    int n = m_treeList.GetCurSel();
    
    if (n >= 0)
        n = (int) m_treeList.GetItemData(n);
    
    SetFileDir(n);
}

/////////////////////////////////////////////////////////////////////////////
    
	static bool KeyFind(int ch_, CListBox& lb_, c4_View vw_, bool indir_)
	{
		int i = lb_.GetCurSel();
		int n = lb_.GetCount();
			
		int step = 1;
		if ('A' <= ch_ && ch_ < 'Z')
		{
			step = -1;
			ch_ |= 0x20;
		}
			
		for (int k = 1; k < n; ++k)
		{
			i += step;
			if (i < 0)
				i += n;
			if (i >= n)
				i -= n;
				
			int j = i;
			if (indir_)
				j = (int) lb_.GetItemData(i);
						
			CString s = pName (vw_[j]);
			if (ch_ == s[0] || 		// case independent match
				('a' <= ch_ && ch_ <= 'z' && ch_ - 0x20 == s[0]))
			{                                                  
				lb_.SetCurSel(i);
				return true;
			}          
		}
			
		MessageBeep(0);
		return false;
	}

	static BOOL inhibitSpaceCharToItem = FALSE;
		
	// simulate a double-click when the space key is pressed
int CMainDlgWindow::OnCharToItem(UINT nChar, CListBox* pListBox, UINT nIndex)
{
//	TRACE("OnCharToItem %d\n", nChar);

		// On NT we get *both* events, so this second event is suppressed	
	if (nChar == ' ' && inhibitSpaceCharToItem)
		return -1;
	
	if (pListBox == &m_catList)
	{
		if (nChar == ' ')	// on Win 95/NT, the space key goes here
			OnSetupBtn();
		else if (KeyFind((char) nChar, m_catList, m_cats, false))
			OnSelchangeCatList();
	}
	else if (pListBox == &m_treeList)
	{
		if (nChar == ' ')	// on Win 95/NT, the space key goes here
			OnDblclkTreeList();
		else if (KeyFind((char) nChar, m_treeList, m_currCat, true))
			OnSelchangeTreeList();
	}
	else if (pListBox == &m_fileList)
	{
		if (nChar == ' ')
			OnDblclkFileList();
		else if (KeyFind((char) nChar, m_fileList, m_fileSort, false))
			OnSelchangeFileList();
	}
	
	return -1;	// make sure positioning continues to work as before
//	return CDialog::OnCharToItem(nChar, pListBox, nIndex);
}

int CMainDlgWindow::OnVKeyToItem(UINT nKey, CListBox* pListBox, UINT nIndex)
{
//	TRACE("OnVKeyToItem %d\n", nKey);
	
		// on Win 3.1x and WinNT, the space key goes here
	if (nKey == VK_SPACE)
	{
		inhibitSpaceCharToItem = TRUE;
		
		if (pListBox == &m_catList)
			OnSetupBtn();       
		else if (pListBox == &m_treeList)
			OnDblclkTreeList();       
		else if (pListBox == &m_fileList)
		    OnDblclkFileList();
		else
			return -1;
			
		return -2;
	}
	
	return -1;	// make sure cursor motion keys continue to work as before
//	return CDialog::OnVKeyToItem(nKey, pListBox, nIndex);
}

    // descend into an entry in the directory tree
void CMainDlgWindow::OnDblclkTreeList()
{
	int dir = 0;
	
    int n = m_treeList.GetCurSel();
    if (n >= 0)
    {
		dir = (int) m_treeList.GetItemData(n);
	    BOOL up = n == 0 || pParent (m_currCat[dir]) != m_treeDir;
            
            // don't allow descending into a dir with no subdirs
        if (pNumD (m_stats[dir]) == 0)
        {
            MessageBeep(0);
            return;
        }
                                  
			// double-click on a parent reverses the collapsed state again
        n = up && n > 0 ? (int) m_treeList.GetItemData(n - 1) : dir;
    }
    
    SetTreeDir(n);
    
    	// find the dir again after expanding its parent
	for (int i = m_treeList.GetCount(); --i >= 0; )
		if (dir == (int) m_treeList.GetItemData(i))
		{
			m_treeList.SetCurSel(i);
    		break;
    	}

	SetFileDir(dir);
}
                              
    // the file selection changed
void CMainDlgWindow::OnSelchangeFileList()
{
    CString s;
    
    int n = m_fileList.GetCurSel();
    if (n >= 0)
        s = pName (m_fileSort[n]);
//		s.Format("%d of %d", n + 1, m_fileSort.GetSize());
    else if (m_fileDir >= 0)
        s.Format("%d files", m_fileSort.GetSize());
	    
    m_infoText.SetWindowText(s);
}

void CMainDlgWindow::OnDblclkFileList()
{
    int n = m_fileList.GetCurSel();
    if (n >= 0)
    {
        c4_RowRef file = m_fileSort[n];
        CString s = pName (file);
        
        CString path = fFullPath(m_currCat, m_fileDir); // also the working dir
        
        for (;;)
        {
        	UINT h = (UINT) ShellExecute(m_hWnd, 0, path + s, 0, path,
        														SW_SHOWNORMAL); 
	        if (h >= 32)
	            return; // selected file succesfully launched
	        
	        if (h != 2 && h != 3)
	        	break; // only file or path not found is a valid reason to ask
	        	
	    	if (AfxMessageBox("Please insert the disk containing:\n   " + path + s, 
	        						MB_OKCANCEL | MB_ICONEXCLAMATION) != IDOK)
	        	return; // launch cancelled
	    } 
    }

  	MessageBeep(0);
}

    // Adjust specified menu entry and label text to indicate current sort order
void CMainDlgWindow::AdjustDisplay(CCmdUI& cui, int ix_, c4_Property& prop_,
                                    int label_, const char* text_)
{
    bool match = m_sortProp == &prop_;
    
    cui.m_nIndex = ix_;
    cui.SetRadio(match);

        // include "+" or "-" in the label corresponding to the current sort field
    CString s = text_;
    if (match)
        s += m_sortReverse ? " (-)" : " (+)";
        
    CWnd* wnd = GetDlgItem(label_);
    ASSERT(wnd);
    
    wnd->SetWindowText(s);
}

    // Sort the file list and adjust menu items and label texts
void CMainDlgWindow::SortFileList(c4_Property& prop_, bool toggle_)
{
    if (m_sortProp != &prop_)
    {
        m_sortProp = &prop_;
        m_sortReverse = false;
    }
    else if (toggle_)
        m_sortReverse = !m_sortReverse;
    
        // update all menu check marks here, since CCmdUI doesn't work in dialogs
    CMenu* menu = GetMenu();
    ASSERT(menu);

    menu = menu->GetSubMenu(0); // the "File" menu
    ASSERT(menu);
    
    CMenu* sub = menu->GetSubMenu(3); // the "Sort Files" entry (ouch!)
    ASSERT(sub);
        
        // use CCmdUI, not CheckMenuItem, because it can set nice bullet marks
    CCmdUI cui;
    cui.m_pMenu = sub;
    cui.m_nIndexMax = sub->GetMenuItemCount();
    ASSERT(cui.m_nIndexMax == 5); // name, size, date, <sep>, reverse
    
    AdjustDisplay(cui, 0, pName, IDC_NAME_LABEL, "File &name");    
    AdjustDisplay(cui, 1, pSize, IDC_SIZE_LABEL, "Size");    
    AdjustDisplay(cui, 2, pDate, IDC_DATE_LABEL, "Date");
    
        // the "Reverse" menu item uses a regular check mark
    cui.m_nIndex = 4;
    cui.SetCheck(m_sortReverse);

        // sorting may take some time
    HCURSOR oldCursor = SetCursor(LoadCursor(0, IDC_WAIT));
        
        // figure out the index of the row that was selected, if any
    int n = m_fileList.GetCurSel();
    if (n >= 0)
    {
        n = m_fileView.GetIndexOf(m_fileSort [n]);
        ASSERT(n >= 0);
    }
       
        // define the sort order and make sure the list is redrawn
    if (m_sortReverse)
        m_fileSort = m_fileView.SortOnReverse(prop_, prop_);
    else
        m_fileSort = m_fileView.SortOn(prop_);
        
    m_fileList.Invalidate(); 
        
        // restore the selection to the original item
    if (n >= 0)
    {
        int m = m_fileSort.Find(m_fileView [n]); // where is that row now?
        ASSERT(m >= 0);
        
        m_fileList.SetCurSel(m);
    }
    
    SetCursor(oldCursor);
    
    OnSelchangeFileList(); // adjust notes and the "X of Y" info tag
}

void CMainDlgWindow::OnSortByName()
{
    SortFileList(pName);
}

void CMainDlgWindow::OnSortBySize()
{
    SortFileList(pSize);
}

void CMainDlgWindow::OnSortByDate()
{
    SortFileList(pDate);
}

void CMainDlgWindow::OnSortReverse()
{
    SortFileList(*m_sortProp, true);
}

void CMainDlgWindow::OnLButtonDown(UINT nFlags, CPoint point)
{
        // catch mouse clicks on the header texts to alter the file sort order
    CWnd* wnd = ChildWindowFromPoint(point);
    if (wnd)
        switch (wnd->GetDlgCtrlID())
        {
            case IDC_NAME_LABEL:    SortFileList(pName, true); return;
            case IDC_SIZE_LABEL:    SortFileList(pSize, true); return;
            case IDC_DATE_LABEL:    SortFileList(pDate, true); return;
        }                     
    
    CDialog::OnLButtonDown(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////
// The following class maintains most of the state required to iterate
// over all entries to satisfy a find request. This is a pretty messy
// approach to be able to use this in both forward and backward modes.
        
    class CFindState
    {
    public:
        CFindState (CMainDlgWindow& dlg_)
            : _dlg (dlg_), findStorage (0)
        {
                // searching may take some time
            oldCursor = SetCursor(LoadCursor(0, IDC_WAIT));
        }
        
        ~CFindState ()
        {                      
            SetCursor(oldCursor);
            
            findCat = c4_View();
            findList = c4_View();
            delete findStorage;
        }
        
        bool Initialize()
        {
            lastCat = _dlg.m_catList.GetCurSel();
            if (lastCat < 0 || _dlg.m_treeDir < 0 || _dlg.m_fileDir < 0)
            {
                MessageBeep(0);
                return false;
            }
        
            findCatName = _dlg.m_currCatName;
            findCat = _dlg.m_currCat;
            findList = _dlg.m_fileSort;
        
                // prepare for iteration    
            lastDir = _dlg.m_fileDir;
            lastSel = _dlg.m_fileList.GetCurSel(); // can be -1
            
            return true;
        }
        
        void SetSort(const c4_View& view_)
        {
            ASSERT(_dlg.m_sortProp);
            c4_Property& prop = *_dlg.m_sortProp;
            
            if (_dlg.m_sortReverse)
                findList = view_.SortOnReverse(prop, prop);
            else
                findList = view_.SortOn(prop);
        }
        
        bool IsStartDir(int dir_, int cat_)
        {
            return dir_ == lastDir && cat_ == lastCat;
        }
        
        void Select(int sel_, int dir_)
        {
                // adjust to the found catalog and directory
            _dlg.SetCatalog(findCatName);
            _dlg.SetTreeDir(dir_);
            _dlg.SetFileDir(dir_);
                // then match the selection and update the status fields
            _dlg.m_fileList.SetCurSel(sel_);
            _dlg.OnSelchangeFileList();
            
            if (sel_ < 0)                   // so arrows work as expected
                _dlg.m_treeList.SetFocus();
            else
                _dlg.m_fileList.SetFocus();
                
            _dlg.m_fileList.UpdateWindow(); // show before new find can start
        }
        
        void UseCatalog(int cat_)
        {
                // show which catalog we're currently searching
            _dlg.m_catList.SetCurSel(cat_);
        
            findCat = c4_View();
            findList = c4_View();
            delete findStorage;
            findStorage = 0;
            
            findCatName = pName (_dlg.m_cats[cat_]) + FILE_TYPE;
            
            c4_String s = _dlg._scanner->ShortName(findCatName);
            ASSERT(!s.IsEmpty());
            
            findStorage = DEBUG_NEW c4_Storage (s, false);
            findCat = findStorage->View("dirs");
        }
        
            // check if any key is pressed, this aborts a lengthy find
        bool WantsToQuit() const
        {
            MSG msg;
                // careful, there may still be keyup's in the queue
            if (!::PeekMessage(&msg, NULL, WM_KEYDOWN, WM_KEYDOWN, PM_NOREMOVE))
                return false;                                       
            
            while (::PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                ; // flush all key events
                
            return true; 
        }
        
        CMainDlgWindow& _dlg;
        int lastCat, lastDir, lastSel;
        c4_Storage* findStorage;
        CString findCatName;
        c4_View findCat, findList;
        HCURSOR oldCursor;
    };
    
/////////////////////////////////////////////////////////////////////////////
// Several aspects of the find code below affect search performance:
//
//  1.  Each catalog is opened without calculating any statistics
//  2.  Only match fields are accessed, for optimal use of on-demand loading
//  3.  Sorting is only performed *after* at least one entry has been found
//
// As a result, searching is quite fast (and THAT is an understatement).

void CMainDlgWindow::OnFindNext()
{
    CFindState state (*this);
    if (!state.Initialize())
        return;

        // prepare for iteration    
    int cat = state.lastCat;    
    int dir = state.lastDir;
    int sel = state.lastSel;
    
    bool mustSort = false; // avoid resorting when a sorted list is available
    bool first = true; // watch out for infinite loop if never found an entry
    
    for (;;) // loop over each catalog
    {
        int dirCount = state.findCat.GetSize();
        
        while (dir < dirCount) // loop over each subdirectory
        {
            c4_View files = pFiles (state.findCat [dir]);
            int selCount = files.GetSize();
            
                // on first entry into dir, first scan for match in unsorted list
                // this *drastically* improves performance if most dirs are a miss
            if (sel < 0 && m_findDlg.NeedsCompare())
            {
                while (++sel < selCount) // loop over each file
                    if (m_findDlg.Match(files[sel]))
                    {
                        sel = -1; // something matches, prepare to search sorted
                        break;
                    }
                
                // at this point sel is either -1 or selCount
            }
            
                // only sort if we're really going to use this to scan
            if (mustSort && sel < selCount)
                state.SetSort(files);
            
            while (++sel < selCount) // loop over each file
                if (m_findDlg.Match(state.findList [sel]))
                {
                    if (!first && sel >= state.lastSel &&
                                    state.IsStartDir(dir, cat))
                        break; // oops, second time around in start dir, fail
                        
                    state.Select(sel, dir);
                    return;                    
                }
            
                // if we fell through start dir for the second time, then fail
                // this scans for too many entries but works on empty start dir
            if (state.WantsToQuit() || !first && state.IsStartDir(dir, cat))
            {
                    // wrapped around, nothing found
                m_catList.SetCurSel(state.lastCat);
                MessageBeep(0);
                return;
            }
            
            first = false;
            
            sel = -1;
            ++dir; // directories are scanned in breadth first order, hmmm...
            mustSort = true;

                // new in 1.7 - match directory names also
            if (dir < dirCount && m_findDlg.MatchDir(state.findCat [dir]))
            {
                state.Select(sel, dir);
                return;                    
            }
        }
        
        dir = 0;
        
        if (m_findDlg.m_singleCat)
            continue; // don't switch to another catalog file
                 
        if (++cat >= m_catList.GetCount())
            cat = 0;
        
        state.UseCatalog(cat);
    }
}

void CMainDlgWindow::OnFindPrev()
{
    CFindState state (*this);
    if (!state.Initialize())
        return;

        // prepare for iteration    
    int cat = state.lastCat;    
    int dir = state.lastDir;
    int sel = state.lastSel;
        
        // if last was dir match, then go back to previous dir now
    if (sel < 0 && dir >= 0)
    	--dir;
    	
    bool mustSort = false; // avoid resorting when a sorted list is available
    bool first = true; // watch out for infinite loop if never found an entry
    
    for (;;) // loop over each catalog
    {
        if (dir < 0)
            dir = state.findCat.GetSize() - 1;
            
        while (dir >= 0) // loop over each subdirectory
        {
            c4_View files = pFiles (state.findCat [dir]);
            int selCount = files.GetSize();
            
            if (sel < 0)
                sel = selCount;
                
                // on first entry into dir, first scan for match in unsorted list
                // this *drastically* improves performance if most dirs are a miss
            if (sel >= selCount && m_findDlg.NeedsCompare())
            {
                while (--sel >= 0) // loop over each file
                    if (m_findDlg.Match(files[sel]))
                    {
                        sel = selCount; // matches, prepare to search sorted
                        break;
                    }
                
                // at this point sel is either -1 or selCount
            }
            
                // only sort if we're really going to use this to scan
            if (mustSort && sel >= 0)
                state.SetSort(files);
            
            while (--sel >= 0) // loop over each file
                if (m_findDlg.Match(state.findList[sel]))
                {
                    if (!first && sel <= state.lastSel &&
                                    state.IsStartDir(dir, cat))
                        break; // oops, second time around in start dir, fail
                        
                    state.Select(sel, dir);
                    return;                    
                }
            
                // if we fell through start dir for the second time, then fail
                // this scans for too many entries but works on empty start dir
            if (state.WantsToQuit() || !first && state.IsStartDir(dir, cat))
            {
                    // wrapped around, nothing found
                m_catList.SetCurSel(state.lastCat);
                MessageBeep(0);
                return;
            }
            
            first = false;
            
            sel = -1;
            --dir; // directories are scanned in breadth first order, hmmm...
            mustSort = true;

                // new in 1.7 - match directory names also
            int d = dir + 1;
            if (d >= 0 && m_findDlg.MatchDir(state.findCat [d]))
            {
                state.Select(sel, d);
                return;                    
            }
        }
        
        ASSERT(dir == -1);
        
        if (m_findDlg.m_singleCat)
            continue; // don't switch to another catalog file
        
        if (cat == 0)
            cat = m_catList.GetCount();
        --cat;
        
        state.UseCatalog(cat);
    }
}

void CMainDlgWindow::OnFileExport()
{ 
    int n = m_catList.GetCurSel();
    if (n == LB_ERR)
    {
    	::MessageBeep(0);
        return; // the easy way out, better would be to disable the menu item
    }
        
    CString s = m_currCatName;
    ASSERT(s.Right(4).CompareNoCase(FILE_TYPE) == 0);
    s = s.Left(s.GetLength() - 4);
    
    CFileDialog dlg (FALSE, "txt", s + ".txt",
//    					0x00200000 |	// long filenames...
                        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR,
                        "Text Files (*.txt) | *.txt |"
                        "All Files (*.*) | *.* ||");     
    
    if (dlg.DoModal() == IDOK)
    {
            // exporting may take some time
        HCURSOR oldCursor = SetCursor(LoadCursor(0, IDC_WAIT));

        CStdioFile output (dlg.GetPathName(), CFile::typeText |
                                CFile::modeReadWrite | CFile::modeCreate);
        
        CWordArray pending;
        pending.Add(0);
        
        CString title;
        GetWindowText(title);
    
        output.WriteString(title);

        if (m_fileDir > 0)
        {
			pending[0] = m_fileDir;
    		output.WriteString("\n\n*** Partial export ***");
        }
        
        do
        {
            int index = pending[0];
            pending.RemoveAt(0);

            output.WriteString("\n\n " + fFullPath(m_currCat, index) + "\n");
            
            c4_View files = pFiles (m_currCat[index]); 
            for (int j = 0; j < files.GetSize(); ++j)
            {
                c4_RowRef file = files[j];
                
                	// fixed 30-1-1998: export alignement of single digit columns
                output.WriteString("\n "
                    + ("  " + CommaNum(pSize (file), 3)).Right(13)
                    + "  " + ShortDate((WORD) pDate (file), false)
                    + "  " + (CString) pName (file));
            }
            
            c4_View children = m_currCat.Select(pParent [index]).SortOn(pName);
            
            int n = children.GetSize();
            if (n > 0)
            {
                pending.InsertAt(0, 0, n);
                for (int i = 0; i < n; ++i)               
                    pending[i] = (WORD) m_currCat.GetIndexOf(children[i]);
            }

                // root has itself as parent, avoid a runaway loop  
            if (pending.GetSize() > 0 && pending[0] == 0)
                pending.RemoveAt(0);
        
        } while (pending.GetSize() > 0); 
        
        output.WriteString("\n\nDone.\n");
        
        SetCursor(oldCursor);
    }
}

/////////////////////////////////////////////////////////////////////////////

void CMainDlgWindow::OnFileDelete()
{
	c4_String name;
	
    int n = m_catList.GetCurSel();
    if (n == LB_ERR)
    {
    	::MessageBeep(0);
    	return;
    }
    
    name = pName (m_cats[n]);
    
    if (AfxMessageBox("Do you want to permanently delete "
                        "the catalog named '" + name + "' ?",
                        MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2) == IDYES)
    {
	    SetCatalog("");
	    
        CFile::Remove(_scanner->ShortName(name + FILE_TYPE));
	
		ListAllCatalogs(false); // update the list, don't warn about GetStats
		m_catList.SetCurSel(-1);
	    OnSelchangeCatList();
    }
}

void CMainDlgWindow::OnFileRename()
{
	c4_String name;
	
    int n = m_catList.GetCurSel();
    if (n == LB_ERR)
    {
    	::MessageBeep(0);
    	return;
    }
    
    name = pName (m_cats[n]);
    
    CRenameDlg dlg;
    dlg._name = name;
    
    c4_String newName;
    if (dlg.DoModal() == IDOK)
    	newName = dlg._name;
    
    SetCatalog("");
	    
    if (!newName.IsEmpty() && 
    		_scanner->Move(name + FILE_TYPE, newName + FILE_TYPE))
    	name = newName;
	else
    	::MessageBeep(0);

	ListAllCatalogs(true); // update the list, and pop up a dialog 

	    // attempt to maintain the current selection
	m_catList.SetCurSel(m_cats.Find(pName [name]));		    
	OnSelchangeCatList();
}

/////////////////////////////////////////////////////////////////////////////
