/* */
call RxFuncAdd "SysLoadFuncs", "RexxUtil", "SysLoadFuncs"
call SysLoadFuncs


say "Warning: This is your last chance to back out. If you do not wish to"
say "continue, please just press ENTER. Otherwise please type ""yes"""

pull letter
if letter <> "YES" then exit

/* Find drive where OS/2 is installed        */
bootdrive = SysSearchPath('PATH', 'OS2.INI')
/* say os2path */
bootdrive = left(bootdrive, 2)

copy "FREETYPE.DLL " || bootdrive || "\os2\dll"
if rc <> 0 then do
   say "Error: Could not copy file!"
   pause
   exit
end

app = "PM_Font_Drivers"
key = "TRUETYPE"
val = "\OS2\DLL\FREETYPE.DLL" || d2c(0)

SysIni('BOTH', app, key, val)
say "Font Driver is installed. Please reboot."
pause
