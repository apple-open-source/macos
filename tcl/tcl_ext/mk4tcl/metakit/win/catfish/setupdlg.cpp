//  setupdlg.cpp  -  setup dialog sample code
//
//  Copyright (C) 1996-2000 Jean-Claude Wippler.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "catfish.h"
#include "setupdlg.h"
#include "pickdir.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSetupDialog dialog

CSetupDialog::CSetupDialog(CMainDlgWindow* pParent)
    : CDialog(CSetupDialog::IDD, pParent), m_parent (pParent),
      m_exists (FALSE), m_timer (0), m_allowScan (FALSE)
{
	ASSERT(pParent != 0);
	
    //{{AFX_DATA_INIT(CSetupDialog)
	m_name = "";
	//}}AFX_DATA_INIT
}

void CSetupDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CSetupDialog)
	DDX_Control(pDX, IDC_DRIVES, m_drives);
	DDX_Control(pDX, IDC_VOL_SERIAL, m_volSerial);
	DDX_Control(pDX, IDC_VOL_NAME, m_volName);
    DDX_Control(pDX, IDCANCEL, m_okBtn);
    DDX_Control(pDX, IDC_BROWSE_BTN, m_browseBtn);
    DDX_Control(pDX, IDC_ADD_BTN, m_addBtn);
    DDX_Control(pDX, IDC_STATUS, m_status);
    DDX_Control(pDX, IDC_ROOT, m_root);
	DDX_Text(pDX, IDC_NAME, m_name);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CSetupDialog, CDialog)
    //{{AFX_MSG_MAP(CSetupDialog)
    ON_BN_CLICKED(IDC_ADD_BTN, OnAddBtn)
    ON_BN_CLICKED(IDC_BROWSE_BTN, OnBrowseBtn)
    ON_EN_CHANGE(IDC_NAME, OnChangeName)
    ON_WM_TIMER()
    ON_WM_CLOSE()
	ON_CBN_SELCHANGE(IDC_DRIVES, OnSelchangeDrives)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSetupDialog message handlers

void CSetupDialog::Execute(BOOL now)
{             
    m_scanNow = now;
    
    DoModal();
}

    // this member is called by the ScanDisk code during its (lengthy) scan
bool CSetupDialog::UpdateStatus(const char* text)
{
    static DWORD lastTick = 0;
    
        // a null pointer forces immediate clear of the status text
    if (!text)
    {
        lastTick = 0;
        text = "";
    }
    
        // only refresh the status message every quarter second
    if (GetTickCount() > lastTick + 250)
    {
        lastTick = GetTickCount();
        m_status.SetWindowText(text);   
    }
    
    return cStatusHandler::UpdateStatus(text) && m_allowScan;
}

    // the name has changed, update other fields (either now, or a little later)
    // this delay is used to avoid excessive activity while a name is typed in
void CSetupDialog::NameChange(BOOL delay)
{
    CString s;
    if (m_name.IsEmpty())
        s = "(please enter name of catalog)";
    m_status.SetWindowText(s);

    m_exists = FALSE;
    
    if (delay)
    {       // arm the timer (if there is a catalog name)
        KillTimer(m_timer);
        m_timer = s.IsEmpty() ? 0 :
                    SetTimer(1, 500, 0); // timeout in 500 mS
    }
    else
        InspectCatalog();
}

    // scan through the directories, but be prepared to deal with premature abort
void CSetupDialog::ScanDirectories()
{
    CString s;
    m_root.GetWindowText(s);
    
        // prepare for a lengthy scan with a cancel option
    AdjustButtons(TRUE);                        
    SetDefID(IDCANCEL);

	CString catFile = m_name + FILE_TYPE;
	
        // clumsy, remove file if it exists, to create smallest file
    remove("catfish.tmp");
    
    {   
	    c4_Storage storage ("catfish.tmp", true);
	    m_allowScan = TRUE;
		
	        // build a catalog of the specified directory tree 
	
		c4_View v = storage.GetAs("dirs[name:S,parent:I,"
										"files[name:S,size:I,date:I]]");
	    bool ok = fScanDirectories(s, v, this);
	                     
	    storage.GetAs("info[i:I,s:S]").InsertAt(0, fDiskInfo(s));
		                            
	        // restore normal situation
	    m_allowScan = FALSE;
	    AdjustButtons();
		                            
	    if (ok)
	    {
	    	storage.Commit();
	    		// make sure this catalog is not in use
	    	m_parent->SetCatalog("");
        }
        else
	        catFile = "catfish.tmp";	// get rid of the incomplete catalog
	}
	
	remove(m_parent->_scanner->ShortName(catFile));
	
		// use the Win16 or the WOW version, as appropriate
	DirScanner::Move("catfish.tmp", catFile); // fails if not stored, that's ok
}     

    // go see if the catalog exists and get its file date and root path
