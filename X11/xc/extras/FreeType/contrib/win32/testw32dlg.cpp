/*********************************************************/
/* Test program driver for freetype  on Win32 Platform   */
/* CopyRight(left) G. Ramat 1998 (gcramat@radiostudio.it)*/
/*                                                       */
/*********************************************************/

// testw32Dlg.cpp : implementation file
//

#include "stdafx.h"
#include "testw32.h"
#include "testw32dlg.h"
#include "gdriver.h"
#include "gevents.h"
#include <fcntl.h>
#include <errno.h>
#include <io.h>
CWnd  *button_OK,*button_Cancel;
DWORD thrd_spool;  // output spooler
HANDLE spool_thread;

//Sync data:
extern "C" {
	HANDLE    evgetevent,evdriverdisplaybitmap,this_cwnd,main_thread,listbox,bitmap;
	TEvent    evevent;
    char  *ev_buffer;
    int   ev_lines,ev_columns;
	char  *save_buffer;
	int   save_lines,save_cols,exit_code;   
	int   ftview(int,char**);
	int   ftdump(int,char**);
	int   ftlint(int,char**);
	int   ftstring(int,char**);
	int   ftstrpnm(int,char**);
	int   ftzoom(int,char**);
	int   call_test_program(int (*)(int,char**),int,char **);

}
//pipe handling variables 
int pipe_std[2]={2*0},pipe_err[2]={2*0},error;
int old_std,old_err;
//end of pipe handling variables


#define TEST_PROG_N 4
//Sync data end
char ProgramName[16];
char fontname[16]; 
char fullfont[MAX_PATH];
char *argv[255];
int   argc;
DWORD WINAPI ThreadHead(LPVOID );
DWORD WINAPI ThreadSpool(LPVOID );
void readpipe(int);


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
extern main(int,char **);
extern int X_Link;

/////////////////////////////////////////////////////////////////////////////
// CTestw32Dlg dialog

CTestw32Dlg::CTestw32Dlg(CWnd* pParent /*=NULL*/)
	: CDialog(CTestw32Dlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTestw32Dlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTestw32Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTestw32Dlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CTestw32Dlg, CDialog)
	//{{AFX_MSG_MAP(CTestw32Dlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_SELECT_ACTION, OnSelectAction)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTestw32Dlg message handlers

