// mkdemo.cpp : Defines the entry point for the application.
//

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>

int _cdecl main();

int WINAPI WinMain( HINSTANCE hInstance,
		    HINSTANCE hPrevInstance,
		    LPTSTR    lpCmdLine,
		    int       nCmdShow)
{
  fclose(stdout);
  fopen("stdout.txt", "w");
  fclose(stderr);
  fopen("stderr.txt", "w");

  return main();
}

