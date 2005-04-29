// mk4lua.cpp --
// $Id: mk4lua.cpp,v 1.3 2003/11/23 01:42:50 wcvs Exp $
// This is part of Metakit, see http://www.equi4.com/metakit/
//
//  This is the Lua-specific code to turn Metakit into a Lua extension.

#include "luna2.h"

#include "mk4.h"
#include "mk4io.h"

/////////////////////////////////////////////////////////////////////////////

static c4_Property nullProp ('S', "?");

class MkProperty : public c4_Property
{
public:
  MkProperty(Lua& lua) : c4_Property (nullProp) {
    char type = ++lua;
    const char* name = ++lua;
    *(c4_Property*) this = c4_Property (type, name);
  }

  void name(Lua& lua) { lua << Name(); }
  void type(Lua& lua) { lua << Type(); }
  void id(Lua& lua) { lua << GetId(); }

  static const char className[];
  static const Luna<MkProperty>::RegType regTable[];
};

const char MkProperty::className[] = "MkProperty";
const Luna<MkProperty>::RegType MkProperty::regTable[] = {
  {"name", &MkProperty::name},
  {"type", &MkProperty::type},
  {"id", &MkProperty::id},
  {0}
};

/////////////////////////////////////////////////////////////////////////////

static c4_Row nullRow;

class MkRowRef : public c4_Cursor
{
  c4_Row* _row;

  static int getprop(lua_State* L)
  {
    assert(lua_isuserdata(L, 1) && lua_tag(L, 1) == Luna<MkRowRef>::otag);
    MkRowRef* r = (MkRowRef*) lua_touserdata(L, 1);
    assert(r != 0);
    Lua lua (L, 1);
    const char* name = ++lua;
    const c4_View& vw = (**r).Container();
    int i = vw.FindPropIndexByName(name);
    if (i >= 0) {
      const c4_Property& p (vw.NthProperty(i));
      switch (p.Type()) {
	case 'I': lua << ((const c4_IntProp&) p).Get(**r); break;
	case 'F': lua << ((const c4_FloatProp&) p).Get(**r); break;
	case 'D': lua << ((const c4_DoubleProp&) p).Get(**r); break;
	case 'S': lua << ((const c4_StringProp&) p).Get(**r); break;
	case 'B':
	  {
	    c4_Bytes data = ((const c4_BytesProp&) p).Get(**r);
	    lua_pushlstring(L, (const char*) data.Contents(), data.Size());
	  }
	  return 1;
	default:
	  assert(false);
      }
    }
    return lua.numresults();
  }
  static int setprop(lua_State* L)
  {
    assert(lua_isuserdata(L, 1) && lua_tag(L, 1) == Luna<MkRowRef>::otag);
    MkRowRef* r = (MkRowRef*) lua_touserdata(L, 1);
    assert(r != 0);
    Lua lua (L, 1);
    const char* name = ++lua;
    const c4_View& vw = (**r).Container();
    int i = vw.FindPropIndexByName(name);
    assert(i >= 0);
    const c4_Property& p (vw.NthProperty(i));
    switch (p.Type()) {
      case 'I': ((const c4_IntProp&) p).Set(**r, ++lua); break;
      case 'F': ((const c4_FloatProp&) p).Set(**r, ++lua); break;
      case 'D': ((const c4_DoubleProp&) p).Set(**r, ++lua); break;
      case 'S': ((const c4_StringProp&) p).Set(**r, ++lua); break;
      case 'B':
	{
	  const char* data = ++lua;
	  int len = lua_strlen(lua, 2);
	  ((const c4_BytesProp&) p).Set(**r, c4_Bytes (data, len));
	}
	break;
      default:
	assert(false);
    }
    return lua.numresults();
  }
public:
  MkRowRef (Lua& lua);
  ~MkRowRef () { delete _row; }

  static const char className[];
  static const Luna<MkRowRef>::RegType regTable[];
};

const char MkRowRef::className[] = "MkRowRef";
const Luna<MkRowRef>::RegType MkRowRef::regTable[] = {
  {0}
};

/////////////////////////////////////////////////////////////////////////////

class MkView : public c4_View
{
public:
  static c4_View* pushnew(Lua& lua) { return Luna<MkView>::create(lua); }

  MkView(Lua& lua) { }

  void getsize(Lua& lua) { lua << GetSize(); }
  void setsize(Lua& lua) { SetSize(++lua); }