void CSetupDialog::InspectCatalog()
{
    UpdateData();
                            
    CString s = m_parent->GetCatalogDate(m_name);
    if (!s.IsEmpty())
        s = "Previously saved on:  " + s;

    UpdateData(FALSE); // adjusts the name
                                                            
    m_status.SetWindowText(s);
    m_exists = !s.IsEmpty();
                                
    if (m_exists)
    {
		int n = m_parent->m_cats.Find(pName [m_name]);
        ASSERT(n >= 0);
        
        c4_RowRef row = m_parent->m_cats[n];

/*
            // load the root path name from the catalog
            // this is quick, due to on-demand loading
        c4_String fn = m_parent->_scanner->ShortName(m_name + FILE_TYPE);
        
        c4_Storage storage (fn, false);
        c4_View dirs = storage.View("dirs");
        m_origRoot = pName (dirs[0]);
        m_root.SetWindowText(m_origRoot);
*/
        m_origRoot = pNamV (row);
        m_root.SetWindowText(m_origRoot);
    }
}

    // adjust the button dimming and titles to reflect the current situation
void CSetupDialog::AdjustButtons(BOOL inScan_)
{
    CString s;
    m_root.GetWindowText(s);
    BOOL valid = !s.IsEmpty() && !inScan_;
    
        // don't enable buttons while the timer is running  
    m_addBtn.EnableWindow(valid && (m_exists || !m_timer && !m_name.IsEmpty()));
    
    ASSERT(!(m_exists && m_timer)); // if it exists, timer cannot be running
    
        // The default button is "OK" for existing files
        // If the root has been set for a new file, the default is "Add"
        // For new files, or if the root is different, the default is "Browse"
        // Never make "Change" the default, since that is a dangerous change
    SetDefID(inScan_ ? IDCANCEL :
             valid && !m_timer ? IDC_ADD_BTN : IDC_BROWSE_BTN);
    
    if (!GetFocus())
        m_okBtn.SetFocus();
}
                            
        // called shortly after a change to the name or root edit control       
void CSetupDialog::OnTimer(UINT nIDEvent)
{
    KillTimer(m_timer);
    m_timer = 0;
    
    InspectCatalog();
    AdjustRoot();
    
    CDialog::OnTimer(nIDEvent);
}
            
BOOL CSetupDialog::OnInitDialog()
{
    CenterWindow();
    CDialog::OnInitDialog();
    
    m_nameEditCtrl.SubclassDlgItem(IDC_NAME, this);
    
    NameChange(FALSE);
    AdjustButtons();

    if (m_scanNow) 
    {
        SetWindowText("Re-scanning disk...");

	    CString s;
	    m_root.GetWindowText(s);
		m_drives.AddString(s.Left(2));
		SetDrive(0);
		
		ShowWindow(SW_SHOW);  
        UpdateWindow(); 

        OnAddBtn();
        OnOK();
    }
    else
    {
  		static char buf [] = "?:";
   		DirScanner::VolInfo info;

	    for (int d = 0; d < 26; ++d)
	    {
	    	UINT type = ::GetDriveType(d);
	    	if (type != 0)
	    	{
	    		buf[0] = 'A' + d;;
				m_drives.AddString(buf);
			}
	    }
	    
	    SetDrive(m_drives.FindString(-1, "C"));
    }
        
    return TRUE;  // return TRUE  unless you set the focus to a control
}

void CSetupDialog::OnCancel()
{
    if (m_allowScan)
        m_allowScan = FALSE;
    else
        CDialog::OnCancel();
}

void CSetupDialog::OnClose()
{
    if (m_allowScan)
        m_allowScan = FALSE;
    else
        EndDialog(IDOK);    // can't use close box during scan
}

void CSetupDialog::OnAddBtn()
{
    ScanDirectories();
    EndDialog(IDOK);
}

void CSetupDialog::OnBrowseBtn()
{
    CString dir = PickDirectory(this);
    if (!dir.IsEmpty())
        m_root.SetWindowText(dir);

    AdjustRoot();
}

void CSetupDialog::AdjustRoot()
{
    CString s;
    m_root.GetWindowText(s);
                            
    m_addBtn.SetWindowText(m_exists ? "&Update" : "&Add");
    
    AdjustButtons();
}

void CSetupDialog::OnChangeName()
{
    VERIFY(UpdateData());

    NameChange(TRUE);
    AdjustButtons();
}

/////////////////////////////////////////////////////////////////////////////

void CSetupDialog::OnSelchangeDrives()
{
	SetDrive(m_drives.GetCurSel());
}

void CSetupDialog::SetDrive(int index_)
{
	m_drives.SetCurSel(index_);
	
	CString s;
	m_drives.GetLBText(index_, s);
	m_root.SetWindowText(s);
    
    DirScanner::VolInfo info;
	c4_String vn = m_parent->_scanner->Info(s, &info);
	
	if (info._serialNum != 0 && vn.IsEmpty())
	{
		vn = "(none)";
		info._volName = "DRIVE-" + s.Left(1);
	}
	
	if (!vn.IsEmpty())
		vn = "Volume name:  " + vn;
	m_volName.SetWindowText(vn);

	char buf [40];
	if (info._serialNum == 0)     
		*buf = 0;
	else
		wsprintf(buf, "Serial #:  %04X-%04X", (WORD) (info._serialNum >> 16),
												(WORD) info._serialNum);
	m_volSerial.SetWindowText(buf);

    UpdateData();
	m_name = info._volName;
    UpdateData(FALSE); // adjusts the name

	NameChange(FALSE);
	
	AdjustRoot();
}

/////////////////////////////////////////////////////////////////////////////
