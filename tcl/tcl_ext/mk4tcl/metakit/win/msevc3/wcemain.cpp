#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// http://msdn.microsoft.com/library/en-us/dnhhpc/html/hpc_ojects.asp

#if defined(WIN32_PLATFORM_PSPC) 
#include <aygshell.h>                      // Add Pocket PC includes 
#pragma comment( lib, "aygshell" )         // Link Pocket PC library 
#endif

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