BOOL CTestw32Dlg::OnInitDialog()
{  int error;
   FILE *retf;
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	//save CWnd objects for utility fns;
    evgetevent=CreateEvent(NULL,FALSE,FALSE,"Get_Event");
	evdriverdisplaybitmap=CreateEvent(NULL,FALSE,FALSE,"Driver_Display_Bitmap");
	listbox=(GetDlgItem(IDC_LIST_BOX))->m_hWnd;
	bitmap=(GetDlgItem(IDC_BITMAP))->m_hWnd;
	button_OK=GetDlgItem(IDOK);
	button_Cancel=GetDlgItem(IDCANCEL);
	error=_pipe(pipe_std,1024,_O_TEXT);
//    error=_pipe(pipe_err,1024,_O_TEXT);
	  // enable piping
	if(-1==_fileno(stdout))
	{retf=freopen("throwaway_stdout.tmp","wt",stdout);
	}
	if(-1==_fileno(stderr))
	{retf=freopen("throwaway_stderr.tmp","wt",stderr);
	}
    old_std=dup(_fileno(stdout));
    old_err=dup(_fileno(stderr));
    error=dup2(pipe_std[1],_fileno(stdout));
    error=dup2(pipe_std[1],_fileno(stderr)); //error=dup2(pipe_err[1],_fileno(stderr));
    save_buffer=NULL;
//	error=write(pipe_std[1],"Pipe_test:Write\n",16);
//	error=fprintf(stdout,"Pipe_test:fprintf");
//	error=fflush(stdout);
// activate spooler 
	spool_thread=CreateThread(NULL,0,ThreadSpool,NULL,0,&thrd_spool);

	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CTestw32Dlg::OnPaint() 
{  
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CTestw32Dlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CTestw32Dlg::OnOK() 
{   
    DWORD thrd_id;
	char Options[256];
	char *p,*pb,*pe;
	int   i;
	// TODO: Add extra validation here
	GetDlgItemText(IDC_TEST_PROGRAM,ProgramName,sizeof(ProgramName)-1);
	GetDlgItemText(IDC_OPTIONS,Options,sizeof(Options)-1);
	argv[0]=ProgramName;
	p=Options;
	i=1;
	while (*p!=0)
	{
	  while (*p==' ') p++;
	  pb=p;
	  while (*p>' ') p++;
	  pe=p;
	  if (pe>pb) 
	  {
		  argv[i]=new char[1+pe-pb];
		  strncpy(argv[i],pb,pe-pb);
		  argv[i][pe-pb]=0;
	      i++;
	  }
	}
	argv[0]=ProgramName;
	argc=i;
	main_thread=CreateThread(NULL,0,ThreadHead,NULL,0,&thrd_id);
	
//	CDialog::OnOK();
}

DWORD WINAPI ThreadHead(LPVOID Parm)
{ int i,rc;

struct {
	char pname[16];
	int (*program)(int,char**);
} tab[TEST_PROG_N]=
{
	{"FTVIEW",&ftview},
	{"FTDUMP",&ftdump},
	{"FTLINT",&ftlint},
	{"FTSTRING",&ftstring}
//	{"FTSTRPNM",&ftstrpnm},
//	{"FTZOOM",&ftzoom}
};
//disable Ok button
  rc=button_OK->EnableWindow(FALSE);
  rc=button_Cancel->EnableWindow(FALSE);

  for (i=0;(i< TEST_PROG_N) &&strcmp(tab[i].pname,ProgramName);i++);
  if (i>= TEST_PROG_N) 
  {
	  MessageBox(NULL,"Please select a valid Test Program Name","FreeType Test ",MB_ICONQUESTION);
  }
  else 
  call_test_program(tab[i].program,argc,(char **)&argv);
 //enable buttons again
  rc=button_OK->EnableWindow(TRUE);
  rc=button_Cancel->EnableWindow(TRUE);
  rc=fflush(stdout);
  rc=fflush(stderr);
  ExitThread(1);
  return 1;
}



void translate_command(char nChar) 
{   int rc,i;
	// TODO: Add your message handler code here and/or call default
 typedef struct  _Translator
  {
    char    key;
    GEvent  event_class;
    int     event_info;
  } Translator;

#define NUM_Translators  20

  static const Translator  trans[ NUM_Translators] =   
  {
	{ 'q',      event_Quit,       0 },    
    { (char)27, event_Quit,             0 },

    { 'x',      event_Rotate_Glyph,     -1 },
    { 'c',      event_Rotate_Glyph,      1 },
    { 'v',      event_Rotate_Glyph,    -16 },
    { 'b',      event_Rotate_Glyph,     16 },

    { '{',      event_Change_Glyph, -10000 },
    { '}',      event_Change_Glyph,  10000 },
    { '(',      event_Change_Glyph,  -1000 },
    { ')',      event_Change_Glyph,   1000 },
    { '9',      event_Change_Glyph,   -100 },
    { '0',      event_Change_Glyph,    100 },
    { 'i',      event_Change_Glyph,    -10 },
    { 'o',      event_Change_Glyph,     10 },
    { 'k',      event_Change_Glyph,     -1 },
    { 'l',      event_Change_Glyph,      1 },

    { '+',      event_Scale_Glyph,      10 },
    { '-',      event_Scale_Glyph,     -10 },
    { 'u',      event_Scale_Glyph,       1 },
    { 'j',      event_Scale_Glyph,      -1 }
  };
    for ( i = 0; i < NUM_Translators; i++ )
    {
      if ( nChar == trans[i].key )
      {
        evevent.what = trans[i].event_class;
        evevent.info = trans[i].event_info;
		break;
      }
    }
    if (i>= NUM_Translators)
	{
	 evevent.what=event_Keyboard;
	 evevent.what=nChar;
	}
    rc=SetEvent(evgetevent);
}

void CTestw32Dlg::OnSelectAction() 
{
  char c[2];
  GetDlgItemText(IDC_ACTION,c,2);
  translate_command(c[0]);
}


DWORD WINAPI ThreadSpool(LPVOID Parm)
{ 
  while(1)
  {
    if (pipe_std[0]) readpipe(pipe_std[0]); // will never get out of there !!!!
//   if (pipe_err[0]) readpipe(pipe_err[0]);
	Sleep(1000);
  }
  return 1;
}
  
void readpipe(int h)
  {	int i,j,rc;

   char buffer[1024],line[1024];
	rc=1;
   while(rc)
   {
    rc=read(h,buffer,sizeof(buffer));
    if (rc)
    { j=0;
    for(i=0;i<rc;i++)
	{
	  if (buffer[i]=='\n')
	  {  line[j]=0;
	     ::SendMessage((HWND)listbox,LB_ADDSTRING,0,(LPARAM)line);
	   j=0;
	  }
	  else
		line[j++]=buffer[i];
    }
    line[j]=0; //flush the buffer
	::SendMessage((HWND)listbox,LB_ADDSTRING,0,(LPARAM)line);
   }
  }

}


void CTestw32Dlg::OnDestroy() 
{ int rc;
	CDialog::OnDestroy();
	if (spool_thread!=0)
	{
	 rc=TerminateThread(spool_thread,2);
	 spool_thread=0;
	}
  dup2(old_std,_fileno(stdout));
  dup2(old_err,_fileno(stderr));
}
