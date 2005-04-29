-- utility code, included in other scripts
-- 01/02/2001 jcw@equi4.com

-- dump data, adapted from lua/test/save.lua
function savevar (n,v)
 if v == nil then return end
 if type(v)=="userdata" or type(v)=="function" then return end
 -- if type(v)=="userdata" or type(v)=="function" then write("\t-- ") end
 write(n,"=")
 if type(v) == "string" then write(format("%q",v))
 elseif type(v) == "table" then
   if v.__visited__ ~= nil then
     write(v.__visited__, "\n")
   else
    write("{}\n")
    v.__visited__ = n
    for r,f in v do
      if r ~= "__visited__" then
	if type(r) == 'string' then
	  savevar(n.."."..r,f)
	else
	  savevar(n.."["..r.."]",f)
	end
      end
    end
   end
   return
 else write(tostring(v)) end
 write("\n")
end
-- pretty displays a value, properly dealing with tables and cycles
local displayvalue=
  function (s)
    if not s or type(s)=='function' or type(s)=='userdata' then
      s=tostring(s)
    elseif type(s)~='number' then
      s=gsub(format('%q',s),'^"([^"\']*)"$',"'%1'")
    end
    return s
  end
local askeystr=
  function (u,s)
    if type(u)=='string' and strfind(u,'^[%w_]+$') then return s..u end
    return '['..%displayvalue(u)..']'
  end
local horizvec=
  function (x,n)
    local o,e='',''
    for i=1,getn(x) do
      if type(x[i])=='table' then return end
      o=o..e..%displayvalue(x[i])
      if strlen(o)>n then return end
      e=','
    end
    return '{'..o..'}'
  end
local horizmap=
  function (x,n)
    local o,e='',''
    for k,v in x do
      if type(v)=='table' then return end
      o=o..e..%askeystr(k,'')..'='..%displayvalue(v)
      if strlen(o)>n then return end
      e=','
    end
    return '{'..o..'}'
  end
function pretty(p,x,h,q)
  if not p then p,x='globals',globals() end
  if type(x)=='table' then
    if not h then h={} end
    if h[x] then
      x=h[x]
    else
      if not q then q=p end
      h[x]=q
      local s={}
      for k,v in x do tinsert(s,k) end
      if getn(s)>0 then
	local n=75-strlen(p)
	local f=getn(s)==getn(x) and %horizvec(x,n)
	if not f then f=%horizmap(x,n) end
	if not f then
	  sort(s,function (a,b)
		   if tag(a)~=tag(b) then a,b=tag(b),tag(a) end
		   return a<b
		 end)
	  for i=1,getn(s) do
	    if s[i] then
	      local u=%askeystr(s[i],'.')
	      pretty(p..u,x[s[i]],h,q..u)
	      p=strrep(' ',strlen(p))
	    end
	  end
	  return
	end
	x=f
      else
        x='{}'
      end
    end
  else
    x=%displayvalue(x)
  end
  print(p..' = '..x)
end
-- lispy is a compact pretty-printer for limited datasets
local showasstr=
  function (s)
    return type(s)=='string' and format('%q',s) or tostring(s)
  end
horizstr=
  function (x,n)
    if type(x)~='table' then return %showasstr(x) end
    if getn(x)==0 then return '()' end
    local o,e=x[1]
    for i=2,getn(x) do
      local s=x[i]
      if type(s)=='table' then
        s=horizstr(s,n-strlen(o))
      else
        s=%showasstr(s)
      end
      if not s then return end
      o=o..' '..s
      if strlen(o)>n then return end
    end
    return '('..o..')'
  end
function lispy(t,l,f)
  local n=strlen(t)
  local h=horizstr(l,70-n)
  if h then
    write(t,' ',h)
  else
    assert(type(l)=='table',l)
    t=t..' ('..l[1]
    local e=''
    for i=2,getn(l) do
      write(e)
      lispy(t,l[i],1)
      t=strrep(' ',strlen(t))
      e='\n'
    end
    write(')')
  end
  if not f then write('\n') end
