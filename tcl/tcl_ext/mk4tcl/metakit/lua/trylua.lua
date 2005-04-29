#!/usr/bin/env lux

dofile('util.lua')

local al,e=dynopen("./Mk4lua.so")
if not al then print(e) end

dyncall(al,"mk4lua_open")

--gdump()
gdump(MkView)
--gdump(MkStorage)

p=MkProperty('I','age')
tester [[
                              p.name == "age"
                              p.type == "I"
                                p.id == 1 
]]

local a = MkStorage("aaa",1)
--local a = MkStorage "aaa"
a:autocommit()

--print(a,tag(a))
--print(gettagmethod(tag(a),"index"))
--print('view',MkView,tag(MkView))
--print('view',gettagmethod(tag(MkView),"gettable"))
--print('Storage',MkStorage,tag(MkStorage))
--print('Storage',gettagmethod(tag(MkStorage),"gettable"))

--print(a:haha())

local v=a:getas("haha[name:S]")
--print('<<<1>>>',v)
print(v:getsize())
v:setsize(123)
print(v:getsize())
v:delete(23)
print(v:getsize())
v:delete(34,10)
print(v:getsize())

print(v,v:sort())
print(a:description())
print(a:contents():getsize())

r=MkRowRef()
print(r,tag(r))
--print("abc",r.abc)

r=MkRowRef(v,12)
r.name="this is a weird name"

print(r.name)

--a:commit()

print('<<<2>>>')
--gdump()
--print('<<<3>>>')
--gdump(a)
--print('<<<4>>>')
--gdump(v)
