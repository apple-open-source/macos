/* $XFree86: xc/lib/font/stubs/regfpefunc.c,v 1.2 1999/08/21 13:48:07 dawes Exp $ */

#include "stubs.h"

int 
RegisterFPEFunctions(NameCheckFunc name_func, 
		     InitFpeFunc init_func, 
		     FreeFpeFunc free_func, 
		     ResetFpeFunc reset_func, 
		     OpenFontFunc open_func, 
		     CloseFontFunc close_func, 
		     ListFontsFunc list_func, 
		     StartLfwiFunc start_lfwi_func, 
		     NextLfwiFunc next_lfwi_func, 
		     WakeupFpeFunc wakeup_func, 
		     ClientDiedFunc client_died, 
		     LoadGlyphsFunc load_glyphs, 
		     StartLaFunc start_list_alias_func, 
		     NextLaFunc next_list_alias_func, 
		     SetPathFunc set_path_func)
{
    return 0;
}

/* end of file */