end
-- dump global names (or another table), summarizing keys (and scalar values)
function gdump(g)
  g=g or globals()
  local s={}
  for i,v in g do
    tinsert(s, i)
  end
  call(sort,{s},"x",nil) --XXX comparisons can fail
  local f,t,u="","",""
  print("\n<<<Scalars:>>>")
  for i=1,getn(s) do
    local n=s[i]
    if type(g[n]) == 'function' then
      f=f.." "..n
    elseif type(g[n]) == 'table' then
      t=t.." "..n
    elseif type(g[n]) == 'userdata' then
      u=u.." "..n
    else
      print(format("  %11s =",n),g[n])
    end
  end
  if f~="" then print("\n<<<Functions>>>",f) end
  if t~="" then print("\n<<<Tables>>>",t) end
  if u~="" then print("\n<<<Userdata>>>",u) end
  print("\nTotal:",getn(s))
end
-- evaluate each line in turn, optionally comparing results
-- if a comparison is included and it matches, nil is returned
-- otherwise, output is such that it can be re-used as match input
function tester(lines)
  local t={}
  gsub(lines,"\n*([^\n]+)\n*", function (v) tinsert(%t,v) end)
  local f = function (s,t)
    local r=dostring('return '..s)
    if type(r)=="nil" then
      r="nil"
    elseif type(r)=="number" then
      r=r..""
    elseif type(r)=="table" then
      r='table, n = '..getn(r)..', tag = '..tag(r)
    elseif type(r)=="userdata" then
      r='userdata, tag = '..tag(r)
    elseif type(r)=="function" then
      r=type(r)
    else
      r=format('%q',r)
    end
    if r~=t then return format('%36s == %s',s,r) end
  end
  local m,c=0,0
  for i=1,getn(t) do
    local l=gsub(gsub(t[i],'^ +',''),' +$','')
    if i==getn(t) and l=="" then break end
    if strfind(l,' == ') then
      c=c+1
      local o=gsub(l,'^(.*) == (.*)$',f,1)
      if o~="" then print(o) else m=m+1 end
    else
      print(f(l))
    end
  end
  if m<c then print(m..' results out of '..c..' matched') end
end
-- join vector with separator string
function join(v,s)
  s=s or " "
  local r=""
  for i=1,getn(v) do
    if i>1 then r=r..s end
    r=r..(v[i] or "nil")
  end
  return r
end
-- split string on character set (freely adapted from cgilua)
function strsplit(s,c)
  local t={}
  gsub(s,"["..c.."]*([^"..c.."]+)["..c.."]*", 
	function (v) tinsert(%t,v) end)
  return t
end
-- return file contents, or nil if reading failed
function fetchfile(p)
  local f,e=openfile(p,'rb')
  if f then
    local d=read(f,'*a')
    closefile(f)
    return d
  else
    error(p..': '..e)
  end
end
-- escape the most basic characters in html
function htmlize(s)
  --s=gsub(s,'&amp;','&')
  s=gsub(s,'&','&amp;')
  s=gsub(s,'<','&lt;')
  s=gsub(s,'>','&gt;')
  return s
end
-- split a string on the newline character
function linesplit(s)
  local t={}
  gsub(s,"([^\n]*)\n",function (v) tinsert(%t,v) end)
  return t
end
-- convert raw string to big-endian int
function beInt(s)
  local v=0
  for i=1,strlen(s) do v=v*256+strbyte(s,i) end
  return v
end
-- convert raw string to little-endian int
function leInt(s)
  local v=0
  for i=strlen(s),1,-1 do v=v*256+strbyte(s,i) end
  return v
end
-- cut up a string in big-endian ints of given size
function beStrCuts(s,...)
  local o,r=1,{}
  for i=1,getn(arg) do
    tinsert(r,beInt(strsub(s,o,o+arg[i]-1)))
    o=o+arg[i]
  end
  return r
end
-- cut up a string in little-endian ints of given size
function leStrCuts(s,...)
  local o,r=1,{}
  for i=1,getn(arg) do
    tinsert(r,leInt(strsub(s,o,o+arg[i]-1)))
    o=o+arg[i]
  end
  return r
end
-- convert string to hex chars
function hex(s)
  -- is this faster than "for i=1,strlen(s) do ... end"?
  local r=gsub(s,'(.)', function (c)
			  return format('%02X',strbyte(c))
			end)
  return r
end
-- map a function to each element in a vector
function map(t,f)
  local r={}
  for i=1,getn(t) do tinsert(r,f(t[i])) end
  return r
end
-- filter only those elements which are selected by f
function filter(t,f)
  local r={}
  for i=1,getn(t) do
    if f(t[i]) then tinsert(r,t[i]) end
  end
  return r
end
-- implement floor, without requiring the math lib
function intval(v)
  return bor(v,0)
end
