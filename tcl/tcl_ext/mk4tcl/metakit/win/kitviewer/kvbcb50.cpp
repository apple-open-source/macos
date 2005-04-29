//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop
USERES("kvbcb50.res");
USEFORM("kview.cpp", MainForm);
USEUNIT("kviewer.cpp");
USELIB("..\bcb50\metakit.lib");
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	try
	{
		Application->Initialize();
		Application->CreateForm(__classid(TMainForm), &MainForm);
		Application->Run();
	}
	catch (Exception &exception)
	{
		Application->ShowException(&exception);
	}
	return 0;
}
//---------------------------------------------------------------------------
