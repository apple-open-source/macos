/* */
call RxFuncAdd "SysLoadFuncs", "RexxUtil", "SysLoadFuncs"
call SysLoadFuncs

Parse Upper Arg a1 a2

app = "FreeType/2"
key = "OPENFACES"

if Arg() = 0 then call usage

if a1 = 'Q' then call query

if a1 = 'S' then call set
call usage

set:

val = a2

if val = '' then do
   say 'Invalid limit!'
   exit
end

if val < 8 then do
   say 'The lowest acceptable limit is 8!'
   pause
   exit
end

szval = val || d2c(0)

rc = SysIni('USER', app, key, szval)
say rc
if rc = 'ERROR:' then do
   say 'Error updating OS2.INI!'
   pause
   exit
end

say "Open faces limit updated to " || val || ". Please reboot to activate changes."
pause
exit

query:
val = SysIni('USER', app, key)
if val = "ERROR:" then val = "not set"
/* strip the terminating NULL character */
else val = substr(val, 1, pos(d2c(0), val) - 1)

say 'The current open faces limit is ' || val
pause
exit

usage:
say 'This program is used to set the limit of concurrently open typefaces for'
say 'FreeType/2. Use lower numbers to limit memory consumption and higher to'
say 'improve performance if you use lots of fonts.'
say
say 'Usage: LIMIT q         - query the current limit'
say '       LIMIT s  <val>  - set limit to <val> (effective on next reboot).'
pause
