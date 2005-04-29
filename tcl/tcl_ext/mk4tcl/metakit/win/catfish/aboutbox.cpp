// aboutbox.cpp : implementation file
//

#include "stdafx.h"
#include "catfish.h"
#include "aboutbox.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CStaticURL

CStaticURL::CStaticURL()
	: m_stdFont (0), m_urlFont (0),
      m_urlCursor (AfxGetApp()->LoadCursor(IDC_URL_CURSOR))
{
}

void CStaticURL::SetupFonts(CFont* std, CFont* url)
{
	m_stdFont = std;
	m_urlFont = url;
}

void CStaticURL::SetURL(const char* name, const char* url)
{
	CFont* font = url && *url ? m_urlFont : m_stdFont;
	if (font)
		SetFont(font);
	
	EnableWindow(); // get rid of gray initial text
	SetWindowText(name);
	m_url = url;
}

BEGIN_MESSAGE_MAP(CStaticURL, CWnd)
	//{{AFX_MSG_MAP(CStaticURL)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CStaticURL message handlers

BOOL CStaticURL::OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam,
					LRESULT* pLResult)
{
	if (!m_url.IsEmpty())
	{
		if (message == WM_CTLCOLOR)
		{
			::SetTextColor((HDC) wParam, RGB(0,0,224));
			::SetBkMode((HDC) wParam, TRANSPARENT);
		
			static CBrush NEAR brush (GetSysColor(COLOR_WINDOW));
			*pLResult = (LRESULT) (int) brush.GetSafeHandle();
			
			return TRUE;
		}
		
		if (message == WM_LBUTTONDOWN)
		{
        	if ((UINT) ShellExecute(0, 0, m_url, 0, 0, SW_SHOWNORMAL) < 32)
            	MessageBeep(0);
			
			return TRUE;
		}
		
		if (message == WM_MOUSEMOVE)
		{
			SetCursor(m_urlCursor);
			return TRUE;
		}
	}
	
	return CStatic::OnChildNotify(message,wParam,lParam,pLResult);
}

/////////////////////////////////////////////////////////////////////////////
// CAboutBox dialog


CAboutBox::CAboutBox(CMainDlgWindow* pParent)
	: CDialog(CAboutBox::IDD, pParent), m_parent (pParent)
{
	//{{AFX_DATA_INIT(CAboutBox)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

void CAboutBox::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutBox)
	DDX_Control(pDX, IDC_EMAIL, m_email);
	DDX_Control(pDX, IDC_URL, m_url);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutBox, CDialog)
	//{{AFX_MSG_MAP(CAboutBox)
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////

	static bool fWithinRect(CPoint point, const CWnd& wnd)
	{
		CRect r;
		wnd.GetWindowRect(&r);
		
		return r.PtInRect(point);
	}

/////////////////////////////////////////////////////////////////////////////
// CAboutBox message handlers

BOOL CAboutBox::OnInitDialog()
{
		// static URLs
	VERIFY(m_url.SubclassDlgItem(IDC_URL, this));
	VERIFY(m_email.SubclassDlgItem(IDC_EMAIL, this));

	CDialog::OnInitDialog();

	CFont& font = m_parent->m_font;
	m_url.SetupFonts(&font, &font);
	m_email.SetupFonts(&font, &font);
	
	m_url.SetURL("http://www.equi4.com", "http://www.equi4.com/");
	m_email.SetURL("jcw@equi4.com", "mailto:jcw@equi4.com");
	
    CenterWindow();
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CAboutBox::OnLButtonDown(UINT nFlags, CPoint point)
{
		// messy, because CStatic doesn't seem to get mouse events
	CPoint p = point;
	ClientToScreen(&p);
	
	if (fWithinRect(p, m_url) && m_url.SendChildNotifyLastMsg()
	 || fWithinRect(p, m_email) && m_email.SendChildNotifyLastMsg())
		return;

	CDialog::OnLButtonDown(nFlags, point);
}

void CAboutBox::OnMouseMove(UINT nFlags, CPoint point)
{
		// messy, because CStatic doesn't seem to get mouse events
	CPoint p = point;
	ClientToScreen(&p);
	
	if (fWithinRect(p, m_url) && m_url.SendChildNotifyLastMsg()
	 || fWithinRect(p, m_email) && m_email.SendChildNotifyLastMsg())
		return;

	SetCursor(LoadCursor(NULL, IDC_ARROW));

	CDialog::OnMouseMove(nFlags, point);
}
