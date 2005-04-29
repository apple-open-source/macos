#include "stdafx.h"

#include "util.h"
#include "scandisk.h"

/////////////////////////////////////////////////////////////////////////////
// CMyEdit subclassed control

BEGIN_MESSAGE_MAP(CMyEdit, CEdit)
    //{{AFX_MSG_MAP(CMyEdit)
    ON_WM_CHAR()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CMyEdit::OnChar(UINT ch_, UINT rep_, UINT flags_)
{
        // only accept the characters which are allowed in a filename
//  bool accept = ('A' <= ch_ && ch_ <= 'Z') ||
//                ('a' <= ch_ && ch_ <= 'z') ||
//                ('0' <= ch_ && ch_ <= '9') ||
//                strchr("$%'+-@{}~`!#()&_", ch_) != 0;
	
	bool accept;
                  
	static bool win32 = DirScanner::CanUseLongNames();
	if (win32)
		accept = ' ' <= ch_ && ch_ <= '~' && strchr("<>:\"/\\?*", ch_) == 0;
	else
		accept = ('A' <= ch_ && ch_ <= 'Z') ||
                 ('a' <= ch_ && ch_ <= 'z') ||
                 ('0' <= ch_ && ch_ <= '9') ||
                 strchr("$%'+-@{}~`!#()&_", ch_) != 0;
	
    if (accept || ch_ == VK_BACK || (acceptDot && strchr(".?* ", ch_)))
        CEdit::OnChar(ch_, rep_, flags_) ;
    else
        MessageBeep(0);
}

/////////////////////////////////////////////////////////////////////////////
// Preferences

	static c4_IntProp    NEAR pInt ("i");	
	static c4_StringProp NEAR pStr ("s");	
	static c4_BytesProp  NEAR pBin ("b");	

Preferences::Preferences (const c4_String& fileName_) 
	: _storage (fileName_, true),
	  _prefs (_storage.GetAs("prefs[b:B,i:I,s:S]"))
{
	_storage.AutoCommit();
}

Preferences::~Preferences ()
{
}
    
void Preferences::Flush()
{
	_storage.Commit();
}
    
c4_IntRef Preferences::Int(int id_)
{
	if (id_ >= _prefs.GetSize())
		_prefs.SetSize(id_ + 1);
	
	return pInt (_prefs[id_]);
}

c4_StringRef Preferences::Str(int id_)
{
	if (id_ >= _prefs.GetSize())
		_prefs.SetSize(id_ + 1);
		
	return pStr (_prefs[id_]);
}
    
bool Preferences::GetBin(int id_, void* ptr_, int len_)
{
	if (id_ >= _prefs.GetSize())
		return false;
		
	c4_Bytes data = pBin (_prefs[id_]);
	if (data.Size() != len_)
		return false;
		
	memcpy(ptr_, data.Contents(), len_);
	return true;
}

void Preferences::SetBin(int id_, const void* ptr_, int len_)
{
	if (id_ >= _prefs.GetSize())
		_prefs.SetSize(id_ + 1);
	
	pBin (_prefs[id_]) = c4_Bytes (ptr_, len_);	
}
    
c4_View Preferences::GetView(const char* name_)
{
	return _storage.GetAs(name_);
}

/////////////////////////////////////////////////////////////////////////////
// Use a simple version of localized date, time, and number formating.

    static c4_String  NEAR sShortDate  = "MM/dd/yy";   // "d.M.yyyy", etc
    static bool     NEAR iTime       = false;        // true if 24h format
    static bool     NEAR iTLZero     = true;         // true if hour has 2 digits
    static char     NEAR sThousand   = ',';          // thousands separator
    static char     NEAR sTime       = ':';          // time separator

void SetInternationalSettings()
{
    iTime = GetProfileInt("intl", "iTime", 0) != 0;
    iTLZero = GetProfileInt("intl", "iTLZero", 1) != 0;
        
    char buf [30];
        
    if (GetProfileString("intl", "sShortDate", "MM/dd/yy", buf, sizeof buf))
        sShortDate = buf;
        
    if (GetProfileString("intl", "sThousand", ",", buf, sizeof buf))
        sThousand = *buf;
        
    if (GetProfileString("intl", "sTime", ":", buf, sizeof buf))
        sTime = *buf;
}
        
