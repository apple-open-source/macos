//---------------------------------------------------------------------------
#include <vcl\vcl.h>
#pragma hdrstop
//---------------------------------------------------------------------------
USERES("kviewer.res");
USEFORM("kview.cpp", MainForm);
USEUNIT("..\..\src\viewx.cpp");
USEUNIT("..\..\src\custom.cpp");
USEUNIT("..\..\src\derived.cpp");
USEUNIT("..\..\src\field.cpp");
USEUNIT("..\..\src\fileio.cpp");
USEUNIT("..\..\src\format.cpp");
USEUNIT("..\..\src\handler.cpp");
USEUNIT("..\..\src\persist.cpp");
USEUNIT("..\..\src\std.cpp");
USEUNIT("..\..\src\store.cpp");
USEUNIT("..\..\src\string.cpp");
USEUNIT("..\..\src\table.cpp");
USEUNIT("..\..\src\univ.cpp");
USEUNIT("..\..\src\view.cpp");
USEUNIT("..\..\src\column.cpp");
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	try
	{
		Application->Initialize();
		Application->Title = "Kit Viewer";
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
