/* */
call RxFuncAdd "SysLoadFuncs", "RexxUtil", "SysLoadFuncs"
call SysLoadFuncs

app = "PM_Font_Drivers"
key = "TRUETYPE"

val = SysIni('USER', app, key)

if val = "ERROR:" then val = "none"
/* strip the terminating NULL character */
else val = substr(val, 1, pos(d2c(0), val) - 1)

say 'The current TrueType driver is ' || val
pause