/////////////////////////////////////////////////////////////////////////////
// Convert a number to comma-separated format, grouped in units of three.
// Optionally prefix with spaces (assuming two spaces is width of one digit).
// Finally, the zero value can be changed to a '-' upon request.
//
// Note:    In many places, the code is simplified by the assumption that
//          every digit has exactly the same width as two space characters.
//          This works for the selected font (MS Sans Serif, font size 8).
//          It allows us to present a nice columnar interface without having
//          to figure out each of the string position in pixels. There are
//          several more assumptions like this (e.g. "k   " is like "Mb").

c4_String CommaNum(DWORD num, int groups, BOOL zero)
{
    c4_String s;
    s.Format("%lu", num);
        
    int g = 0;
    int n = s.GetLength();
    while (n > 3)
    {
        n -= 3;
        s = s.Left(n) + sThousand + s.Mid(n);
        ++g;
    }
        
    if (--groups >= 0)
    {
        int w = ((3 - n) % 3) * 2;
        if (g < groups)
            w += 7 * (groups - g);

        s = c4_String (' ', w) + s;
    }
        
    if (!zero && (s == "0" || s.Right(2) == " 0"))
        s = s.Left(s.GetLength() - 1) + "- ";
            
    return s;
}
    
/////////////////////////////////////////////////////////////////////////////
// Return a kilobyte value as a string

c4_String SizeString(DWORD kb_)
{
	return kb_ <= 99999 ? CommaNum(kb_, 2) + " k   "
						: CommaNum((kb_ + 1023) / 1024, 2) + " Mb";
}
	
/////////////////////////////////////////////////////////////////////////////
// Convert a DOS date and TIME words to short format strings.
// Lets be nice to a lot of people and adopt their local conventions.

c4_String ShortDate(WORD date, bool display)
{
    if (date == 0)
        return "";  // will be ok as long as the date is last item
            
    int w = 0;
        
    char buf [10];
    char* q = buf;
        
        // decode the short date, deal with 1- and 2-digit fields
    const char* p = sShortDate;
    while (*p)
    {
        int i;
            
        switch (*p++ | 0x20)
        {            
            default:    *q++ = *(p-1);
                        continue;
                
            case 'd':   i = date & 0x1F;
                        break;
                            
            case 'm':   i = (date >> 5) & 0x0F;
                        break;
                            
            case 'y':   i = ((date >> 9) + 80) % 100;
                        break; // 4-digit years are treated as 2-digit
                            
        }
            
        if (i < 10 && *p != *(p-1))
            ++w;
        else
            *q++ = (char) (i / 10 + '0');
            
        *q++ = (char) (i % 10 + '0');

        while (*p == *(p-1))
            ++p;
    }
        
        // alignment is easy, since one digit is as wide as two spaces
    c4_String t (' ', (display ? 2 : 1) * w);
        // alignment depends on whether the year is first or last 
    if (sShortDate[0] == 'y')
        return c4_String (buf, q - buf) + t;
        
    return t + c4_String (buf, q - buf);
}
    
c4_String ShortTime(long time)
{
    int h = (int) ((time / 3600) % 24);
    int m = (int) ((time / 60) % 60);
    char ampm = "ap" [h / 12];
        
    if (!iTime)
        h = (h + 11) % 12 + 1; // dec, then inc, so 0 becomes 12
            
    c4_String s;
    s.Format("%02d%c%02d", h, sTime, m);
        
    if (!iTime)
        s += ampm;
        
    if (!iTLZero && s[0] == '0')
        s = "  " + s.Mid(1); // replace leading zero with two spaces
            
    return s;
}
    
/////////////////////////////////////////////////////////////////////////////
// Make a string fit in the specified number of pixels on given device.
// Characters at the end are replaced by an ellipsis to make the string fit.
// There is some trickery in here to optimize this very common calculation.

BOOL FitString(CDC* dc, CString& text, int width)
{
    CSize sz = dc->GetTextExtent(text, text.GetLength());
    if (sz.cx <= width)
        return TRUE;    // make the most common case fast
        
        // Assumption: "...xyz" is just as wide as "xyz..." 
    CString s = "..." + text;
        
    int n = s.GetLength();
    while (--n > 3)
    {            
        sz = dc->GetTextExtent(text, n);
        if (sz.cx <= width)
            break;
    }
             
    text = text.Left(n - 3) + "...";
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
    
CSize GetSize(const CWnd& wnd_)
{
    CRect r;
    wnd_.GetWindowRect(&r);
    return r.Size();
}
	
CPoint GetOrigin(const CWnd& wnd_)
{
    CRect r;
    wnd_.GetWindowRect(&r);
    return r.TopLeft();
}
	
/////////////////////////////////////////////////////////////////////////////
