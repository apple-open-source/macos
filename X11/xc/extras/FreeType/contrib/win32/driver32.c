/*********************************************************/
/* Test program driver for freetype  on Win32 Platform   */
/* CopyRight(left) G. Ramat 1998 (gcramat@radiostudio.it)*/
/*                                                       */
/*********************************************************/

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>

#include "freetype.h"
#include "gdriver.h"

HANDLE    evgetevent,evdriverdisplaybitmap,this_wnd,main_thread,listbox,bitmap;
TEvent    evevent;
char message_32[256];
char *ev_buffer;
jmp_buf Env;
long TTMemory_Allocated = 0;  // just to have a clean link with ftdump 
// save last rendered image Data
int   save_lines,save_cols,exit_code;
char *save_buffer;
extern int vio_Width,vio_Height,vio_ScanLineWidth;
extern  TT_Raster_Map  Bit;
HDC hdc,memdc;
HBITMAP hbm,hbm1;

//________________________________________________________________________________
void Get_Event(TEvent *event)
{   
	WaitForSingleObject(evgetevent,INFINITE);  // wait for completion
    *event=evevent;       //set by message handler before posting waited upon event
	return;
}

int  Driver_Set_Graphics( int  mode )
{   RECT rect;
	 GetClientRect(bitmap,&rect);
	 vio_Width=rect.right-rect.left;
	 vio_Height = rect.bottom-rect.top;
	 vio_ScanLineWidth=vio_Width;
	 return 1;
	 
 }
int	 Driver_Restore_Mode()
{return 1;}

int  Driver_Display_Bitmap( char*  buffer, int  lines, int  cols )
 {
  long rc;
  int i;
  char *top,*bottom;
  HANDLE rgdi;
  RECT rect;
  char *w_buffer;
//  bitmap=listbox;
  hdc=GetDC(bitmap);
  memdc=CreateCompatibleDC(hdc);
  GetClientRect(bitmap,&rect);
  //hbm=CreateCompatibleBitmap(hdc,lines,cols);
  // need to  set upside down bitmap .
  if (buffer != save_buffer)     //new buffer
  {
    if (save_buffer!=NULL) 
	  free(save_buffer);
    save_buffer=(char *)malloc(Bit.size);
    memcpy(save_buffer,buffer,Bit.size);
  }
  w_buffer=malloc(Bit.size);  // hope it succeeds 
  top=buffer;
  bottom=w_buffer+Bit.size-cols;
  for(i=0;i<Bit.size;i+=cols)
  { 
    memcpy(bottom,top,cols);
	top+=cols;
	bottom-=cols;  
  };
  save_lines=lines;
  save_cols=cols;
  hbm=CreateBitmap(vio_Width,vio_Height,1,1,w_buffer);
  rgdi=SelectObject(memdc,hbm);
 // rc=SetBitmapBits(hbm,Bit.size,buffer);  //redundant 
  rc=StretchBlt(hdc,0,0,rect.right,rect.bottom,memdc,0,0,rect.right,rect.bottom,MERGECOPY);
  ReleaseDC(bitmap,hdc);
  DeleteObject(memdc);
  rc=UpdateWindow(bitmap);
  return 1;
 }


int   call_test_program(int (*program)(int,char**),int argc,char **argv)
{int rc;
// prepare return  address ( for exit)
if(0==setjmp(Env)) //env set : call prog
  rc=program(argc,argv);
 return rc;
}

void force_exit(int code)
{ char *p=NULL;
	longjmp(Env,code);
	//disable piping
}