  void insert(Lua& lua) {
    assert(false);//XXX
  }
  void append(Lua& lua) {
    assert(false);//XXX
  }
  void xdelete(Lua& lua) {
    int pos = ++lua;
    int count = 1; lua >> count;
    RemoveAt(pos-1, count);
  }
  void select(Lua& lua) {
    assert(false);//XXX
  }
  void sort(Lua& lua) {
    *pushnew(lua) = Sort();
  }
  void product(Lua& lua) {
    MkView* a = Luna<MkView>::getarg(lua); assert(a != 0);
    *pushnew(lua) = Product(*a);
  }
  void xunion(Lua& lua) {
    MkView* a = Luna<MkView>::getarg(lua); assert(a != 0);
    *pushnew(lua) = Union(*a);
  }
  void intersect(Lua& lua) {
    MkView* a = Luna<MkView>::getarg(lua); assert(a != 0);
    *pushnew(lua) = Intersect(*a);
  }
  void different(Lua& lua) {
    MkView* a = Luna<MkView>::getarg(lua); assert(a != 0);
    *pushnew(lua) = Different(*a);
  }
  void minus(Lua& lua) {
    MkView* a = Luna<MkView>::getarg(lua); assert(a != 0);
    *pushnew(lua) = Minus(*a);
  }
  void remapwith(Lua& lua) {
    MkView* a = Luna<MkView>::getarg(lua); assert(a != 0);
    *pushnew(lua) = RemapWith(*a);
  }
  void pair(Lua& lua) {
    MkView* a = Luna<MkView>::getarg(lua); assert(a != 0);
    *pushnew(lua) = Pair(*a);
  }

  static const char className[];
  static const Luna<MkView>::RegType regTable[];
};

const char MkView::className[] = "MkView";
const Luna<MkView>::RegType MkView::regTable[] = {
  {"getsize", &MkView::getsize},
  {"setsize", &MkView::setsize},
  {"insert", &MkView::insert},
  {"append", &MkView::append},
  {"delete", &MkView::xdelete},
  {"select", &MkView::select},
  {"sort", &MkView::sort},
  {"product", &MkView::product},
  {"union", &MkView::xunion},
  {"intersect", &MkView::intersect},
  {"different", &MkView::different},
  {"minus", &MkView::minus},
  {"remapwith", &MkView::remapwith},
  {"pair", &MkView::pair},
  {0}
};

/////////////////////////////////////////////////////////////////////////////

class MkStorage : public c4_Storage
{
public:
  MkStorage(Lua& lua) {
    const char* n = ++lua;
    int f = 0; lua >> f;
    *(c4_Storage*) this = c4_Storage (n, f);
  }

  void view(Lua& lua) { *MkView::pushnew(lua) = View(++lua); }
  void getas(Lua& lua) { *MkView::pushnew(lua) = GetAs(++lua); }
  void autocommit(Lua& lua) { AutoCommit(); }
  void commit(Lua& lua) { int n = 0; lua >> n; lua << Commit(n); }
  void rollback(Lua& lua) { int n = 0; lua >> n; lua << Rollback(n); }
  void contents(Lua& lua) { *MkView::pushnew(lua) = *this; }

  void aside(Lua& lua) {
    MkStorage* a = Luna<MkStorage>::getarg(lua); assert(a != 0);
    SetAside(*a);
  }
  void description(Lua& lua) {
    const char* n = 0; lua >> n;
    lua << Description(n);
  }

  static const char className[];
  static const Luna<MkStorage>::RegType regTable[];
};

const char MkStorage::className[] = "MkStorage";
const Luna<MkStorage>::RegType MkStorage::regTable[] = {
  {"view", &MkStorage::view},
  {"getas", &MkStorage::getas},
  {"autocommit", &MkStorage::autocommit},
  {"commit", &MkStorage::commit},
  {"rollback", &MkStorage::rollback},
  {"contents", &MkStorage::contents},
  {"aside", &MkStorage::aside},
  {"description", &MkStorage::description},
  {0}
};

/////////////////////////////////////////////////////////////////////////////

MkRowRef::MkRowRef (Lua& lua) : c4_Cursor (&nullRow), _row (0) {
  static bool first = true;
  if (first) {
    first = false; // take over indexed access
    lua_pushcfunction(lua, getprop);
    lua_settagmethod(lua, Luna<MkRowRef>::otag, "gettable");
    lua_pushcfunction(lua, setprop);
    lua_settagmethod(lua, Luna<MkRowRef>::otag, "settable");
  }
  if (lua.more()) {
    MkView* v = (MkView*) lua_touserdata(++lua, 2); // skip table
    assert(v != 0);
    if (lua.more()) { // got <view,index>, create a rowref from it
      *(c4_Cursor*) this = & (*(c4_View*) v)[++lua];
    } else { // got <view>, use it as template for an empty row
    }
  } else {
    _row = new c4_Row;
    *(c4_Cursor*) this = &(*_row);
  }
}

/////////////////////////////////////////////////////////////////////////////

extern "C" int mk4lua_open(lua_State *L);

int mk4lua_open(lua_State *L)
{
  Luna<MkProperty>::Register(L, 1); // access as fields, not functions
  Luna<MkRowRef>::Register(L);
  Luna<MkView>::Register(L);
  Luna<MkStorage>::Register(L);
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
