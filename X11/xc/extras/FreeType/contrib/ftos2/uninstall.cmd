/* */
call RxFuncAdd "SysLoadFuncs", "RexxUtil", "SysLoadFuncs"
call SysLoadFuncs


say "This utility will remove the FreeType/2 font driver and install "
say "the original TRUETYPE.DLL if it exists. If you do not wish to"
say "continue, please just press ENTER. Otherwise please type ""y"""

pull letter
if letter <> "Y" then exit

/* Find drive where OS/2 is installed        */
bootdrive = SysSearchPath('PATH', 'OS2.INI')
bootdrive = left(bootdrive, 1)

app = "PM_Font_Drivers"
key = "TRUETYPE"

/* look for TRUETYPE.DLL */
rc = SysFileTree(left(bootdrive,1) || ":\OS2\DLL\TRUETYPE.DLL", "file", "F")

if file.0 = 1 then do
   say "Restoring TRUETYPE.DLL..."
   val = "\OS2\DLL\TRUETYPE.DLL" || d2c(0)
   SysIni('BOTH', app, key, val)
end
else do
   say "Uninstalling FREETYPE.DLL..."
   SysIni('BOTH', app, key, "DELETE:")
end

say "FTIFI is uninstalled. Please reboot."
pause
